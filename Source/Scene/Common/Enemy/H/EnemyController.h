#pragma once

// 敵の種類別の更新（接近型の追従と近接、射撃型の距離維持と発射、範囲攻撃型の接近と範囲の予告・判定、ボスの接近と2種類の攻撃）、敵の弾、範囲の予告、ウェーブの生成、敵側の描画を担当する。
// プレイヤーの位置・状態とHPは読み取るだけで、命中・回避の結果は攻撃イベントとして呼出側へ通知する。
// ダメージ・怯み・ヒットストップの反映、スローの開始、判定結果の記録、部屋の進行状態の更新、プレイヤーの攻撃による敵のダメージは担当しない。

//========= C++標準ライブラリ インクルード=========
#include <array> // 敵の固定枠ごとの攻撃予告と再攻撃待ちを、枠数と同じ長さの配列で保持する。
#include <cstddef> // std::size_tで、敵の固定枠の番号を受け取る。
#include <functional> // std::functionで、攻撃イベントの通知先（呼出側の関数）を保持する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/JustDodgeJudge.h"
#include "Scene/Common/Enemy/H/AreaTelegraph.h"
#include "Scene/Common/Enemy/H/BossController.h"
#include "Scene/Common/Enemy/H/EnemyManager.h"
#include "Scene/Common/Enemy/H/EnemyProjectile.h"
#include "Scene/Common/Enemy/H/EnemyWaveTable.h"

//========= 前方宣言=========
class CollisionWorld;
class GraphicsSystem;
class ModelRegistry;
class PlayerController;
class PlayerHealth;

// 敵の攻撃がプレイヤーへ及ぼした結果1件。呼出側が、ダメージ・スロー・表示用の記録へ反映する。
struct ENEMY_ATTACK_EVENT
{
	// 判定の結果。e_HITは命中、e_DODGEDは通常の回避、e_JUST_DODGEDはジャスト回避を表す。
	DODGE_JUDGE_RESULT result{DODGE_JUDGE_RESULT::e_NONE};
	// 命中した場合にプレイヤーへ与えるダメージ。resultがe_HITの場合だけ使う。
	float damage{};
	// 直近の判定結果として、デバッグUIへ記録するか。ボス・範囲攻撃型と敵の弾はtrue、接近型はfalse。
	bool shouldRecordResult{};
	// スローを開始するか。実際に回避中のジャスト回避の場合だけtrue。
	bool shouldStartSlowMotion{};
};

// EnemyControllerが借用する参照先と、攻撃イベントの通知先をまとめる。参照先は、Uninitまで有効であること。
struct ENEMY_CONTROLLER_LINKS
{
	// Cube Rendererの借用元と、敵の生成に使う共有Registry。Initialize済みであること。
	ModelRegistry* modelRegistry{};
	// 敵のColliderの登録先と、移動の押し戻しに使うCollisionWorld。
	CollisionWorld* collisionWorld{};
	// 位置・動作の状態・無敵・回避の経過時間の読み取り元となるプレイヤー。読み取りだけを行う。
	const PlayerController* playerController{};
	// 死亡の確認に使うプレイヤーのHP。読み取りだけを行う。
	const PlayerHealth* playerHealth{};
	// 攻撃イベントの通知先。命中・回避が起きた瞬間に、更新の途中で呼ばれる。空のままではInitializeが失敗する。
	std::function<void(const ENEMY_ATTACK_EVENT&)> onAttackEvent{};
};

// 現在のウェーブの敵・敵の弾・範囲の予告・敵ごとの攻撃予告を所有し、更新と描画を行う。
class EnemyController final
{
public:
	//========= 初期化・終了関数=========
	// 参照先と通知先を登録し、敵の弾と範囲の予告のCube Rendererを借用し、ボスの更新へ参照先を渡す。
	// links: 借用する参照先と通知先。必須の項目が欠けている場合は何も登録しない。
	// 戻り値: 登録と借用に成功したらtrue。falseの場合も、呼出側がUninitで部分的な状態を解放する。
	[[nodiscard]] bool Initialize(const ENEMY_CONTROLLER_LINKS& links);
	// 全枠のColliderを解除し、敵の弾・範囲の予告を解放して、参照先を手放す。未初期化でも、繰り返しても呼出可能。
	// 登録先のCollisionWorldと共有Registryより先に呼ぶこと。
	void Uninit();
	// 全枠の攻撃予告と再攻撃待ちを初期状態へ戻し、飛行中の敵の弾と範囲の予告を消し、ボスの攻撃の状態を戻す。旧ウェーブの状態を持ち越さないために使う。
	void ResetAttackState();

	//========= 生成関数=========
	// 指定した部屋とウェーブの敵を生成し、成功した場合だけ攻撃状態・敵の弾・範囲の予告を初期化する。Initialize成功後に呼ぶ。
	// roomNumber: 1始まりの部屋番号。
	// waveNumber: 1始まりのウェーブ番号。
	// 戻り値: 全ての敵の登録に成功したらtrue。失敗した場合はEnemyManagerが空にし、攻撃状態は変えない。
	[[nodiscard]] bool TrySpawnWave(int roomNumber,int waveNumber);

