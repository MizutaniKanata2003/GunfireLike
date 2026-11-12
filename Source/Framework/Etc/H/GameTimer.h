#pragma once

//========= Windows インクルード=========
#include <windows.h>

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

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
	[[nodiscard]] float GetDeltaTime() const { return m_deltaTime; }
	// ゲーム開始からの総経過時間を返す。
	[[nodiscard]] float GetTotalTime() const { return m_totalTime; }
	// 直近1秒間から計算したFPSを返す。
	[[nodiscard]] float GetFrameRate() const { return m_frameRate; }

	//========= Setter関数=========
	// ゲーム時間の進行倍率を設定する。
	// timeScale: 進行倍率。負値は0へ制限する。
	void SetTimeScale(float timeScale) { m_timeScale = std::max(0.0f,timeScale); }

private:
	//========= 高精度タイマー情報=========
	// 高精度カウンターの周波数。
	LARGE_INTEGER m_frequency{};
	// 前フレーム時点の高精度カウンター値。
	LARGE_INTEGER m_previousCounter{};

	//========= 時間情報=========
	// 直近フレームの経過時間。
	float m_deltaTime{};
	// TimeScale適用後のゲーム総経過時間。
	float m_totalTime{};
	// ゲーム時間へ掛ける進行倍率。
	float m_timeScale{1.0f};

	//========= FPS計算情報=========
	// FPS計算用の実時間累積値。
	float m_frameAccumulator{};
	// FPS計算用の累積フレーム数。
	unsigned int m_frameCount{};
	// 直近1秒間の平均FPS。
	float m_frameRate{};
};