#pragma once

#include <algorithm>

namespace PlayerHealthConstants
{
	inline constexpr float MIN_MAX_HP = 1.0f;
	inline constexpr float MIN_DAMAGE = 0.0f;
	inline constexpr float DEAD_HP = 0.0f;
	inline constexpr float DEFAULT_MAX_HP = 100.0f;
}

class PlayerHealth final
{
public:
	void Initialize( float maxHp )
	{
		m_MaxHp = std::max( PlayerHealthConstants::MIN_MAX_HP, maxHp );
		m_CurrentHp = m_MaxHp;
	}

	void ResetToMaxHp( float maxHp )
	{
		m_MaxHp = std::max( PlayerHealthConstants::MIN_MAX_HP, maxHp );
		m_CurrentHp = m_MaxHp;
	}

	void TakeDamage( float damage )
	{
		if ( damage <= PlayerHealthConstants::MIN_DAMAGE || IsDead() )
		{
			return;
		}

		m_CurrentHp = std::max(
			PlayerHealthConstants::DEAD_HP,
			m_CurrentHp - damage );
	}

	[[nodiscard]] bool IsDead() const
	{
		return m_CurrentHp <= PlayerHealthConstants::DEAD_HP;
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
	float m_CurrentHp = PlayerHealthConstants::DEFAULT_MAX_HP;
	float m_MaxHp = PlayerHealthConstants::DEFAULT_MAX_HP;
};