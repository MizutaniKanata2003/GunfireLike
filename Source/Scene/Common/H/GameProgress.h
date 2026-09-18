#pragma once

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <array>

//========= Scene インクルード=========
#include "StageData.h"

//========= ゲームバランス定数=========
namespace GameBalance
{
	// Playerの初期能力。
	inline constexpr float INITIAL_MAX_HP = 100.0f;
	inline constexpr float INITIAL_GUN_DAMAGE = 10.0f;
	inline constexpr float INITIAL_SPECIAL_ATTACK_COOLDOWN_SECONDS = 10.0f;

	// 敵への与ダメージから通貨へ変換する割合。
	inline constexpr float DAMAGE_PER_CURRENCY = 10.0f;

	// 最大HP強化の上昇量と必要通貨。
	inline constexpr float MAX_HP_UPGRADE_AMOUNT = 25.0f;
	inline constexpr int MAX_HP_UPGRADE_COST = 10;

	// 銃ダメージ強化の上昇量と必要通貨。
	inline constexpr float GUN_DAMAGE_UPGRADE_AMOUNT = 5.0f;
	inline constexpr int GUN_DAMAGE_UPGRADE_COST = 15;

	// 特殊攻撃の解放に必要な通貨。
	inline constexpr int SPECIAL_ATTACK_UNLOCK_COST = 30;

	// 特殊攻撃クールダウン強化の上昇量、必要通貨、下限秒数。
	inline constexpr float SPECIAL_ATTACK_COOLDOWN_UPGRADE_AMOUNT_SECONDS = 1.0f;
	inline constexpr int SPECIAL_ATTACK_COOLDOWN_UPGRADE_COST = 20;
	inline constexpr float MIN_SPECIAL_ATTACK_COOLDOWN_SECONDS = 1.0f;

	// 死亡時に失う通貨を計算する除数。
	inline constexpr int DEATH_PENALTY_DIVISOR = 4;
}

//========= 列挙型=========
enum class UpgradeType
{
	e_MAX_HP,
	e_GUN_DAMAGE,
	e_UNLOCK_SPECIAL_ATTACK,
	e_SPECIAL_ATTACK_COOLDOWN
};

//========= 構造体=========
// Playerの恒久強化後の能力値を保持する。
struct PlayerStats
{
	// Playerの最大HPと銃ダメージ。
	float maxHp{ GameBalance::INITIAL_MAX_HP };
	float gunDamage{ GameBalance::INITIAL_GUN_DAMAGE };

	// 特殊攻撃の解放状態とクールダウン秒数。
	bool isSpecialAttackUnlocked{};
	float specialAttackCooldown{ GameBalance::INITIAL_SPECIAL_ATTACK_COOLDOWN_SECONDS };
};

// Sceneをまたいで維持するStage進行、通貨、強化、累計記録を管理する。
class GameProgress final
{
public:
	//========= 初期化・更新関数=========
	// 新しいゲーム開始時のStage進行、通貨、強化、累計記録を初期化する。
	void Initialize()
	{
		m_CurrentStage = StageConstants::FIRST_STAGE_NUMBER;
		m_Currency = {};
		m_DamageCurrencyRemainder = {};
		m_TotalDamageDealt = {};
		m_TotalDeaths = {};
		m_TotalEnemiesDefeated = {};
		m_TotalPlayTime = {};
		m_PlayerStats = {};
		m_StageCleared.fill( false );
	}

	// 経過時間を累計プレイ時間へ加算する。
	void Update( float deltaTime )
	{
		m_TotalPlayTime += deltaTime;
	}

	//========= 通貨・累計記録操作関数=========
	// 指定した通貨を加算する。
	void AddCurrency( int amount )
	{
		if ( amount > 0 ) m_Currency += amount;
	}

	// 与えたダメージを累計し、一定量ごとに通貨を加算する。
	void AddDamageReward( float damage )
	{
		if ( damage <= 0.0f ) return;

		m_TotalDamageDealt += damage;
		m_DamageCurrencyRemainder += damage;

		while ( m_DamageCurrencyRemainder >= GameBalance::DAMAGE_PER_CURRENCY )
		{
			m_DamageCurrencyRemainder -= GameBalance::DAMAGE_PER_CURRENCY;
			++m_Currency;
		}
	}

	// 死亡回数を1増やす。
	void AddDeath()
	{
		++m_TotalDeaths;
	}

	// 所持通貨の死亡ペナルティを適用し、失った通貨を返す。
	[[nodiscard]] int ApplyDeathCurrencyPenalty()
	{
		const int lostCurrency = m_Currency / GameBalance::DEATH_PENALTY_DIVISOR;

		m_Currency -= lostCurrency;

		return lostCurrency;
	}

	// 敵撃破数を1増やす。
	void AddEnemyDefeat()
	{
		++m_TotalEnemiesDefeated;
	}

	//========= Stage進行操作関数=========
	// 指定したStageが解放済みの場合に現在Stageへ設定する。
	bool TrySetCurrentStage( int stageNumber )
	{
		if ( !IsStageUnlocked( stageNumber ) ) return false;

		m_CurrentStage = stageNumber;

		return true;
	}

