#pragma once

// 射撃の更新（弾薬・連射・リロードの進行と、命中判定の結果の反映）と、命中表示を担当する。
// 武器1丁の状態はWeaponSystemが持ち、このクラスが所有する。弾道の終点をCollisionの可視化へ伝え、敵レイヤーへ命中した場合に、敵へダメージと怯みを与える。
// 武器の装備（種類の切替）と、購入した予備弾薬の加算を受け付ける。購入・強化・所持数の管理は担当しない。暫定の切替の入力（数字キー）だけを持つ。
// 特殊強化の効果（敵へ与える怯み時間の倍率、ジャスト回避の後の射撃ダメージの増加）は、呼出側から受け取る。強化の取得数は管理しない。
// プレイヤーの動作の更新・視線の更新・近接攻撃・敵の行動・ヒットストップの管理は担当しない。

//========= Scene インクルード=========
#include "Scene/Common/Weapon/H/WeaponSystem.h"

//========= 前方宣言=========
class CollisionDebugView;
class CollisionWorld;
class EnemyManager;
class FpsCamera;
class InputSystem;
class PlayerCombatController;
class PlayerController;

// ShotControllerが借用する参照先をまとめる。参照先は、Uninitまで有効であること。
struct SHOT_CONTROLLER_LINKS
{
	// 構え・射撃の可否の読み取り元となるプレイヤー。読み取りだけを行う。
	const PlayerController* playerController{};
	// 発射の始点と方向の取得元で、発射時に反動を加える先となるカメラ。視線と位置を、先に更新しておくこと。
	FpsCamera* camera{};
	// 左クリック（射撃）、Rキー（リロード）、数字キー（暫定の武器切替）の取得元。読み取りだけを行う。
	const InputSystem* inputSystem{};
	// 命中判定を行うCollisionWorld。敵が死亡する際にColliderを解除するため、読み取り専用ではない。
	CollisionWorld* collisionWorld{};
	// 命中した敵の検索と、ダメージの反映先となる敵の所有者。
	EnemyManager* enemyManager{};
	// 一撃で倒す設定を反映した、プレイヤーの攻撃のダメージの取得元。読み取りだけを行う。
	const PlayerCombatController* playerCombat{};
	// 弾道の終点の通知先。
	CollisionDebugView* collisionDebugView{};
};

// 射撃を更新し、命中した敵へダメージと怯みを与え、命中表示の時間を管理する。
class ShotController final
{
public:
	//========= 初期化・終了関数=========
	// 参照先を登録し、使用中の武器の反動の設定をカメラへ渡す。
	// links: 借用する参照先。1つでも欠けている場合は何も登録しない。
	// 戻り値: 全ての参照先が揃っていて登録できたらtrue。
	[[nodiscard]] bool Initialize(const SHOT_CONTROLLER_LINKS& links);
	// 参照先を手放す。未初期化でも、繰り返しても呼出可能。
	void Uninit();
	// 初期武器（連射銃）を、満タンの弾薬で装備し直し、命中表示を消し、怯み時間の倍率とダメージの増加を通常へ戻す。参照先は変えない。
	void Reset();

