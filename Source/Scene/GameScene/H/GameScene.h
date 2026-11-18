#pragma once

// 移動・ダッシュ・しゃがみ・スライディング・回避・蹴り・殴打・視点・構え・射撃・リロード・回復と、設定表から生成した床・壁、部屋ごとの敵と敵の弾と範囲の予告、出口、画像のHUD、Collisionの可視化を行う最小構成のGameSceneを宣言する。
// 各クラスの更新を仲介し、共有Renderer・生成したObject・CollisionWorldを所有する。Scene切替はManagerへ予約する。

//========= C++標準ライブラリ インクルード=========
#include <array> // 敵の固定枠ごとの攻撃予告と再攻撃待ちを、枠数と同じ長さの配列で保持する。
#include <cstddef> // std::size_tで、敵の固定枠の番号を受け取る。
#include <cstdint> // std::uint32_tで、スライディングで転倒させた敵のID別の記録を保持する。
#include <vector> // 設定表から生成したSceneObjectを、表の順に保持する可変長配列として使用する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Input/H/FpsCamera.h"

//========= Scene インクルード=========
#include "Scene/Common/H/IScene.h"
#include "Scene/Common/H/ModelRegistry.h"
#include "Scene/Common/H/SceneObject.h"
#include "Scene/GameScene/H/AreaTelegraph.h"
#include "Scene/GameScene/H/CollisionDebugView.h"
#include "Scene/GameScene/H/EnemyManager.h"
#include "Scene/GameScene/H/EnemyProjectile.h"
#include "Scene/GameScene/H/ExitDoor.h"
#include "Scene/GameScene/H/GameHud.h"
#include "Scene/GameScene/H/HealItems.h"
#include "Scene/GameScene/H/JustDodgeJudge.h"
#include "Scene/GameScene/H/PlayerController.h"
#include "Scene/GameScene/H/PlayerHealth.h"
#include "Scene/GameScene/H/RoomController.h"
#include "Scene/GameScene/H/WeaponSystem.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// テスト用の操作・3D描画・HUD・デバッグ表示と、タイトルへの遷移予約を担当するScene。
class GameScene final: public IScene
{
public:
	//========= 生成関数=========
	// Sceneで使用する非所有参照を登録する。Resource生成はInitで行う。
	// sceneManager: タイトルへの遷移予約先。Sceneの使用期間中は有効であること。
	// inputSystem: キー入力の取得先とCapture設定先。Sceneより長く生存すること。
	// graphicsSystem: Resource生成・描画先サイズ取得・描画の基盤。Resource終了まで有効であること。
	// audioSystem: Game用BGMの再生先。Sceneの使用期間中は有効であること。
	GameScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

