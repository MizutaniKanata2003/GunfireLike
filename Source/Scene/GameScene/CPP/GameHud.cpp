#include "../H/GameHud.h"

// HUDの画像の読込・描画と、弾薬・リロード・回復アイテム数・通貨の文字の描画を実装する。
// 画像の配置は表から読み、状態に応じた拡縮・色・表示割合・文字の大きさだけをこのファイルで決める。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampでHPの割合を0〜1へ制限し、std::minで文字の拡大率を領域に収まる値に制限する。
#include <string> // std::to_wstringで弾薬数・進み具合・通貨の数字を文字列へ変換し、結合する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/UI/H/UiLayout.h"

// HUDの表示ルールに使う固定設定をまとめる。企画書で未確定の見た目の数値は、仮値。
namespace
{
	//========= 低HPの設定=========
	// HPの割合がこの値以下なら、低HPとして警告を表示し、HPバーを赤くする。企画書や旧実装の25%を維持する。
	constexpr float ns_LOW_HP_RATIO = 0.25f;

	//========= 照準の設定=========
	// 構えている間の、照準の大きさの倍率。中心を保ったまま小さくして、精度が上がったことを示す。仮値。
	constexpr float ns_AIM_CROSSHAIR_SCALE = 0.5f;

	//========= 色=========
	// 画像に乗算しない色。白で、画像本来の色になる。
	const XMFLOAT4 ns_WHITE{1.0f,1.0f,1.0f,1.0f};
	// 通常時のHPバーの色。緑。画像は白のグラデーションのため、この色が反映される。
	const XMFLOAT4 ns_HP_NORMAL_COLOR{0.2f,0.95f,0.3f,1.0f};
	// 低HP時のHPバーの色。赤。
	const XMFLOAT4 ns_HP_LOW_COLOR{1.0f,0.2f,0.15f,1.0f};
	// 弾薬の文字の色。白。
	const XMVECTORF32 ns_AMMO_TEXT_COLOR{{{1.0f,1.0f,1.0f,1.0f}}};
	// リロード表示の文字の色。橙。
	const XMVECTORF32 ns_RELOAD_TEXT_COLOR{{{1.0f,0.6f,0.2f,1.0f}}};
	// 回復アイテム数の文字の色。緑がかった白。
	const XMVECTORF32 ns_HEAL_TEXT_COLOR{{{0.8f,1.0f,0.8f,1.0f}}};
	// 通貨の数字の色。金色。通貨のアイコンの色に合わせる。
	const XMVECTORF32 ns_CURRENCY_TEXT_COLOR{{{1.0f,0.85f,0.3f,1.0f}}};

	//========= 弾薬の文字の配置=========
	// 弾薬の枠の左端から、数字を置く領域の左端までの距離。基準Canvas上の大きさ。左側の弾のアイコンを避けるための値。
	constexpr float ns_AMMO_TEXT_AREA_LEFT = 52.0f;
	// 弾薬の枠の右端から、数字を置く領域の右端までの余白。基準Canvas上の大きさ。
	constexpr float ns_AMMO_TEXT_AREA_RIGHT_MARGIN = 10.0f;
	// 弾薬の数字の高さの目標。基準Canvas上の大きさ。フォントの元の大きさに関係なく、この高さに近づけて描く。
	constexpr float ns_AMMO_TEXT_TARGET_HEIGHT = 30.0f;

	//========= リロード表示の文字の配置=========
	// リロード表示の高さの目標。基準Canvas上の大きさ。
	constexpr float ns_RELOAD_TEXT_TARGET_HEIGHT = 24.0f;
	// リロード表示の領域の高さ。弾薬の枠の真上に、この高さの領域を置く。基準Canvas上の大きさ。
	constexpr float ns_RELOAD_TEXT_AREA_HEIGHT = 28.0f;

	//========= 回復アイテム数の文字の配置=========
	// 回復アイテム数の文字の高さの目標。基準Canvas上の大きさ。
	constexpr float ns_HEAL_TEXT_TARGET_HEIGHT = 22.0f;
	// 回復アイテム数の領域の幅。HPバーの左端に合わせ、HPバーの真上に置く。基準Canvas上の大きさ。
	constexpr float ns_HEAL_TEXT_AREA_WIDTH = 120.0f;
	// 回復アイテム数の領域の高さ。HPバーの上端との間の高さ。基準Canvas上の大きさ。
	constexpr float ns_HEAL_TEXT_AREA_HEIGHT = 28.0f;

