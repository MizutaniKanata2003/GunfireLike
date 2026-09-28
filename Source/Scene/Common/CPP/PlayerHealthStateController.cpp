#include "../H/PlayerHealthStateController.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

// Stage開始時の低HP状態を初期化する。
void PlayerHealthStateController::Initialize( float currentHp, float maxHp, float lowHealthRatioThreshold )
{
	const PlayerHealthStateResult initialState = Update( currentHp, maxHp, lowHealthRatioThreshold );

	m_WasLowHealth = initialState.isLowHealth;
}

// 現在HPから低HP状態の変化と死亡状態を返す。
PlayerHealthStateResult PlayerHealthStateController::Update( float currentHp, float maxHp, float lowHealthRatioThreshold )
{
	PlayerHealthStateResult result{};

	const float safeMaxHp = std::max( 0.0f, maxHp );

	const float safeCurrentHp = std::max( 0.0f, currentHp );

	const float safeLowHealthRatioThreshold = std::clamp( lowHealthRatioThreshold, 0.0f, 1.0f );

	result.isPlayerDead = safeCurrentHp <= 0.0f;

	result.isLowHealth = safeMaxHp > 0.0f && safeCurrentHp > 0.0f && safeCurrentHp <= safeMaxHp * safeLowHealthRatioThreshold;

	result.didEnterLowHealth = !m_WasLowHealth && result.isLowHealth;

	result.didLeaveLowHealth = m_WasLowHealth && !result.isLowHealth;

	m_WasLowHealth = result.isLowHealth;

	return result;
}