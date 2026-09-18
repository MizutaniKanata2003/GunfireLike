#include "../H/GameTimer.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

//========= Framework インクルード=========
#include "../H/AppConfig.h"

// 高精度タイマーを初期化し、最初の時刻を記録する。
void GameTimer::Initialize()
{
	QueryPerformanceFrequency( &m_Frequency );
	QueryPerformanceCounter( &m_PreviousCounter );
}

// DeltaTime、総経過時間、FPSを更新する。
void GameTimer::Update()
{
	// 現在時刻と前フレーム時刻から経過Tick数を取得する。
	LARGE_INTEGER currentCounter{};
	QueryPerformanceCounter( &currentCounter );

	const double elapsedTicks =
		static_cast<double>( currentCounter.QuadPart - m_PreviousCounter.QuadPart );

	m_PreviousCounter = currentCounter;

	// 高精度カウンターのTick数を実時間の秒数へ変換する。
	const float realDeltaTime = static_cast<float>( elapsedTicks / static_cast<double>( m_Frequency.QuadPart ) );

	// 最大DeltaTimeとTimeScaleを反映してゲーム時間を更新する。
	m_DeltaTime = std::min( realDeltaTime, Config::MAX_DELTA_TIME ) * m_TimeScale;
	m_TotalTime += m_DeltaTime;

	// 直近1秒間の実時間とフレーム数を累積してFPSを計算する。
	m_FrameAccumulator += realDeltaTime;
	++m_FrameCount;

	if ( m_FrameAccumulator < 1.0f ) return;

	m_FrameRate = static_cast<float>( m_FrameCount ) / m_FrameAccumulator;
	m_FrameAccumulator = 0.0f;
	m_FrameCount = 0;
}

// ゲーム時間の進行倍率を設定する。
void GameTimer::SetTimeScale( float timeScale )
{
	m_TimeScale = std::max( 0.0f, timeScale );
}