	//========= 更新関数=========
	// ダメージの増加の残り時間を進め、暫定の武器切替の入力を処理し、射撃・リロードを更新して、弾が敵へ命中していればダメージと怯みを与える。Initialize成功後に呼ぶ。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。
	void Update(float gameplayDeltaTime);
	// 命中表示の残り時間を進める。Captureやヒットストップに関係なく毎フレーム呼ぶ。
	// deltaTime: この更新で進める経過時間。単位は秒。実時間。
	void UpdateHitMarker(float deltaTime);
	// リロードを途中でやめる。弾は補充しない。殴打など、リロードをキャンセルする動作から呼ぶ。
	void CancelReload() { m_weaponSystem.CancelReload(); }
	// 武器を装備する。武器の性能値と、カメラの反動の設定を切り替える。弾薬は、装備した武器の初期値になる。弾薬を消費しないデバッグ設定は、引き継ぐ。
	// weaponType: 装備する武器の種類。表の範囲内であること。
	void EquipWeapon(WEAPON_TYPE weaponType);
	// 使用中の武器の予備弾薬を増やす。弾薬の購入の効果。上限を超える分は加えない。
	// amount: 増やす弾数。
	// maxReserve: 予備弾薬の上限。
	// 戻り値: 実際に加えた弾数。0以上。
	int AddReserveAmmo(int amount,int maxReserve) { return m_weaponSystem.AddReserveAmmo(amount,maxReserve); }
	// デバッグ用に、弾薬を消費しない設定を切り替える。
	void ToggleInfiniteAmmo() { m_weaponSystem.ToggleInfiniteAmmo(); }
	// 射撃で敵へ与える怯み時間の倍率を設定する。特殊強化（制圧射撃）の効果。ResetやUninitでは引き継がないため、呼出側が毎回の更新で渡す。
	// flinchScale: 怯み時間の倍率。1.0で通常。正の値であること。
	void SetFlinchScale(float flinchScale) { m_flinchScale = flinchScale; }
	// 一定時間、射撃のダメージを増やす。特殊強化（回避反撃）の効果で、ジャスト回避の時に呼ぶ。すでに増えている最中なら、倍率と時間を、新しい値へ置き換える（時間の延長ではない）。
	// damageScale: 射撃のダメージの倍率。1.0より大きい値。
	// durationSeconds: ダメージが増える時間。単位は秒。プレイヤー側の時間（ヒットストップを反映した時間）で数える。
	void ActivateDamageBoost(float damageScale,float durationSeconds)
	{
		m_damageBoostScale = damageScale;
		m_damageBoostRemainingSeconds = durationSeconds;
	}

	//========= Getter関数=========
	// 武器の読み取り専用参照を返す。HUDの弾薬・リロード表示に使う。戻り値の参照は、このControllerの寿命を超えて保持しない。
	[[nodiscard]] const WeaponSystem& GetWeaponSystem() const { return m_weaponSystem; }
	// 直近の命中を表示中かを返す。
	[[nodiscard]] bool IsHitMarkerActive() const { return m_hitMarkerRemainingSeconds > 0.0f; }
	// 弾薬を消費しないデバッグ設定が有効かを返す。
	[[nodiscard]] bool IsInfiniteAmmoEnabled() const { return m_weaponSystem.IsInfiniteAmmoEnabled(); }
	// ジャスト回避の後の、射撃のダメージの増加が、効いている最中かを返す。
	[[nodiscard]] bool IsDamageBoostActive() const { return m_damageBoostRemainingSeconds > 0.0f; }

private:
	//========= 補助関数=========
	// 数字キー（1から順に、表の武器の並び）が押された瞬間に、対応する武器を装備する。暫定の操作で、武器の所持・購入の実装までの代わり。
	void UpdateWeaponSelection();

	//========= 参照先=========
	// 構え・射撃の可否の読み取り元。非所有。未登録はnullptr。
	const PlayerController* m_playerController{};
	// 発射の始点・方向と、反動の加算先。非所有。未登録はnullptr。
	FpsCamera* m_camera{};
	// 射撃・リロード・武器切替の入力の取得元。非所有。未登録はnullptr。
	const InputSystem* m_inputSystem{};
	// 命中判定とColliderの解除に使うCollisionWorld。非所有。未登録はnullptr。
	CollisionWorld* m_collisionWorld{};
	// 敵の検索とダメージの対象。非所有。未登録はnullptr。
	EnemyManager* m_enemyManager{};
	// ダメージの取得元。非所有。未登録はnullptr。
	const PlayerCombatController* m_playerCombat{};
	// 弾道の終点の通知先。非所有。未登録はnullptr。
	CollisionDebugView* m_collisionDebugView{};

	//========= 射撃の状態=========
	// 弾薬・連射・リロード・射撃判定を管理する武器。
	WeaponSystem m_weaponSystem{};
	// 直近の命中を表示する残り時間。単位は秒。0より大きい間、命中した旨を表示する。
	float m_hitMarkerRemainingSeconds{};

	//========= 特殊強化の効果=========
	// 射撃で敵へ与える怯み時間の倍率。1.0で通常。特殊強化（制圧射撃）で大きくなる。
	float m_flinchScale{1.0f};
	// ジャスト回避の後の、射撃のダメージの倍率。残り時間が0より大きい間だけ、使う。
	float m_damageBoostScale{1.0f};
	// ジャスト回避の後の、射撃のダメージの増加の残り時間。単位は秒。0以下なら、増えていない。
	float m_damageBoostRemainingSeconds{};
};