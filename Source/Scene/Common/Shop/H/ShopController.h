#pragma once

// 武器と弾薬の購入・武器の売却・武器の強化を行う画面（ショップ）の、開閉・ページ・ボタンの当たりとホバー・各操作ができる状態の判定を管理する。価格の表も持つ。
// 通貨の増減・武器の所持と強化の記録・武器の装備・弾薬の加算・画面の表示・時間の停止は担当しない。購入・売却・強化が決まったことを、結果として呼出側へ返す。
// 画面は、売買のページと、装備中の武器の強化のページの2つで、切替ボタンで行き来する。武器の売却は、誤操作を防ぐため、同じボタンを2回押して確定する。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、ボタンと各行の状態を固定長で保持する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/UI/H/UiButton.h"
#include "Scene/Common/UI/H/UiLayout.h"
#include "Scene/Common/Weapon/H/WeaponInventory.h"
#include "Scene/Common/Weapon/H/WeaponTable.h"
#include "Scene/Common/Weapon/H/WeaponUpgrade.h"

// ショップの画面のページ。
enum class SHOP_PAGE
{
	e_TRADE,  // 武器の購入と売却、弾薬の購入のページ。
	e_UPGRADE // 装備中の武器の強化のページ。
};

// ショップの表をまとめる。文字は、HUDのFontで描ける英字にする。価格・弾数は企画書で未確定の仮値。
namespace SHOP_TABLE
{
	//========= 行とボタン=========
	// 武器の行の数。武器の種類の数と同じで、全ての武器を並べる。弾薬も、武器ごとに1系統で、同じ数になる。
	inline constexpr int ns_ROW_COUNT = WEAPON_TABLE::ns_WEAPON_COUNT;
	// 強化の行の数。強化の項目の数と同じ。
	inline constexpr int ns_UPGRADE_COUNT = WEAPON_UPGRADE_TABLE::ns_ITEM_COUNT;
	// 弾薬の購入ボタンの、最初の添字。武器のボタン（添字0から）の次。行の添字へ足して使う。
	inline constexpr int ns_AMMO_BUTTON_OFFSET = ns_ROW_COUNT;
	// 強化のボタンの、最初の添字。弾薬の購入ボタンの次。強化の項目の添字へ足して使う。
	inline constexpr int ns_UPGRADE_BUTTON_OFFSET = ns_ROW_COUNT * 2;
	// ページ切替ボタンの添字。強化のボタンの次。
	inline constexpr int ns_SWITCH_INDEX = ns_UPGRADE_BUTTON_OFFSET + ns_UPGRADE_COUNT;
	// 閉じるボタンの添字。ページ切替ボタンの次。
	inline constexpr int ns_CLOSE_INDEX = ns_SWITCH_INDEX + 1;
	// ボタンの数。武器、弾薬、強化、ページ切替、閉じるボタン。
	inline constexpr int ns_BUTTON_COUNT = ns_CLOSE_INDEX + 1;

	//========= 武器の価格と名前=========
	// 武器ごとの購入の価格。添字は、WEAPON_TYPEの値と同じ。連射銃は、最初から所持しているが、売却した後に買い戻せるよう、価格を決める。通貨の獲得（敵1体あたり10〜20）に対する仮値。
	inline constexpr int ns_WEAPON_PRICES[ns_ROW_COUNT]{60,100,150};
	// 武器ごとの売却の基本の価格。添字は、WEAPON_TYPEの値と同じ。購入の価格の半分。強化に使った費用は、別に回収する（GetSellPriceを参照）。仮値。
	inline constexpr int ns_WEAPON_SELL_PRICES[ns_ROW_COUNT]{30,50,75};
	// 武器ごとの表示名。添字は、WEAPON_TYPEの値と同じ。静的な文字列定数を指し、所有しない。
	inline constexpr const wchar_t* ns_WEAPON_NAMES[ns_ROW_COUNT]{L"ASSAULT RIFLE",L"SHOTGUN",L"HAND CANNON"};

	//========= 売却の価格=========
	// 武器の売却で得る通貨を返す。購入の価格の半分に、強化に使った費用の半分を足した値。企画書の「売却時に購入・強化費用の一部を回収する」に当たる。
	// weaponIndex: 武器の種類の値（表の添字）。範囲内であること。
	// upgradeSpent: その武器の強化に使った費用の合計。
	[[nodiscard]] inline constexpr int GetSellPrice(int weaponIndex,int upgradeSpent)
	{
		return ns_WEAPON_SELL_PRICES[weaponIndex] + upgradeSpent / 2;
	}

