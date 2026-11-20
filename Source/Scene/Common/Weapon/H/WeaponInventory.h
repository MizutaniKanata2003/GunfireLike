#pragma once

// プレイヤーが所持している武器の記録と、武器ごとの強化の段階・強化に使った費用・切替で保存した弾薬を担当する。所持数の上限と下限を守り、武器の種類ごとの所持の有無を返す。
// 武器の装備・弾薬の消費・性能値・購入と売却と強化の画面は担当しない。購入・売却・強化・切替と装備の流れは、呼出側が行う。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minとstd::maxで、保存した予備弾薬の加算を上限内に収める。
#include <array> // std::arrayで、武器の種類ごとの所持の有無と強化の状態を固定長で保持する。

//========= Scene インクルード=========
#include "Scene/Common/Weapon/H/WeaponTable.h" // 武器の種類（WEAPON_TYPE）と、種類の数を使用する。
#include "Scene/Common/Weapon/H/WeaponUpgrade.h" // 強化の段階の配列と、段階の上限を使用する。

// 武器の所持の上限と下限をまとめる。
namespace WEAPON_INVENTORY_TABLE
{
	// 同時に所持できる武器の数の上限。企画書の「最大2丁所持」に当たる。
	inline constexpr int ns_MAX_WEAPONS = 2;
	// 常に所持する武器の数の下限。最後の1丁は、売却できない。武器が無くなって、射撃できなくなることを防ぐ。
	inline constexpr int ns_MIN_WEAPONS = 1;
}

// 武器を持ち替えた時に保存する、その武器の弾薬。
struct WEAPON_AMMO_RECORD
{
	// 装填弾数。
	int magazineAmmo{};
	// 予備弾薬の数。
	int reserveAmmo{};
};

// 所持している武器と、武器ごとの強化・保存した弾薬を記録する。
class WeaponInventory final
{
public:
	//========= 初期化関数=========
	// 所持・強化・保存した弾薬を初期状態へ戻す。初期武器の連射銃だけを、所持している状態にする。新しいプレイの開始の時に呼ぶ。
	void Initialize()
	{
		m_isOwned.fill(false);
		m_isOwned[static_cast<int>(WEAPON_TYPE::e_ASSAULT_RIFLE)] = true;
		for(WEAPON_UPGRADE_LEVELS& levels : m_upgradeLevels){
			levels.fill(0);
		}
		m_upgradeSpent.fill(0);
		m_hasSavedAmmo.fill(false);
		m_savedAmmo.fill(WEAPON_AMMO_RECORD{});
	}

