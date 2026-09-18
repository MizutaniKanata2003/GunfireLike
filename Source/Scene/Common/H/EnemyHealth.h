#pragma once

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

//========= HP定数=========
namespace EnemyHealthConstants
{
	// HPとダメージの下限値。
	inline constexpr float MIN_MAX_HP = 1.0f;
	inline constexpr float MIN_DAMAGE = 0.0f;
	inline constexpr float DEAD_HP = 0.0f;

	// 敵の初期最大HP。
	inline constexpr float DEFAULT_MAX_HP = 100.0f;
}

// 敵の現在HPと最大HPを管理する。
class EnemyHealth final
{
public:
	//========= 初期化・リセット関数=========
	// 指定した最大HPで現在HPと最大HPを初期化する。
	void Initialize( float maxHp )
	{
		m_MaxHp = std::max( EnemyHealthConstants::MIN_MAX_HP, maxHp );
		m_CurrentHp = m_MaxHp;
	}

	// 指定した最大HPで現在HPを最大まで回復する。
	void ResetToMaxHp( float maxHp )
	{
		m_MaxHp = std::max( EnemyHealthConstants::MIN_MAX_HP, maxHp );
		m_CurrentHp = m_MaxHp;
	}

	//========= HP操作関数=========
	// 指定ダメージを受け、実際に減少したHP量を返す。
	[[nodiscard]] float TakeDamage( float damage )
	{
		if ( damage <= EnemyHealthConstants::MIN_DAMAGE || IsDead() ) return EnemyHealthConstants::DEAD_HP;

		const float actualDamage = std::min( damage, m_CurrentHp );
		m_CurrentHp -= actualDamage;

		return actualDamage;
	}

	//========= Getter関数=========
	// 現在HPが0以下かを返す。
	[[nodiscard]] bool IsDead() const
	{
		return m_CurrentHp <= EnemyHealthConstants::DEAD_HP;
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
	float m_CurrentHp{ EnemyHealthConstants::DEFAULT_MAX_HP };
	float m_MaxHp{ EnemyHealthConstants::DEFAULT_MAX_HP };
};