	//========= 弾薬の価格と数=========
	// 弾薬1回の購入で増える予備弾薬の数。添字は、WEAPON_TYPEの値と同じ。装弾数のおよそ2倍。仮値。
	inline constexpr int ns_AMMO_PACK_SIZES[ns_ROW_COUNT]{40,12,6};
	// 弾薬1回の購入の価格。添字は、WEAPON_TYPEの値と同じ。仮値。
	inline constexpr int ns_AMMO_PRICES[ns_ROW_COUNT]{20,25,30};
	// 予備弾薬の上限。添字は、WEAPON_TYPEの値と同じ。初期の予備弾薬（120・36・30）のおよそ2倍。仮値。
	inline constexpr int ns_AMMO_RESERVE_CAPS[ns_ROW_COUNT]{240,72,60};

	//========= ページごとのボタン=========
	// ボタンが、指定したページで使われるかを返す。ページ切替と閉じるボタンは、どちらのページでも使う。
	// buttonIndex: ボタンの添字。
	// page: 調べるページ。
	[[nodiscard]] inline constexpr bool IsButtonOnPage(int buttonIndex,SHOP_PAGE page)
	{
		if(buttonIndex >= ns_SWITCH_INDEX)return true;
		return page == SHOP_PAGE::e_UPGRADE ? buttonIndex >= ns_UPGRADE_BUTTON_OFFSET : buttonIndex < ns_UPGRADE_BUTTON_OFFSET;
	}
}

// 武器の行の、購入と売却の状態。
enum class SHOP_ROW_STATE
{
	e_AVAILABLE,      // 購入できる。
	e_OWNED,          // 所持していて、売却できない（最後の1丁）。
	e_SELLABLE,       // 所持していて、売却できる。
	e_SELL_CONFIRM,   // 売却の確認中。もう1度押すと、売却が確定する。
	e_INVENTORY_FULL, // 所持していなくて、所持数が上限に達している。
	e_TOO_EXPENSIVE   // 所持していなくて、通貨が足りない。
};

// 弾薬の行の、購入の状態。弾薬は、所持している全ての武器について購入できる（装備していない武器は、保存した予備弾薬へ加える）。
enum class SHOP_AMMO_STATE
{
	e_AVAILABLE,     // 購入できる。
	e_NOT_OWNED,     // 武器を所持していない。
	e_NOT_EQUIPPED,  // 現在は発生しない。HUDの文字の整理（GameHud.cpp）で、この値の参照を外してから、削除する。
	e_AMMO_FULL,     // 予備弾薬が上限に達している。
	e_TOO_EXPENSIVE  // 通貨が足りない。
};

// 強化の行の、購入の状態。強化は、装備している武器にだけ行う。
enum class SHOP_UPGRADE_STATE
{
	e_AVAILABLE,     // 強化できる。
	e_MAX_LEVEL,     // 段階が上限に達している。
	e_TOO_EXPENSIVE, // 通貨が足りない。
	e_NO_WEAPON      // 装備中の武器が判別できない。
};

// ショップの更新で起きた出来事。
enum class SHOP_UPDATE_RESULT
{
	e_NONE,                    // 状態の変化はない。
	e_PURCHASE_REQUESTED,      // 武器の購入の操作があった。購入する行は、GetLastPurchaseIndexで取得する。通貨の支払いと所持の記録は、呼出側が行う。
	e_SELL_REQUESTED,          // 武器の売却が確定した。売却する行は、GetLastPurchaseIndexで取得する。通貨の加算と所持の削除は、呼出側が行う。
	e_AMMO_PURCHASE_REQUESTED, // 弾薬の購入の操作があった。購入する行は、GetLastPurchaseIndexで取得する。通貨の支払いと弾薬の加算は、呼出側が行う。
	e_UPGRADE_REQUESTED,       // 強化の購入の操作があった。強化の項目は、GetLastPurchaseIndexで取得する。通貨の支払いと段階の記録は、呼出側が行う。
	e_CLOSED                   // ショップを閉じた。
};

// ショップの更新に渡す入力。
struct SHOP_INPUT
{
	// カーソルの位置。基準Canvas上の座標。ボタンのホバーとクリックの判定に使う。
	XMFLOAT2 cursorCanvasPosition{};
	// クリックが、この更新で押されたか。
	bool isClickTriggered{};
	// この更新で押された、キーに対応するボタンの添字。押されていなければ-1。範囲外の値は無視する。
	int keyChoiceIndex{-1};
};

