#include "../H/GameHud.h"

// ショップ（武器と弾薬の購入・売却・強化の画面）の画像と文字の描画を実装する。HUDと報酬選択の描画は、GameHud.cppに実装する。
// ボタンの画像・位置は表から読み、状態に応じたボタンの文字・色・押せるかの判定だけを、このファイルで決める。

//========= C++標準ライブラリ インクルード=========
#include <string> // std::to_wstringで価格・弾数・所持数・段階の数字を文字列へ変換し、結合する。

//========= Scene インクルード=========
#include "Scene/Common/UI/H/UiLayout.h"
#include "Scene/Common/Weapon/H/WeaponInventory.h"

// ショップの表示ルールに使う固定設定と、ボタンの補助関数をまとめる。企画書で未確定の見た目の数値は、仮値。
namespace
{
	//========= ボタンの文字の色=========
	// ボタンの文字の色（押せるボタン）。白。
	const XMVECTORF32 ns_BUTTON_TEXT_COLOR{{{1.0f,1.0f,1.0f,1.0f}}};
	// ボタンの文字の色（押せないボタン）。灰色。
	const XMVECTORF32 ns_BUTTON_DISABLED_TEXT_COLOR{{{0.6f,0.6f,0.6f,1.0f}}};
	// ボタンの文字の色（売却の確認中）。橙。押すと売却が確定することを、目立たせる。
	const XMVECTORF32 ns_BUTTON_CONFIRM_TEXT_COLOR{{{1.0f,0.6f,0.2f,1.0f}}};

	//========= 文字の配置（共通）=========
	// 見出しの領域。左上は基準Canvas上の座標で、パネル（上端90）の上部の左側へ置く。大きさ（480×70）と文字の高さの目標（56）も、基準Canvas上の大きさ。
	const HUD_TEXT_AREA ns_TITLE_AREA{XMFLOAT2{200.0f,105.0f},XMFLOAT2{480.0f,70.0f},56.0f};
	// 右上の情報（売買のページの所持数、強化のページの武器名）の領域。左上は基準Canvas上の座標で、パネルの上部の右側へ置く。大きさ（360×50）と文字の高さの目標（30）も、基準Canvas上の大きさ。
	const HUD_TEXT_AREA ns_INFO_AREA{XMFLOAT2{720.0f,115.0f},XMFLOAT2{360.0f,50.0f},30.0f};
	// 操作の案内の領域。左上は基準Canvas上の座標で、最後の行（下端478）と、閉じるボタン（上端546）の間へ置く。大きさ（880×40）と文字の高さの目標（24）も、基準Canvas上の大きさ。
	const HUD_TEXT_AREA ns_GUIDE_AREA{XMFLOAT2{200.0f,490.0f},XMFLOAT2{880.0f,40.0f},24.0f};
	// ボタンの文字の高さの目標。基準Canvas上の大きさ。ボタンの高さ（64）に収まる大きさ。
	constexpr float ns_BUTTON_TEXT_TARGET_HEIGHT = 30.0f;

	//========= 文字の配置（売買のページ）=========
	// 各行の武器名の領域。X座標は基準Canvas上の絶対座標で、武器のボタン（左端580）の手前に収める。Yは、行の上端からの距離で、行の高さ（96）の上側へ置く。大きさは（360×44）、文字の高さの目標は30。
	const HUD_TEXT_AREA ns_TRADE_NAME_AREA{XMFLOAT2{200.0f,8.0f},XMFLOAT2{360.0f,44.0f},30.0f};
	// 各行の予備弾薬の領域。X座標は絶対座標で、Yは、行の上端からの距離。行の高さ（96）の下側へ置く。大きさは（360×36）、文字の高さの目標は22。基準Canvas上の大きさ。
	const HUD_TEXT_AREA ns_TRADE_RESERVE_AREA{XMFLOAT2{200.0f,52.0f},XMFLOAT2{360.0f,36.0f},22.0f};