	//========= 通貨の数字の配置=========
	// 通貨の数字の高さの目標。基準Canvas上の大きさ。アイコン（48）より小さくして、上下の余白を残す。
	constexpr float ns_CURRENCY_TEXT_TARGET_HEIGHT = 30.0f;
	// 通貨の数字を置く領域の幅。アイコンの左隣に置く。5桁（上限99999）が収まる大きさ。基準Canvas上の大きさ。
	constexpr float ns_CURRENCY_TEXT_AREA_WIDTH = 132.0f;
	// 通貨の数字を置く領域の右端と、アイコンの左端との間の余白。基準Canvas上の大きさ。
	constexpr float ns_CURRENCY_TEXT_GAP = 8.0f;
}

//========= 初期化・描画関数=========
// 画像用・文字用のRendererを初期化し、表の画像を全て読み込む。
bool GameHud::Initialize(GraphicsSystem& graphicsSystem)
{
	// 旧Resourceを先に解放する。以後の失敗でも旧状態へは復帰しない。
	Uninit();

	// 画像用Rendererの初期化失敗を検出したら、解放してfalseを返す。
	if(!m_spriteRenderer.Initialize(graphicsSystem)){
		Uninit();
		return false;
	}

	// 文字用Rendererの初期化失敗（Device・Contextなし）を検出したら、先に初期化した画像用Rendererも含めて解放してfalseを返す。
	if(!m_textRenderer.Initialize(graphicsSystem)){
		Uninit();
		return false;
	}

	// 配置表の画像を順に読み込む。識別値が0（読込失敗）のものがあっても、全体の失敗にはせず、その画像だけを描かない。
	m_crosshairTexture = m_spriteRenderer.LoadTexture(graphicsSystem,HUD_LAYOUT::ns_CROSSHAIR.texturePath);
	m_hpFrameTexture = m_spriteRenderer.LoadTexture(graphicsSystem,HUD_LAYOUT::ns_HP_FRAME.texturePath);
	m_hpFillTexture = m_spriteRenderer.LoadTexture(graphicsSystem,HUD_LAYOUT::ns_HP_FILL.texturePath);
	m_ammoFrameTexture = m_spriteRenderer.LoadTexture(graphicsSystem,HUD_LAYOUT::ns_AMMO_FRAME.texturePath);
	m_currencyIconTexture = m_spriteRenderer.LoadTexture(graphicsSystem,HUD_LAYOUT::ns_CURRENCY_ICON.texturePath);
	m_lowHpWarningTexture = m_spriteRenderer.LoadTexture(graphicsSystem,HUD_LAYOUT::ns_LOW_HP_WARNING.texturePath);
	return true;
}