	//========= Sceneライフサイクル関数=========
	// プレイヤー・カメラ・武器・HP・回復アイテム・Collision・判定の状態・部屋の進行・敵の弾と範囲の予告・表示状態を初期状態へ戻し、生成済みObjectを空にして、カメラをプレイヤー位置へ合わせる。
	void Initialize() override;
	// 共有Renderer・HUD・Collision可視化を初期化し、設定表から床と壁（とそのCollider）、出口、敵の弾、範囲の予告、第1部屋の敵を生成して、成功後にFPS操作とGame用BGMを有効にする。
	// 戻り値: 各初期化・Object生成・敵の配置が成功したらtrue。falseを検出した場合はFinalize後にfalseを返す。
	// 下位処理の例外は捕捉しない。
	bool Init() override;
	// F2の遷移予約、F3〜F8の確認操作、F1のCapture切替、ヒットストップ・スローの管理、部屋の進行、出口の通過、視点・移動・動作・射撃・近接攻撃・回復・接触・敵の行動と弾の更新と反映を行う。
	// deltaTime: 各更新へ渡す経過時間。単位は秒。ヒットストップ中は0、スロー中はプレイヤー以外へ縮めた値を、それぞれの更新へ渡す。
	// Capture解除中は視点・移動・射撃・出口の通過・敵の行動の更新を行わない。F2で予約した場合は後続処理を行わない。
	void Update(float deltaTime) override;
	// 描画先サイズに合わせて行列を作り、生成済みのObject・範囲の予告・出口・敵・敵の弾・Collisionの可視化を描画し、続けて画像のHUDと読み取り専用のデバッグUIを描画する。
	// 幅または高さが0なら何もしない。ImGuiフレーム開始後、描画データ確定前に呼ぶ。
	void Draw() override;
	// 敵・敵の弾・範囲の予告・出口・生成したObjectを先に破棄し、その後に共有Renderer・HUD・Collision可視化・Collisionを解放する。繰り返し呼出可能。CaptureやBGMは変更しない。
	void Finalize() override;

private:
	//========= 更新の補助関数=========
	// 確認用の攻撃が当たる瞬間を判定し、結果に応じてプレイヤーへのダメージとスローを反映する。
	// worldDeltaTime: スローを反映した経過時間。単位は秒。ヒットストップ中は0。
	void UpdateDodgeJudge(float worldDeltaTime);
	// 全枠の敵を出現順に、種類に応じて更新し、予告中の敵の表示を切り替える。
	// worldDeltaTime: 敵側の経過時間。単位は秒。スローとヒットストップ反映済み。
	void UpdateEnemyActions(float worldDeltaTime);
	// 指定した1枠の接近型について、追従・攻撃予告・命中候補を更新する。
	// slotIndex: EnemyManagerの固定枠の番号。0始まりで、攻撃状態の配列と同じ範囲であること。
	// worldDeltaTime: 敵側の経過時間。単位は秒。
	void UpdateEnemyMeleeSlot(std::size_t slotIndex,float worldDeltaTime);
	// 指定した1枠の射撃型について、距離維持の移動・攻撃予告・発射を更新する。発射のタイミングに達したら、弾を生成する。
	// slotIndex: EnemyManagerの固定枠の番号。0始まりで、攻撃状態の配列と同じ範囲であること。
	// worldDeltaTime: 敵側の経過時間。単位は秒。
	void UpdateEnemyRangedSlot(std::size_t slotIndex,float worldDeltaTime);
	// 指定した1枠の範囲攻撃型について、接近・範囲の予告を更新する。予告の終わりでは、範囲の判定をまだ行わない。
	// slotIndex: EnemyManagerの固定枠の番号。0始まりで、攻撃状態の配列と同じ範囲であること。
	// worldDeltaTime: 敵側の経過時間。単位は秒。
	void UpdateEnemyAreaSlot(std::size_t slotIndex,float worldDeltaTime);
	// 敵の弾を進め、プレイヤーへの命中・回避の結果に応じて、ダメージ・怯み・ヒットストップ・スローを反映する。
	// worldDeltaTime: 敵側の経過時間。単位は秒。スローとヒットストップ反映済み。
	void UpdateEnemyProjectiles(float worldDeltaTime);
	// 全枠の攻撃予告と再攻撃待ちを破棄する。初期化と再配置で、旧ウェーブの状態を持ち越さないために使う。
	void ResetEnemyMelee();
	// 現在の部屋とウェーブの敵を生成し、全枠の攻撃状態・敵の弾・範囲の予告を初期化して、進行管理へ生成完了を通知する。
	// 戻り値: 登録に成功したらtrue。失敗した場合はEnemyManagerが空にし、進行管理には通知しない。
	[[nodiscard]] bool TrySpawnCurrentWave();
	// 出口が開いていて、プレイヤーが通過範囲に入った場合だけ、次の部屋へ進め、プレイヤーを開始位置へ戻す。
	void UpdateExitPass();
	// プレイヤーへ被弾の反応（ダメージ・怯み・ヒットストップ）を与える。
	// damage: 与えるダメージ。接触の場合は0。
	// hitStopSeconds: 設定するヒットストップの長さ。単位は秒。
	void ApplyPlayerHitReaction(float damage,float hitStopSeconds);
	// 近接攻撃の当たりが発生していれば、射程と範囲の内の敵へ、蹴り・殴打の効果を与える。
	void UpdateMelee();
	// 敵との体の接触を調べ、スライディング中は敵を転倒させ、それ以外では条件を満たす場合に接触の反応を与える。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。接触の反応の間隔を進める。
	void UpdateContact(float gameplayDeltaTime);
	// 射撃・リロードを更新し、弾が敵へ命中していればダメージを与える。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。
	void UpdateShooting(float gameplayDeltaTime);
	// Qの入力で、回復アイテムを使ってHPを回復する。HPが満タンの場合は消費しない。他の動作は中断しない。
	void UpdateHeal();

	//========= System・Scene参照=========
	// Scene遷移の予約先となる非所有参照。
	SceneManager& m_sceneManager;
	// 操作入力とCapture設定に使用する非所有参照。
	InputSystem& m_inputSystem;
	// Resource初期化と描画に使用する非所有参照。
	GraphicsSystem& m_graphicsSystem;
	// Init成功後のGame用BGM再生に使用する非所有参照。
	AudioSystem& m_audioSystem;

