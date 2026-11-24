#pragma once

// 高精度カウンターの差分を秒へ換算し、更新用のゲーム時間と実時間基準の平均FPSを保持する。
// フレームレート制限や待機は行わない。ゲーム時間の上限制限・TimeScaleと、FPSの実時間集計を区別する。

//========= Windows インクルード=========
#include <windows.h> // LARGE_INTEGERでカウンター値を保持し、QueryPerformanceFrequency/Counterで周波数と時刻を取得する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // Header内のSetTimeScaleでstd::maxを使い、負の進行倍率を0へ制限する。

// Initialize後に毎フレームUpdateを呼び、Sceneなどへ渡す直近の経過秒数を提供する。
class GameTimer final
{
public:
  //========= 初期化・更新関数=========
  // 換算用周波数と差分計算の基準値を取得する。時間・FPS・TimeScaleの保存値はリセットしない。
  // Win32 APIの成否は判定しない。有効な周波数と基準値を取得できることをUpdateの前提とする。
  void Initialize();
  // 基準値からの差分を取得し、ゲーム時間とFPS集計値を更新する。先にInitializeを呼ぶこと。
  // 周波数0の検証、取得失敗の復旧、負の実時間や非有限値の追加検証は行わない。
  void Update();

  //========= Getter関数=========
  // 実時間をConfig::MAX_DELTA_TIME以下に制限してからTimeScaleを掛けた、直近のゲーム経過秒数。
  // TimeScale>1では最終値がその上限を超えることがある。Initialize直後に値を再設定する処理はない。
  [[nodiscard]] float GetDeltaTime() const { return m_deltaTime; }
  // Updateで加算したゲーム時間の累積秒数。TimeScaleと上限制限を含み、実時間の経過とは一致しない場合がある。
  [[nodiscard]] float GetTotalTime() const { return m_totalTime; }
  // 実時間が1秒以上となった直近集計区間の平均FPS。最初の集計前は0で、次の集計までは前の値を維持する。
  [[nodiscard]] float GetFrameRate() const { return m_frameRate; }

  //========= Setter関数=========
  // 次回Update以降のゲーム時間へ掛ける倍率を設定する。過去の時間や実時間基準FPSには適用しない。
  // timeScale: 単位なしの進行倍率。負値は0へ制限する。上限の設定や追加の有限値検証は行わない。
  void SetTimeScale(float timeScale) { m_timeScale = std::max(0.0f,timeScale); }

private:
  //========= 高精度タイマー情報=========
  // カウンターの1秒あたりのTick数。初期値0は未取得であり、秒への換算に使える周波数ではない。
  LARGE_INTEGER m_frequency{};
  // 前回の取得値。Initializeで登録し、Updateでは差分を求めた直後に現在値へ進める。
  LARGE_INTEGER m_previousCounter{};

  //========= 時間情報=========
  // 上限制限と倍率を反映した直近フレームのゲーム経過秒数。構築直後は0。
  float m_deltaTime{};
  // ゲーム経過秒数の累積。構築直後は0、Initializeの再呼出ではクリアしない。
  float m_totalTime{};
  // 既定1は制限後の時間を等倍で進める設定。0ではゲーム時間を進めないが、FPSの集計は続ける。
  float m_timeScale{1.0f};

  //========= FPS計算情報=========
  // 上限制限・倍率適用前の実時間累積秒数。構築時と集計完了時は0。余剰時間は繰り越さない。
  float m_frameAccumulator{};
  // 現集計区間のUpdate回数。構築時と集計完了時は0で、実際のPresent成功数を検査する値ではない。
  unsigned int m_frameCount{};
  // 確定済みの区間平均FPS。初期値0はまだ集計を完了していない状態を表す。
  float m_frameRate{};
};