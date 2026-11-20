#pragma once

// プレイヤーが所持している武器の記録を担当する。所持数の上限と下限を守り、武器の種類ごとの所持の有無を返す。
// 武器の装備・弾薬・性能値・購入と売却の画面は担当しない。購入・売却と装備の流れは、呼出側が行う。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、武器の種類ごとの所持の有無を固定長で保持する。

//========= Scene インクルード=========
#include "Scene/Common/Weapon/H/WeaponTable.h" // 武器の種類（WEAPON_TYPE）と、種類の数を使用する。

// 武器の所持の上限と下限をまとめる。
namespace WEAPON_INVENTORY_TABLE
{
	// 同時に所持できる武器の数の上限。企画書の「最大2丁所持」に当たる。
	inline constexpr int ns_MAX_WEAPONS = 2;
	// 常に所持する武器の数の下限。最後の1丁は、売却できない。武器が無くなって、射撃できなくなることを防ぐ。
	inline constexpr int ns_MIN_WEAPONS = 1;
}

// 所持している武器を記録する。
class WeaponInventory final
{
public:
	//========= 初期化関数=========
	// 所持を初期状態へ戻す。初期武器の連射銃だけを、所持している状態にする。新しいプレイの開始の時に呼ぶ。
	void Initialize()
	{
		m_isOwned.fill(false);
		m_isOwned[static_cast<int>(WEAPON_TYPE::e_ASSAULT_RIFLE)] = true;
	}

	//========= 更新関数=========
	// 武器を所持に加える。
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
	// 武器を所持から外す。
	// weaponType: 外す武器の種類。表の範囲内であること。
	// 戻り値: 外せたらtrue。所持していない、所持数が下限に達している、または範囲外の種類の場合は、何も変えずfalse。
	[[nodiscard]] bool TryRemove(WEAPON_TYPE weaponType)
	{
		// 外せない場合は、何も変えない。
		if(!CanRemove(weaponType))return false;

		m_isOwned[static_cast<int>(weaponType)] = false;
		return true;
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
};