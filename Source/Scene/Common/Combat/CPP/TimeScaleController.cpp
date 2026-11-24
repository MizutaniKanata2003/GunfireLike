#include "../H/TimeScaleController.h"

// ヒットストップとスローの残り時間の管理と、各系統へ渡す経過時間の算出を実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで、残り時間を0未満にせず、ヒットストップの要求では長い方を残す。

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/CombatTable.h"

//========= 初期化関数=========
// ヒットストップとスローを解除する。
void TimeScaleController::Reset()
{
  m_hitStopRemainingSeconds = 0.0f;
  m_slowMotionRemainingSeconds = 0.0f;
}

//========= 要求関数=========
// ヒットストップを求める。すでに残っている場合は、長い方を使う。
void TimeScaleController::RequestHitStop(float seconds)
{
  m_hitStopRemainingSeconds = std::max(m_hitStopRemainingSeconds,seconds);
}

// スローを開始する。連続した場合は、時間を数え直す。
void TimeScaleController::StartSlowMotion()
{
  m_slowMotionRemainingSeconds = COMBAT_TABLE::ns_SLOW_MOTION_SECONDS;
}

//========= 更新関数=========
// 残り時間を実時間で進め、この更新で各系統へ渡す経過時間を返す。
TIME_SCALE_DELTAS TimeScaleController::Advance(float deltaTime)
{
  // ヒットストップ中かを、この更新の開始時点の残り時間で決め、残り時間を実時間で減らす。
  const bool is_hit_stopped = m_hitStopRemainingSeconds > 0.0f;
  m_hitStopRemainingSeconds = std::max(0.0f,m_hitStopRemainingSeconds - deltaTime);

  // スローの残り時間を実時間で減らし、スロー中なら、敵・判定側の時間の進みを遅くする。
  m_slowMotionRemainingSeconds = std::max(0.0f,m_slowMotionRemainingSeconds - deltaTime);
  const float slowed_delta_time = m_slowMotionRemainingSeconds > 0.0f ? deltaTime * COMBAT_TABLE::ns_SLOW_MOTION_SCALE : deltaTime;

  // プレイヤー側の時間と、敵・判定側の時間を決める。ヒットストップ中は、どちらも0にする。
  TIME_SCALE_DELTAS deltas{};
  deltas.gameplayDeltaTime = is_hit_stopped ? 0.0f : deltaTime;
  deltas.worldDeltaTime = is_hit_stopped ? 0.0f : slowed_delta_time;
  return deltas;
}
