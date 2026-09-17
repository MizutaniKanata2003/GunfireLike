#pragma once

#include <array>

#include "StageData.h"

namespace GameBalance
{
	inline constexpr float INITIAL_MAX_HP = 100.0f;
	inline constexpr float INITIAL_GUN_DAMAGE = 10.0f;
	inline constexpr float INITIAL_SPECIAL_ATTACK_COOLDOWN_SECONDS = 10.0f;

	inline constexpr float DAMAGE_PER_CURRENCY = 10.0f;

	inline constexpr float MAX_HP_UPGRADE_AMOUNT = 25.0f;
	inline constexpr int MAX_HP_UPGRADE_COST = 10;

	inline constexpr float GUN_DAMAGE_UPGRADE_AMOUNT = 5.0f;
	inline constexpr int GUN_DAMAGE_UPGRADE_COST = 15;

	inline constexpr int SPECIAL_ATTACK_UNLOCK_COST = 30;

	inline constexpr float SPECIAL_ATTACK_COOLDOWN_UPGRADE_AMOUNT_SECONDS = 1.0f;
	inline constexpr int SPECIAL_ATTACK_COOLDOWN_UPGRADE_COST = 20;
	inline constexpr float MIN_SPECIAL_ATTACK_COOLDOWN_SECONDS = 1.0f;

	inline constexpr int DEATH_PENALTY_DIVISOR = 4;
}

enum class UpgradeType
{
	e_MAX_HP,
	e_GUN_DAMAGE,
	e_UNLOCK_SPECIAL_ATTACK,
	e_SPECIAL_ATTACK_COOLDOWN
};

struct PlayerStats
{
	float maxHp = GameBalance::INITIAL_MAX_HP;
	float gunDamage = GameBalance::INITIAL_GUN_DAMAGE;
	bool isSpecialAttackUnlocked = false;
	float specialAttackCooldown =
		GameBalance::INITIAL_SPECIAL_ATTACK_COOLDOWN_SECONDS;
};

class GameProgress final
{
public:
	void Initialize()
	{
		m_CurrentStage = StageConstants::FIRST_STAGE_NUMBER;
		m_Currency = 0;
		m_DamageCurrencyRemainder = 0.0f;
		m_TotalDamageDealt = 0.0f;
		m_TotalDeaths = 0;
		m_TotalEnemiesDefeated = 0;
		m_TotalPlayTime = 0.0f;
		m_PlayerStats = {};
		m_StageCleared.fill( false );
	}

	void Update( float deltaTime )
	{
		m_TotalPlayTime += deltaTime;
	}

	void AddCurrency( int amount )
	{
		if ( amount > 0 )
		{
			m_Currency += amount;
		}
	}

	void AddDamageReward( float damage )
	{
		if ( damage <= 0.0f )
		{
			return;
		}

		m_TotalDamageDealt += damage;
		m_DamageCurrencyRemainder += damage;

		while ( m_DamageCurrencyRemainder >=
			GameBalance::DAMAGE_PER_CURRENCY )
		{
			m_DamageCurrencyRemainder -=
				GameBalance::DAMAGE_PER_CURRENCY;
			++m_Currency;
		}
	}

	void AddDeath()
	{
		++m_TotalDeaths;
	}

	[[nodiscard]] int ApplyDeathCurrencyPenalty()
	{
		const int lostCurrency =
			m_Currency / GameBalance::DEATH_PENALTY_DIVISOR;

		m_Currency -= lostCurrency;
		return lostCurrency;
	}

	void AddEnemyDefeat()
	{
		++m_TotalEnemiesDefeated;
	}

	[[nodiscard]] int GetCurrentStage() const
	{
		return m_CurrentStage;
	}

	bool TrySetCurrentStage( int stageNumber )
	{
		if ( !IsStageUnlocked( stageNumber ) )
		{
			return false;
		}

		m_CurrentStage = stageNumber;
		return true;
	}

	[[nodiscard]] const StageData& GetCurrentStageData() const
	{
		return STAGE_DATA[ m_CurrentStage -
			StageConstants::FIRST_STAGE_NUMBER ];
	}

	[[nodiscard]] bool IsStageCleared( int stageNumber ) const
	{
		const int stageIndex = stageNumber -
			StageConstants::FIRST_STAGE_NUMBER;

		if ( stageIndex < 0 ||
			stageIndex >= static_cast<int>( m_StageCleared.size() ) )
		{
			return false;
		}

		return m_StageCleared[ stageIndex ];
	}