	//========= 文字の配置（強化のページ）=========
	// 強化の各行の、項目名と段階の領域のX座標。基準Canvas上の絶対座標。強化のボタン（左端840）の手前に収める。
	constexpr float ns_UPGRADE_NAME_AREA_X = 200.0f;
	// 強化の各行の、項目名と段階の領域の幅。基準Canvas上の大きさ。
	constexpr float ns_UPGRADE_NAME_AREA_WIDTH = 620.0f;
	// 強化の項目名と段階の高さの目標。基準Canvas上の大きさ。
	constexpr float ns_UPGRADE_NAME_TARGET_HEIGHT = 32.0f;

	//========= ボタンの補助関数=========
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
		// 状態ごとに、購入・所持・売却・確認・満杯・通貨不足を表す文字へ対応させる。購入と売却には、金額を添える。
		switch(rowState){
		case SHOP_ROW_STATE::e_AVAILABLE:      return L"BUY " + std::to_wstring(price); // 購入できる。価格を添える。
		case SHOP_ROW_STATE::e_OWNED:          return L"LAST GUN"; // 最後の1丁のため売却できない。
		case SHOP_ROW_STATE::e_SELLABLE:       return L"SELL " + std::to_wstring(sellPrice); // 売却できる。得る通貨を添える。
		case SHOP_ROW_STATE::e_SELL_CONFIRM:   return L"CONFIRM " + std::to_wstring(sellPrice); // 売却の確認中。押すと確定する。
		case SHOP_ROW_STATE::e_INVENTORY_FULL: return L"FULL"; // 所持の枠が満杯で購入できない。
		case SHOP_ROW_STATE::e_TOO_EXPENSIVE:  return L"NEED MORE"; // 通貨が足りない。
		}