// 画像を描いてから、文字を描く。画像と文字でBatchの状態が異なるため、この順に分ける。
void GameHud::Draw(GraphicsSystem& graphicsSystem,const GAME_HUD_STATE& state)
{
	// 最大HPが正の場合だけ、HPの割合を0〜1で求める。0以下なら割合を0とし、HPに関する表示を省く。
	const bool has_health = state.maxHP > 0.0f;
	const float health_ratio = has_health ? std::clamp(state.currentHP / state.maxHP,0.0f,1.0f) : 0.0f;

	// HPの割合が低HPの閾値以下なら、画面全体の警告を最初に描き、他のHUDがその上に重なるようにする。
	if(has_health && health_ratio <= ns_LOW_HP_RATIO){
		m_spriteRenderer.DrawSprite(graphicsSystem,m_lowHpWarningTexture,HUD_LAYOUT::ns_LOW_HP_WARNING.position,
																HUD_LAYOUT::ns_LOW_HP_WARNING.size,ns_WHITE,1.0f);
	}

	// 照準を、配置表の中心を保ったまま、構え中は小さく描く。中心 = 左上 + 大きさ/2、描く左上 = 中心 - 描く大きさ/2。
	const float crosshair_scale = state.isAiming ? ns_AIM_CROSSHAIR_SCALE : 1.0f;
	const XMFLOAT2 crosshair_size{HUD_LAYOUT::ns_CROSSHAIR.size.x * crosshair_scale,HUD_LAYOUT::ns_CROSSHAIR.size.y * crosshair_scale};
	const XMFLOAT2 crosshair_position{HUD_LAYOUT::ns_CROSSHAIR.position.x + (HUD_LAYOUT::ns_CROSSHAIR.size.x - crosshair_size.x) * 0.5f,
		HUD_LAYOUT::ns_CROSSHAIR.position.y + (HUD_LAYOUT::ns_CROSSHAIR.size.y - crosshair_size.y) * 0.5f};
	m_spriteRenderer.DrawSprite(graphicsSystem,m_crosshairTexture,crosshair_position,crosshair_size,ns_WHITE,1.0f);

	// HPバーの枠を描き、続けて中身を、HPの割合の分だけ左から描く。低HPなら色を赤にする。最大HPがない場合は、HPバー自体を描かない。
	if(has_health){
		m_spriteRenderer.DrawSprite(graphicsSystem,m_hpFrameTexture,HUD_LAYOUT::ns_HP_FRAME.position,HUD_LAYOUT::ns_HP_FRAME.size,ns_WHITE,1.0f);
		const XMFLOAT4& fill_color = health_ratio <= ns_LOW_HP_RATIO ? ns_HP_LOW_COLOR : ns_HP_NORMAL_COLOR;
		m_spriteRenderer.DrawSprite(graphicsSystem,m_hpFillTexture,HUD_LAYOUT::ns_HP_FILL.position,HUD_LAYOUT::ns_HP_FILL.size,fill_color,health_ratio);
	}

	// 弾薬の枠と、通貨のアイコンを描く。数字は、画像の後に文字として描く。
	m_spriteRenderer.DrawSprite(graphicsSystem,m_ammoFrameTexture,HUD_LAYOUT::ns_AMMO_FRAME.position,HUD_LAYOUT::ns_AMMO_FRAME.size,ns_WHITE,1.0f);
	m_spriteRenderer.DrawSprite(graphicsSystem,m_currencyIconTexture,HUD_LAYOUT::ns_CURRENCY_ICON.position,HUD_LAYOUT::ns_CURRENCY_ICON.size,ns_WHITE,1.0f);

	// 文字を描く。装填弾数と予備弾薬を「装填 / 予備」の形で枠の右側へ、リロード中なら進み具合を枠の真上へ描く。
	m_textRenderer.Begin();
	const XMFLOAT2 ammo_area_position{HUD_LAYOUT::ns_AMMO_FRAME.position.x + ns_AMMO_TEXT_AREA_LEFT,HUD_LAYOUT::ns_AMMO_FRAME.position.y};
	const XMFLOAT2 ammo_area_size{HUD_LAYOUT::ns_AMMO_FRAME.size.x - ns_AMMO_TEXT_AREA_LEFT - ns_AMMO_TEXT_AREA_RIGHT_MARGIN,
		HUD_LAYOUT::ns_AMMO_FRAME.size.y};
	DrawFittedText(std::to_wstring(state.magazineAmmo) + L" / " + std::to_wstring(state.reserveAmmo),ammo_area_position,ammo_area_size,
								 ns_AMMO_TEXT_TARGET_HEIGHT,ns_AMMO_TEXT_COLOR);

	// 回復アイテムの所持数を、HPバーの真上へ「Q x数」の形で描く。HPバーの左端に合わせ、HPバーの上端から領域の高さだけ上に置く。
	const XMFLOAT2 heal_area_position{HUD_LAYOUT::ns_HP_FRAME.position.x,HUD_LAYOUT::ns_HP_FRAME.position.y - ns_HEAL_TEXT_AREA_HEIGHT};
	const XMFLOAT2 heal_area_size{ns_HEAL_TEXT_AREA_WIDTH,ns_HEAL_TEXT_AREA_HEIGHT};
	DrawFittedText(L"Q x" + std::to_wstring(state.healItemCount),heal_area_position,heal_area_size,ns_HEAL_TEXT_TARGET_HEIGHT,ns_HEAL_TEXT_COLOR);

	// 通貨の所持額を、アイコンの左隣へ描く。領域の右端がアイコンの左端から余白だけ離れる位置に置き、高さはアイコンと同じにして、上下の中央へ揃える。
	const XMFLOAT2 currency_area_position{HUD_LAYOUT::ns_CURRENCY_ICON.position.x - ns_CURRENCY_TEXT_GAP - ns_CURRENCY_TEXT_AREA_WIDTH,
		HUD_LAYOUT::ns_CURRENCY_ICON.position.y};
	const XMFLOAT2 currency_area_size{ns_CURRENCY_TEXT_AREA_WIDTH,HUD_LAYOUT::ns_CURRENCY_ICON.size.y};
	DrawFittedText(std::to_wstring(state.currency),currency_area_position,currency_area_size,ns_CURRENCY_TEXT_TARGET_HEIGHT,ns_CURRENCY_TEXT_COLOR);

	// リロード中の場合だけ、進み具合を百分率で、弾薬の枠の真上の領域へ描く。進み具合の小数は切り捨てる。
	if(state.isReloading){
		const XMFLOAT2 reload_area_position{HUD_LAYOUT::ns_AMMO_FRAME.position.x,HUD_LAYOUT::ns_AMMO_FRAME.position.y - ns_RELOAD_TEXT_AREA_HEIGHT};
		const XMFLOAT2 reload_area_size{HUD_LAYOUT::ns_AMMO_FRAME.size.x,ns_RELOAD_TEXT_AREA_HEIGHT};
		const int reload_percent = static_cast<int>(std::clamp(state.reloadProgress,0.0f,1.0f) * 100.0f);
		DrawFittedText(L"RELOADING " + std::to_wstring(reload_percent) + L"%",reload_area_position,reload_area_size,ns_RELOAD_TEXT_TARGET_HEIGHT,
									 ns_RELOAD_TEXT_COLOR);
	}
	m_textRenderer.End();
}

