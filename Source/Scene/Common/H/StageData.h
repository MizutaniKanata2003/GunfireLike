#pragma once

#include <array>

namespace StageConstants
{
	inline constexpr int FIRST_STAGE_NUMBER = 1;
	inline constexpr int MAX_STAGE_COUNT = 5;

	inline constexpr float STAGE_1_ENEMY_MAX_HP = 100.0f;
	inline constexpr float STAGE_1_ENEMY_DAMAGE = 5.0f;
	inline constexpr float STAGE_1_SPECIAL_ATTACK_INTERVAL = 8.0f;
	inline constexpr float STAGE_1_SPECIAL_ATTACK_HITBOX_RADIUS = 1.0f;
	inline constexpr int STAGE_1_FIRST_CLEAR_REWARD = 20;

	inline constexpr float STAGE_2_ENEMY_MAX_HP = 350.0f;
	inline constexpr float STAGE_2_ENEMY_DAMAGE = 10.0f;
	inline constexpr float STAGE_2_SPECIAL_ATTACK_INTERVAL = 6.5f;
	inline constexpr float STAGE_2_SPECIAL_ATTACK_HITBOX_RADIUS = 1.5f;
	inline constexpr int STAGE_2_FIRST_CLEAR_REWARD = 40;

	inline constexpr float STAGE_3_ENEMY_MAX_HP = 900.0f;
	inline constexpr float STAGE_3_ENEMY_DAMAGE = 18.0f;
	inline constexpr float STAGE_3_SPECIAL_ATTACK_INTERVAL = 5.0f;
	inline constexpr float STAGE_3_SPECIAL_ATTACK_HITBOX_RADIUS = 2.0f;
	inline constexpr int STAGE_3_FIRST_CLEAR_REWARD = 70;

	inline constexpr float STAGE_4_ENEMY_MAX_HP = 2200.0f;
	inline constexpr float STAGE_4_ENEMY_DAMAGE = 30.0f;
	inline constexpr float STAGE_4_SPECIAL_ATTACK_INTERVAL = 3.5f;
	inline constexpr float STAGE_4_SPECIAL_ATTACK_HITBOX_RADIUS = 2.8f;
	inline constexpr int STAGE_4_FIRST_CLEAR_REWARD = 120;

	inline constexpr float STAGE_5_ENEMY_MAX_HP = 5000.0f;
	inline constexpr float STAGE_5_ENEMY_DAMAGE = 50.0f;
	inline constexpr float STAGE_5_SPECIAL_ATTACK_INTERVAL = 2.5f;
	inline constexpr float STAGE_5_SPECIAL_ATTACK_HITBOX_RADIUS = 3.5f;
	inline constexpr int STAGE_5_FIRST_CLEAR_REWARD = 200;

	inline constexpr const char* STAGE_1_ENEMY_NAME = "Training Drone";
	inline constexpr const char* STAGE_2_ENEMY_NAME = "Sand Raider";
	inline constexpr const char* STAGE_3_ENEMY_NAME = "Forest Beast";
	inline constexpr const char* STAGE_4_ENEMY_NAME = "Frozen Guardian";
	inline constexpr const char* STAGE_5_ENEMY_NAME = "Ancient Destroyer";
}

// 1ステージ分の敵・報酬設定。
struct StageData
{
	int stageNumber = StageConstants::FIRST_STAGE_NUMBER;
	const char* enemyName = StageConstants::STAGE_1_ENEMY_NAME;

	float enemyMaxHp = StageConstants::STAGE_1_ENEMY_MAX_HP;
	float enemyDamage = StageConstants::STAGE_1_ENEMY_DAMAGE;
	float specialAttackInterval = StageConstants::STAGE_1_SPECIAL_ATTACK_INTERVAL;
	float specialAttackHitboxRadius = StageConstants::STAGE_1_SPECIAL_ATTACK_HITBOX_RADIUS;
	int clearReward = StageConstants::STAGE_1_FIRST_CLEAR_REWARD;
};

inline constexpr std::array<StageData, StageConstants::MAX_STAGE_COUNT>
STAGE_DATA =
{
	StageData
	{
		StageConstants::FIRST_STAGE_NUMBER,
		StageConstants::STAGE_1_ENEMY_NAME,
		StageConstants::STAGE_1_ENEMY_MAX_HP,
		StageConstants::STAGE_1_ENEMY_DAMAGE,
		StageConstants::STAGE_1_SPECIAL_ATTACK_INTERVAL,
		StageConstants::STAGE_1_SPECIAL_ATTACK_HITBOX_RADIUS,
		StageConstants::STAGE_1_FIRST_CLEAR_REWARD
	},
	StageData
	{
		StageConstants::FIRST_STAGE_NUMBER + 1,
		StageConstants::STAGE_2_ENEMY_NAME,
		StageConstants::STAGE_2_ENEMY_MAX_HP,
		StageConstants::STAGE_2_ENEMY_DAMAGE,
		StageConstants::STAGE_2_SPECIAL_ATTACK_INTERVAL,
		StageConstants::STAGE_2_SPECIAL_ATTACK_HITBOX_RADIUS,
		StageConstants::STAGE_2_FIRST_CLEAR_REWARD
	},
	StageData
	{
		StageConstants::FIRST_STAGE_NUMBER + 2,
		StageConstants::STAGE_3_ENEMY_NAME,
		StageConstants::STAGE_3_ENEMY_MAX_HP,
		StageConstants::STAGE_3_ENEMY_DAMAGE,
		StageConstants::STAGE_3_SPECIAL_ATTACK_INTERVAL,
		StageConstants::STAGE_3_SPECIAL_ATTACK_HITBOX_RADIUS,
		StageConstants::STAGE_3_FIRST_CLEAR_REWARD
	},
	StageData
	{
		StageConstants::FIRST_STAGE_NUMBER + 3,
		StageConstants::STAGE_4_ENEMY_NAME,
		StageConstants::STAGE_4_ENEMY_MAX_HP,
		StageConstants::STAGE_4_ENEMY_DAMAGE,
		StageConstants::STAGE_4_SPECIAL_ATTACK_INTERVAL,
		StageConstants::STAGE_4_SPECIAL_ATTACK_HITBOX_RADIUS,
		StageConstants::STAGE_4_FIRST_CLEAR_REWARD
	},
	StageData
	{
		StageConstants::MAX_STAGE_COUNT,
		StageConstants::STAGE_5_ENEMY_NAME,
		StageConstants::STAGE_5_ENEMY_MAX_HP,
		StageConstants::STAGE_5_ENEMY_DAMAGE,
		StageConstants::STAGE_5_SPECIAL_ATTACK_INTERVAL,
		StageConstants::STAGE_5_SPECIAL_ATTACK_HITBOX_RADIUS,
		StageConstants::STAGE_5_FIRST_CLEAR_REWARD
	}
};