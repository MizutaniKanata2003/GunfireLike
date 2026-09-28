#pragma once

// Player HP状態更新の結果。
struct PlayerHealthStateResult
{
	bool isLowHealth{};
	bool didEnterLowHealth{};
	bool didLeaveLowHealth{};
	bool isPlayerDead{};
};

// Player HPから低HP状態変化と死亡状態を判定する。
// Audio、GameProgress、SceneManagerは所有しない。
class PlayerHealthStateController final
{
public:
	//========= 初期化関数=========
	// Stage開始時の低HP状態を初期化する。
	void Initialize( float currentHp, float maxHp, float lowHealthRatioThreshold );

	//========= 更新関数=========
	// 現在HPから低HP状態の変化と死亡状態を返す。
	[[nodiscard]] PlayerHealthStateResult Update( float currentHp, float maxHp, float lowHealthRatioThreshold );

private:
	//========= 低HP状態=========
	// 前回Update時点で低HP状態だったか。
	bool m_WasLowHealth{};
};