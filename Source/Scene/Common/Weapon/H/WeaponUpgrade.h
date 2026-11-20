#pragma once

// 武器の強化（威力・連射速度・反動抑制・リロード速度・装弾数の5項目）の、段階ごとの効果と価格の表を定義し、強化後の性能値を求める。
// 強化の段階の保持・購入の判断・画面の表示は担当しない。数値は企画書で未確定の仮値で、プレイ確認後に調整する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampで、強化の段階を0〜上限へ収める。
#include <array> // std::arrayで、5項目の段階を固定長で受け取る。
#include <cmath> // std::ceilで、強化後の装弾数を切り上げる。

//========= Scene インクルード=========
#include "Scene/Common/Weapon/H/WeaponTable.h" // 強化の元になる性能値（WEAPON_DATA）を使用する。

// 強化の項目と数値の表をまとめる。
namespace WEAPON_UPGRADE_TABLE
{
  //========= 項目=========
  // 強化の項目。値は、段階の配列・価格・名前の表の添字と同じ。
  enum class UPGRADE_ITEM
  {
    e_DAMAGE,    // 威力。命中1発あたりのダメージが増える。
    e_FIRE_RATE, // 連射速度。発射の間隔が短くなる。
    e_RECOIL,    // 反動抑制。1発ごとの反動が小さくなる。
    e_RELOAD,    // リロード速度。リロードの時間が短くなる。
    e_MAGAZINE   // 装弾数。装填できる弾数が増える。
  };
  // 強化の項目の数。
  inline constexpr int ns_ITEM_COUNT = 5;
  // 各項目の段階の上限。企画書では上限の数は未確定のため、仮値。
  inline constexpr int ns_MAX_LEVEL = 3;

  //========= 名前と価格=========
  // 項目ごとの表示名。添字は、UPGRADE_ITEMの値と同じ。静的な文字列定数を指し、所有しない。
  inline constexpr const wchar_t* ns_ITEM_NAMES[ns_ITEM_COUNT]{L"DAMAGE",L"FIRE RATE",L"RECOIL",L"RELOAD",L"MAGAZINE"};
  // 項目ごとの基本の価格。添字は、UPGRADE_ITEMの値と同じ。次の段階へ上げる価格は、基本の価格に、(現在の段階 + 1)を掛けた値。仮値。
  inline constexpr int ns_BASE_PRICES[ns_ITEM_COUNT]{60,60,50,40,50};

  //========= 段階あたりの効果=========
  // 威力が、段階ごとに増える割合。1段階で、基本のダメージの115%になる。
  inline constexpr float ns_DAMAGE_BONUS_PER_LEVEL = 0.15f;
  // 発射の間隔が、段階ごとに短くなる割合。1段階で、基本の間隔の90%になる。
  inline constexpr float ns_FIRE_INTERVAL_REDUCTION_PER_LEVEL = 0.10f;
  // 1発ごとの反動（上と左右）が、段階ごとに小さくなる割合。1段階で、基本の反動の85%になる。
  inline constexpr float ns_RECOIL_REDUCTION_PER_LEVEL = 0.15f;
  // リロードの時間が、段階ごとに短くなる割合。1段階で、基本の時間の88%になる。
  inline constexpr float ns_RELOAD_REDUCTION_PER_LEVEL = 0.12f;
  // 装弾数が、段階ごとに増える割合。1段階で、基本の装弾数の125%（端数は切り上げ）になる。
  inline constexpr float ns_MAGAZINE_BONUS_PER_LEVEL = 0.25f;

  //========= 価格の計算=========
  // 次の段階へ上げる価格を返す。段階が高いほど、高くなる。
  // itemIndex: 項目の添字。範囲内であること。
  // currentLevel: 現在の段階。0以上。
  // 戻り値: 次の段階へ上げる価格。
  [[nodiscard]] inline constexpr int GetPrice(int itemIndex,int currentLevel)
  {
    return ns_BASE_PRICES[itemIndex] * (currentLevel + 1);
  }
}

// 武器1丁の、5項目の強化の段階。添字は、UPGRADE_ITEMの値と同じ。
using WEAPON_UPGRADE_LEVELS = std::array<int,WEAPON_UPGRADE_TABLE::ns_ITEM_COUNT>;

// 元の性能値へ、強化の段階を反映した性能値を返す。表示名・反動の制御・散布・射程は、元のまま。
// baseData: 強化の元になる性能値。
// levels: 項目ごとの強化の段階。範囲外の段階は、0〜上限へ収める。
// 戻り値: 強化を反映した性能値。
[[nodiscard]] inline WEAPON_DATA BuildUpgradedWeaponData(const WEAPON_DATA& baseData,const WEAPON_UPGRADE_LEVELS& levels)
{
  using namespace WEAPON_UPGRADE_TABLE;

  // 項目ごとの段階を、範囲内の値として取り出す。
  const float damage_level = static_cast<float>(std::clamp(levels[static_cast<int>(UPGRADE_ITEM::e_DAMAGE)],0,ns_MAX_LEVEL));
  const float fire_level = static_cast<float>(std::clamp(levels[static_cast<int>(UPGRADE_ITEM::e_FIRE_RATE)],0,ns_MAX_LEVEL));
  const float recoil_level = static_cast<float>(std::clamp(levels[static_cast<int>(UPGRADE_ITEM::e_RECOIL)],0,ns_MAX_LEVEL));
  const float reload_level = static_cast<float>(std::clamp(levels[static_cast<int>(UPGRADE_ITEM::e_RELOAD)],0,ns_MAX_LEVEL));
  const float magazine_level = static_cast<float>(std::clamp(levels[static_cast<int>(UPGRADE_ITEM::e_MAGAZINE)],0,ns_MAX_LEVEL));

  // 元の性能値をコピーし、強化する項目だけを書き換える。
  WEAPON_DATA upgraded_data = baseData;
  upgraded_data.damage = baseData.damage * (1.0f + ns_DAMAGE_BONUS_PER_LEVEL * damage_level);
  upgraded_data.fireIntervalSeconds = baseData.fireIntervalSeconds * (1.0f - ns_FIRE_INTERVAL_REDUCTION_PER_LEVEL * fire_level);
  upgraded_data.recoilPitchDegrees = baseData.recoilPitchDegrees * (1.0f - ns_RECOIL_REDUCTION_PER_LEVEL * recoil_level);
  upgraded_data.recoilYawDegrees = baseData.recoilYawDegrees * (1.0f - ns_RECOIL_REDUCTION_PER_LEVEL * recoil_level);
  upgraded_data.reloadSeconds = baseData.reloadSeconds * (1.0f - ns_RELOAD_REDUCTION_PER_LEVEL * reload_level);
  upgraded_data.magazineSize = static_cast<int>(std::ceil(static_cast<float>(baseData.magazineSize) * (1.0f + ns_MAGAZINE_BONUS_PER_LEVEL * magazine_level)));
  return upgraded_data;
}
