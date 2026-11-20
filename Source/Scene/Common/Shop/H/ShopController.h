#pragma once

// 武器の購入画面（ショップ）の、開閉・ボタンの当たりとホバー・購入できる状態の判定を管理する。武器と弾薬の価格の表も持つ。
// 通貨の増減・武器の所持の記録・武器の装備・弾薬の加算・画面の表示・時間の停止は担当しない。購入が決まったことを、結果として呼出側へ返す。
// 武器と弾薬の購入を扱う。武器の売却はE9、武器の強化はE10で追加する。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、ボタンと各行の状態を固定長で保持する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/UI/H/UiButton.h"
#include "Scene/Common/UI/H/UiLayout.h"
#include "Scene/Common/Weapon/H/WeaponInventory.h"
#include "Scene/Common/Weapon/H/WeaponTable.h"

// ショップの表をまとめる。文字は、HUDのFontで描ける英字にする。価格・弾数は企画書で未確定の仮値。
namespace SHOP_TABLE
{
	//========= 行とボタン=========
	// 武器の行の数。武器の種類の数と同じで、全ての武器を並べる。弾薬も、武器ごとに1系統で、同じ数になる。
	inline constexpr int ns_ROW_COUNT = WEAPON_TABLE::ns_WEAPON_COUNT;
	// ボタンの数。武器の購入ボタン、弾薬の購入ボタン、閉じるボタン。
	inline constexpr int ns_BUTTON_COUNT = ns_ROW_COUNT * 2 + 1;
	// 弾薬の購入ボタンの、最初の添字。武器の購入ボタンの次。行の添字へ足して使う。
	inline constexpr int ns_AMMO_BUTTON_OFFSET = ns_ROW_COUNT;
	// 閉じるボタンの添字。弾薬の購入ボタンの次。
	inline constexpr int ns_CLOSE_INDEX = ns_ROW_COUNT * 2;

	//========= 武器の価格と名前=========
	// 武器ごとの価格。添字は、WEAPON_TYPEの値と同じ。初期武器の連射銃は、最初から所持しているため0。通貨の獲得（敵1体あたり10〜20）に対する仮値。
	inline constexpr int ns_WEAPON_PRICES[ns_ROW_COUNT]{0,100,150};
	// 武器ごとの表示名。添字は、WEAPON_TYPEの値と同じ。静的な文字列定数を指し、所有しない。
	inline constexpr const wchar_t* ns_WEAPON_NAMES[ns_ROW_COUNT]{L"ASSAULT RIFLE",L"SHOTGUN",L"HAND CANNON"};

	//========= 弾薬の価格と数=========
	// 弾薬1回の購入で増える予備弾薬の数。添字は、WEAPON_TYPEの値と同じ。装弾数のおよそ2倍。仮値。
	inline constexpr int ns_AMMO_PACK_SIZES[ns_ROW_COUNT]{40,12,6};
	// 弾薬1回の購入の価格。添字は、WEAPON_TYPEの値と同じ。仮値。
	inline constexpr int ns_AMMO_PRICES[ns_ROW_COUNT]{20,25,30};
	// 予備弾薬の上限。添字は、WEAPON_TYPEの値と同じ。初期の予備弾薬（120・36・30）のおよそ2倍。仮値。
	inline constexpr int ns_AMMO_RESERVE_CAPS[ns_ROW_COUNT]{240,72,60};
}

// 武器の行の、購入の状態。
enum class SHOP_ROW_STATE
{
	e_AVAILABLE,      // 購入できる。
	e_OWNED,          // すでに所持している。
	e_INVENTORY_FULL, // 所持数が上限に達している。
	e_TOO_EXPENSIVE   // 通貨が足りない。
};

// 弾薬の行の、購入の状態。弾薬は、装備している武器の分だけ購入できる（武器ごとの予備弾薬の保存は、F1の切替の実装で行う）。
enum class SHOP_AMMO_STATE
{
	e_AVAILABLE,     // 購入できる。
	e_NOT_OWNED,     // 武器を所持していない。
	e_NOT_EQUIPPED,  // 武器を所持しているが、装備していない。
	e_AMMO_FULL,     // 予備弾薬が上限に達している。
	e_TOO_EXPENSIVE  // 通貨が足りない。
};