	//========= Scene所有状態=========
	// 位置・動作の状態・構え・近接攻撃・怯み・壁からの押し戻しを管理するプレイヤー。
	PlayerController m_playerController{};
	// プレイヤーのHP。
	PlayerHealth m_playerHealth{};
	// 回復アイテムの所持数と、連続使用の間隔。
	HealItems m_healItems{};
	// マウスによる視線方向と、プレイヤー位置に追従するView行列の生成状態。
	FpsCamera m_FPSCamera{};
	// 弾薬・連射・リロード・射撃判定を管理する武器。
	WeaponSystem m_weaponSystem{};
	// 現在のウェーブの敵を所有する。Finalizeで登録先のCollisionWorldより先に解除する。
	EnemyManager m_enemyManager{};
	// 射撃型の敵が撃った、飛行中の弾。Cube Rendererを借用するため、Finalizeで共有Rendererより先に解除する。
	EnemyProjectileManager m_enemyProjectiles{};
	// 範囲攻撃型の敵が予告中の、床の上の範囲。Cube Rendererを借用するため、Finalizeで共有Rendererより先に解除する。
	AreaTelegraph m_areaTelegraph{};
	// 3部屋×2ウェーブの進行状態。敵の生成と全滅判定は、GameSceneが仲介する。
	RoomController m_roomController{};
	// 現在のウェーブの生成に失敗したか。trueの間は生成を再試行せず、F4で最初からやり直す。
	bool m_hasWaveSpawnFailed{};
	// 部屋の出口。進行状態に合わせて開閉し、閉じている間は通行を止め、開いている間は通過を判定する。Finalizeで登録先のCollisionWorldより先に解除する。
	ExitDoor m_exitDoor{};
	// 現在の視野角。単位は度。構え・ダッシュの状態に応じた目標値へ滑らかに近づく。
	float m_fovDegrees{};

	//========= 判定・時間の状態=========
	// 確認用に発生させた攻撃の、命中の瞬間の判定（無敵・ジャスト回避）。
	JustDodgeJudge m_dodgeJudge{};
	// 敵の固定枠ごとの攻撃予告。接近型は命中時の回避判定に、射撃型・範囲攻撃型は発射・判定のタイミングの計測に使う。F7の判定とは状態を共有しない。
	std::array<JustDodgeJudge,ENEMY_WAVE_TABLE::ns_SLOT_COUNT> m_enemyMeleeJudges{};
	// 敵の固定枠ごとの、次の攻撃を予告できるまでの残り時間。単位は敵側の秒。初期状態は待ち時間なし。
	std::array<float,ENEMY_WAVE_TABLE::ns_SLOT_COUNT> m_enemyMeleeRecoverySeconds{};
	// 直近の判定結果。デバッグUIへの表示に使う。
	DODGE_JUDGE_RESULT m_lastJudgeResult{DODGE_JUDGE_RESULT::e_NONE};
	// スローの残り時間。単位は実時間の秒。0より大きい間、プレイヤー以外の時間の進みが遅くなる。
	float m_slowMotionRemainingSeconds{};
	// ヒットストップの残り時間。単位は実時間の秒。0より大きい間、プレイヤーと敵の時間が止まる。
	float m_hitStopRemainingSeconds{};
	// 敵への接触の反応が、次に起きるまでの残り時間。単位は秒。0なら起きる。
	float m_contactCooldownRemainingSeconds{};
	// 現在のスライディングで転倒させた敵の記録。ビット番号が敵IDに対応し、同じスライディングで同じ敵を繰り返し転倒させない。
	std::uint32_t m_slideKnockedDownEnemyMask{};
	// 1回前の更新での、プレイヤーの動作の状態。殴打の開始を検出するために使う。
	PLAYER_ACTION_STATE m_previousActionState{PLAYER_ACTION_STATE::e_NORMAL};

	//========= 描画ResourceとObject=========
	// Cube・OBJの共有Rendererを所有する。m_stageObjects・m_enemyManager・m_enemyProjectiles・m_areaTelegraph・m_exitDoorが借用するため、このメンバーを先に宣言し、後に破棄する。
	ModelRegistry m_modelRegistry{};
	// STAGE_LAYOUTの表から生成した床と壁。Rendererを借用するだけで所有しない。表の順が描画順になる。
	std::vector<SceneObject> m_stageObjects{};
	// 画像で描くHUD（照準・HPバー・弾薬・リロード表示・回復アイテム数・低HP警告）。
	GameHud m_gameHud{};

	//========= Collision=========
	// 床・壁・出口のAABBと、敵の球を登録するCollision。
	CollisionWorld m_collisionWorld{};
	// Collisionの可視化（Colliderの線・視線のRay・弾道）。F3で表示を切り替える。
	CollisionDebugView m_collisionDebugView{};

	//========= 表示状態=========
	// 直近の命中を表示する残り時間。単位は秒。0より大きい間、命中した旨をデバッグUIへ表示する。
	float m_hitMarkerRemainingSeconds{};
};