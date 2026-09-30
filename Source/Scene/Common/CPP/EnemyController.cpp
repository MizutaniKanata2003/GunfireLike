#include "../H/EnemyController.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

// Stage設定からEnemyのHPと攻撃状態を初期化する。
void EnemyController::Initialize( float maxHp, float normalAttackInterval, float normalAttackRangeSquared, float normalAttackDamage,
								  float specialAttackInterval, float specialAttackRangeSquared, float specialAttackDamage )
{
	m_State = EnemyState::e_IDLE;
	m_MaxHp = std::max( 0.0f, maxHp );
	m_CurrentHp = m_MaxHp;
	m_NormalAttackInterval = std::max( 0.0f, normalAttackInterval );
	m_NormalAttackRangeSquared = std::max( 0.0f, normalAttackRangeSquared );
	m_NormalAttackDamage = std::max( 0.0f, normalAttackDamage );
	m_SpecialAttackInterval = std::max( 0.0f, specialAttackInterval );
	m_SpecialAttackRangeSquared = std::max( 0.0f, specialAttackRangeSquared );
	m_SpecialAttackDamage = std::max( 0.0f, specialAttackDamage );

	m_NormalAttackElapsedTime = {};
	m_SpecialAttackElapsedTime = {};
}

// Enemy攻撃Timerを更新し、通常攻撃または特殊攻撃の結果を返す。
// 特殊攻撃を優先し、発動フレームに通常攻撃は実行しない。
EnemyAttackResult EnemyController::UpdateCombat( float deltaTime, bool isCombatActive, float playerToEnemyDistanceSquared )
{
	EnemyAttackResult result {};

	if ( m_State == EnemyState::e_DEAD || !isCombatActive )return result;

	const float safeDeltaTime = std::max( 0.0f, deltaTime );

	m_NormalAttackElapsedTime += safeDeltaTime;
	m_SpecialAttackElapsedTime += safeDeltaTime;

	// 現行GameSceneと同じく特殊攻撃を先に判定する。
	const bool isSpecialAttackReady = m_SpecialAttackInterval > 0.0f && m_SpecialAttackElapsedTime >= m_SpecialAttackInterval;

	if ( isSpecialAttackReady )
	{
		m_SpecialAttackElapsedTime = 0.0f;

		result.didAttack = true;
		result.isSpecialAttack = true;
		result.didHitPlayer = playerToEnemyDistanceSquared <= m_SpecialAttackRangeSquared;

		if ( result.didHitPlayer )result.playerDamage = m_SpecialAttackDamage;

		return result;
	}

	const bool isNormalAttackReady = m_NormalAttackInterval > 0.0f && m_NormalAttackElapsedTime >= m_NormalAttackInterval;

	if ( !isNormalAttackReady )return result;

	m_NormalAttackElapsedTime = 0.0f;

	result.didAttack = true;
	result.isSpecialAttack = false;
	result.didHitPlayer = playerToEnemyDistanceSquared <= m_NormalAttackRangeSquared;

	if ( result.didHitPlayer )result.playerDamage = m_NormalAttackDamage;

	return result;
}

// EnemyへDamageを与え、実ダメージ量と撃破結果を返す。
EnemyDamageResult EnemyController::TakeDamage( float damage )
{
	EnemyDamageResult result {};

	if ( m_State == EnemyState::e_DEAD || damage <= 0.0f )return result;

	const float previousHp = m_CurrentHp;

	m_CurrentHp = std::max( 0.0f, m_CurrentHp - damage );

	result.actualDamage = previousHp - m_CurrentHp;

	if ( m_CurrentHp > 0.0f )return result;

	m_State = EnemyState::e_DEAD;
	result.didDefeatEnemy = true;

	return result;
}