// 画像用・文字用のRendererを解放する。
void GameHud::Uninit()
{
	// 文字用・画像用のRendererを解放する。画像の識別値は、画像用Rendererの解放で無効になるため、0へ戻す。
	m_textRenderer.Uninit();
	m_spriteRenderer.Uninit();
	m_crosshairTexture = 0;
	m_hpFrameTexture = 0;
	m_hpFillTexture = 0;
	m_ammoFrameTexture = 0;
	m_currencyIconTexture = 0;
	m_lowHpWarningTexture = 0;
}

//========= 補助関数=========
// 文字の大きさを測り、高さの目標と領域の幅から拡大率を決めて、領域の中央へ描く。
void GameHud::DrawFittedText(const std::wstring& text,const XMFLOAT2& areaPosition,const XMFLOAT2& areaSize,float targetHeight,
														 const XMVECTORF32& color)
{
	// 文字を拡大率1で測った幅と高さ。Font未生成や空文字列では0になる。
	const XMFLOAT2 base_size = m_textRenderer.MeasureText(text,1.0f);
	// 測った幅か高さが0以下なら、拡大率を求められない（0除算になる）ため、何も描かずに戻る。
	if(base_size.x <= 0.0f || base_size.y <= 0.0f)return;

	// 高さを目標に合わせる拡大率を求め、その拡大率で幅が領域を超える場合は、幅が収まる拡大率へ小さくする。
	const float height_scale = targetHeight / base_size.y;
	const float width_scale = areaSize.x / base_size.x;
	const float text_scale = std::min(height_scale,width_scale);

	// 拡大後の大きさを求め、領域の中央に置く左上の位置を計算する。中央 = 領域の左上 + (領域の大きさ - 文字の大きさ)/2。
	const XMFLOAT2 text_size{base_size.x * text_scale,base_size.y * text_scale};
	const XMFLOAT2 text_position{areaPosition.x + (areaSize.x - text_size.x) * 0.5f,areaPosition.y + (areaSize.y - text_size.y) * 0.5f};
	m_textRenderer.DrawText(text,text_position,color,text_scale);
}