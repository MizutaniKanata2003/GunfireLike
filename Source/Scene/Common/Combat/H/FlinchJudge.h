#pragma once

// 射撃で命中した敵が怯むかを、確率で判定する。確率は、与えるダメージが敵の最大HPに占める割合から求め、頭を撃った場合は高くする。
// 敵の状態（すでに怯んでいるか・怯みの耐性中か）は見ない。呼出側が、怯める状態の敵だけを判定に渡す。
// 近接攻撃（殴打）の怯みは、確率にしない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampで、確率を0〜1へ制限する。
#include <random> // std::mt19937とstd::uniform_real_distributionで、乱数を作る。

// 怯みの確率の設定と、判定の関数をまとめる。
namespace FLINCH_JUDGE
{
  //========= 怯みの確率の設定=========
  // 「ダメージ ÷ 敵の最大HP」に掛ける倍率。確率の基本の値になる。接近型（最大HP100）に、1発10のダメージで、25%になる。
  inline constexpr float ns_CHANCE_PER_DAMAGE_RATIO = 2.5f;
  // 頭を撃った場合に、確率へ掛ける倍率。
  inline constexpr float ns_HEADSHOT_CHANCE_SCALE = 2.5f;

  //========= 判定関数=========
  // 怯みの確率を求める。
  // damage: この弾が敵へ与えるダメージ。
  // maxHP: 敵の最大HP。0以下の場合は、必ず怯む（1.0）として扱う。
  // isHeadshot: 頭に当たったか。
  // 戻り値: 怯む確率。0〜1。
  inline float CalculateChance(float damage,float maxHP,bool isHeadshot)
  {
    // 最大HPが不正な場合は、ダメージの割合を求められないため、必ず怯むものとする。
    if(maxHP <= 0.0f)return 1.0f;

    const float base_chance = damage / maxHP * ns_CHANCE_PER_DAMAGE_RATIO;
    return std::clamp(isHeadshot ? base_chance * ns_HEADSHOT_CHANCE_SCALE : base_chance,0.0f,1.0f);
  }

  // 確率で、成功か失敗かを決める。
  // chance: 成功する確率。0〜1。1以上なら、必ず成功し、0以下なら、必ず失敗する。
  // 戻り値: 成功したらtrue。
  inline bool Roll(float chance)
  {
    // 乱数の生成器は、スレッドごとに1つ持ち、最初の呼出しで、実行ごとに違う種で初期化する。
    thread_local std::mt19937 engine{std::random_device{}()};
    std::uniform_real_distribution<float> distribution{0.0f,1.0f};
    return distribution(engine) < chance;
  }
}