	[[nodiscard]] bool IsStageUnlocked( int stageNumber ) const
	{
		if ( stageNumber < StageConstants::FIRST_STAGE_NUMBER ||
			stageNumber > StageConstants::MAX_STAGE_COUNT )
		{
			return false;
		}

		if ( stageNumber == StageConstants::FIRST_STAGE_NUMBER )
		{
			return true;
		}

		const int previousStage = stageNumber -
			StageConstants::FIRST_STAGE_NUMBER - 1;

		return m_StageCleared[ previousStage ];
	}

	[[nodiscard]] bool MarkCurrentStageCleared()
	{
		const int stageIndex = m_CurrentStage -
			StageConstants::FIRST_STAGE_NUMBER;

		if ( m_StageCleared[ stageIndex ] )
		{
			return false;
		}

		m_StageCleared[ stageIndex ] = true;
		return true;
	}

	void AddStageClearReward()
	{
		m_Currency += GetCurrentStageData().clearReward;
	}

	[[nodiscard]] int GetUpgradeCost( UpgradeType upgradeType ) const
	{
		switch ( upgradeType )
		{
			case UpgradeType::e_MAX_HP:
			return GameBalance::MAX_HP_UPGRADE_COST;

			case UpgradeType::e_GUN_DAMAGE:
			return GameBalance::GUN_DAMAGE_UPGRADE_COST;

			case UpgradeType::e_UNLOCK_SPECIAL_ATTACK:
			return GameBalance::SPECIAL_ATTACK_UNLOCK_COST;

			case UpgradeType::e_SPECIAL_ATTACK_COOLDOWN:
			return GameBalance::SPECIAL_ATTACK_COOLDOWN_UPGRADE_COST;

			default:
			return 0;
		}
	}

	bool TryPurchaseUpgrade( UpgradeType upgradeType )
	{
		if ( upgradeType == UpgradeType::e_UNLOCK_SPECIAL_ATTACK &&
			m_PlayerStats.isSpecialAttackUnlocked )
		{
			return false;
		}

		if ( upgradeType == UpgradeType::e_SPECIAL_ATTACK_COOLDOWN &&
			!m_PlayerStats.isSpecialAttackUnlocked )
		{
			return false;
		}

		const int upgradeCost = GetUpgradeCost( upgradeType );

		if ( m_Currency < upgradeCost )
		{
			return false;
		}

		m_Currency -= upgradeCost;

		switch ( upgradeType )
		{
			case UpgradeType::e_MAX_HP:
			m_PlayerStats.maxHp +=
				GameBalance::MAX_HP_UPGRADE_AMOUNT;
			break;

			case UpgradeType::e_GUN_DAMAGE:
			m_PlayerStats.gunDamage +=
				GameBalance::GUN_DAMAGE_UPGRADE_AMOUNT;
			break;

			case UpgradeType::e_UNLOCK_SPECIAL_ATTACK:
			m_PlayerStats.isSpecialAttackUnlocked = true;
			break;

			case UpgradeType::e_SPECIAL_ATTACK_COOLDOWN:
			m_PlayerStats.specialAttackCooldown = std::max(
				GameBalance::MIN_SPECIAL_ATTACK_COOLDOWN_SECONDS,
				m_PlayerStats.specialAttackCooldown -
				GameBalance::SPECIAL_ATTACK_COOLDOWN_UPGRADE_AMOUNT_SECONDS );
			break;

			default:
			return false;
		}

		return true;
	}

	[[nodiscard]] int GetCurrency() const
	{
		return m_Currency;
	}

	[[nodiscard]] float GetTotalDamageDealt() const
	{
		return m_TotalDamageDealt;
	}

	[[nodiscard]] int GetTotalDeaths() const
	{
		return m_TotalDeaths;
	}

	[[nodiscard]] int GetTotalEnemiesDefeated() const
	{
		return m_TotalEnemiesDefeated;
	}

	[[nodiscard]] float GetTotalPlayTime() const
	{
		return m_TotalPlayTime;
	}

	[[nodiscard]] const PlayerStats& GetPlayerStats() const
	{
		return m_PlayerStats;
	}

private:
	int m_CurrentStage = StageConstants::FIRST_STAGE_NUMBER;
	int m_Currency = 0;

	float m_DamageCurrencyRemainder = 0.0f;
	float m_TotalDamageDealt = 0.0f;
	int m_TotalDeaths = 0;
	int m_TotalEnemiesDefeated = 0;
	float m_TotalPlayTime = 0.0f;

	std::array<bool, StageConstants::MAX_STAGE_COUNT>
		m_StageCleared{};

	PlayerStats m_PlayerStats{};
};