// ショップの更新に渡す、ゲームの状態。購入できるかの判定に使う。
struct SHOP_CONTEXT
{
	// 現在の通貨の所持額。
	int balance{};
	// 装備している武器の種類の値（表の添字）。判別できない場合は-1。強化の対象を決めるために使う。
	int equippedWeaponIndex{-1};
	// 武器ごとの予備弾薬の数。添字は、WEAPON_TYPEの値と同じ。装備中の武器は現在の値、装備していない武器は保存した値。所持していない武器は、使わない。
	std::array<int,SHOP_TABLE::ns_ROW_COUNT> reserveAmmo{};
};

// 武器と弾薬の購入、武器の売却、武器の強化の画面の状態を持つ。
class ShopController final
{
public:
	//========= 初期化関数=========
	// ボタンの矩形を配置表から設定し、ショップを閉じる。新しいプレイの開始の時に呼ぶ。
	void Initialize()
	{
		// 配置表のボタンの位置と大きさを、ボタンへ設定する。
		for(int button_index = 0; button_index < SHOP_TABLE::ns_BUTTON_COUNT; ++button_index){
			m_buttons[button_index].Initialize(SHOP_LAYOUT::ns_BUTTON_POSITIONS[button_index],SHOP_LAYOUT::ns_BUTTON_SIZE);
		}
		m_lastPurchaseIndex = -1;
		m_rowStates.fill(SHOP_ROW_STATE::e_AVAILABLE);
		m_sellPrices.fill(0);
		m_ammoStates.fill(SHOP_AMMO_STATE::e_AVAILABLE);
		m_upgradeStates.fill(SHOP_UPGRADE_STATE::e_NO_WEAPON);
		Close();
	}
	// ショップを閉じ、ホバーと売却の確認を解除する。購入の記録は消さない。
	void Close()
	{
		m_isOpen = false;
		m_isJustOpened = false;
		m_hoveredIndex = -1;
		m_armedSellIndex = -1;
		for(UiButton& button : m_buttons){
			button.ClearHover();
		}
	}
	// ショップを開く。売買のページから始める。開いた更新では、押されていた操作を、購入や閉じる操作として受け取らない。
	void Open()
	{
		m_isOpen = true;
		m_isJustOpened = true;
		m_page = SHOP_PAGE::e_TRADE;
		m_hoveredIndex = -1;
		m_armedSellIndex = -1;
	}

	//========= 更新関数=========
	// 各行の状態を更新し、ボタンのホバーとクリックから、購入・売却・強化・ページ切替・閉じる操作を判定する。ショップが開いている間だけ、毎回の更新で呼ぶ。
	// input: カーソルの位置・クリック・キーの入力。
	// context: 通貨・装備中の武器・武器ごとの予備弾薬。購入できるかの判定に使う。
	// inventory: 現在の武器の所持と強化の段階。所持済み・上限・売却・強化の判定に使う。
	// 戻り値: この更新で起きた出来事。閉じている場合は、e_NONE。
	[[nodiscard]] SHOP_UPDATE_RESULT Update(const SHOP_INPUT& input,const SHOP_CONTEXT& context,const WeaponInventory& inventory)
	{
		// 閉じている間は、何も更新しない。
		if(!m_isOpen)return SHOP_UPDATE_RESULT::e_NONE;

		// 表示のために、各行の状態は、開いた更新から更新する。
		UpdateStates(context,inventory);

		// 開いた更新は、その更新で押されていた操作（カードを選んだクリックや数字キー）を使わず、ここで戻る。
		if(m_isJustOpened){
			m_isJustOpened = false;
			return SHOP_UPDATE_RESULT::e_NONE;
		}

		// 範囲内で、現在のページにあるボタンのキーの選択があれば、それを選ばれたボタンとして始める。
		int chosen_index = (input.keyChoiceIndex >= 0 && input.keyChoiceIndex < SHOP_TABLE::ns_BUTTON_COUNT &&
												SHOP_TABLE::IsButtonOnPage(input.keyChoiceIndex,m_page)) ? input.keyChoiceIndex : -1;

		// 現在のページのボタンのホバーを更新する。他のページのボタンは、判定しない。複数のボタンが同時にクリックされた場合や、キーとクリックが同時の場合は、先に決まった選択を残す。
		m_hoveredIndex = -1;
		for(int button_index = 0; button_index < SHOP_TABLE::ns_BUTTON_COUNT; ++button_index){
			if(!SHOP_TABLE::IsButtonOnPage(button_index,m_page)){
				m_buttons[button_index].ClearHover();
				continue;
			}

			const bool is_clicked = m_buttons[button_index].Update(input.cursorCanvasPosition,input.isClickTriggered);
			if(m_buttons[button_index].IsHovered()){
				m_hoveredIndex = button_index;
			}
			if(is_clicked && chosen_index < 0){
				chosen_index = button_index;
			}
		}

		// 売却の確認中の行以外が選ばれた場合や、ボタンのない所をクリックした場合は、売却の確認を解除する。
		if(chosen_index != m_armedSellIndex && (chosen_index >= 0 || input.isClickTriggered)){
			m_armedSellIndex = -1;
		}

		// 選ばれたボタンがなければ、何も起こさない。
		if(chosen_index < 0)return SHOP_UPDATE_RESULT::e_NONE;
		return HandleButton(chosen_index);
	}

