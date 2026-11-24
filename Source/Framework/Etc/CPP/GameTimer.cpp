#include "../H/GameTimer.h"

// 高精度カウンターを秒へ変換し、ゲーム時間へ制限・倍率を適用する一方、FPSは元の実時間で計算する。
// 演算型、キャスト位置、適用順、集計区間のリセット方式は変更しない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで倍率適用前の実時間をConfig::MAX_DELTA_TIME以下へ制限する。

//========= Framework インクルード=========
#include "../H/AppConfig.h"

//========= 初期化・更新関数=========
// 換算周波数と次回Updateの基準値を取得し、フレーム間隔の計測を開始する。
void GameTimer::Initialize()
{
  // 周波数はTick/秒、基準値はカウンターのTick値。APIの成否判定・ログ・復旧は追加しない。
  // 再初期化でもゲーム時間、集計途中のFPS情報、TimeScaleは保存済みの値を維持する。
  QueryPerformanceFrequency(&m_frequency);
  QueryPerformanceCounter(&m_previousCounter);
}

// 前回との差分からゲーム時間を進め、実時間の集計区間が1秒以上になれば平均FPSを確定する。
void GameTimer::Update()
{
  // 現在値を取得して整数Tickの差を求め、その差をdoubleへ変換する。差を取る前のキャストには変更しない。
  // 次回の差分が今回の取得値から始まるよう、計算後すぐに基準値を進める。
  LARGE_INTEGER current_counter{};
  QueryPerformanceCounter(&current_counter);
  const double elapsed_ticks = static_cast<double>(current_counter.QuadPart - m_previousCounter.QuadPart);
  m_previousCounter = current_counter;

  // Tick差をTick/秒で割って秒へ換算し、最後にfloatへ保存する。Initializeで有効な周波数を取得済みとする。
  // ここで得る実時間は、ゲーム時間の制限とFPS集計の両方に使用する。
  const float real_delta_time = static_cast<float>(elapsed_ticks / static_cast<double>(m_frequency.QuadPart));

  // 最初に実時間を上限制限してから倍率を掛ける。倍率適用後のDeltaTimeへ上限を掛け直す処理ではない。
  // 長い停止中の実時間すべてをゲーム時間へ加えず、制限後の値を進行倍率に応じて累積する既存方式を維持する。
  m_deltaTime = std::min(real_delta_time,Config::MAX_DELTA_TIME) * m_timeScale;
  m_totalTime += m_deltaTime;

  // FPSは上限やTimeScaleを適用しない実時間を累積し、今回のUpdateも1フレームとして数える。
  m_frameAccumulator += real_delta_time;
  ++m_frameCount;
  // 実時間が1秒に満たない間はFPS確定だけを省く。上のゲーム時間更新・回数加算は既に実行されている。
  if(m_frameAccumulator < 1.0f)return;

  // フレーム数/実際の集計秒数で区間平均を求める。瞬間値や直近1秒の移動平均ではない。
  // 完了区間の時間と回数をともに0へ戻し、1秒を超えた分を次の区間へ繰り越さない。
  m_frameRate = static_cast<float>(m_frameCount) / m_frameAccumulator;
  m_frameAccumulator = 0.0f;
  m_frameCount = 0;
}