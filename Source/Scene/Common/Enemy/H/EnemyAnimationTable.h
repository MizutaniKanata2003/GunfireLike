#pragma once

// 敵の簡易アニメーション（待機の揺れ・怯みの傾き・予告の前傾・攻撃の突進）の数値を、表としてまとめる。
// 数値の定義だけを持ち、アニメーションの計算や描画は担当しない。骨格のアニメーションは使わず、位置と回転だけを動かす。企画書で未確定の数値は、プレイ確認後に調整する仮値。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tで、待機の揺れの位相をずらす段数を表す。

// 敵の簡易アニメーションの数値をまとめる。
namespace ENEMY_ANIMATION
{
  //========= 共通=========
  // 円周率。突進の動きを、sinの半周期（0から最大を経て0へ戻る動き）にするために使う。
  inline constexpr float ns_PI = 3.14159265f;
  // 円周率の2倍。揺れの位相（周期の数）を、正弦波の角度（ラジアン）へ変換するために使う。
  inline constexpr float ns_TWO_PI = 6.28318530718f;

  //========= 待機の揺れ=========
  // 待機の揺れの振幅。高さ方向に、中心から上下へこの量だけ動く。単位はワールド距離単位。仮値。
  inline constexpr float ns_IDLE_BOB_AMPLITUDE = 0.05f;
  // 待機の揺れの速さ。1秒あたりの周期の数。0.8なら、1周に1.25秒かかる。仮値。
  inline constexpr float ns_IDLE_BOB_CYCLES_PER_SECOND = 0.8f;
  // 敵ごとに、揺れの位相をずらす段数。敵の識別値をこの数で割った余りの位置から揺れを始め、同時に生成した敵が揃って揺れないようにする。
  inline constexpr std::uint32_t ns_IDLE_PHASE_STEP_COUNT = 8;

  //========= 怯みの傾き=========
  // 怯みの傾きの最大の角度。敵の向きに対する横方向（Roll）へ傾ける。単位はラジアン。0.1745は約10度。仮値。
  inline constexpr float ns_FLINCH_TILT_RADIANS = 0.1745f;

  //========= 予告の前傾=========
  // 攻撃の予告中の前傾の最大の角度。敵の向きに対して前へ（Pitch）傾ける。単位はラジアン。0.2618は約15度。仮値。
  inline constexpr float ns_TELEGRAPH_LEAN_RADIANS = 0.2618f;
  // 予告の開始から、前傾が最大になるまでの時間。単位は秒。種類ごとの予告の長さに関わらず、同じ時間で傾ける。仮値。
  inline constexpr float ns_TELEGRAPH_LEAN_SECONDS = 0.3f;

  //========= 攻撃の突進=========
  // 攻撃の瞬間に、正面へ出る最大の距離。単位はワールド距離単位。見た目だけで、Colliderと攻撃の判定の位置は変えない。仮値。
  inline constexpr float ns_ATTACK_LUNGE_DISTANCE = 0.3f;
  // 攻撃の突進で、出て戻るまでの時間。単位は秒。仮値。
  inline constexpr float ns_ATTACK_LUNGE_SECONDS = 0.1f;
}