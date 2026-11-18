#pragma once

// プレイヤーのHPを保持し、ダメージ・回復と、HPが少ない状態の判定を提供する。
// 被弾の判定・演出・死亡後の進行は担当しない。HPは0以上で最大HP以下に保つ。

// プレイヤーのHPの状態。
class PlayerHealth final
{
public:
	//========= 初期化関数=========
	// 最大HPを設定し、HPを満タンにする。
	// maxHP: 最大HP。正の値を指定する。0以下では、HPが常に0として扱われる。
	void Initialize(float maxHP)
	{
		m_maxHP = maxHP;
		m_currentHP = maxHP;
	}

	//========= 更新関数=========
	// HPを減らす。0未満にはならない。
	// damage: 減らす量。正の値。負の値では何もしない。
	void ApplyDamage(float damage)
	{
		// 負のダメージはHPを増やしてしまうため、回復はHealに任せて、ここでは何もしない。
		if(damage < 0.0f)return;

		// ダメージ分を引く。
		m_currentHP -= damage;
		// 引いた結果が負なら、HPが0未満にならないよう0へ丸める。
		if(m_currentHP < 0.0f){
			m_currentHP = 0.0f;
		}
	}
	// HPを増やす。最大HPを超えない。
	// amount: 増やす量。正の値。負の値では何もしない。
	void Heal(float amount)
	{
		// 負の回復はHPを減らしてしまうため、ダメージはApplyDamageに任せて、ここでは何もしない。
		if(amount < 0.0f)return;

		// 回復分を足す。
		m_currentHP += amount;
		// 足した結果が最大HPを超えたら、最大HPへ丸める。
		if(m_currentHP > m_maxHP){
			m_currentHP = m_maxHP;
		}
	}
	// HPを満タンへ戻す。
	void HealFull() { m_currentHP = m_maxHP; }

	//========= Getter関数=========
	// 現在のHPを返す。
	[[nodiscard]] float GetCurrentHP() const { return m_currentHP; }
	// 最大HPを返す。
	[[nodiscard]] float GetMaxHP() const { return m_maxHP; }
	// HPが0かを返す。
	[[nodiscard]] bool IsDead() const { return m_currentHP <= 0.0f; }
	// HPが最大かを返す。
	[[nodiscard]] bool IsFull() const { return m_currentHP >= m_maxHP; }

private:
	//========= HPの状態=========
	// 最大HP。
	float m_maxHP{};
	// 現在のHP。0以上で最大HP以下。
	float m_currentHP{};
};