		// 未定義の状態は、文字を描かない。
		return L"";
	}

	// 弾薬の購入の状態に応じた、ボタンの文字を返す。購入できる時は、増える弾数と価格を添える。
	// ammoState: 弾薬の行の購入の状態。
	// packSize: 1回の購入で増える弾数。
	// price: 弾薬の価格。
	std::wstring GetAmmoButtonLabel(SHOP_AMMO_STATE ammoState,int packSize,int price)
	{
		// 状態ごとに、購入・武器なし・未装備・満杯・通貨不足を表す文字へ対応させる。
		switch(ammoState){
		case SHOP_AMMO_STATE::e_AVAILABLE:     return L"AMMO +" + std::to_wstring(packSize) + L" / " + std::to_wstring(price); // 購入できる。弾数と価格を添える。
		case SHOP_AMMO_STATE::e_NOT_OWNED:     return L"NO WEAPON"; // その武器を所持していない。
		case SHOP_AMMO_STATE::e_NOT_EQUIPPED:  return L"EQUIP FIRST"; // その武器を装備していない。
		case SHOP_AMMO_STATE::e_AMMO_FULL:     return L"AMMO FULL"; // 予備弾薬が上限に達している。
		case SHOP_AMMO_STATE::e_TOO_EXPENSIVE: return L"NEED MORE"; // 通貨が足りない。
		}

		// 未定義の状態は、文字を描かない。
		return L"";
	}

	// 強化の購入の状態に応じた、ボタンの文字を返す。強化できる時は、価格を添える。
	// upgradeState: 強化の項目の購入の状態。
	// price: 次の段階へ上げる価格。
	std::wstring GetUpgradeButtonLabel(SHOP_UPGRADE_STATE upgradeState,int price)
	{
		// 状態ごとに、強化・最大段階・通貨不足・武器なしを表す文字へ対応させる。
		switch(upgradeState){
		case SHOP_UPGRADE_STATE::e_AVAILABLE:     return L"UP " + std::to_wstring(price); // 強化できる。価格を添える。
		case SHOP_UPGRADE_STATE::e_MAX_LEVEL:     return L"MAX LEVEL"; // 最大の段階に達している。
		case SHOP_UPGRADE_STATE::e_TOO_EXPENSIVE: return L"NEED MORE"; // 通貨が足りない。
		case SHOP_UPGRADE_STATE::e_NO_WEAPON:     return L"NO WEAPON"; // 強化の対象の武器がない。
		}

		// 未定義の状態は、文字を描かない。
		return L"";
	}

	// ボタンが、押せる状態かを返す。ページ切替と閉じるボタンは、常に押せる。
	// buttonIndex: ボタンの添字。
	// shop: 描画するショップの値。
	bool IsShopButtonPressable(int buttonIndex,const HUD_SHOP_STATE& shop)
	{
		// 武器のボタンは、行の状態から押せるかを決める。
		if(buttonIndex < SHOP_TABLE::ns_AMMO_BUTTON_OFFSET){
			return IsWeaponButtonPressable(shop.rowStates[buttonIndex]);
		}

		// 弾薬のボタンは、購入できる状態の時だけ押せる。
		if(buttonIndex < SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET){
			return shop.ammoStates[buttonIndex - SHOP_TABLE::ns_AMMO_BUTTON_OFFSET] == SHOP_AMMO_STATE::e_AVAILABLE;
		}

		// 強化のボタンは、強化できる状態の時だけ押せる。
		if(buttonIndex < SHOP_TABLE::ns_SWITCH_INDEX){
			return shop.upgradeStates[buttonIndex - SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET] == SHOP_UPGRADE_STATE::e_AVAILABLE;
		}

		// ページ切替と閉じるボタンは、常に押せる。
		return true;
	}

	// 武器のボタンの文字の色を返す。購入・売却できる時は白、売却の確認中は橙、他は灰色。
	// rowState: 武器の行の状態。
	const XMVECTORF32& GetWeaponButtonTextColor(SHOP_ROW_STATE rowState)
	{
		// 売却の確認中は、確定することを目立たせる色にする。
		if(rowState == SHOP_ROW_STATE::e_SELL_CONFIRM)return ns_BUTTON_CONFIRM_TEXT_COLOR;

		// 押せる状態は白、押せない状態は灰色にする。
		return IsWeaponButtonPressable(rowState) ? ns_BUTTON_TEXT_COLOR : ns_BUTTON_DISABLED_TEXT_COLOR;
	}

	// 弾薬・強化のボタンの文字の色を返す。購入・強化できる時だけ白、他は灰色。
	// isAvailable: 購入・強化できる状態か。
	const XMVECTORF32& GetAvailableButtonTextColor(bool isAvailable)
	{
		return isAvailable ? ns_BUTTON_TEXT_COLOR : ns_BUTTON_DISABLED_TEXT_COLOR;
	}

	// ボタンの文字を置く領域を返す。ボタンの位置と大きさに合わせ、文字の高さの目標はボタン共通の値にする。
	// buttonIndex: ボタンの添字。
	HUD_TEXT_AREA GetButtonTextArea(int buttonIndex)
	{
		return HUD_TEXT_AREA{SHOP_LAYOUT::ns_BUTTON_POSITIONS[buttonIndex],SHOP_LAYOUT::ns_BUTTON_SIZE,ns_BUTTON_TEXT_TARGET_HEIGHT};
	}
}

//========= ショップの描画関数=========
// ショップのパネルと、現在のページのボタンの画像を描く。
void GameHud::DrawShopSprites(GraphicsSystem& graphicsSystem,const HUD_SHOP_STATE& shop)
{
	// パネルを先に描く。
	m_spriteRenderer.DrawSprite(graphicsSystem,m_shopPanelTexture,SHOP_LAYOUT::ns_PANEL.position,SHOP_LAYOUT::ns_PANEL.size,GAME_HUD_STYLE::ns_WHITE,1.0f);

	// 現在のページのボタンだけを、添字の順に描く。押せないボタンは、押せない時の画像にする。押せるボタンは、カーソルが乗っていればホバー用の画像にする。
	for(int button_index = 0; button_index < SHOP_LAYOUT::ns_BUTTON_COUNT; ++button_index){
		// 現在のページに属さないボタンは、描かずに次へ進む。
		if(!SHOP_TABLE::IsButtonOnPage(button_index,shop.page))continue;

		HudTextureHandle button_texture = m_shopButtonDisabledTexture;
		// 押せるボタンだけ、通常の画像か、カーソルが乗っている時の画像へ切り替える。
		if(IsShopButtonPressable(button_index,shop)){
			button_texture = (button_index == shop.hoveredIndex) ? m_shopButtonHoverTexture : m_shopButtonTexture;
		}
		m_spriteRenderer.DrawSprite(graphicsSystem,button_texture,SHOP_LAYOUT::ns_BUTTON_POSITIONS[button_index],SHOP_LAYOUT::ns_BUTTON_SIZE,
																GAME_HUD_STYLE::ns_WHITE,1.0f);
	}
}

