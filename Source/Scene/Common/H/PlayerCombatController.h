#pragma once

// Player攻撃要求の種別。
enum class PlayerAttackType
{
	e_NONE,
	e_NORMAL_SHOT,
	e_SPECIAL_ATTACK
};

// Player攻撃要求。
// GameSceneがSE、Projectile、HitScan、Damage適用を担当する。
struct PlayerAttackRequest
{
	PlayerAttackType attackType{
	PlayerAttackType::e_NONE };
};

// 通常射撃と特殊攻撃の入力・解放条件・Cooldownを管理する。
// Audio、Projectile、Enemy、GameProgress、SceneManagerは所有しない。
class PlayerCombatController final
{
public:
	//========= 初期化関数=========
	// Stage開始時の特殊攻撃Cooldownを初期化する。
	void Initialize() { m_SpecialAttackCooldownRemainingTime = 0.0f; }

	//========= 更新関数=========
	// 特殊攻撃のCooldownを更新する。
	void Update( float deltaTime );

	//========= 攻撃要求関数=========
	// 左クリック入力から通常射撃要求を返す。
	[[nodiscard]] PlayerAttackRequest RequestNormalShot( bool isShootTriggered, bool isCombatActive, bool isEnemyDead ) const;
	// Qキー入力、解放状態、Cooldownから特殊攻撃要求を返す。
	// 特殊攻撃要求を返した場合はCooldownを開始する。
	[[nodiscard]] PlayerAttackRequest RequestSpecialAttack( bool isSpecialAttackTriggered, bool isCombatActive,
	bool isEnemyDead, bool isSpecialAttackUnlocked, float specialAttackCooldown );

	//========= 取得関数=========
	// 特殊攻撃が現在使用可能か返す。
	[[nodiscard]] bool IsSpecialAttackReady() const { return m_SpecialAttackCooldownRemainingTime <= 0.0f; }
	// 特殊攻撃の残りCooldown時間を返す。
	[[nodiscard]] float GetSpecialAttackCooldownRemainingTime() const { return m_SpecialAttackCooldownRemainingTime; }
private:
	//========= Cooldown状態=========
	// 特殊攻撃が再使用可能になるまでの残り時間。
	float m_SpecialAttackCooldownRemainingTime{};
};