// ショップの更新で起きた出来事。
enum class SHOP_UPDATE_RESULT
{
	e_NONE,                    // 状態の変化はない。
	e_PURCHASE_REQUESTED,      // 武器の購入の操作があった。購入する行は、GetLastPurchaseIndexで取得する。通貨の支払いと所持の記録は、呼出側が行う。
	e_AMMO_PURCHASE_REQUESTED, // 弾薬の購入の操作があった。購入する行は、GetLastPurchaseIndexで取得する。通貨の支払いと弾薬の加算は、呼出側が行う。
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
	// 装備している武器の種類の値（表の添字）。判別できない場合は-1。
	int equippedWeaponIndex{-1};
	// 装備している武器の予備弾薬の数。
	int equippedReserveAmmo{};
};

// 武器の購入画面の状態を持つ。
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
		m_ammoStates.fill(SHOP_AMMO_STATE::e_AVAILABLE);
		Close();
	}
	// ショップを閉じ、ホバーを解除する。購入の記録は消さない。
	void Close()
	{
		m_isOpen = false;
		m_isJustOpened = false;
		m_hoveredIndex = -1;
		for(UiButton& button : m_buttons){
			button.ClearHover();
		}
	}
	// ショップを開く。開いた更新では、押されていた操作を、購入や閉じる操作として受け取らない。
	void Open()
	{
		m_isOpen = true;
		m_isJustOpened = true;
		m_hoveredIndex = -1;
	}

	//========= 更新関数=========
	// 各行の購入の状態を更新し、ボタンのホバーとクリックから、購入と閉じる操作を判定する。ショップが開いている間だけ、毎回の更新で呼ぶ。
	// input: カーソルの位置・クリック・キーの入力。
	// context: 通貨・装備中の武器・予備弾薬。購入できるかの判定に使う。
	// inventory: 現在の武器の所持。所持済みと、上限の判定に使う。
	// 戻り値: この更新で起きた出来事。閉じている場合は、e_NONE。
	[[nodiscard]] SHOP_UPDATE_RESULT Update(const SHOP_INPUT& input,const SHOP_CONTEXT& context,const WeaponInventory& inventory)
	{
		// 閉じている間は、何も更新しない。
		if(!m_isOpen)return SHOP_UPDATE_RESULT::e_NONE;

		// 表示のために、購入の状態は、開いた更新から更新する。
		UpdateStates(context,inventory);

		// 開いた更新は、その更新で押されていた操作（カードを選んだクリックや数字キー）を使わず、ここで戻る。
		if(m_isJustOpened){
			m_isJustOpened = false;
			return SHOP_UPDATE_RESULT::e_NONE;
		}

		// 範囲内のキーの選択があれば、それを選ばれたボタンとして始める。
		int chosen_index = (input.keyChoiceIndex >= 0 && input.keyChoiceIndex < SHOP_TABLE::ns_BUTTON_COUNT) ? input.keyChoiceIndex : -1;

		// 全てのボタンのホバーを更新する。複数のボタンが同時にクリックされた場合や、キーとクリックが同時の場合は、先に決まった選択を残す。
		m_hoveredIndex = -1;
		for(int button_index = 0; button_index < SHOP_TABLE::ns_BUTTON_COUNT; ++button_index){
			const bool is_clicked = m_buttons[button_index].Update(input.cursorCanvasPosition,input.isClickTriggered);
			if(m_buttons[button_index].IsHovered()){
				m_hoveredIndex = button_index;
			}
			if(is_clicked && chosen_index < 0){
				chosen_index = button_index;
			}
		}

		// 選ばれたボタンがなければ、何も起こさない。
		if(chosen_index < 0)return SHOP_UPDATE_RESULT::e_NONE;

		// 閉じるボタンなら、ショップを閉じる。
		if(chosen_index == SHOP_TABLE::ns_CLOSE_INDEX){
			Close();
			return SHOP_UPDATE_RESULT::e_CLOSED;
		}

		// 武器のボタンは、購入できる状態の時だけ、購入の操作として受け取る。所持済み・上限・通貨不足の行は、何も起こさない。
		if(chosen_index < SHOP_TABLE::ns_AMMO_BUTTON_OFFSET){
			if(m_rowStates[chosen_index] != SHOP_ROW_STATE::e_AVAILABLE)return SHOP_UPDATE_RESULT::e_NONE;

			m_lastPurchaseIndex = chosen_index;
			return SHOP_UPDATE_RESULT::e_PURCHASE_REQUESTED;
		}

		// 弾薬のボタンは、購入できる状態の時だけ、購入の操作として受け取る。
		const int ammo_row_index = chosen_index - SHOP_TABLE::ns_AMMO_BUTTON_OFFSET;
		if(m_ammoStates[ammo_row_index] != SHOP_AMMO_STATE::e_AVAILABLE)return SHOP_UPDATE_RESULT::e_NONE;

		m_lastPurchaseIndex = ammo_row_index;
		return SHOP_UPDATE_RESULT::e_AMMO_PURCHASE_REQUESTED;
	}

	//========= Getter関数=========
	// ショップが開いているかを返す。開いている間は、時間とプレイヤーの操作が止まる。
	[[nodiscard]] bool IsOpen() const { return m_isOpen; }
	// カーソルが乗っているボタンの添字を返す。どのボタンにも乗っていなければ-1。
	[[nodiscard]] int GetHoveredIndex() const { return m_hoveredIndex; }
	// 直近に購入が求められた行の添字を返す。まだなければ-1。武器・弾薬のどちらでも、添字は、WEAPON_TYPEの値と同じ。
	[[nodiscard]] int GetLastPurchaseIndex() const { return m_lastPurchaseIndex; }
	// 各行の武器の購入の状態を返す。添字は、WEAPON_TYPEの値と同じ。戻り値の参照は、このShopControllerの寿命を超えて保持しない。
	[[nodiscard]] const std::array<SHOP_ROW_STATE,SHOP_TABLE::ns_ROW_COUNT>& GetRowStates() const { return m_rowStates; }
	// 各行の弾薬の購入の状態を返す。添字は、WEAPON_TYPEの値と同じ。戻り値の参照は、このShopControllerの寿命を超えて保持しない。
	[[nodiscard]] const std::array<SHOP_AMMO_STATE,SHOP_TABLE::ns_ROW_COUNT>& GetAmmoStates() const { return m_ammoStates; }

