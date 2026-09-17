#pragma once

#include <algorithm>

namespace EnemyHealthConstants
{
	inline constexpr float MIN_MAX_HP = 1.0f;
	inline constexpr float MIN_DAMAGE = 0.0f;
	inline constexpr float DEAD_HP = 0.0f;
	inline constexpr float DEFAULT_MAX_HP = 100.0f;
}

class EnemyHealth final
{
public:
	void Initialize( float maxHp )
	{
		m_MaxHp = std::max( EnemyHealthConstants::MIN_MAX_HP, maxHp );
		m_CurrentHp = m_MaxHp;
	}

	void ResetToMaxHp( float maxHp )
	{
		m_MaxHp = std::max( EnemyHealthConstants::MIN_MAX_HP, maxHp );
		m_CurrentHp = m_MaxHp;
	}

	[[nodiscard]] float TakeDamage( float damage )
	{
		if ( damage <= EnemyHealthConstants::MIN_DAMAGE || IsDead() )
		{
			return EnemyHealthConstants::DEAD_HP;
		}

		const float actualDamage = std::min( damage, m_CurrentHp );
		m_CurrentHp -= actualDamage;

		return actualDamage;
	}

	[[nodiscard]] bool IsDead() const
	{
		return m_CurrentHp <= EnemyHealthConstants::DEAD_HP;
	}

	[[nodiscard]] float GetCurrentHp() const
	{
		return m_CurrentHp;
	}

	[[nodiscard]] float GetMaxHp() const
	{
		return m_MaxHp;
	}

private:
	float m_CurrentHp = EnemyHealthConstants::DEFAULT_MAX_HP;
	float m_MaxHp = EnemyHealthConstants::DEFAULT_MAX_HP;
};