// ショップの文字を、現在のページに応じて描く。
void GameHud::DrawShopTexts(const GAME_HUD_STATE& state)
{
	// 現在のページの文字を描く。
	if(state.shop.page == SHOP_PAGE::e_TRADE){
		DrawShopTradeTexts(state);
	}
	// 売買のページでなければ、強化のページの文字を描く。
	else{
		DrawShopUpgradeTexts(state.shop);
	}

	// ページ切替と閉じるボタンの文字は、どちらのページでも描く。切替ボタンは、行き先のページの名前にする。
	DrawFittedText(state.shop.page == SHOP_PAGE::e_TRADE ? L"UPGRADES" : L"BACK",GetButtonTextArea(SHOP_TABLE::ns_SWITCH_INDEX),ns_BUTTON_TEXT_COLOR);
	DrawFittedText(L"CLOSE",GetButtonTextArea(SHOP_TABLE::ns_CLOSE_INDEX),ns_BUTTON_TEXT_COLOR);
}

// 売買のページの文字を描く。
void GameHud::DrawShopTradeTexts(const GAME_HUD_STATE& state)
{
	// 見出しと、所持数（上限との比）と、操作の案内を描く。
	DrawFittedText(L"SHOP",ns_TITLE_AREA,GAME_HUD_STYLE::ns_TITLE_TEXT_COLOR);
	DrawFittedText(L"SLOTS " + std::to_wstring(state.shop.ownedWeaponCount) + L" / " + std::to_wstring(WEAPON_INVENTORY_TABLE::ns_MAX_WEAPONS),
								 ns_INFO_AREA,GAME_HUD_STYLE::ns_BODY_TEXT_COLOR);
	DrawFittedText(L"CLICK  (1-3: WEAPON  4-6: AMMO)   SELL: PRESS TWICE   U: UPGRADES   ENTER: CLOSE",ns_GUIDE_AREA,GAME_HUD_STYLE::ns_BODY_TEXT_COLOR);

	// 各行の武器名・予備弾薬・2つのボタンの文字を、行の順に描く。
	for(int row_index = 0; row_index < SHOP_TABLE::ns_ROW_COUNT; ++row_index){
		DrawShopTradeRowTexts(state,row_index);
	}
}

// 売買のページの1行の文字を描く。
void GameHud::DrawShopTradeRowTexts(const GAME_HUD_STATE& state,int rowIndex)
{
	// 武器名を、行の上側へ描く。領域のYは、行の上端からの距離のため、行の上端の座標を加える。
	const XMFLOAT2 row_origin{0.0f,SHOP_LAYOUT::ns_ROW_TOP + SHOP_LAYOUT::ns_ROW_HEIGHT * static_cast<float>(rowIndex)};
	DrawFittedText(SHOP_TABLE::ns_WEAPON_NAMES[rowIndex],GAME_HUD_STYLE::ShiftTextArea(ns_TRADE_NAME_AREA,row_origin),GAME_HUD_STYLE::ns_BODY_TEXT_COLOR);

	// 予備弾薬は、装備している武器の行にだけ、「RESERVE 現在 / 上限」の形で、行の下側へ描く。
	if(rowIndex == state.shop.equippedWeaponIndex){
		DrawFittedText(L"RESERVE " + std::to_wstring(state.reserveAmmo) + L" / " + std::to_wstring(SHOP_TABLE::ns_AMMO_RESERVE_CAPS[rowIndex]),
									 GAME_HUD_STYLE::ShiftTextArea(ns_TRADE_RESERVE_AREA,row_origin),GAME_HUD_STYLE::ns_CURRENCY_TEXT_COLOR);
	}

	// 武器のボタンの文字。購入・売却できる時は白、売却の確認中は橙、他は灰色。
	const SHOP_ROW_STATE row_state = state.shop.rowStates[rowIndex];
	DrawFittedText(GetWeaponButtonLabel(row_state,SHOP_TABLE::ns_WEAPON_PRICES[rowIndex],state.shop.sellPrices[rowIndex]),GetButtonTextArea(rowIndex),
								 GetWeaponButtonTextColor(row_state));

	// 弾薬の購入ボタンの文字。購入できる時だけ白、他は灰色。
	const SHOP_AMMO_STATE ammo_state = state.shop.ammoStates[rowIndex];
	DrawFittedText(GetAmmoButtonLabel(ammo_state,SHOP_TABLE::ns_AMMO_PACK_SIZES[rowIndex],SHOP_TABLE::ns_AMMO_PRICES[rowIndex]),
								 GetButtonTextArea(SHOP_TABLE::ns_AMMO_BUTTON_OFFSET + rowIndex),GetAvailableButtonTextColor(ammo_state == SHOP_AMMO_STATE::e_AVAILABLE));
}

