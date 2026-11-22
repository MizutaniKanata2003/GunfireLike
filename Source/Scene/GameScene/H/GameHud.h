#pragma once

// ゲーム中のHUD（照準・HPバー・弾薬・リロード表示・回復アイテム数・通貨・低HP警告）と、報酬選択のカード、ショップ（武器と弾薬の購入・売却・強化の画面）を、画像と文字で描画する。
// 画像の配置はUiLayoutの表から読み、描画する値はGAME_HUD_STATEとして受け取る。値の決定・ゲームの進行・クリックの判定は担当しない。
// 実装は、GameHud.cpp（HUD・報酬選択）とGameHudShop.cpp（ショップ）に分ける。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、報酬ごとの取得数と、ショップの行ごとの状態を固定長で受け取る。
#include <string> // 描画する文字列を、std::wstringの参照として受け取る。

//========= Framework インクルード=========
#include "Framework/2D/H/HudSpriteRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/Reward/H/RewardController.h" // 報酬の表（見出し・説明）と、カードの数を使用する。
#include "Scene/Common/Shop/H/ShopController.h"   // ショップの表（価格・名前・弾薬の上限）と、ページ・行の状態を使用する。
#include "Scene/GameScene/H/GameHudStyle.h"       // 文字を置く領域の型（HUD_TEXT_AREA）を使用する。

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
	// ショップの現在のページ。売買か、強化か。
	SHOP_PAGE shopPage{SHOP_PAGE::e_TRADE};
	// カーソルが乗っているボタンの添字。乗っていなければ-1。武器、弾薬の購入、強化、ページ切替、閉じるボタンの順。
	int hoveredShopIndex{-1};
	// 武器の行ごとの購入と売却の状態。添字は、武器の種類（WEAPON_TYPE）の値と同じ。ボタンの文字と画像を決める。
	std::array<SHOP_ROW_STATE,SHOP_TABLE::ns_ROW_COUNT> shopRowStates{};
	// 武器の行ごとの、売却した時に得る通貨。添字は、武器の種類の値と同じ。
	std::array<int,SHOP_TABLE::ns_ROW_COUNT> shopSellPrices{};
	// 弾薬の行ごとの購入の状態。添字は、武器の種類の値と同じ。ボタンの文字と画像を決める。
	std::array<SHOP_AMMO_STATE,SHOP_TABLE::ns_ROW_COUNT> shopAmmoStates{};
	// 強化の項目ごとの購入の状態。添字は、強化の項目の添字と同じ。
	std::array<SHOP_UPGRADE_STATE,SHOP_TABLE::ns_UPGRADE_COUNT> shopUpgradeStates{};
	// 装備中の武器の、強化の項目ごとの段階。添字は、強化の項目の添字と同じ。
	std::array<int,SHOP_TABLE::ns_UPGRADE_COUNT> shopUpgradeLevels{};
	// 所持している武器の数。売買のページの右上に「SLOTS 数 / 上限」の形で表示する。
	int ownedWeaponCount{};
	// 装備している武器の種類の値（表の添字）。その行に予備弾薬の数を、強化のページの右上に武器名を表示する。判別できない場合は-1。
	int equippedWeaponIndex{-1};
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
	//========= 画像の描画関数（GameHud.cpp）=========
	// 画像を、描く順に描く。暗幕、HUDの画像、報酬のカード、ショップの画像の順。
	// graphicsSystem: 描画基盤。
	// state: 描画する値。
	void DrawSprites(GraphicsSystem& graphicsSystem,const GAME_HUD_STATE& state);
	// 低HP警告・照準・HPバー・弾薬の枠・通貨のアイコンを描く。
	// graphicsSystem: 描画基盤。
	// state: 描画する値。HP・構えを使う。
	void DrawStatusSprites(GraphicsSystem& graphicsSystem,const GAME_HUD_STATE& state);
	// 照準を、配置表の中心を保ったまま、構え中は小さく描く。
	// graphicsSystem: 描画基盤。
	// isAiming: 構えているか。
	void DrawCrosshair(GraphicsSystem& graphicsSystem,bool isAiming);
	// 報酬のカードの枠を、カードの数だけ描く。カーソルが乗っているカードは、ホバー用の画像で描く。
	// graphicsSystem: 描画基盤。
	// hoveredIndex: カーソルが乗っているカードの添字。乗っていなければ-1。
	void DrawRewardCards(GraphicsSystem& graphicsSystem,int hoveredIndex);

	//========= 文字の描画関数（GameHud.cpp）=========
	// 文字を、指定した領域の中央へ、領域に収まる大きさで描く。Begin・Endの間に呼ぶ。
	// text: 描く文字列。空なら何もしない。
	// area: 文字を置く領域。位置・大きさ・文字の高さの目標を持つ。
	// color: 文字のRGBA色。
	void DrawFittedText(const std::wstring& text,const HUD_TEXT_AREA& area,const XMVECTORF32& color);
	// 弾薬・回復アイテム数・通貨・リロード表示の文字を描く。Begin・Endの間に呼ぶ。
	// state: 描画する値。
	void DrawHudTexts(const GAME_HUD_STATE& state);
	// 報酬選択の文字（見出し・各カードの見出しと説明と取得数・操作の案内）を描く。Begin・Endの間に呼ぶ。
	// ownedCounts: 報酬ごとの取得数。
	void DrawRewardTexts(const std::array<int,REWARD_TABLE::ns_CARD_COUNT>& ownedCounts);

	//========= ショップの描画関数（GameHudShop.cpp）=========
	// ショップのパネルと、現在のページのボタンの画像を描く。押せるボタンは、カーソルが乗っていればホバー用の画像で、押せないボタンは、押せない時の画像で描く。
	// graphicsSystem: 描画基盤。
	// state: 描画する値。ショップの項目を使う。
	void DrawShopSprites(GraphicsSystem& graphicsSystem,const GAME_HUD_STATE& state);
	// ショップの文字を、現在のページに応じて描く。Begin・Endの間に呼ぶ。ページ切替と閉じるボタンの文字は、どちらのページでも描く。
	// state: 描画する値。ショップの項目を使う。
	void DrawShopTexts(const GAME_HUD_STATE& state);
	// 売買のページの文字（見出し・所持数・操作の案内と、各行の文字）を描く。Begin・Endの間に呼ぶ。
	// state: 描画する値。ショップの項目を使う。
	void DrawShopTradeTexts(const GAME_HUD_STATE& state);
	// 売買のページの1行（武器名・予備弾薬・武器と弾薬のボタンの文字）を描く。Begin・Endの間に呼ぶ。
	// state: 描画する値。
	// rowIndex: 行の添字。武器の種類の値と同じ。
	void DrawShopTradeRowTexts(const GAME_HUD_STATE& state,int rowIndex);
	// 強化のページの文字（見出し・装備中の武器名と、各項目の文字）を描く。Begin・Endの間に呼ぶ。
	// state: 描画する値。ショップの項目を使う。
	void DrawShopUpgradeTexts(const GAME_HUD_STATE& state);
	// 強化のページの1項目（名前と段階・強化のボタンの文字）を描く。Begin・Endの間に呼ぶ。
	// state: 描画する値。
	// itemIndex: 強化の項目の添字。
	void DrawShopUpgradeRowTexts(const GAME_HUD_STATE& state,int itemIndex);

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