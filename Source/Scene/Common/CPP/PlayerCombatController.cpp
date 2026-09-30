#include "../H/PlayerCombatController.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

// 特殊攻撃のCooldownを更新する。
void PlayerCombatController::Update( float deltaTime )
{
	const float safeDeltaTime = std::max( 0.0f, deltaTime );

	m_SpecialAttackCooldownRemainingTime = std::max( 0.0f, m_SpecialAttackCooldownRemainingTime - safeDeltaTime );
}

// 左クリック入力から通常射撃要求を返す。
PlayerAttackRequest PlayerCombatController::RequestNormalShot( bool isShootTriggered, bool isCombatActive, bool isEnemyDead ) const
{
	PlayerAttackRequest request {};

	if ( !isShootTriggered || !isCombatActive || isEnemyDead )return request;

	request.attackType = PlayerAttackType::e_NORMAL_SHOT;

	return request;
}

// Qキー入力、解放状態、Cooldownから特殊攻撃要求を返す。
// 特殊攻撃要求を返した場合はCooldownを開始する。
PlayerAttackRequest PlayerCombatController::RequestSpecialAttack( bool isSpecialAttackTriggered, bool isCombatActive, bool isEnemyDead,
																  bool isSpecialAttackUnlocked, float specialAttackCooldown )
{
	PlayerAttackRequest request {};

	if ( !isSpecialAttackTriggered || !isCombatActive || isEnemyDead || !isSpecialAttackUnlocked || !IsSpecialAttackReady() )
	{
		return request;
	}

	m_SpecialAttackCooldownRemainingTime = std::max( 0.0f, specialAttackCooldown );

	request.attackType = PlayerAttackType::e_SPECIAL_ATTACK;

	return request;
}