	//========= 更新関数=========
	// 武器を所持に加える。強化の段階は、0から始まる。
	// weaponType: 加える武器の種類。表の範囲内であること。
	// 戻り値: 加えられたらtrue。すでに所持している、所持数が上限に達している、または範囲外の種類の場合は、何も変えずfalse。
	[[nodiscard]] bool TryAdd(WEAPON_TYPE weaponType)
	{
		// 範囲外の種類、所持済み、上限に達している場合は、加えない。
		const int weapon_index = static_cast<int>(weaponType);
		if(weapon_index < 0 || weapon_index >= WEAPON_TABLE::ns_WEAPON_COUNT)return false;
		if(m_isOwned[weapon_index] || IsFull())return false;

		m_isOwned[weapon_index] = true;
		return true;
	}
	// 武器を所持から外す。その武器の強化の段階・強化に使った費用・保存した弾薬も、消す。
	// weaponType: 外す武器の種類。表の範囲内であること。
	// 戻り値: 外せたらtrue。所持していない、所持数が下限に達している、または範囲外の種類の場合は、何も変えずfalse。
	[[nodiscard]] bool TryRemove(WEAPON_TYPE weaponType)
	{
		// 外せない場合は、何も変えない。
		if(!CanRemove(weaponType))return false;

		const int weapon_index = static_cast<int>(weaponType);
		m_isOwned[weapon_index] = false;
		m_upgradeLevels[weapon_index].fill(0);
		m_upgradeSpent[weapon_index] = 0;
		m_hasSavedAmmo[weapon_index] = false;
		m_savedAmmo[weapon_index] = WEAPON_AMMO_RECORD{};
		return true;
	}
	// 所持している武器の、1項目の強化を、1段階上げる。
	// weaponType: 強化する武器の種類。所持していること。
	// itemIndex: 強化の項目の添字。範囲内であること。
	// price: 強化に支払った価格。売却時の回収の計算のため、記録する。0以上。
	// 戻り値: 上げられたらtrue。所持していない、段階が上限に達している、または範囲外の場合は、何も変えずfalse。
	[[nodiscard]] bool TryUpgrade(WEAPON_TYPE weaponType,int itemIndex,int price)
	{
		// 所持していない、項目が範囲外、価格が負の場合は、上げない。
		const int weapon_index = static_cast<int>(weaponType);
		if(!IsOwned(weaponType) || itemIndex < 0 || itemIndex >= WEAPON_UPGRADE_TABLE::ns_ITEM_COUNT || price < 0)return false;

		// 段階が上限に達している場合は、上げない。
		int& level = m_upgradeLevels[weapon_index][itemIndex];
		if(level >= WEAPON_UPGRADE_TABLE::ns_MAX_LEVEL)return false;

		++level;
		m_upgradeSpent[weapon_index] += price;
		return true;
	}
	// 持ち替える前の武器の弾薬を、保存する。同じ武器へ持ち替えた時に、この弾薬を戻す。
	// weaponIndex: 武器の種類の値（表の添字）。所持している武器であること。範囲外や未所持の場合は、何もしない。
	// magazineAmmo: 保存する装填弾数。
	// reserveAmmo: 保存する予備弾薬の数。
	void SaveAmmo(int weaponIndex,int magazineAmmo,int reserveAmmo)
	{
		if(!IsOwned(static_cast<WEAPON_TYPE>(weaponIndex)))return;

		m_savedAmmo[weaponIndex] = WEAPON_AMMO_RECORD{std::max(0,magazineAmmo),std::max(0,reserveAmmo)};
		m_hasSavedAmmo[weaponIndex] = true;
	}
	// 保存した予備弾薬を増やす。上限を超える分は加えない。装備していない武器の弾薬の購入から呼ぶ。
	// weaponIndex: 武器の種類の値（表の添字）。保存した弾薬がある武器であること。
	// amount: 増やす弾数。0以下の場合は何もしない。
	// maxReserve: 予備弾薬の上限。
	// 戻り値: 実際に加えた弾数。保存した弾薬がない、または範囲外の場合は0。
	int AddSavedReserve(int weaponIndex,int amount,int maxReserve)
	{
		// 保存した弾薬がない武器や、増やす数が正でない場合は、何も加えない。
		if(!HasSavedAmmo(weaponIndex) || amount <= 0)return 0;

		WEAPON_AMMO_RECORD& record = m_savedAmmo[weaponIndex];
		const int added_ammo = std::min(amount,std::max(0,maxReserve - record.reserveAmmo));
		record.reserveAmmo += added_ammo;
		return added_ammo;
	}

