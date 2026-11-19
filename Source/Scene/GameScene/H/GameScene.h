#pragma once

// 移動・ダッシュ・しゃがみ・スライディング・回避・蹴り・殴打・視点・構え・射撃・リロード・回復と、設定表から生成した床・壁、部屋ごとの敵と敵の弾と範囲の予告、出口、画像のHUD、Collisionの可視化を行う最小構成のGameSceneを宣言する。
// 各クラスの更新を仲介し、共有Renderer・生成したObject・CollisionWorldを所有する。Scene切替はManagerへ予約する。

//========= C++標準ライブラリ インクルード=========
#include <vector> // 設定表から生成したSceneObjectを、表の順に保持する可変長配列として使用する。

//========= Debug インクルード=========
#include "Debug/H/CollisionDebugView.h"

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Input/H/FpsCamera.h"

//========= Scene インクルード=========
#include "Scene/Common/AnyObject/H/ExitDoor.h"
#include "Scene/Common/AnyObject/H/RoomController.h"
#include "Scene/Common/Camera/H/CameraFovController.h"
#include "Scene/Common/Combat/H/JustDodgeJudge.h"
#include "Scene/Common/Combat/H/ShotController.h"
#include "Scene/Common/Enemy/H/EnemyController.h"
#include "Scene/Common/Etc/H/ModelRegistry.h"
#include "Scene/Common/Etc/H/TimeScaleController.h"
#include "Scene/Common/Player/H/HealItems.h"
#include "Scene/Common/Player/H/PlayerCombatController.h"
#include "Scene/Common/Player/H/PlayerController.h"
#include "Scene/Common/Player/H/PlayerHealth.h"
#include "Scene/Common/Scene/H/IScene.h"
#include "Scene/Common/Scene/H/SceneObject.h"
#include "Scene/GameScene/H/GameHud.h"

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
	// プレイヤー・カメラ・武器・HP・回復アイテム・Collision・判定の状態・部屋の進行・敵の弾と範囲の予告・デバッグ設定・表示状態を初期状態へ戻し、生成済みObjectを空にして、カメラをプレイヤー位置へ合わせる。
	void Initialize() override;
	// 共有Renderer・HUD・Collision可視化を初期化し、設定表から床と壁（とそのCollider）、出口、敵の弾、範囲の予告、第1部屋の敵を生成して、成功後にFPS操作とGame用BGMを有効にする。
	// 戻り値: 各初期化・Object生成・敵の配置が成功したらtrue。falseを検出した場合はFinalize後にfalseを返す。
	// 下位処理の例外は捕捉しない。
	bool Init() override;
	// F2の遷移予約、F3〜F9・F11の確認操作、F1のCapture切替、ヒットストップ・スローの管理、部屋の進行、出口の通過、視点・移動・動作・射撃・近接攻撃・回復・接触・敵の行動と弾の更新と反映を行う。
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
	// 敵の攻撃イベントを受け取り、命中ならプレイヤーへの被弾の反応を、ジャスト回避ならスローを反映し、求められた場合は直近の判定結果を記録する。
	// attackEvent: EnemyControllerが通知した攻撃イベント。敵の更新の途中で呼ばれる。
	void ApplyEnemyAttackEvent(const ENEMY_ATTACK_EVENT& attackEvent);
	// 現在の部屋とウェーブの敵をEnemyControllerへ生成させ、成功した場合は、スライディングの転倒記録を初期化して、進行管理へ生成完了を通知する。
	// 戻り値: 登録に成功したらtrue。失敗した場合はEnemyManagerが空にし、進行管理には通知しない。
	[[nodiscard]] bool TrySpawnCurrentWave();
	// デバッグ用に、現在のウェーブの生きている敵を全て倒し、敵の予告・弾・範囲の予告を消す。次の更新の全滅判定で、次のウェーブまたは出口の開放へ進む。
	void SkipCurrentWave();
	// 出口が開いていて、プレイヤーが通過範囲に入った場合だけ、次の部屋へ進め、プレイヤーを開始位置へ戻す。
	void UpdateExitPass();

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
	// 射撃・リロードの更新、弾道の終点の通知、命中した敵へのダメージ、命中表示の時間を担当し、武器を所有する。
	ShotController m_shotController{};
	// 現在のウェーブの敵・敵の弾・範囲の予告・敵ごとの攻撃予告を所有し、更新と描画を行う。Finalizeで、登録先のCollisionWorldと共有Rendererより先に解除する。
	EnemyController m_enemyController{};
	// 蹴り・殴打の当たり・敵との接触・スライディングの転倒・回復アイテムの使用・被弾の反応と、一撃で倒すデバッグ設定を担当する。ヒットストップは、TimeScaleControllerへ求める。
	PlayerCombatController m_playerCombat{};
	// ヒットストップとスローの残り時間を持ち、更新ごとに、プレイヤー側と、敵・判定側の経過時間を求める。
	TimeScaleController m_timeScale{};
	// 3部屋×2ウェーブの進行状態。敵の生成と全滅判定は、GameSceneが仲介する。
	RoomController m_roomController{};
	// 現在のウェーブの生成に失敗したか。trueの間は生成を再試行せず、F4で最初からやり直す。
	bool m_hasWaveSpawnFailed{};
	// 部屋の出口。進行状態に合わせて開閉し、閉じている間は通行を止め、開いている間は通過を判定する。Finalizeで登録先のCollisionWorldより先に解除する。
	ExitDoor m_exitDoor{};
	// 動作の状態に応じた視野角の目標値と、現在の視野角の補間を担当する。
	CameraFovController m_cameraFov{};

	//========= 判定・時間の状態=========
	// 確認用に発生させた攻撃の、命中の瞬間の判定（無敵・ジャスト回避）。
	JustDodgeJudge m_dodgeJudge{};
	// 直近の判定結果。デバッグUIへの表示に使う。
	DODGE_JUDGE_RESULT m_lastJudgeResult{DODGE_JUDGE_RESULT::e_NONE};
	// 1回前の更新での、プレイヤーの動作の状態。殴打の開始を検出するために使う。
	PLAYER_ACTION_STATE m_previousActionState{PLAYER_ACTION_STATE::e_NORMAL};

	//========= 描画ResourceとObject=========
	// Cube・OBJの共有Rendererを所有する。m_stageObjects・m_enemyController・m_exitDoorが借用するため、このメンバーを先に宣言し、後に破棄する。
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
};