#pragma once

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

//========= HP定数=========
namespace PlayerHealthConstants
{
	// HPとダメージの下限値。
	inline constexpr float MIN_MAX_HP = 1.0f;
	inline constexpr float MIN_DAMAGE = 0.0f;
	inline constexpr float DEAD_HP = 0.0f;

	// Playerの初期最大HP。
	inline constexpr float DEFAULT_MAX_HP = 100.0f;
}

// Playerの現在HPと最大HPを管理する。
class PlayerHealth final
{
public:
	//========= 初期化・リセット関数=========
	// 指定した最大HPで現在HPと最大HPを初期化する。
	void Initialize( float maxHp )
	{
		m_MaxHp = std::max( PlayerHealthConstants::MIN_MAX_HP, maxHp );
		m_CurrentHp = m_MaxHp;
	}

	// 指定した最大HPで現在HPを最大まで回復する。
	void ResetToMaxHp( float maxHp )
	{
		m_MaxHp = std::max( PlayerHealthConstants::MIN_MAX_HP, maxHp );
		m_CurrentHp = m_MaxHp;
	}

	//========= HP操作関数=========
	// 指定したダメージを受け、現在HPを0未満にしない。
	void TakeDamage( float damage )
	{
		if ( damage <= PlayerHealthConstants::MIN_DAMAGE || IsDead() ) return;

		m_CurrentHp = std::max( PlayerHealthConstants::DEAD_HP, m_CurrentHp - damage );
	}

	//========= Getter関数=========
	// 現在HPが0以下かを返す。
	[[nodiscard]] bool IsDead() const
	{
		return m_CurrentHp <= PlayerHealthConstants::DEAD_HP;
	}

	// 現在HPを返す。
	[[nodiscard]] float GetCurrentHp() const
	{
		return m_CurrentHp;
	}

	// 最大HPを返す。
	[[nodiscard]] float GetMaxHp() const
	{
		return m_MaxHp;
	}

private:
	//========= HP状態=========
	// 現在HPと最大HPを保持する。
	float m_CurrentHp{ PlayerHealthConstants::DEFAULT_MAX_HP };
	float m_MaxHp{ PlayerHealthConstants::DEFAULT_MAX_HP };
};