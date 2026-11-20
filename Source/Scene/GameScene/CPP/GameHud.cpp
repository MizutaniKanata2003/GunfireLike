#include "../H/GameHud.h"

// HUDの画像の読込・描画と、弾薬・リロード・回復アイテム数・通貨の文字、報酬選択のカード、ショップの描画を実装する。
// 画像の配置は表から読み、状態に応じた拡縮・色・表示割合・文字の大きさだけをこのファイルで決める。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampでHPの割合を0〜1へ制限し、std::minで文字の拡大率を領域に収まる値に制限する。
#include <string> // std::to_wstringで弾薬数・進み具合・通貨・取得数・価格の数字を文字列へ変換し、結合する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/UI/H/UiLayout.h"
#include "Scene/Common/Weapon/H/WeaponInventory.h"

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
	// 報酬選択・ショップの見出しと、カードの見出しの文字の色。金色。
	const XMVECTORF32 ns_REWARD_TITLE_COLOR{{{1.0f,0.85f,0.3f,1.0f}}};
	// 報酬選択・ショップの説明・取得数・操作の案内の文字の色。白に近い色。
	const XMVECTORF32 ns_REWARD_BODY_COLOR{{{0.92f,0.92f,0.92f,1.0f}}};
	// ショップのボタンの文字の色（押せるボタン）。白。
	const XMVECTORF32 ns_SHOP_BUTTON_TEXT_COLOR{{{1.0f,1.0f,1.0f,1.0f}}};
	// ショップのボタンの文字の色（押せないボタン）。灰色。
	const XMVECTORF32 ns_SHOP_BUTTON_DISABLED_TEXT_COLOR{{{0.6f,0.6f,0.6f,1.0f}}};
	// ショップのボタンの文字の色（売却の確認中）。橙。押すと売却が確定することを、目立たせる。
	const XMVECTORF32 ns_SHOP_BUTTON_CONFIRM_TEXT_COLOR{{{1.0f,0.6f,0.2f,1.0f}}};

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

	//========= 報酬選択の文字の配置（画面全体）=========
	// 見出しを置く領域の左上の位置。基準Canvas上の座標。カードの上の余白（0〜150）へ置く。
	const XMFLOAT2 ns_REWARD_TITLE_AREA_POSITION{240.0f,30.0f};
	// 見出しを置く領域の大きさ。基準Canvas上の大きさ。
	const XMFLOAT2 ns_REWARD_TITLE_AREA_SIZE{800.0f,90.0f};
	// 見出しの高さの目標。基準Canvas上の大きさ。
	constexpr float ns_REWARD_TITLE_TARGET_HEIGHT = 64.0f;
	// 操作の案内を置く領域の左上の位置。基準Canvas上の座標。カードの下（630〜）へ置く。
	const XMFLOAT2 ns_REWARD_GUIDE_AREA_POSITION{240.0f,645.0f};
	// 操作の案内を置く領域の大きさ。基準Canvas上の大きさ。
	const XMFLOAT2 ns_REWARD_GUIDE_AREA_SIZE{800.0f,50.0f};
	// 操作の案内の高さの目標。基準Canvas上の大きさ。
	constexpr float ns_REWARD_GUIDE_TARGET_HEIGHT = 28.0f;

	//========= 報酬選択の文字の配置（カードの中）=========
	// カードの左上から、見出しの領域の左上までの距離。基準Canvas上の大きさ。カードの幅（360）の内側へ、左右20ずつの余白を取る。
	const XMFLOAT2 ns_CARD_TITLE_OFFSET{20.0f,28.0f};
	// 見出しの領域の大きさ。基準Canvas上の大きさ。
	const XMFLOAT2 ns_CARD_TITLE_SIZE{320.0f,56.0f};
	// 見出しの高さの目標。基準Canvas上の大きさ。
	constexpr float ns_CARD_TITLE_TARGET_HEIGHT = 30.0f;
	// カードの左上から、説明の領域の左上までの距離。基準Canvas上の大きさ。
	const XMFLOAT2 ns_CARD_DESCRIPTION_OFFSET{20.0f,130.0f};
	// 説明の領域の大きさ。基準Canvas上の大きさ。複数行を、この領域の中央へ置く。
	const XMFLOAT2 ns_CARD_DESCRIPTION_SIZE{320.0f,190.0f};
	// 説明（複数行全体）の高さの目標。基準Canvas上の大きさ。
	constexpr float ns_CARD_DESCRIPTION_TARGET_HEIGHT = 110.0f;
	// カードの左上から、取得数の領域の左上までの距離。基準Canvas上の大きさ。
	const XMFLOAT2 ns_CARD_OWNED_OFFSET{20.0f,400.0f};
	// 取得数の領域の大きさ。基準Canvas上の大きさ。
	const XMFLOAT2 ns_CARD_OWNED_SIZE{320.0f,50.0f};
	// 取得数の高さの目標。基準Canvas上の大きさ。
	constexpr float ns_CARD_OWNED_TARGET_HEIGHT = 26.0f;

	//========= ショップの文字の配置（共通）=========
	// 見出しを置く領域の左上の位置。基準Canvas上の座標。パネル（上端90）の上部の左側へ置く。
	const XMFLOAT2 ns_SHOP_TITLE_AREA_POSITION{200.0f,105.0f};
	// 見出しを置く領域の大きさ。基準Canvas上の大きさ。
	const XMFLOAT2 ns_SHOP_TITLE_AREA_SIZE{480.0f,70.0f};
	// 見出しの高さの目標。基準Canvas上の大きさ。
	constexpr float ns_SHOP_TITLE_TARGET_HEIGHT = 56.0f;
	// 右上の情報（売買のページの所持数、強化のページの武器名）を置く領域の左上の位置。基準Canvas上の座標。パネルの上部の右側へ置く。
	const XMFLOAT2 ns_SHOP_INFO_AREA_POSITION{720.0f,115.0f};
	// 右上の情報を置く領域の大きさ。基準Canvas上の大きさ。
	const XMFLOAT2 ns_SHOP_INFO_AREA_SIZE{360.0f,50.0f};
	// 右上の情報の高さの目標。基準Canvas上の大きさ。
	constexpr float ns_SHOP_INFO_TARGET_HEIGHT = 30.0f;
	// ボタンの文字の高さの目標。基準Canvas上の大きさ。ボタンの高さ（64）に収まる大きさ。
	constexpr float ns_SHOP_BUTTON_TEXT_TARGET_HEIGHT = 30.0f;
	// 操作の案内を置く領域の左上の位置。基準Canvas上の座標。最後の行（下端478）と、閉じるボタン（上端546）の間へ置く。
	const XMFLOAT2 ns_SHOP_GUIDE_AREA_POSITION{200.0f,490.0f};
	// 操作の案内を置く領域の大きさ。基準Canvas上の大きさ。
	const XMFLOAT2 ns_SHOP_GUIDE_AREA_SIZE{880.0f,40.0f};
	// 操作の案内の高さの目標。基準Canvas上の大きさ。
	constexpr float ns_SHOP_GUIDE_TARGET_HEIGHT = 24.0f;

	//========= ショップの文字の配置（売買のページ）=========
	// 各行の左側（武器名と予備弾薬）を置く領域の、左端のX座標と幅。基準Canvas上の座標と大きさ。武器のボタン（左端580）の手前に収める。
	constexpr float ns_SHOP_NAME_AREA_X = 200.0f;
	constexpr float ns_SHOP_NAME_AREA_WIDTH = 360.0f;
	// 行の上端から、武器名の領域の上端までの距離と、領域の高さ。基準Canvas上の大きさ。行の高さ（96）の上側へ置く。
	constexpr float ns_SHOP_NAME_AREA_OFFSET_Y = 8.0f;
	constexpr float ns_SHOP_NAME_AREA_HEIGHT = 44.0f;
	// 武器名の高さの目標。基準Canvas上の大きさ。
	constexpr float ns_SHOP_NAME_TARGET_HEIGHT = 30.0f;
	// 行の上端から、予備弾薬の領域の上端までの距離と、領域の高さ。基準Canvas上の大きさ。行の高さ（96）の下側へ置く。
	constexpr float ns_SHOP_RESERVE_AREA_OFFSET_Y = 52.0f;
	constexpr float ns_SHOP_RESERVE_AREA_HEIGHT = 36.0f;
	// 予備弾薬の高さの目標。基準Canvas上の大きさ。
	constexpr float ns_SHOP_RESERVE_TARGET_HEIGHT = 22.0f;

	//========= ショップの文字の配置（強化のページ）=========
	// 強化の各行の、項目名と段階を置く領域の、左端のX座標と幅。基準Canvas上の座標と大きさ。強化のボタン（左端840）の手前に収める。
	constexpr float ns_SHOP_UPGRADE_NAME_AREA_X = 200.0f;
	constexpr float ns_SHOP_UPGRADE_NAME_AREA_WIDTH = 620.0f;
	// 強化の項目名と段階の高さの目標。基準Canvas上の大きさ。
	constexpr float ns_SHOP_UPGRADE_NAME_TARGET_HEIGHT = 32.0f;

	//========= ショップのボタンの補助関数=========
	// 武器のボタンが、押せる状態かを返す。購入できる、売却できる、売却の確認中の場合だけ、押せる。
	// rowState: 武器の行の状態。
	bool IsWeaponButtonPressable(SHOP_ROW_STATE rowState)
	{
		return rowState == SHOP_ROW_STATE::e_AVAILABLE || rowState == SHOP_ROW_STATE::e_SELLABLE || rowState == SHOP_ROW_STATE::e_SELL_CONFIRM;
	}

	// 武器の行の状態に応じた、ボタンの文字を返す。購入できる時は購入の価格、売却できる時は売却で得る通貨を添える。
	// rowState: 武器の行の状態。
	// price: 武器の購入の価格。
	// sellPrice: 武器の売却で得る通貨。
	std::wstring GetWeaponButtonLabel(SHOP_ROW_STATE rowState,int price,int sellPrice)
	{
		switch(rowState){
		case SHOP_ROW_STATE::e_AVAILABLE:      return L"BUY " + std::to_wstring(price);
		case SHOP_ROW_STATE::e_OWNED:          return L"LAST GUN";
		case SHOP_ROW_STATE::e_SELLABLE:       return L"SELL " + std::to_wstring(sellPrice);
		case SHOP_ROW_STATE::e_SELL_CONFIRM:   return L"CONFIRM " + std::to_wstring(sellPrice);
		case SHOP_ROW_STATE::e_INVENTORY_FULL: return L"FULL";
		case SHOP_ROW_STATE::e_TOO_EXPENSIVE:  return L"NEED MORE";
		}
		return L"";
	}

	// 弾薬の購入の状態に応じた、ボタンの文字を返す。購入できる時は、増える弾数と価格を添える。
	// ammoState: 弾薬の行の購入の状態。
	// packSize: 1回の購入で増える弾数。
	// price: 弾薬の価格。
	std::wstring GetAmmoButtonLabel(SHOP_AMMO_STATE ammoState,int packSize,int price)
	{
		switch(ammoState){
		case SHOP_AMMO_STATE::e_AVAILABLE:     return L"AMMO +" + std::to_wstring(packSize) + L" / " + std::to_wstring(price);
		case SHOP_AMMO_STATE::e_NOT_OWNED:     return L"NO WEAPON";
		case SHOP_AMMO_STATE::e_NOT_EQUIPPED:  return L"EQUIP FIRST";
		case SHOP_AMMO_STATE::e_AMMO_FULL:     return L"AMMO FULL";
		case SHOP_AMMO_STATE::e_TOO_EXPENSIVE: return L"NEED MORE";
		}
		return L"";
	}

	// 強化の購入の状態に応じた、ボタンの文字を返す。強化できる時は、価格を添える。
	// upgradeState: 強化の項目の購入の状態。
	// price: 次の段階へ上げる価格。
	std::wstring GetUpgradeButtonLabel(SHOP_UPGRADE_STATE upgradeState,int price)
	{
		switch(upgradeState){
		case SHOP_UPGRADE_STATE::e_AVAILABLE:     return L"UP " + std::to_wstring(price);
		case SHOP_UPGRADE_STATE::e_MAX_LEVEL:     return L"MAX LEVEL";
		case SHOP_UPGRADE_STATE::e_TOO_EXPENSIVE: return L"NEED MORE";
		case SHOP_UPGRADE_STATE::e_NO_WEAPON:     return L"NO WEAPON";
		}
		return L"";
	}

	// ボタンが、押せる状態かを返す。ページ切替と閉じるボタンは、常に押せる。
	// buttonIndex: ボタンの添字。
	// state: 描画する値。
	bool IsShopButtonPressable(int buttonIndex,const GAME_HUD_STATE& state)
	{
		if(buttonIndex < SHOP_TABLE::ns_AMMO_BUTTON_OFFSET){
			return IsWeaponButtonPressable(state.shopRowStates[buttonIndex]);
		}
		if(buttonIndex < SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET){
			return state.shopAmmoStates[buttonIndex - SHOP_TABLE::ns_AMMO_BUTTON_OFFSET] == SHOP_AMMO_STATE::e_AVAILABLE;
		}
		if(buttonIndex < SHOP_TABLE::ns_SWITCH_INDEX){
			return state.shopUpgradeStates[buttonIndex - SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET] == SHOP_UPGRADE_STATE::e_AVAILABLE;
		}
		return true;
	}
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
	m_rewardBackdropTexture = m_spriteRenderer.LoadTexture(graphicsSystem,REWARD_LAYOUT::ns_BACKDROP.texturePath);
	m_rewardCardTexture = m_spriteRenderer.LoadTexture(graphicsSystem,REWARD_LAYOUT::ns_CARD_FRAME_PATH);
	m_rewardCardHoverTexture = m_spriteRenderer.LoadTexture(graphicsSystem,REWARD_LAYOUT::ns_CARD_FRAME_HOVER_PATH);
	m_shopPanelTexture = m_spriteRenderer.LoadTexture(graphicsSystem,SHOP_LAYOUT::ns_PANEL.texturePath);
	m_shopButtonTexture = m_spriteRenderer.LoadTexture(graphicsSystem,SHOP_LAYOUT::ns_BUTTON_PATH);
	m_shopButtonHoverTexture = m_spriteRenderer.LoadTexture(graphicsSystem,SHOP_LAYOUT::ns_BUTTON_HOVER_PATH);
	m_shopButtonDisabledTexture = m_spriteRenderer.LoadTexture(graphicsSystem,SHOP_LAYOUT::ns_BUTTON_DISABLED_PATH);
	return true;
}

