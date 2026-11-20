#pragma once

// プレイヤーの戦闘面（蹴り・殴打の当たりによる敵へのダメージと転倒、敵との接触の反応、スライディングによる転倒、回復アイテムの使用、被弾の反応）を担当する。
// 一撃で敵を倒すデバッグ設定と、プレイヤーの攻撃が敵へ与えるダメージの決定も担当する。特殊強化（殴打追撃）の効果として、殴打を当てた敵へ、射撃のダメージの印を付ける。
// 位置・動作の状態の更新、射撃・リロード、敵の行動、ヒットストップとスローの時間管理は担当しない。ヒットストップは呼出側へ通知するだけで、残り時間は持たない。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tで、スライディングで転倒させた敵のID別の記録を保持する。
#include <functional> // std::functionで、ヒットストップの通知先（呼出側の関数）を保持する。

//========= 前方宣言=========
class CollisionWorld;
class Enemy;
class EnemyManager;
class FpsCamera;
class HealItems;
class InputSystem;
class PlayerController;
class PlayerHealth;

// PlayerCombatControllerが借用する参照先と、ヒットストップの通知先をまとめる。参照先は、Uninitまで有効であること。
struct PLAYER_COMBAT_LINKS
{
	// 近接攻撃の当たりの取得、動作の状態・無敵・体の球の読み取り、怯みの設定に使うプレイヤー。
	PlayerController* playerController{};
	// 被弾のダメージと、回復アイテムによる回復の反映先となるプレイヤーのHP。
	PlayerHealth* playerHealth{};
	// 回復アイテムの所持数と使用の間隔。
	HealItems* healItems{};
	// 近接攻撃の対象検索、接触の判定、ダメージと転倒の反映先となる敵の所有者。
	EnemyManager* enemyManager{};
	// 敵が死亡する際にColliderを解除する登録先のCollisionWorld。
	CollisionWorld* collisionWorld{};
	// 近接攻撃の対象検索に使う視点と視線の取得元。読み取りだけを行う。
	const FpsCamera* camera{};
	// 回復の入力（Q）の取得元。読み取りだけを行う。
	const InputSystem* inputSystem{};
	// ヒットストップの通知先。引数は求めるヒットストップの長さ（単位は秒）。更新の途中で呼ばれる。空のままではInitializeが失敗する。
	std::function<void(float)> onHitStopRequested{};
};

// プレイヤーの攻撃・接触・回復・被弾の反応を扱う。
class PlayerCombatController final
{
public:
	//========= 初期化・終了関数=========
	// 参照先と通知先を登録する。
	// links: 借用する参照先と通知先。必須の項目が欠けている場合は何も登録しない。
	// 戻り値: 全ての項目が揃っていて登録できたらtrue。
	[[nodiscard]] bool Initialize(const PLAYER_COMBAT_LINKS& links);
	// 参照先と通知先を手放す。未初期化でも、繰り返しても呼出可能。
	void Uninit();
	// 接触の反応の間隔・スライディングの転倒記録・一撃で倒す設定・殴打追撃の効果を、初期状態へ戻す。参照先は変えない。
	void Reset();

	//========= 更新関数=========
	// 近接攻撃の当たり・敵との接触・回復の入力を、この順に更新する。Initialize成功後に呼ぶ。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。接触の反応の間隔を進める。
	void Update(float gameplayDeltaTime);
	// プレイヤーへ被弾の反応（ダメージ・怯み・ヒットストップの通知）を与える。Initialize成功後に呼ぶ。
	// damage: 与えるダメージ。接触の場合は0。
	// hitStopSeconds: 求めるヒットストップの長さ。単位は秒。
	void ApplyHitReaction(float damage,float hitStopSeconds);