	// 現在Stageを初回クリア済みに設定し、初回設定に成功したかを返す。
	[[nodiscard]] bool MarkCurrentStageCleared()
	{
		const int stageIndex = m_CurrentStage - StageConstants::FIRST_STAGE_NUMBER;

		if ( m_StageCleared[ stageIndex ] ) return false;

		m_StageCleared[ stageIndex ] = true;

		return true;
	}

	// 現在Stageの初回クリア報酬を通貨へ加算する。
	void AddStageClearReward()
	{
		m_Currency += GetCurrentStageData().clearReward;
	}

	//========= 強化操作関数=========
	// 指定した強化に必要な通貨を返す。
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
			return {};
		}
	}

	// 通貨と解放条件を満たす場合に指定した恒久強化を購入する。
	bool TryPurchaseUpgrade( UpgradeType upgradeType )
	{
		if ( upgradeType == UpgradeType::e_UNLOCK_SPECIAL_ATTACK &&
			m_PlayerStats.isSpecialAttackUnlocked ) return false;

		if ( upgradeType == UpgradeType::e_SPECIAL_ATTACK_COOLDOWN &&
			!m_PlayerStats.isSpecialAttackUnlocked ) return false;

		const int upgradeCost = GetUpgradeCost( upgradeType );
		if ( m_Currency < upgradeCost ) return false;

		m_Currency -= upgradeCost;

		switch ( upgradeType )
		{
			case UpgradeType::e_MAX_HP:
			m_PlayerStats.maxHp += GameBalance::MAX_HP_UPGRADE_AMOUNT;
			break;

			case UpgradeType::e_GUN_DAMAGE:
			m_PlayerStats.gunDamage += GameBalance::GUN_DAMAGE_UPGRADE_AMOUNT;
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

	//========= Getter関数=========
	// 現在選択しているStage番号を返す。
	[[nodiscard]] int GetCurrentStage() const
	{
		return m_CurrentStage;
	}

	// 現在選択しているStageの設定を返す。
	[[nodiscard]] const StageData& GetCurrentStageData() const
	{
		return STAGE_DATA[ m_CurrentStage - StageConstants::FIRST_STAGE_NUMBER ];
	}

	// 指定したStageが初回クリア済みかを返す。
	[[nodiscard]] bool IsStageCleared( int stageNumber ) const
	{
		const int stageIndex = stageNumber - StageConstants::FIRST_STAGE_NUMBER;

		if ( stageIndex < 0 ||
			stageIndex >= static_cast<int>( m_StageCleared.size() ) ) return false;

		return m_StageCleared[ stageIndex ];
	}

	// 指定したStageが移動可能な状態かを返す。
	[[nodiscard]] bool IsStageUnlocked( int stageNumber ) const
	{
		if ( stageNumber < StageConstants::FIRST_STAGE_NUMBER ||
			stageNumber > StageConstants::MAX_STAGE_COUNT ) return false;

		if ( stageNumber == StageConstants::FIRST_STAGE_NUMBER ) return true;

		const int previousStage = stageNumber - StageConstants::FIRST_STAGE_NUMBER - 1;

		return m_StageCleared[ previousStage ];
	}

	// 現在所持している通貨を返す。
	[[nodiscard]] int GetCurrency() const
	{
		return m_Currency;
	}

	// 累計で敵へ与えたダメージを返す。
	[[nodiscard]] float GetTotalDamageDealt() const
	{
		return m_TotalDamageDealt;
	}

	// 累計死亡回数を返す。
	[[nodiscard]] int GetTotalDeaths() const
	{
		return m_TotalDeaths;
	}

	// 累計敵撃破数を返す。
	[[nodiscard]] int GetTotalEnemiesDefeated() const
	{
		return m_TotalEnemiesDefeated;
	}

	// 累計プレイ時間を秒で返す。
	[[nodiscard]] float GetTotalPlayTime() const
	{
		return m_TotalPlayTime;
	}

	// 現在のPlayer恒久強化後能力を返す。
	[[nodiscard]] const PlayerStats& GetPlayerStats() const
	{
		return m_PlayerStats;
	}

private:
	//========= Stage進行状態=========
	// 現在選択しているStage番号。
	int m_CurrentStage{ StageConstants::FIRST_STAGE_NUMBER };
	// 各Stageの初回クリア状態。
	std::array<bool, StageConstants::MAX_STAGE_COUNT> m_StageCleared{};

	//========= 通貨・累計記録=========
	// 現在所持している通貨。
	int m_Currency{};
	// 通貨に変換しきれていない与ダメージの端数。
	float m_DamageCurrencyRemainder{};
	// 累計で敵へ与えたダメージ。
	float m_TotalDamageDealt{};
	// 累計死亡回数。
	int m_TotalDeaths{};
	// 累計敵撃破数。
	int m_TotalEnemiesDefeated{};
	// 累計プレイ時間を秒で保持する。
	float m_TotalPlayTime{};

	//========= Player恒久強化状態=========
	// Playerの恒久強化後能力。
	PlayerStats m_PlayerStats{};
};