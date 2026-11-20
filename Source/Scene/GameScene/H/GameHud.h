#pragma once

// ゲーム中のHUD（照準・HPバー・弾薬・リロード表示・回復アイテム数・通貨・低HP警告）と、報酬選択のカード、ショップ（武器の購入画面）を、画像と文字で描画する。
// 画像の配置はUiLayoutの表から読み、描画する値はGAME_HUD_STATEとして受け取る。値の決定・ゲームの進行・クリックの判定は担当しない。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、報酬ごとの取得数と、ショップの行ごとの状態を固定長で受け取る。
#include <string> // 描画する文字列を、std::wstringの参照として受け取る。

//========= Framework インクルード=========
#include "Framework/2D/H/HudSpriteRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/Reward/H/RewardController.h" // 報酬の表（見出し・説明）と、カードの数を使用する。
#include "Scene/Common/Shop/H/ShopController.h"    // ショップの表（価格・名前）と、行の状態を使用する。

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

	//========= 通貨=========
	// 現在の通貨の所持額。右上のアイコンの隣に、数字で表示する。
	int currency{};

	//========= 報酬選択=========
	// 報酬選択の最中か。trueなら、画面を暗くして、見出しと、報酬のカードと、操作の案内を描く。
	bool isRewardSelecting{};
	// カーソルが乗っているカードの添字。乗っていなければ-1。そのカードは、枠をホバー用の画像で描く。
	int hoveredRewardIndex{-1};
	// 報酬ごとの取得数。添字は報酬の表の並びと同じ。各カードの下に「OWNED x数」の形で表示する。
	std::array<int,REWARD_TABLE::ns_CARD_COUNT> rewardOwnedCounts{};

	//========= ショップ=========
	// ショップが開いているか。trueなら、画面を暗くして、購入パネルを描く。
	bool isShopOpen{};
	// カーソルが乗っているボタンの添字。乗っていなければ-1。武器の行のボタン、閉じるボタンの順。
	int hoveredShopIndex{-1};
	// 武器の行ごとの購入の状態。添字は、武器の種類（WEAPON_TYPE）の値と同じ。ボタンの文字と画像を決める。
	std::array<SHOP_ROW_STATE,SHOP_TABLE::ns_ROW_COUNT> shopRowStates{};
	// 所持している武器の数。パネルの右上に「SLOTS 数 / 上限」の形で表示する。
	int ownedWeaponCount{};
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
	// 画像を描き、続けて文字を描く。報酬選択中・ショップが開いている間は、背景の暗幕を最初に、カードやパネルの画像を他の画像の後に描き、文字は最後に重ねる。
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
	// 報酬のカードの枠を、カードの数だけ描く。カーソルが乗っているカードは、ホバー用の画像で描く。
	// graphicsSystem: 描画基盤。
	// hoveredIndex: カーソルが乗っているカードの添字。乗っていなければ-1。
	void DrawRewardCards(GraphicsSystem& graphicsSystem,int hoveredIndex);
	// 報酬選択の文字（見出し・各カードの見出しと説明と取得数・操作の案内）を描く。Begin・Endの間に呼ぶ。
	// ownedCounts: 報酬ごとの取得数。
	void DrawRewardTexts(const std::array<int,REWARD_TABLE::ns_CARD_COUNT>& ownedCounts);
	// ショップのパネルと、ボタンの画像を描く。押せるボタンは、カーソルが乗っていればホバー用の画像で、押せないボタンは、押せない時の画像で描く。
	// graphicsSystem: 描画基盤。
	// state: 描画する値。ショップの項目を使う。
	void DrawShopSprites(GraphicsSystem& graphicsSystem,const GAME_HUD_STATE& state);
	// ショップの文字（見出し・所持数・各行の武器名と価格・ボタンの文字・操作の案内）を描く。Begin・Endの間に呼ぶ。
	// state: 描画する値。ショップの項目を使う。
	void DrawShopTexts(const GAME_HUD_STATE& state);

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
	// 通貨のアイコンの画像。
	HudTextureHandle m_currencyIconTexture{};
	// 報酬選択とショップの背景（暗幕）の画像。
	HudTextureHandle m_rewardBackdropTexture{};
	// 報酬のカードの枠の画像（通常）。
	HudTextureHandle m_rewardCardTexture{};
	// 報酬のカードの枠の画像（カーソルが乗っている時）。
	HudTextureHandle m_rewardCardHoverTexture{};
	// ショップのパネルの画像。
	HudTextureHandle m_shopPanelTexture{};
	// ショップのボタンの画像（通常）。
	HudTextureHandle m_shopButtonTexture{};
	// ショップのボタンの画像（カーソルが乗っている時）。
	HudTextureHandle m_shopButtonHoverTexture{};
	// ショップのボタンの画像（押せない時）。
	HudTextureHandle m_shopButtonDisabledTexture{};
};