// 強化のページの文字を描く。
void GameHud::DrawShopUpgradeTexts(const HUD_SHOP_STATE& shop)
{
	// 見出しを描く。
	DrawFittedText(L"UPGRADE",ns_TITLE_AREA,GAME_HUD_STYLE::ns_TITLE_TEXT_COLOR);

	// 強化の対象（装備中の武器）の名前を、右上へ描く。装備中の武器が判別できない場合は、名前を描かない。
	if(shop.equippedWeaponIndex >= 0 && shop.equippedWeaponIndex < SHOP_TABLE::ns_ROW_COUNT){
		DrawFittedText(SHOP_TABLE::ns_WEAPON_NAMES[shop.equippedWeaponIndex],ns_INFO_AREA,GAME_HUD_STYLE::ns_CURRENCY_TEXT_COLOR);
	}

	// 各項目の名前と段階、強化のボタンの文字を、項目の順に描く。
	for(int item_index = 0; item_index < SHOP_TABLE::ns_UPGRADE_COUNT; ++item_index){
		DrawShopUpgradeRowTexts(shop,item_index);
	}
}

// 強化のページの1項目の文字を描く。
void GameHud::DrawShopUpgradeRowTexts(const HUD_SHOP_STATE& shop,int itemIndex)
{
	// 項目の名前と段階を「名前   LV 現在 / 最大」の形で描く。領域は、項目の行の上端から、行の高さの分だけ取る。
	const float row_top = SHOP_LAYOUT::ns_UPGRADE_ROW_TOP + SHOP_LAYOUT::ns_UPGRADE_ROW_STEP * static_cast<float>(itemIndex);
	const int level = shop.upgradeLevels[itemIndex];
	const HUD_TEXT_AREA name_area{XMFLOAT2{ns_UPGRADE_NAME_AREA_X,row_top},XMFLOAT2{ns_UPGRADE_NAME_AREA_WIDTH,SHOP_LAYOUT::ns_UPGRADE_ROW_HEIGHT},
		ns_UPGRADE_NAME_TARGET_HEIGHT};
	DrawFittedText(std::wstring{WEAPON_UPGRADE_TABLE::ns_ITEM_NAMES[itemIndex]} + L"   LV " + std::to_wstring(level) + L" / " +
								 std::to_wstring(WEAPON_UPGRADE_TABLE::ns_MAX_LEVEL),name_area,GAME_HUD_STYLE::ns_BODY_TEXT_COLOR);

	// 強化のボタンの文字。強化できる時は、次の段階の価格を添えて白、他は灰色。
	const SHOP_UPGRADE_STATE upgrade_state = shop.upgradeStates[itemIndex];
	DrawFittedText(GetUpgradeButtonLabel(upgrade_state,WEAPON_UPGRADE_TABLE::GetPrice(itemIndex,level)),
								 GetButtonTextArea(SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET + itemIndex),
								 GetAvailableButtonTextColor(upgrade_state == SHOP_UPGRADE_STATE::e_AVAILABLE));
}