	//========= Getter関数=========
	// ショップが開いているかを返す。開いている間は、時間とプレイヤーの操作が止まる。
	[[nodiscard]] bool IsOpen() const { return m_isOpen; }
	// 現在のページを返す。
	[[nodiscard]] SHOP_PAGE GetPage() const { return m_page; }
	// カーソルが乗っているボタンの添字を返す。どのボタンにも乗っていなければ-1。
	[[nodiscard]] int GetHoveredIndex() const { return m_hoveredIndex; }
	// 直近に購入・売却・強化が求められた行や項目の添字を返す。まだなければ-1。武器・弾薬は、WEAPON_TYPEの値、強化は、強化の項目の添字と同じ。
	[[nodiscard]] int GetLastPurchaseIndex() const { return m_lastPurchaseIndex; }
	// 各行の武器の状態を返す。添字は、WEAPON_TYPEの値と同じ。戻り値の参照は、このShopControllerの寿命を超えて保持しない。
	[[nodiscard]] const std::array<SHOP_ROW_STATE,SHOP_TABLE::ns_ROW_COUNT>& GetRowStates() const { return m_rowStates; }
	// 各行の武器を売却した時に得る通貨を返す。強化に使った費用の回収を含む。添字は、WEAPON_TYPEの値と同じ。戻り値の参照は、このShopControllerの寿命を超えて保持しない。
	[[nodiscard]] const std::array<int,SHOP_TABLE::ns_ROW_COUNT>& GetSellPrices() const { return m_sellPrices; }
	// 各行の弾薬の購入の状態を返す。添字は、WEAPON_TYPEの値と同じ。戻り値の参照は、このShopControllerの寿命を超えて保持しない。
	[[nodiscard]] const std::array<SHOP_AMMO_STATE,SHOP_TABLE::ns_ROW_COUNT>& GetAmmoStates() const { return m_ammoStates; }
	// 各強化の項目の購入の状態を返す。添字は、強化の項目の添字と同じ。戻り値の参照は、このShopControllerの寿命を超えて保持しない。
	[[nodiscard]] const std::array<SHOP_UPGRADE_STATE,SHOP_TABLE::ns_UPGRADE_COUNT>& GetUpgradeStates() const { return m_upgradeStates; }

private:
	//========= 補助関数=========
	// 選ばれたボタンの種類に応じて、動作を決める。
	// buttonIndex: 選ばれたボタンの添字。範囲内で、現在のページにあること。
	// 戻り値: この操作で起きた出来事。
	[[nodiscard]] SHOP_UPDATE_RESULT HandleButton(int buttonIndex)
	{
		// 閉じるボタンなら、ショップを閉じる。
		if(buttonIndex == SHOP_TABLE::ns_CLOSE_INDEX){
			Close();
			return SHOP_UPDATE_RESULT::e_CLOSED;
		}

		// ページ切替ボタンなら、ページを切り替える。ホバーは、ボタンの配置が変わるため、次の更新まで解除する。
		if(buttonIndex == SHOP_TABLE::ns_SWITCH_INDEX){
			m_page = (m_page == SHOP_PAGE::e_TRADE) ? SHOP_PAGE::e_UPGRADE : SHOP_PAGE::e_TRADE;
			m_armedSellIndex = -1;
			m_hoveredIndex = -1;
			return SHOP_UPDATE_RESULT::e_NONE;
		}

		// 強化のボタンは、強化できる状態の時だけ、強化の操作として受け取る。
		if(buttonIndex >= SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET){
			const int item_index = buttonIndex - SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET;
			if(m_upgradeStates[item_index] != SHOP_UPGRADE_STATE::e_AVAILABLE)return SHOP_UPDATE_RESULT::e_NONE;

			m_lastPurchaseIndex = item_index;
			return SHOP_UPDATE_RESULT::e_UPGRADE_REQUESTED;
		}

		// 武器のボタンは、行の状態に応じて、購入・売却の確認・売却の確定に分ける。
		if(buttonIndex < SHOP_TABLE::ns_AMMO_BUTTON_OFFSET){
			return HandleWeaponButton(buttonIndex);
		}

		// 弾薬のボタンは、購入できる状態の時だけ、購入の操作として受け取る。
		const int ammo_row_index = buttonIndex - SHOP_TABLE::ns_AMMO_BUTTON_OFFSET;
		if(m_ammoStates[ammo_row_index] != SHOP_AMMO_STATE::e_AVAILABLE)return SHOP_UPDATE_RESULT::e_NONE;

		m_lastPurchaseIndex = ammo_row_index;
		return SHOP_UPDATE_RESULT::e_AMMO_PURCHASE_REQUESTED;
	}
	// 武器のボタンが選ばれた時の、状態ごとの動作を決める。
	// rowIndex: 選ばれた行の添字。
	// 戻り値: この操作で起きた出来事。
	[[nodiscard]] SHOP_UPDATE_RESULT HandleWeaponButton(int rowIndex)
	{
		switch(m_rowStates[rowIndex]){
			// 購入できる行は、購入の操作として受け取る。
		case SHOP_ROW_STATE::e_AVAILABLE:
		m_lastPurchaseIndex = rowIndex;
		return SHOP_UPDATE_RESULT::e_PURCHASE_REQUESTED;

		// 売却できる行は、まず確認の状態にする。続けて、同じボタンを押した時に、売却が確定する。
		case SHOP_ROW_STATE::e_SELLABLE:
		m_armedSellIndex = rowIndex;
		m_rowStates[rowIndex] = SHOP_ROW_STATE::e_SELL_CONFIRM;
		return SHOP_UPDATE_RESULT::e_NONE;

		// 売却の確認中の行は、売却を確定する。
		case SHOP_ROW_STATE::e_SELL_CONFIRM:
		m_armedSellIndex = -1;
		m_lastPurchaseIndex = rowIndex;
		return SHOP_UPDATE_RESULT::e_SELL_REQUESTED;

		// 他の状態は、何も起こさない。
		case SHOP_ROW_STATE::e_OWNED:
		case SHOP_ROW_STATE::e_INVENTORY_FULL:
		case SHOP_ROW_STATE::e_TOO_EXPENSIVE:
		break;
		}
		return SHOP_UPDATE_RESULT::e_NONE;
	}
	// 武器・弾薬・強化の、各行の状態と、売却の価格を決める。
	// context: 通貨・装備中の武器・武器ごとの予備弾薬。
	// inventory: 現在の武器の所持と強化の段階。
	void UpdateStates(const SHOP_CONTEXT& context,const WeaponInventory& inventory)
	{
		for(int row_index = 0; row_index < SHOP_TABLE::ns_ROW_COUNT; ++row_index){
			const WEAPON_TYPE weapon_type = static_cast<WEAPON_TYPE>(row_index);
			const bool is_owned = inventory.IsOwned(weapon_type);

			// 売却の価格。購入の価格の半分に、強化に使った費用の一部を足す。
			m_sellPrices[row_index] = SHOP_TABLE::GetSellPrice(row_index,inventory.GetUpgradeSpent(row_index));

			// 武器の状態。所持している行は、売却できるか（確認中か）で分け、所持していない行は、上限、通貨不足の順に調べる。
			if(is_owned){
				if(!inventory.CanRemove(weapon_type)){
					m_rowStates[row_index] = SHOP_ROW_STATE::e_OWNED;
				}
				else{
					m_rowStates[row_index] = (m_armedSellIndex == row_index) ? SHOP_ROW_STATE::e_SELL_CONFIRM : SHOP_ROW_STATE::e_SELLABLE;
				}
			}
			else if(inventory.IsFull()){
				m_rowStates[row_index] = SHOP_ROW_STATE::e_INVENTORY_FULL;
			}
			else if(context.balance < SHOP_TABLE::ns_WEAPON_PRICES[row_index]){
				m_rowStates[row_index] = SHOP_ROW_STATE::e_TOO_EXPENSIVE;
			}
			else{
				m_rowStates[row_index] = SHOP_ROW_STATE::e_AVAILABLE;
			}

			// 弾薬の購入の状態。未所持、上限、通貨不足の順に調べる。装備の有無は問わない。
			if(!is_owned){
				m_ammoStates[row_index] = SHOP_AMMO_STATE::e_NOT_OWNED;
			}
			else if(context.reserveAmmo[row_index] >= SHOP_TABLE::ns_AMMO_RESERVE_CAPS[row_index]){
				m_ammoStates[row_index] = SHOP_AMMO_STATE::e_AMMO_FULL;
			}
			else if(context.balance < SHOP_TABLE::ns_AMMO_PRICES[row_index]){
				m_ammoStates[row_index] = SHOP_AMMO_STATE::e_TOO_EXPENSIVE;
			}
			else{
				m_ammoStates[row_index] = SHOP_AMMO_STATE::e_AVAILABLE;
			}
		}

		// 強化の状態。装備中の武器の段階を調べる。装備中の武器が判別できない場合は、全て強化できない。上限、通貨不足の順に調べる。
		for(int item_index = 0; item_index < SHOP_TABLE::ns_UPGRADE_COUNT; ++item_index){
			if(context.equippedWeaponIndex < 0){
				m_upgradeStates[item_index] = SHOP_UPGRADE_STATE::e_NO_WEAPON;
				continue;
			}

			const int level = inventory.GetUpgradeLevel(context.equippedWeaponIndex,item_index);
			if(level >= WEAPON_UPGRADE_TABLE::ns_MAX_LEVEL){
				m_upgradeStates[item_index] = SHOP_UPGRADE_STATE::e_MAX_LEVEL;
			}
			else if(context.balance < WEAPON_UPGRADE_TABLE::GetPrice(item_index,level)){
				m_upgradeStates[item_index] = SHOP_UPGRADE_STATE::e_TOO_EXPENSIVE;
			}
			else{
				m_upgradeStates[item_index] = SHOP_UPGRADE_STATE::e_AVAILABLE;
			}
		}
	}