	//========= ダメージ・デバッグ設定関数=========
	// プレイヤーの攻撃が敵へ与えるダメージを返す。一撃で倒す設定が有効なら、その敵の残りHPを返し、無効なら基本のダメージを返す。
	// baseDamage: 設定が無効の場合に使う、攻撃の基本のダメージ。
	// target: ダメージを受ける敵。
	[[nodiscard]] float GetAttackDamage(float baseDamage,const Enemy& target) const;
	// 一撃で倒す設定を切り替える。デバッグ用で、F9の操作から呼ばれる。
	void ToggleOneHitKill() { m_isOneHitKillEnabled = !m_isOneHitKillEnabled; }
	// 一撃で倒す設定が有効かを返す。
	[[nodiscard]] bool IsOneHitKillEnabled() const { return m_isOneHitKillEnabled; }
	// スライディングで転倒させた敵の記録を消す。新しいウェーブや再開で、旧ウェーブの記録を持ち越さないために使う。
	void ClearSlideKnockdownRecord() { m_slideKnockedDownEnemyMask = 0; }

	//========= 特殊強化の効果=========
	// 殴打追撃の効果を設定する。殴打が敵に当たった時、その敵へ、射撃で受けるダメージの印を付ける。ResetやUninitでは引き継がないため、呼出側が毎回の更新で渡す。
	// damageScale: 印が付いた敵が、射撃で受けるダメージの倍率。1.0以下なら、殴打追撃が未取得として、印を付けない。
	// durationSeconds: 印が続く時間。単位は秒。
	void SetFollowUpStrike(float damageScale,float durationSeconds)
	{
		m_followUpDamageScale = damageScale;
		m_followUpSeconds = durationSeconds;
	}

private:
	//========= 更新の補助関数=========
	// 近接攻撃の当たりが発生していれば、射程と範囲の内の敵へ、蹴り・殴打の効果を与える。
	void UpdateMelee();
	// 敵との体の接触を調べ、スライディング中は敵を転倒させ、それ以外では条件を満たす場合に接触の反応を与える。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。接触の反応の間隔を進める。
	void UpdateContact(float gameplayDeltaTime);
	// Qの入力で、回復アイテムを使ってHPを回復する。HPが満タンの場合は消費しない。他の動作は中断しない。
	void UpdateHeal();
	// ヒットストップを通知先へ求める。通知先が空の場合は何もしない。
	// hitStopSeconds: 求めるヒットストップの長さ。単位は秒。
	void RequestHitStop(float hitStopSeconds) const;

	//========= 参照先=========
	// 近接攻撃の当たり・動作の状態・怯みの対象。非所有。未登録はnullptr。
	PlayerController* m_playerController{};
	// 被弾と回復の反映先となるHP。非所有。未登録はnullptr。
	PlayerHealth* m_playerHealth{};
	// 回復アイテムの所持状態。非所有。未登録はnullptr。
	HealItems* m_healItems{};
	// 敵の検索・ダメージ・転倒の対象。非所有。未登録はnullptr。
	EnemyManager* m_enemyManager{};
	// 敵の死亡時にColliderを解除する登録先。非所有。未登録はnullptr。
	CollisionWorld* m_collisionWorld{};
	// 視点と視線の取得元。非所有。未登録はnullptr。
	const FpsCamera* m_camera{};
	// 回復の入力の取得元。非所有。未登録はnullptr。
	const InputSystem* m_inputSystem{};
	// ヒットストップの通知先。未登録の間は空。
	std::function<void(float)> m_onHitStopRequested{};

	//========= 戦闘の状態=========
	// 敵への接触の反応が、次に起きるまでの残り時間。単位は秒。0なら起きる。
	float m_contactCooldownRemainingSeconds{};
	// 現在のスライディングで転倒させた敵の記録。ビット番号が敵IDに対応し、同じスライディングで同じ敵を繰り返し転倒させない。
	std::uint32_t m_slideKnockedDownEnemyMask{};
	// 一撃で敵を倒す設定が有効か。有効の間、プレイヤーの射撃・蹴り・殴打が、敵の残りHPと同じダメージを与える。
	bool m_isOneHitKillEnabled{};

	//========= 特殊強化の効果=========
	// 殴打追撃で、殴打を当てた敵へ付ける、射撃で受けるダメージの倍率。1.0以下なら、印を付けない。
	float m_followUpDamageScale{1.0f};
	// 殴打追撃の印が続く時間。単位は秒。
	float m_followUpSeconds{};
};