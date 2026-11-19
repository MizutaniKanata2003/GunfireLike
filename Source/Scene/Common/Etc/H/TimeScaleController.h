#pragma once

// ヒットストップとスローの残り時間を管理し、1回の更新ごとに、プレイヤー側の時間と、敵・判定側の時間を求める。
// ヒットストップ中は、どちらの時間も0になる。スロー中は、敵・判定側の時間だけが遅くなり、プレイヤー側の時間（プレイヤー・武器）は影響を受けない。
// 時間の使い道（どの更新へどちらの時間を渡すか）は担当しない。残り時間の長さの数値はCombatTableにある。

// 1回の更新で、各系統へ渡す経過時間。
struct TIME_SCALE_DELTAS
{
  // プレイヤー側の経過時間。単位は秒。ヒットストップ中は0。スローの影響を受けない。
  float gameplayDeltaTime{};
  // 敵・判定側の経過時間。単位は秒。ヒットストップ中は0。スロー中は縮める。
  float worldDeltaTime{};
};

// ヒットストップとスローの状態を保持する。
class TimeScaleController final
{
public:
  //========= 初期化関数=========
  // ヒットストップとスローを解除する。
  void Reset();

  //========= 要求関数=========
  // ヒットストップを求める。すでに残っている場合は、長い方の残り時間にする。
  // seconds: 求めるヒットストップの長さ。単位は実時間の秒。
  void RequestHitStop(float seconds);
  // スローを開始する。すでにスロー中の場合は、時間を数え直す。長さはCombatTableの値。
  void StartSlowMotion();

  //========= 更新関数=========
  // 残り時間を実時間で進め、この更新で各系統へ渡す経過時間を返す。ヒットストップ中かは、この更新の開始時点の残り時間で決める。
  // deltaTime: 実時間の経過時間。単位は秒。
  // 戻り値: プレイヤー側と、敵・判定側の経過時間。
  [[nodiscard]] TIME_SCALE_DELTAS Advance(float deltaTime);

  //========= Getter関数=========
  // ヒットストップ中かを返す。
  [[nodiscard]] bool IsHitStopActive() const { return m_hitStopRemainingSeconds > 0.0f; }
  // スロー中かを返す。
  [[nodiscard]] bool IsSlowMotionActive() const { return m_slowMotionRemainingSeconds > 0.0f; }

private:
  //========= 時間の状態=========
  // ヒットストップの残り時間。単位は実時間の秒。0より大きい間、プレイヤーと敵の時間が止まる。
  float m_hitStopRemainingSeconds{};
  // スローの残り時間。単位は実時間の秒。0より大きい間、プレイヤー以外の時間の進みが遅くなる。
  float m_slowMotionRemainingSeconds{};
};