	//========= Getter関数=========
	// 武器を所持しているかを返す。範囲外の種類は、所持していないとして扱う。
	// weaponType: 調べる武器の種類。
	[[nodiscard]] bool IsOwned(WEAPON_TYPE weaponType) const
	{
		const int weapon_index = static_cast<int>(weaponType);
		return weapon_index >= 0 && weapon_index < WEAPON_TABLE::ns_WEAPON_COUNT && m_isOwned[weapon_index];
	}
	// 武器を所持から外せるかを返す。所持していて、外しても所持数が下限を下回らない場合だけtrue。
	// weaponType: 調べる武器の種類。
	[[nodiscard]] bool CanRemove(WEAPON_TYPE weaponType) const
	{
		return IsOwned(weaponType) && GetCount() > WEAPON_INVENTORY_TABLE::ns_MIN_WEAPONS;
	}
	// 所持している武器のうち、表の並びで最も先の武器の種類の値を返す。売却で装備中の武器を手放した時に、代わりに装備する武器を選ぶために使う。
	// 戻り値: 武器の種類の値（表の添字）。1丁も所持していない場合は-1。
	[[nodiscard]] int FindFirstOwnedIndex() const
	{
		for(int weapon_index = 0; weapon_index < WEAPON_TABLE::ns_WEAPON_COUNT; ++weapon_index){
			if(m_isOwned[weapon_index])return weapon_index;
		}
		return -1;
	}
	// 武器の強化の段階を、5項目まとめて返す。範囲外の武器は、全て0の段階を返す。戻り値の参照は、このWeaponInventoryの寿命を超えて保持しない。
	// weaponIndex: 武器の種類の値（表の添字）。
	[[nodiscard]] const WEAPON_UPGRADE_LEVELS& GetUpgradeLevels(int weaponIndex) const
	{
		return (weaponIndex >= 0 && weaponIndex < WEAPON_TABLE::ns_WEAPON_COUNT) ? m_upgradeLevels[weaponIndex] : m_zeroLevels;
	}
	// 武器の、1項目の強化の段階を返す。範囲外は0。
	// weaponIndex: 武器の種類の値（表の添字）。
	// itemIndex: 強化の項目の添字。
	[[nodiscard]] int GetUpgradeLevel(int weaponIndex,int itemIndex) const
	{
		return (itemIndex >= 0 && itemIndex < WEAPON_UPGRADE_TABLE::ns_ITEM_COUNT) ? GetUpgradeLevels(weaponIndex)[itemIndex] : 0;
	}
	// 武器の強化に使った費用の合計を返す。売却の時に、一部を回収するために使う。範囲外は0。
	// weaponIndex: 武器の種類の値（表の添字）。
	[[nodiscard]] int GetUpgradeSpent(int weaponIndex) const
	{
		return (weaponIndex >= 0 && weaponIndex < WEAPON_TABLE::ns_WEAPON_COUNT) ? m_upgradeSpent[weaponIndex] : 0;
	}
	// 保存した弾薬があるかを返す。持ち替えて手放した武器と、売却済み・購入直後の武器を区別する。範囲外は、ないとして扱う。
	// weaponIndex: 武器の種類の値（表の添字）。
	[[nodiscard]] bool HasSavedAmmo(int weaponIndex) const
	{
		return weaponIndex >= 0 && weaponIndex < WEAPON_TABLE::ns_WEAPON_COUNT && m_hasSavedAmmo[weaponIndex];
	}
	// 保存した弾薬を返す。保存した弾薬がない場合は、全て0。
	// weaponIndex: 武器の種類の値（表の添字）。
	[[nodiscard]] WEAPON_AMMO_RECORD GetSavedAmmo(int weaponIndex) const
	{
		return HasSavedAmmo(weaponIndex) ? m_savedAmmo[weaponIndex] : WEAPON_AMMO_RECORD{};
	}
	// 所持している武器の数を返す。
	[[nodiscard]] int GetCount() const
	{
		int owned_count = 0;
		for(const bool is_owned : m_isOwned){
			if(is_owned){
				++owned_count;
			}
		}
		return owned_count;
	}
	// 所持数が上限に達しているかを返す。
	[[nodiscard]] bool IsFull() const { return GetCount() >= WEAPON_INVENTORY_TABLE::ns_MAX_WEAPONS; }

private:
	//========= 状態=========
	// 武器の種類ごとの所持の有無。添字は、WEAPON_TYPEの値と同じ。
	std::array<bool,WEAPON_TABLE::ns_WEAPON_COUNT> m_isOwned{};
	// 武器の種類ごとの、5項目の強化の段階。所持していない武器は、全て0。
	std::array<WEAPON_UPGRADE_LEVELS,WEAPON_TABLE::ns_WEAPON_COUNT> m_upgradeLevels{};
	// 武器の種類ごとの、強化に使った費用の合計。所持していない武器は0。
	std::array<int,WEAPON_TABLE::ns_WEAPON_COUNT> m_upgradeSpent{};
	// 武器の種類ごとの、保存した弾薬があるか。
	std::array<bool,WEAPON_TABLE::ns_WEAPON_COUNT> m_hasSavedAmmo{};
	// 武器の種類ごとの、保存した弾薬。保存した弾薬がない武器は、使わない。
	std::array<WEAPON_AMMO_RECORD,WEAPON_TABLE::ns_WEAPON_COUNT> m_savedAmmo{};
	// 範囲外の武器に対して返す、全て0の強化の段階。
	WEAPON_UPGRADE_LEVELS m_zeroLevels{};
};