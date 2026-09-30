#pragma once

//========= 列挙型=========
// Enemyのゲーム上の状態。
enum class EnemyState
{
	e_IDLE,
	e_DEAD
};

//========= 構造体=========
// Enemyの攻撃更新結果。
// GameSceneがPlayerへのDamage適用、SE再生を担当する。
struct EnemyAttackResult
{
	bool didAttack {};
	bool didHitPlayer {};
	bool isSpecialAttack {};
	float playerDamage {};
};
// EnemyへのDamage適用結果。
// GameSceneがDamage Reward、撃破SE、Stage Clearを担当する。
struct EnemyDamageResult
{
	float actualDamage {};
	bool didDefeatEnemy {};
};

// EnemyのHP、生死、通常攻撃・特殊攻撃の経過時間を管理する。
// EnemyVisualはModel描画・Transform・Animationのみを担当する。
class EnemyController final
{
public:
	//========= 初期化関数=========
	// Stage設定からEnemyのHPと攻撃状態を初期化する。
	void Initialize( float maxHp, float normalAttackInterval, float normalAttackRangeSquared,
					 float normalAttackDamage, float specialAttackInterval, float specialAttackRangeSquared, float specialAttackDamage );

	//========= 更新関数=========
	// Enemy攻撃Timerを更新し、通常攻撃または特殊攻撃の結果を返す。
	// 特殊攻撃を優先し、発動フレームに通常攻撃は実行しない。
	EnemyAttackResult UpdateCombat( float deltaTime, bool isCombatActive, float playerToEnemyDistanceSquared );

	//========= Damage関数=========
	// EnemyへDamageを与え、実ダメージ量と撃破結果を返す。
	EnemyDamageResult TakeDamage( float damage );

	//========= 取得関数=========
	[[nodiscard]] float GetCurrentHp() const { return m_CurrentHp; }
	[[nodiscard]] float GetMaxHp() const { return m_MaxHp; }
	[[nodiscard]] bool IsDead() const { return m_State == EnemyState::e_DEAD; }
	[[nodiscard]] EnemyState GetState() const { return m_State; }
private:
	//========= Enemy状態=========
	EnemyState m_State { EnemyState::e_IDLE };

	//========= HP=========
	float m_CurrentHp {};
	float m_MaxHp {};

	//========= 通常攻撃設定=========
	float m_NormalAttackInterval {};
	float m_NormalAttackRangeSquared {};
	float m_NormalAttackDamage {};

	//========= 特殊攻撃設定=========
	float m_SpecialAttackInterval {};
	float m_SpecialAttackRangeSquared {};
	float m_SpecialAttackDamage {};

	//========= 攻撃Timer=========
	// GameTimerから渡されたDeltaTimeを蓄積する。
	float m_NormalAttackElapsedTime {};
	float m_SpecialAttackElapsedTime {};
};