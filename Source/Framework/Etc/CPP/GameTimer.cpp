#include "../H/GameTimer.h"

//========= Framework インクルード=========
#include "../H/AppConfig.h"

// 高精度タイマーを初期化し、最初の時刻を記録する。
void GameTimer::Initialize()
{
	QueryPerformanceFrequency(&m_frequency);
	QueryPerformanceCounter(&m_previousCounter);
}

// DeltaTime、総経過時間、FPSを更新する。
void GameTimer::Update()
{
	// 現在時刻と前フレーム時刻から経過Tick数を取得する。
	LARGE_INTEGER current_counter{};
	QueryPerformanceCounter(&current_counter);

	const double elapsed_ticks = static_cast<double>(current_counter.QuadPart - m_previousCounter.QuadPart);

	m_previousCounter = current_counter;

	// 高精度カウンターのTick数を実時間の秒数へ変換する。
	const float real_delta_time = static_cast<float>(elapsed_ticks / static_cast<double>(m_frequency.QuadPart));

	// 最大DeltaTimeとTimeScaleを反映してゲーム時間を更新する。
	m_deltaTime = std::min(real_delta_time,Config::MAX_DELTA_TIME) * m_timeScale;
	m_totalTime += m_deltaTime;

	// 直近1秒間の実時間とフレーム数を累積してFPSを計算する。
	m_frameAccumulator += real_delta_time;
	++m_frameCount;

	if(m_frameAccumulator < 1.0f)return;

	m_frameRate = static_cast<float>(m_frameCount) / m_frameAccumulator;
	m_frameAccumulator = 0.0f;
	m_frameCount = 0;
}