#include "../H/GameTimer.h"

// カウンター差分から実時間を算出し、ゲーム時間更新と実時間基準のFPS集計を実装する。
// 計算精度・上限制限と倍率の適用順・集計区間のリセット方法は既存のまま維持する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

//========= Framework インクルード=========
#include "../H/AppConfig.h"

//========= 初期化・更新関数=========
// 時間換算に必要な周波数と、次の差分計算の基準時刻を取得する。
void GameTimer::Initialize()
{
	// 既存どおり取得APIを呼ぶ。時間・FPSの累積値やTimeScaleはここでは初期化し直さない。
	QueryPerformanceFrequency(&m_frequency);
	QueryPerformanceCounter(&m_previousCounter);
}

// ゲーム時間を更新し、実時間の集計区間が1秒以上になった場合だけ平均FPSを確定する。
void GameTimer::Update()
{
	// カウンター差分をdoubleで保持し、次回の差分計算のために基準値を現在値へ進める。
	LARGE_INTEGER current_counter{};
	QueryPerformanceCounter(&current_counter);
	const double elapsed_ticks = static_cast<double>(current_counter.QuadPart - m_previousCounter.QuadPart);
	m_previousCounter = current_counter;

	// 周波数で割って秒へ変換する。Initializeで有効な周波数を取得済みであることを前提とする。
	const float real_delta_time = static_cast<float>(elapsed_ticks / static_cast<double>(m_frequency.QuadPart));

	// 長い停止時間の影響を倍率適用前に制限し、TimeScale反映後の値をゲーム時間へ累積する。
	m_deltaTime = std::min(real_delta_time,Config::MAX_DELTA_TIME) * m_timeScale;
	m_totalTime += m_deltaTime;

	// FPSはゲームの速度変更やDeltaTime上限制限の影響を受けないよう、元の実時間で集計する。
	m_frameAccumulator += real_delta_time;
	++m_frameCount;
	if(m_frameAccumulator < 1.0f)return;

	// 1秒以上の実際の集計区間で平均FPSを求め、余剰時間を繰り越さず区間全体をリセットする。
	m_frameRate = static_cast<float>(m_frameCount) / m_frameAccumulator;
	m_frameAccumulator = 0.0f;
	m_frameCount = 0;
}