// 画像を描いてから、文字を描く。画像と文字でBatchの状態が異なるため、この順に分ける。
void GameHud::Draw(GraphicsSystem& graphicsSystem,const GAME_HUD_STATE& state)
{
	// 最大HPが正の場合だけ、HPの割合を0〜1で求める。0以下なら割合を0とし、HPに関する表示を省く。
	const bool has_health = state.maxHP > 0.0f;
	const float health_ratio = has_health ? std::clamp(state.currentHP / state.maxHP,0.0f,1.0f) : 0.0f;

	// 報酬選択中とショップが開いている間は、画面全体を暗くする背景を最初に描き、他のHUDとカード・パネルがその上に重なるようにする。
	if(state.isRewardSelecting || state.isShopOpen){
		m_spriteRenderer.DrawSprite(graphicsSystem,m_rewardBackdropTexture,REWARD_LAYOUT::ns_BACKDROP.position,REWARD_LAYOUT::ns_BACKDROP.size,
																ns_WHITE,1.0f);
	}

	// HPの割合が低HPの閾値以下なら、画面全体の警告を描き、他のHUDがその上に重なるようにする。
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

	// 報酬選択中は、他の画像の後に、カードの枠を描く。ショップが開いている間は、パネルとボタンを描く。文字は、次の文字の描画で、上に重ねる。
	if(state.isRewardSelecting){
		DrawRewardCards(graphicsSystem,state.hoveredRewardIndex);
	}
	if(state.isShopOpen){
		DrawShopSprites(graphicsSystem,state);
	}

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

	// 報酬選択中とショップが開いている間は、その文字を、最後に重ねて描く。
	if(state.isRewardSelecting){
		DrawRewardTexts(state.rewardOwnedCounts);
	}
	if(state.isShopOpen){
		DrawShopTexts(state);
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
	m_rewardBackdropTexture = 0;
	m_rewardCardTexture = 0;
	m_rewardCardHoverTexture = 0;
	m_shopPanelTexture = 0;
	m_shopButtonTexture = 0;
	m_shopButtonHoverTexture = 0;
	m_shopButtonDisabledTexture = 0;
}

//========= 補助関数=========
// 文字の大きさを測り、高さの目標と領域の幅から拡大率を決めて、領域の中央へ描く。
void GameHud::DrawFittedText(const std::wstring& text,const XMFLOAT2& areaPosition,const XMFLOAT2& areaSize,float targetHeight,
														 const XMVECTORF32& color)
{
	// 文字を拡大率1で測った幅と高さ。Font未生成や空文字列では0になる。改行を含む文字列は、全行を合わせた大きさになる。
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

// 報酬のカードの枠を、カードの数だけ描く。
void GameHud::DrawRewardCards(GraphicsSystem& graphicsSystem,int hoveredIndex)
{
	// 全てのカードを左から順に描く。カーソルが乗っているカードだけ、ホバー用の画像にする。
	for(int card_index = 0; card_index < REWARD_LAYOUT::ns_CARD_COUNT; ++card_index){
		const HudTextureHandle card_texture = (card_index == hoveredIndex) ? m_rewardCardHoverTexture : m_rewardCardTexture;
		m_spriteRenderer.DrawSprite(graphicsSystem,card_texture,REWARD_LAYOUT::ns_CARD_POSITIONS[card_index],REWARD_LAYOUT::ns_CARD_SIZE,ns_WHITE,1.0f);
	}
}

// 報酬選択の文字を描く。
void GameHud::DrawRewardTexts(const std::array<int,REWARD_TABLE::ns_CARD_COUNT>& ownedCounts)
{
	// 画面の見出しと、操作の案内を描く。
	DrawFittedText(L"REWARD",ns_REWARD_TITLE_AREA_POSITION,ns_REWARD_TITLE_AREA_SIZE,ns_REWARD_TITLE_TARGET_HEIGHT,ns_REWARD_TITLE_COLOR);
	DrawFittedText(L"CLICK A CARD  (OR PRESS 1 / 2 / 3)",ns_REWARD_GUIDE_AREA_POSITION,ns_REWARD_GUIDE_AREA_SIZE,ns_REWARD_GUIDE_TARGET_HEIGHT,
								 ns_REWARD_BODY_COLOR);

	// 各カードの見出し・説明・取得数を、カードの左上からの距離で配置して描く。
	for(int card_index = 0; card_index < REWARD_LAYOUT::ns_CARD_COUNT; ++card_index){
		const XMFLOAT2& card_position = REWARD_LAYOUT::ns_CARD_POSITIONS[card_index];
		const REWARD_TABLE::REWARD_DEFINITION& reward = REWARD_TABLE::ns_REWARDS[card_index];

		DrawFittedText(reward.title,XMFLOAT2{card_position.x + ns_CARD_TITLE_OFFSET.x,card_position.y + ns_CARD_TITLE_OFFSET.y},ns_CARD_TITLE_SIZE,
									 ns_CARD_TITLE_TARGET_HEIGHT,ns_REWARD_TITLE_COLOR);
		DrawFittedText(reward.description,XMFLOAT2{card_position.x + ns_CARD_DESCRIPTION_OFFSET.x,card_position.y + ns_CARD_DESCRIPTION_OFFSET.y},
									 ns_CARD_DESCRIPTION_SIZE,ns_CARD_DESCRIPTION_TARGET_HEIGHT,ns_REWARD_BODY_COLOR);
		DrawFittedText(L"OWNED x" + std::to_wstring(ownedCounts[card_index]),
									 XMFLOAT2{card_position.x + ns_CARD_OWNED_OFFSET.x,card_position.y + ns_CARD_OWNED_OFFSET.y},ns_CARD_OWNED_SIZE,
									 ns_CARD_OWNED_TARGET_HEIGHT,ns_REWARD_BODY_COLOR);
	}
}

// ショップのパネルと、現在のページのボタンの画像を描く。
void GameHud::DrawShopSprites(GraphicsSystem& graphicsSystem,const GAME_HUD_STATE& state)
{
	// パネルを先に描く。
	m_spriteRenderer.DrawSprite(graphicsSystem,m_shopPanelTexture,SHOP_LAYOUT::ns_PANEL.position,SHOP_LAYOUT::ns_PANEL.size,ns_WHITE,1.0f);

	// 現在のページのボタンだけを、添字の順に描く。押せないボタンは、押せない時の画像にする。押せるボタンは、カーソルが乗っていればホバー用の画像にする。
	for(int button_index = 0; button_index < SHOP_LAYOUT::ns_BUTTON_COUNT; ++button_index){
		if(!SHOP_TABLE::IsButtonOnPage(button_index,state.shopPage))continue;

		HudTextureHandle button_texture = m_shopButtonDisabledTexture;
		if(IsShopButtonPressable(button_index,state)){
			button_texture = (button_index == state.hoveredShopIndex) ? m_shopButtonHoverTexture : m_shopButtonTexture;
		}
		m_spriteRenderer.DrawSprite(graphicsSystem,button_texture,SHOP_LAYOUT::ns_BUTTON_POSITIONS[button_index],SHOP_LAYOUT::ns_BUTTON_SIZE,ns_WHITE,1.0f);
	}
}

// ショップの文字を、現在のページに応じて描く。
void GameHud::DrawShopTexts(const GAME_HUD_STATE& state)
{
	// 現在のページの文字を描く。
	if(state.shopPage == SHOP_PAGE::e_TRADE){
		DrawShopTradeTexts(state);
	}
	else{
		DrawShopUpgradeTexts(state);
	}

	// ページ切替と閉じるボタンの文字は、どちらのページでも描く。切替ボタンは、行き先のページの名前にする。
	DrawFittedText(state.shopPage == SHOP_PAGE::e_TRADE ? L"UPGRADES" : L"BACK",SHOP_LAYOUT::ns_BUTTON_POSITIONS[SHOP_TABLE::ns_SWITCH_INDEX],
								 SHOP_LAYOUT::ns_BUTTON_SIZE,ns_SHOP_BUTTON_TEXT_TARGET_HEIGHT,ns_SHOP_BUTTON_TEXT_COLOR);
	DrawFittedText(L"CLOSE",SHOP_LAYOUT::ns_BUTTON_POSITIONS[SHOP_TABLE::ns_CLOSE_INDEX],SHOP_LAYOUT::ns_BUTTON_SIZE,ns_SHOP_BUTTON_TEXT_TARGET_HEIGHT,
								 ns_SHOP_BUTTON_TEXT_COLOR);
}

// 売買のページの文字を描く。
void GameHud::DrawShopTradeTexts(const GAME_HUD_STATE& state)
{
	// 見出しと、所持数（上限との比）と、操作の案内を描く。
	DrawFittedText(L"SHOP",ns_SHOP_TITLE_AREA_POSITION,ns_SHOP_TITLE_AREA_SIZE,ns_SHOP_TITLE_TARGET_HEIGHT,ns_REWARD_TITLE_COLOR);
	DrawFittedText(L"SLOTS " + std::to_wstring(state.ownedWeaponCount) + L" / " + std::to_wstring(WEAPON_INVENTORY_TABLE::ns_MAX_WEAPONS),
								 ns_SHOP_INFO_AREA_POSITION,ns_SHOP_INFO_AREA_SIZE,ns_SHOP_INFO_TARGET_HEIGHT,ns_REWARD_BODY_COLOR);
	DrawFittedText(L"CLICK  (1-3: WEAPON  4-6: AMMO)   SELL: PRESS TWICE   U: UPGRADES   ENTER: CLOSE",ns_SHOP_GUIDE_AREA_POSITION,
								 ns_SHOP_GUIDE_AREA_SIZE,ns_SHOP_GUIDE_TARGET_HEIGHT,ns_REWARD_BODY_COLOR);

	// 各行の武器名・予備弾薬・2つのボタンの文字を描く。予備弾薬は、装備している武器の行にだけ、「RESERVE 現在 / 上限」の形で表示する。
	for(int row_index = 0; row_index < SHOP_TABLE::ns_ROW_COUNT; ++row_index){
		const float row_top = SHOP_LAYOUT::ns_ROW_TOP + SHOP_LAYOUT::ns_ROW_HEIGHT * static_cast<float>(row_index);
		DrawFittedText(SHOP_TABLE::ns_WEAPON_NAMES[row_index],XMFLOAT2{ns_SHOP_NAME_AREA_X,row_top + ns_SHOP_NAME_AREA_OFFSET_Y},
									 XMFLOAT2{ns_SHOP_NAME_AREA_WIDTH,ns_SHOP_NAME_AREA_HEIGHT},ns_SHOP_NAME_TARGET_HEIGHT,ns_REWARD_BODY_COLOR);
		if(row_index == state.equippedWeaponIndex){
			DrawFittedText(L"RESERVE " + std::to_wstring(state.reserveAmmo) + L" / " + std::to_wstring(SHOP_TABLE::ns_AMMO_RESERVE_CAPS[row_index]),
										 XMFLOAT2{ns_SHOP_NAME_AREA_X,row_top + ns_SHOP_RESERVE_AREA_OFFSET_Y},XMFLOAT2{ns_SHOP_NAME_AREA_WIDTH,ns_SHOP_RESERVE_AREA_HEIGHT},
										 ns_SHOP_RESERVE_TARGET_HEIGHT,ns_CURRENCY_TEXT_COLOR);
		}

		// 武器のボタンの文字。購入・売却できる時は白、売却の確認中は橙、他は灰色。
		const SHOP_ROW_STATE row_state = state.shopRowStates[row_index];
		const XMVECTORF32& weapon_text_color = row_state == SHOP_ROW_STATE::e_SELL_CONFIRM ? ns_SHOP_BUTTON_CONFIRM_TEXT_COLOR
			: (IsWeaponButtonPressable(row_state) ? ns_SHOP_BUTTON_TEXT_COLOR : ns_SHOP_BUTTON_DISABLED_TEXT_COLOR);
		DrawFittedText(GetWeaponButtonLabel(row_state,SHOP_TABLE::ns_WEAPON_PRICES[row_index],state.shopSellPrices[row_index]),
									 SHOP_LAYOUT::ns_BUTTON_POSITIONS[row_index],SHOP_LAYOUT::ns_BUTTON_SIZE,ns_SHOP_BUTTON_TEXT_TARGET_HEIGHT,weapon_text_color);

		// 弾薬の購入ボタンの文字。購入できる時だけ白、他は灰色。
		const SHOP_AMMO_STATE ammo_state = state.shopAmmoStates[row_index];
		DrawFittedText(GetAmmoButtonLabel(ammo_state,SHOP_TABLE::ns_AMMO_PACK_SIZES[row_index],SHOP_TABLE::ns_AMMO_PRICES[row_index]),
									 SHOP_LAYOUT::ns_BUTTON_POSITIONS[SHOP_TABLE::ns_AMMO_BUTTON_OFFSET + row_index],SHOP_LAYOUT::ns_BUTTON_SIZE,
									 ns_SHOP_BUTTON_TEXT_TARGET_HEIGHT,ammo_state == SHOP_AMMO_STATE::e_AVAILABLE ? ns_SHOP_BUTTON_TEXT_COLOR : ns_SHOP_BUTTON_DISABLED_TEXT_COLOR);
	}
}

// 強化のページの文字を描く。
void GameHud::DrawShopUpgradeTexts(const GAME_HUD_STATE& state)
{
	// 見出しと、強化の対象（装備中の武器）の名前を描く。装備中の武器が判別できない場合は、名前を描かない。
	DrawFittedText(L"UPGRADE",ns_SHOP_TITLE_AREA_POSITION,ns_SHOP_TITLE_AREA_SIZE,ns_SHOP_TITLE_TARGET_HEIGHT,ns_REWARD_TITLE_COLOR);
	if(state.equippedWeaponIndex >= 0 && state.equippedWeaponIndex < SHOP_TABLE::ns_ROW_COUNT){
		DrawFittedText(SHOP_TABLE::ns_WEAPON_NAMES[state.equippedWeaponIndex],ns_SHOP_INFO_AREA_POSITION,ns_SHOP_INFO_AREA_SIZE,ns_SHOP_INFO_TARGET_HEIGHT,
									 ns_CURRENCY_TEXT_COLOR);
	}

	// 各項目の名前と段階、強化のボタンの文字を描く。強化できる時は、次の段階の価格を添える。
	for(int item_index = 0; item_index < SHOP_TABLE::ns_UPGRADE_COUNT; ++item_index){
		const float row_top = SHOP_LAYOUT::ns_UPGRADE_ROW_TOP + SHOP_LAYOUT::ns_UPGRADE_ROW_STEP * static_cast<float>(item_index);
		const int level = state.shopUpgradeLevels[item_index];
		DrawFittedText(std::wstring{WEAPON_UPGRADE_TABLE::ns_ITEM_NAMES[item_index]} + L"   LV " + std::to_wstring(level) + L" / " +
									 std::to_wstring(WEAPON_UPGRADE_TABLE::ns_MAX_LEVEL),XMFLOAT2{ns_SHOP_UPGRADE_NAME_AREA_X,row_top},
									 XMFLOAT2{ns_SHOP_UPGRADE_NAME_AREA_WIDTH,SHOP_LAYOUT::ns_UPGRADE_ROW_HEIGHT},ns_SHOP_UPGRADE_NAME_TARGET_HEIGHT,ns_REWARD_BODY_COLOR);

		const SHOP_UPGRADE_STATE upgrade_state = state.shopUpgradeStates[item_index];
		DrawFittedText(GetUpgradeButtonLabel(upgrade_state,WEAPON_UPGRADE_TABLE::GetPrice(item_index,level)),
									 SHOP_LAYOUT::ns_BUTTON_POSITIONS[SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET + item_index],SHOP_LAYOUT::ns_BUTTON_SIZE,
									 ns_SHOP_BUTTON_TEXT_TARGET_HEIGHT,upgrade_state == SHOP_UPGRADE_STATE::e_AVAILABLE ? ns_SHOP_BUTTON_TEXT_COLOR : ns_SHOP_BUTTON_DISABLED_TEXT_COLOR);
	}
}