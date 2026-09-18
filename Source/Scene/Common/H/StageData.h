#pragma once

//========= C++標準ライブラリ インクルード=========
#include <array>

//========= Stage設定定数=========
namespace StageConstants
{
	// Stage番号の開始値と最大Stage数。
	inline constexpr int FIRST_STAGE_NUMBER = 1;
	inline constexpr int MAX_STAGE_COUNT = 5;
}

// 1ステージ分の敵・報酬設定を保持する。
struct StageData
{
	// Stage番号と敵の表示名。
	int stageNumber{ StageConstants::FIRST_STAGE_NUMBER };
	const char* enemyName{};

	// 敵の戦闘パラメータと初回クリア報酬。
	float enemyMaxHp{};
	float enemyDamage{};
	float specialAttackInterval{};
	float specialAttackHitboxRadius{};
	int clearReward{};
};

//========= Stageデータ=========
inline constexpr std::array<StageData, StageConstants::MAX_STAGE_COUNT> STAGE_DATA
{
	StageData
	{
		1,
		"Training Drone",
		100.0f,
		5.0f,
		8.0f,
		1.0f,
		20
	},
	StageData
	{
		2,
		"Sand Raider",
		350.0f,
		10.0f,
		6.5f,
		1.5f,
		40
	},
	StageData
	{
		3,
		"Forest Beast",
		900.0f,
		18.0f,
		5.0f,
		2.0f,
		70
	},
	StageData
	{
		4,
		"Frozen Guardian",
		2200.0f,
		30.0f,
		3.5f,
		2.8f,
		120
	},
	StageData
	{
		5,
		"Ancient Destroyer",
		5000.0f,
		50.0f,
		2.5f,
		3.5f,
		200
	}
};