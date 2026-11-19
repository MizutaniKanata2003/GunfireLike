#pragma once

// 射撃の更新（弾薬・連射・リロードの進行と、命中判定の結果の反映）と、命中表示を担当する。
// 武器1丁の状態はWeaponSystemが持ち、このクラスが所有する。弾道の終点をCollisionの可視化へ伝え、敵レイヤーへ命中した場合に、敵へダメージを与える。
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
	// 左クリック（射撃）とRキー（リロード）の取得元。読み取りだけを行う。
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

// 射撃を更新し、命中した敵へダメージを与え、命中表示の時間を管理する。
class ShotController final
{
public:
	//========= 初期化・終了関数=========
	// 参照先を登録する。
	// links: 借用する参照先。1つでも欠けている場合は何も登録しない。
	// 戻り値: 全ての参照先が揃っていて登録できたらtrue。
	[[nodiscard]] bool Initialize(const SHOT_CONTROLLER_LINKS& links);
	// 参照先を手放す。未初期化でも、繰り返しても呼出可能。
	void Uninit();
	// 武器を初期の性能値・満タンの弾薬へ戻し、命中表示を消す。参照先は変えない。
	void Reset();

	//========= 更新関数=========
	// 射撃・リロードを更新し、弾が敵へ命中していればダメージを与える。Initialize成功後に呼ぶ。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。
	void Update(float gameplayDeltaTime);
	// 命中表示の残り時間を進める。Captureやヒットストップに関係なく毎フレーム呼ぶ。
	// deltaTime: この更新で進める経過時間。単位は秒。実時間。
	void UpdateHitMarker(float deltaTime);
	// リロードを途中でやめる。弾は補充しない。殴打など、リロードをキャンセルする動作から呼ぶ。
	void CancelReload() { m_weaponSystem.CancelReload(); }

	//========= Getter関数=========
	// 武器の読み取り専用参照を返す。HUDの弾薬・リロード表示に使う。戻り値の参照は、このControllerの寿命を超えて保持しない。
	[[nodiscard]] const WeaponSystem& GetWeaponSystem() const { return m_weaponSystem; }
	// 直近の命中を表示中かを返す。
	[[nodiscard]] bool IsHitMarkerActive() const { return m_hitMarkerRemainingSeconds > 0.0f; }

private:
	//========= 参照先=========
	// 構え・射撃の可否の読み取り元。非所有。未登録はnullptr。
	const PlayerController* m_playerController{};
	// 発射の始点・方向と、反動の加算先。非所有。未登録はnullptr。
	FpsCamera* m_camera{};
	// 射撃・リロードの入力の取得元。非所有。未登録はnullptr。
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
};
