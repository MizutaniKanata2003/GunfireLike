#pragma once

//========= Windows インクルード=========
#include <windows.h>

// フレームごとの経過時間、総経過時間、FPSを管理する。
class GameTimer final
{
public:
	//========= 初期化・更新関数=========
	// 高精度タイマーを初期化し、最初の時刻を記録する。
	void Initialize();
	// DeltaTime、総経過時間、FPSを更新する。
	void Update();

	//========= Getter関数=========
	// 直近フレームのDeltaTimeを返す。
	[[nodiscard]] float GetDeltaTime() const
	{
		return m_DeltaTime;
	}

	// ゲーム開始からの総経過時間を返す。
	[[nodiscard]] float GetTotalTime() const
	{
		return m_TotalTime;
	}

	// 直近1秒間から計算したFPSを返す。
	[[nodiscard]] float GetFrameRate() const
	{
		return m_FrameRate;
	}

	//========= Setter関数=========
	// ゲーム時間の進行倍率を設定する。
	void SetTimeScale( float timeScale );

private:
	//========= 高精度タイマー情報=========
	// 高精度カウンターの周波数。
	LARGE_INTEGER m_Frequency{};
	// 前フレーム時点の高精度カウンター値。
	LARGE_INTEGER m_PreviousCounter{};

	//========= 時間情報=========
	// 直近フレームの経過時間。
	float m_DeltaTime{};
	// TimeScale適用後のゲーム総経過時間。
	float m_TotalTime{};
	// ゲーム時間へ掛ける進行倍率。
	float m_TimeScale{ 1.0f };

	//========= FPS計算情報=========
	// FPS計算用の実時間累積値。
	float m_FrameAccumulator{};
	// FPS計算用の累積フレーム数。
	unsigned int m_FrameCount{};
	// 直近1秒間の平均FPS。
	float m_FrameRate{};
};