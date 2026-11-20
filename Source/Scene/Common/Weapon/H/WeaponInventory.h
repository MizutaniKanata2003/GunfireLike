#pragma once

// プレイヤーが所持している武器の記録を担当する。所持数の上限を守り、武器の種類ごとの所持の有無を返す。
// 武器の装備・弾薬・性能値・購入の画面は担当しない。購入と装備の流れは、呼出側が行う。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、武器の種類ごとの所持の有無を固定長で保持する。

//========= Scene インクルード=========
#include "Scene/Common/Weapon/H/WeaponTable.h" // 武器の種類（WEAPON_TYPE）と、種類の数を使用する。

// 武器の所持の上限をまとめる。
namespace WEAPON_INVENTORY_TABLE
{
  // 同時に所持できる武器の数。企画書の「最大2丁所持」に当たる。
  inline constexpr int ns_MAX_WEAPONS = 2;
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

  //========= Getter関数=========
  // 武器を所持しているかを返す。範囲外の種類は、所持していないとして扱う。
  // weaponType: 調べる武器の種類。
  [[nodiscard]] bool IsOwned(WEAPON_TYPE weaponType) const
  {
    const int weapon_index = static_cast<int>(weaponType);
    return weapon_index >= 0 && weapon_index < WEAPON_TABLE::ns_WEAPON_COUNT && m_isOwned[weapon_index];
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
