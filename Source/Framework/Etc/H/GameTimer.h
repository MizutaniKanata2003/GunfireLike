#pragma once

// 高精度カウンターからフレーム時間を取得し、ゲーム時間と実時間基準の平均FPSを保持する。
// ゲーム時間には上限制限とTimeScaleを適用する。FPSの集計にはTimeScale適用前の実時間を使う。

//========= Windows インクルード=========
#include <windows.h>

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

// 毎フレームUpdateを呼び、Scene更新などへ渡す経過時間を提供する。
class GameTimer final
{
public:
	//========= 初期化・更新関数=========
	// カウンター周波数と基準時刻を取得する。Updateより先に呼ぶ。
	// 時間・FPS・TimeScaleのメンバーはリセットしない。Win32 APIの成否判定も追加しない。
	void Initialize();
	// 前回からの実時間を取得し、ゲーム時間とFPS集計値を更新する。
	// 前提: Initializeで有効な周波数と基準時刻を取得済みであること。
	void Update();

	//========= Getter関数=========
	// 実時間をMAX_DELTA_TIME以下に制限してからTimeScaleを掛けた直近の秒数を返す。
	// TimeScaleが1を超える場合、返却値がMAX_DELTA_TIMEを超えることがある。
	[[nodiscard]] float GetDeltaTime() const { return m_deltaTime; }
	// Updateで加算したゲーム時間の累積秒数を返す。TimeScaleと上限制限が反映される。
	[[nodiscard]] float GetTotalTime() const { return m_totalTime; }
	// 実時間の累積が1秒以上になった集計区間の平均FPSを返す。最初の集計前は0。
	[[nodiscard]] float GetFrameRate() const { return m_frameRate; }

	//========= Setter関数=========
	// ゲーム時間に掛ける進行倍率を変更する。実時間基準のFPS集計には反映しない。
	// timeScale: 進行倍率。負値は0へ制限する。変更は次のUpdateからゲーム時間に反映される。
	void SetTimeScale(float timeScale) { m_timeScale = std::max(0.0f,timeScale); }

private:
	//========= 高精度タイマー情報=========
	// カウンターの1秒あたりのTick数。実時間への換算に使用する。
	LARGE_INTEGER m_frequency{};
	// 前回取得したカウンター値。Updateの最後ではなく、差分取得直後に現在値へ更新する。
	LARGE_INTEGER m_previousCounter{};

	//========= 時間情報=========
	// 上限制限後にTimeScaleを掛けた、直近フレームのゲーム経過秒数。
	float m_deltaTime{};
	// m_deltaTimeの累積値。単位は秒。Initializeの再呼出ではリセットしない。
	float m_totalTime{};
	// ゲーム時間の進行倍率。既定値は1で、0ならゲーム時間を進めない。
	float m_timeScale{1.0f};

	//========= FPS計算情報=========
	// 現在の集計区間における、上限制限・TimeScale適用前の実時間累積。単位は秒。
	float m_frameAccumulator{};
	// 現在の集計区間でUpdateしたフレーム数。
	unsigned int m_frameCount{};
	// 最後に確定した集計区間の平均FPS。次の集計完了までは維持する。
	float m_frameRate{};
};