	//========= 更新・描画関数=========
	// 全枠の敵を出現順に種類別に更新し、続けて敵の弾を進める。命中・回避の結果は、起きた順に攻撃イベントとして通知する。Initialize成功後に呼ぶ。
	// worldDeltaTime: 敵側の経過時間。単位は秒。スローとヒットストップ反映済みで、ヒットストップ中は0。
	void Update(float worldDeltaTime);
	// 予告中の範囲を、床の上の赤い板として描く。敵・出口より先に描くことで、敵の体が予告の上に重なる。
	// graphicsSystem: Rendererが使う描画基盤。
	// viewMatrix: World空間からView空間への変換行列。
	// projectionMatrix: View空間からClip空間への変換行列。
	void DrawAreaTelegraph(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;
	// 全枠の敵を出現順に描き、続けて飛行中の敵の弾を描く。死亡枠はEnemy::Drawが描画を省略する。
	// graphicsSystem: Rendererが使う描画基盤。
	// viewMatrix: World空間からView空間への変換行列。
	// projectionMatrix: View空間からClip空間への変換行列。
	void DrawEnemies(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

	//========= Getter関数=========
	// 敵の所有者を返す。プレイヤーの攻撃の対象検索・ダメージ・デバッグ操作で使う。戻り値の参照は、このControllerの寿命を超えて保持しない。
	[[nodiscard]] EnemyManager& GetEnemyManager() { return m_enemyManager; }
	// 敵の所有者を、読み取り専用で返す。
	[[nodiscard]] const EnemyManager& GetEnemyManager() const { return m_enemyManager; }

private:
	//========= 更新の補助関数=========
	// 全枠の敵を出現順に、種類に応じて更新し、予告中の敵の表示を切り替える。
	// worldDeltaTime: 敵側の経過時間。単位は秒。
	void UpdateActions(float worldDeltaTime);
	// 指定した1枠の接近型について、追従・攻撃予告・命中の判定を更新する。
	// slotIndex: EnemyManagerの固定枠の番号。0始まりで、攻撃状態の配列と同じ範囲であること。
	// worldDeltaTime: 敵側の経過時間。単位は秒。
	void UpdateMeleeSlot(std::size_t slotIndex,float worldDeltaTime);
	// 指定した1枠の射撃型について、距離維持の移動・攻撃予告・弾の生成を更新する。
	// slotIndex: EnemyManagerの固定枠の番号。0始まりで、攻撃状態の配列と同じ範囲であること。
	// worldDeltaTime: 敵側の経過時間。単位は秒。
	void UpdateRangedSlot(std::size_t slotIndex,float worldDeltaTime);
	// 指定した1枠の範囲攻撃型について、接近・範囲の予告・予告の終わりの範囲の判定を更新する。
	// slotIndex: EnemyManagerの固定枠の番号。0始まりで、攻撃状態の配列と同じ範囲であること。
	// worldDeltaTime: 敵側の経過時間。単位は秒。
	void UpdateAreaSlot(std::size_t slotIndex,float worldDeltaTime);
	// 指定した1枠のボスについて、接近・攻撃の予告・判定を、BossControllerに更新させ、判定の結果を攻撃イベントとして通知する。
	// slotIndex: EnemyManagerの固定枠の番号。0始まりで、ボスがいる枠。
	// worldDeltaTime: 敵側の経過時間。単位は秒。
	void UpdateBossSlot(std::size_t slotIndex,float worldDeltaTime);
	// 敵の弾を進め、命中・回避した弾の結果を攻撃イベントとして通知する。
	// worldDeltaTime: 敵側の経過時間。単位は秒。
	void UpdateProjectiles(float worldDeltaTime);
	// 攻撃イベントを通知先へ渡す。通知先が空の場合は何もしない。
	// attackEvent: 通知する攻撃イベント。
	void NotifyAttackEvent(const ENEMY_ATTACK_EVENT& attackEvent) const;

	//========= 参照先=========
	// 敵の生成に使う共有Registry。非所有で、Uninitまで有効。未登録はnullptr。
	ModelRegistry* m_modelRegistry{};
	// 敵のColliderの登録先と、移動の押し戻しに使うCollisionWorld。非所有で、Uninitまで有効。未登録はnullptr。
	CollisionWorld* m_collisionWorld{};
	// 位置・動作の状態・無敵・回避の経過時間の読み取り元。非所有。未登録はnullptr。
	const PlayerController* m_playerController{};
	// 死亡の確認に使うプレイヤーのHP。非所有。未登録はnullptr。
	const PlayerHealth* m_playerHealth{};
	// 攻撃イベントの通知先。命中・回避が起きた瞬間に呼ぶ。未登録の間は空。
	std::function<void(const ENEMY_ATTACK_EVENT&)> m_onAttackEvent{};

	//========= 所有状態=========
	// 現在のウェーブの敵を所有する。Uninitで、登録先のCollisionWorldより先に解除する。
	EnemyManager m_enemyManager{};
	// 射撃型の敵が撃った、飛行中の弾。Cube Rendererを借用するため、Uninitで共有Registryより先に解除する。
	EnemyProjectileManager m_enemyProjectiles{};
	// 範囲攻撃型の敵とボスが予告中の、床の上の範囲。Cube Rendererを借用するため、Uninitで共有Registryより先に解除する。
	AreaTelegraph m_areaTelegraph{};
	// ボスの接近・攻撃の選択・予告・判定。範囲の表示先として、m_areaTelegraphを借用するため、その後に宣言する。
	BossController m_bossController{};
	// 敵の固定枠ごとの攻撃予告。接近型は命中時の回避判定に、射撃型は発射のタイミングの計測に、範囲攻撃型は範囲の判定のタイミングの計測に使う。ボスは、BossControllerが持つ。
	std::array<JustDodgeJudge,ENEMY_WAVE_TABLE::ns_SLOT_COUNT> m_attackJudges{};
	// 敵の固定枠ごとの、次の攻撃を予告できるまでの残り時間。単位は敵側の秒。初期状態は待ち時間なし。ボスは、BossControllerが持つ。
	std::array<float,ENEMY_WAVE_TABLE::ns_SLOT_COUNT> m_recoverySeconds{};
};