	//========= 状態=========
	// ショップが開いているか。
	bool m_isOpen{};
	// 開いた直後の更新か。その更新では、押されていた操作を受け取らない。
	bool m_isJustOpened{};
	// 現在のページ。開くたびに、売買のページへ戻る。
	SHOP_PAGE m_page{SHOP_PAGE::e_TRADE};
	// カーソルが乗っているボタンの添字。乗っていなければ-1。
	int m_hoveredIndex{-1};
	// 直近に購入・売却・強化が求められた行や項目の添字。まだなければ-1。
	int m_lastPurchaseIndex{-1};
	// 売却の確認中の行の添字。確認中でなければ-1。他のボタンを押す、何もない所をクリックする、ページを切り替える、閉じる、のいずれかで解除する。
	int m_armedSellIndex{-1};

	//========= ボタンと行=========
	// ボタンごとの当たり。並びは、武器、弾薬の購入、強化、ページ切替、閉じるボタンの順。
	std::array<UiButton,SHOP_TABLE::ns_BUTTON_COUNT> m_buttons{};
	// 武器の行ごとの状態。
	std::array<SHOP_ROW_STATE,SHOP_TABLE::ns_ROW_COUNT> m_rowStates{};
	// 武器の行ごとの、売却した時に得る通貨。
	std::array<int,SHOP_TABLE::ns_ROW_COUNT> m_sellPrices{};
	// 弾薬の行ごとの購入の状態。
	std::array<SHOP_AMMO_STATE,SHOP_TABLE::ns_ROW_COUNT> m_ammoStates{};
	// 強化の項目ごとの購入の状態。
	std::array<SHOP_UPGRADE_STATE,SHOP_TABLE::ns_UPGRADE_COUNT> m_upgradeStates{};
};

// 配置表のボタンの数と、ショップの表のボタンの数が、一致していることを確認する。
static_assert(SHOP_TABLE::ns_BUTTON_COUNT == SHOP_LAYOUT::ns_BUTTON_COUNT,"ショップの表のボタンの数と、配置表のボタンの数を一致させること。");
// 配置表の武器の行と強化の行の数が、ショップの表と一致していることを確認する。
static_assert(SHOP_TABLE::ns_ROW_COUNT == SHOP_LAYOUT::ns_ROW_COUNT && SHOP_TABLE::ns_UPGRADE_COUNT == SHOP_LAYOUT::ns_UPGRADE_COUNT,
							"ショップの表の行の数と、配置表の行の数を一致させること。");