private:
	//========= 補助関数=========
	// 武器と弾薬の、各行の購入の状態を決める。
	// context: 通貨・装備中の武器・予備弾薬。
	// inventory: 現在の武器の所持。
	void UpdateStates(const SHOP_CONTEXT& context,const WeaponInventory& inventory)
	{
		for(int row_index = 0; row_index < SHOP_TABLE::ns_ROW_COUNT; ++row_index){
			const bool is_owned = inventory.IsOwned(static_cast<WEAPON_TYPE>(row_index));

			// 武器の購入の状態。所持済み、上限、通貨不足の順に調べる。
			if(is_owned){
				m_rowStates[row_index] = SHOP_ROW_STATE::e_OWNED;
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

			// 弾薬の購入の状態。未所持、未装備、上限、通貨不足の順に調べる。
			if(!is_owned){
				m_ammoStates[row_index] = SHOP_AMMO_STATE::e_NOT_OWNED;
			}
			else if(context.equippedWeaponIndex != row_index){
				m_ammoStates[row_index] = SHOP_AMMO_STATE::e_NOT_EQUIPPED;
			}
			else if(context.equippedReserveAmmo >= SHOP_TABLE::ns_AMMO_RESERVE_CAPS[row_index]){
				m_ammoStates[row_index] = SHOP_AMMO_STATE::e_AMMO_FULL;
			}
			else if(context.balance < SHOP_TABLE::ns_AMMO_PRICES[row_index]){
				m_ammoStates[row_index] = SHOP_AMMO_STATE::e_TOO_EXPENSIVE;
			}
			else{
				m_ammoStates[row_index] = SHOP_AMMO_STATE::e_AVAILABLE;
			}
		}
	}

	//========= 状態=========
	// ショップが開いているか。
	bool m_isOpen{};
	// 開いた直後の更新か。その更新では、押されていた操作を受け取らない。
	bool m_isJustOpened{};
	// カーソルが乗っているボタンの添字。乗っていなければ-1。
	int m_hoveredIndex{-1};
	// 直近に購入が求められた行の添字。まだなければ-1。
	int m_lastPurchaseIndex{-1};

	//========= ボタンと行=========
	// ボタンごとの当たり。並びは、武器の購入、弾薬の購入、閉じるボタンの順。
	std::array<UiButton,SHOP_TABLE::ns_BUTTON_COUNT> m_buttons{};
	// 武器の行ごとの購入の状態。
	std::array<SHOP_ROW_STATE,SHOP_TABLE::ns_ROW_COUNT> m_rowStates{};
	// 弾薬の行ごとの購入の状態。
	std::array<SHOP_AMMO_STATE,SHOP_TABLE::ns_ROW_COUNT> m_ammoStates{};
};

// 配置表のボタンの数と、ショップの表のボタンの数が、一致していることを確認する。
static_assert(SHOP_TABLE::ns_BUTTON_COUNT == SHOP_LAYOUT::ns_BUTTON_COUNT,"ショップの表のボタンの数と、配置表のボタンの数を一致させること。");