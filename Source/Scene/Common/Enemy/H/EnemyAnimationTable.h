#pragma once

// 敵のCube描画の簡易アニメーション（待機の揺れ・怯みの傾き・予告の前傾・攻撃の突進）と、モデルの動きの再生の数値を、表としてまとめる。
// 数値の定義だけを持ち、アニメーションの計算や描画は担当しない。Cubeの描画では、骨格のアニメーションは使わず、位置と回転だけを動かす。企画書で未確定の数値は、プレイ確認後に調整する仮値。

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

  //========= モデルの動き=========
  // 待機の動きの開始位置を、敵の識別値ごとにずらす幅。単位は秒。待機の揺れの位相の段数（ns_IDLE_PHASE_STEP_COUNT）を掛けて、待機の動き（4秒）を、ほぼ1周分ずらす。仮値。
  inline constexpr float ns_MODEL_IDLE_STEP_SECONDS = 0.5f;
  // 移動中と判定する、水平の移動の速さの下限。単位はワールド距離単位/秒。敵どうしの押し離しなどの小さな動きで、歩く動きにならないようにする。仮値。
  inline constexpr float ns_MODEL_WALK_MIN_SPEED = 0.3f;
  // 死亡の動きが終わった後に、倒れたままの最後のポーズを見せ続ける時間。単位は秒。過ぎたら、描画をやめる（H8e-3cで、徐々に消える形へ変える）。仮値。
  inline constexpr float ns_MODEL_DEATH_HOLD_SECONDS = 3.0f;
  // 動きの再生の速さの倍率。1.0が等倍。待機・移動・攻撃・死亡は、等倍。
  inline constexpr float ns_MODEL_SPEED_NORMAL = 1.0f;
  // 怯み（Hit、2.17秒）の再生の倍率。ゲーム側の怯みの時間（0.3秒）に対して長いため、速くする。仮値。
  inline constexpr float ns_MODEL_SPEED_HIT = 3.0f;
  // 転倒（Down、2.2秒）の再生の倍率。ゲーム側の転倒の時間（1.5秒）に、ほぼ合わせる。仮値。
  inline constexpr float ns_MODEL_SPEED_DOWN = 1.5f;
  // 起き上がり（GetUp、6.0秒）の再生の倍率。長いため、約2秒にする。仮値。
  inline constexpr float ns_MODEL_SPEED_GET_UP = 3.0f;
}