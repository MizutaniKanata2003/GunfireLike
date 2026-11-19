#pragma once

// ゲーム中のHUD（照準・HPバー・弾薬・リロード表示・回復アイテム数・低HP警告）を、画像と文字で描画する。
// 画像の配置はUiLayoutの表から読み、描画する値はGAME_HUD_STATEとして受け取る。値の決定・ゲームの進行は担当しない。

//========= C++標準ライブラリ インクルード=========
#include <string> // 描画する文字列を、std::wstringの参照として受け取る。

//========= Framework インクルード=========
#include "Framework/2D/H/HudSpriteRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

//========= 前方宣言=========
class GraphicsSystem;

// HUDの描画に必要な、ゲームの状態の値。
struct GAME_HUD_STATE
{
	//========= HP=========
	// 現在のHP。
	float currentHP{};
	// 最大HP。0以下ならHPバーと低HP警告を描かない。
	float maxHP{};
	// 所持している回復アイテムの数。HPバーの上に「Q x数」の形で表示する。
	int healItemCount{};

	//========= 弾薬=========
	// 現在の装填弾数。
	int magazineAmmo{};
	// 装弾数の上限。
	int magazineSize{};
	// 予備弾薬の数。
	int reserveAmmo{};
	// リロード中か。
	bool isReloading{};
	// リロードの進み具合。0〜1。
	float reloadProgress{};

	//========= 構え=========
	// 構えているか。trueなら照準を小さく描く。
	bool isAiming{};
};

// 画像と文字でHUDを描画するクラス。
class GameHud final
{
public:
	//========= 初期化・描画関数=========
	// 画像用・文字用のRendererを初期化し、配置表の画像を全て読み込む。
	// graphicsSystem: Resource生成と描画に使う描画基盤。Uninitまで有効であること。
	// 戻り値: 全ての初期化と読込に成功すればtrue。失敗した場合は、部分Resourceを解放してfalse。
	// COMの初期化は呼出側が済ませておくこと。
	[[nodiscard]] bool Initialize(GraphicsSystem& graphicsSystem);
	// 低HP警告・照準・HPバー・弾薬の枠の画像を描き、続けて弾薬・リロード・回復アイテム数の文字を描く。
	// graphicsSystem: 描画基盤。呼出前に、画面UI用のRender Pass（e_SCREEN_UI）を設定しておくこと。
	// state: 描画する値。参照は保持しない。
	// 画像の読込に失敗している場合、その画像は描かれない。
	void Draw(GraphicsSystem& graphicsSystem,const GAME_HUD_STATE& state);
	// 画像用・文字用のRendererを解放する。未初期化・終了済みでも呼出可能。
	void Uninit();

private:
	//========= 補助関数=========
	// 文字を、指定した領域の中央へ、領域に収まる大きさで描く。Begin・Endの間に呼ぶ。
	// text: 描く文字列。空なら何もしない。
	// areaPosition: 領域の左上の位置。基準Canvas上の座標。
	// areaSize: 領域の幅と高さ。基準Canvas上の大きさ。
	// targetHeight: 文字の高さの目標。基準Canvas上の大きさ。領域の幅に収まらない場合は、幅に合わせて小さくする。
	// color: 文字のRGBA色。
	void DrawFittedText(const std::wstring& text,const XMFLOAT2& areaPosition,const XMFLOAT2& areaSize,float targetHeight,const XMVECTORF32& color);

	//========= 描画Renderer=========
	// 画像を矩形で描画するRenderer。
	HudSpriteRenderer m_spriteRenderer{};
	// 文字を描画するRenderer。
	HudTextRenderer m_textRenderer{};

	//========= 読み込んだ画像=========
	// 照準の画像。
	HudTextureHandle m_crosshairTexture{};
	// HPバーの枠の画像。
	HudTextureHandle m_hpFrameTexture{};
	// HPバーの中身の画像。
	HudTextureHandle m_hpFillTexture{};
	// 弾薬の枠の画像。
	HudTextureHandle m_ammoFrameTexture{};
	// 低HP警告の画像。
	HudTextureHandle m_lowHpWarningTexture{};
};