#include "../H/GameScene.h"

// テストSceneの初期化、入力に応じた状態更新、射撃・近接攻撃・回避・回復・被弾の反映、部屋ごとの敵と敵の弾と範囲の予告と出口、設定表から生成した床・壁・画像のHUD・デバッグUIの描画、Resource終了を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_F1〜VK_F9とVK_F11の仮想キーコードで、Capture切替・タイトル遷移・可視化切替・最初の部屋からの再開・確認操作・デバッグ操作のキーを指定する。

//========= C++標準ライブラリ インクルード=========

//========= 外部ライブラリ インクルード=========
#include <DirectXMath.h>

//========= Debug インクルード=========
#include "Debug/H/GameDebugUi.h"

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/H/CombatResolver.h"
#include "Scene/Common/H/CombatTable.h"
#include "Scene/Common/H/SceneManager.h"
#include "Scene/Common/H/SceneObjectFactory.h"
#include "Scene/Common/H/StageLayout.h"
#include "Scene/TitleScene/H/TitleScene.h"

// このSceneの投影面（Clip面）の距離、開始位置に使用する固定設定をまとめる。戦闘の数値はCombatTableにある。
namespace
{
	//========= Projection設定=========
	// View空間での近距離Clip面の距離。単位はワールド距離単位。これより近い物は描画されない。
	constexpr float ns_NEAR_CLIP = 0.1f;
	// View空間での遠距離Clip面の距離。単位はワールド距離単位。これより遠い物は描画されない。
	constexpr float ns_FAR_CLIP = 1000.0f;

	//========= 配置の設定=========
	// プレイヤーの開始位置。ワールド座標(0,1.6,0)で、床の手前寄りの中央。Yは立っている時のカメラの高さ。壁の内側にある。
	constexpr XMFLOAT3 ns_PLAYER_START_POSITION{0.0f,1.6f,0.0f};
}

//========= 生成関数=========
// 共有SystemやManagerを所有せず、Sceneで使用する参照だけを登録する。
GameScene::GameScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem)
	: m_sceneManager(sceneManager),m_inputSystem(inputSystem),m_graphicsSystem(graphicsSystem),m_audioSystem(audioSystem)
{}

//========= Sceneライフサイクル関数=========
// GPU Resource生成の前に、操作と配置の初期状態を準備する。
void GameScene::Initialize()
{
	// プレイヤー・HP・回復アイテム・カメラ・武器・視野角を既定へ戻す。
	m_playerController.Initialize(ns_PLAYER_START_POSITION);
	m_playerHealth.Initialize(COMBAT_TABLE::ns_PLAYER_MAX_HP);
	m_healItems.Initialize(COMBAT_TABLE::ns_HEAL_INITIAL_COUNT,COMBAT_TABLE::ns_HEAL_MAX_COUNT);
	m_FPSCamera = FpsCamera{};
	m_shotController.Reset();
	m_cameraFov.Reset();

	// 進行状態を第1部屋の第1ウェーブへ戻し、前回の生成失敗を引き継がない。
	m_roomController.Initialize();
	m_hasWaveSpawnFailed = false;

	// 判定・時間・接触・表示の状態を既定へ戻し、前回の生成ObjectとColliderを空にする。Rendererの生成、表からのObject生成、出口と敵の配置はInitで行う。
	m_dodgeJudge.Initialize();
	m_enemyController.ResetAttackState();
	m_lastJudgeResult = DODGE_JUDGE_RESULT::e_NONE;
	m_timeScale.Reset();
	m_playerCombat.Reset();
	m_previousActionState = PLAYER_ACTION_STATE::e_NORMAL;
	m_stageObjects.clear();
	m_collisionWorld.Clear();

	// カメラの位置をプレイヤーの開始位置へ合わせる。壁のColliderはInitで登録するため、押し戻しはInit後の更新から始まる。
	m_FPSCamera.SetPosition(m_playerController.GetPosition());
}

// 描画Resourceと生成Objectの準備に成功した後にだけ、ゲーム操作とBGMへ切り替える。
bool GameScene::Init()
{
	// 共有Rendererの初期化失敗を検出したら、部分Resourceを終了して管理側へ失敗を返す。Captureと再生は変更しない。
	if(!m_modelRegistry.Initialize(m_graphicsSystem)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの基本Mesh初期化に失敗しました。");
		Finalize();
		return false;
	}

	// Collision可視化の初期化失敗を検出したら、生成済みのRendererを含めて終了して失敗を返す。
	if(!m_collisionDebugView.Initialize(m_graphicsSystem)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの線描画Renderer初期化に失敗しました。");
		Finalize();
		return false;
	}

	// 画像のHUDの初期化失敗を検出したら、生成済みのResourceを含めて終了して失敗を返す。個々の画像の読込失敗はここでは失敗にならない。
	if(!m_gameHud.Initialize(m_graphicsSystem)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneのHUD初期化に失敗しました。");
		Finalize();
		return false;
	}

	// 設定表から床と4面の壁を、表の順に生成し、壁のAABB ColliderをCollisionWorldへ登録する。
	// 全件成功しなければ、生成済みのRendererを含めて終了して失敗を返す。
	if(!SceneObjectFactory::Build(STAGE_LAYOUT::ns_OBJECTS,STAGE_LAYOUT::ns_OBJECT_COUNT,m_modelRegistry,&m_collisionWorld,m_stageObjects)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneのステージObject生成に失敗しました。");
		Finalize();
		return false;
	}

	// 閉じた状態の出口を登録する。Cube Rendererを借用できなければ、生成済みのResourceを含めて終了して失敗を返す。
	if(!m_exitDoor.Initialize(m_modelRegistry,m_collisionWorld)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの出口の初期化に失敗しました。");
		Finalize();
		return false;
	}

	// 敵の更新に使う参照と、攻撃イベントの通知先（被弾・スロー・記録の反映）を、EnemyControllerへ渡す。
	ENEMY_CONTROLLER_LINKS enemy_links{};
	enemy_links.modelRegistry = &m_modelRegistry;
	enemy_links.collisionWorld = &m_collisionWorld;
	enemy_links.playerController = &m_playerController;
	enemy_links.playerHealth = &m_playerHealth;
	enemy_links.onAttackEvent = [this](const ENEMY_ATTACK_EVENT& attackEvent){
		ApplyEnemyAttackEvent(attackEvent);
	};

	// 敵の弾と範囲の予告のCube Rendererを借用する。借用できなければ、生成済みのResourceを含めて終了して失敗を返す。
	if(!m_enemyController.Initialize(enemy_links)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの敵の更新の初期化に失敗しました。");
		Finalize();
		return false;
	}

	// プレイヤーの戦闘に使う参照と、ヒットストップの通知先を、PlayerCombatControllerへ渡す。ヒットストップは、すでに残っている場合は長い方を使う。
	PLAYER_COMBAT_LINKS player_combat_links{};
	player_combat_links.playerController = &m_playerController;
	player_combat_links.playerHealth = &m_playerHealth;
	player_combat_links.healItems = &m_healItems;
	player_combat_links.enemyManager = &m_enemyController.GetEnemyManager();
	player_combat_links.collisionWorld = &m_collisionWorld;
	player_combat_links.camera = &m_FPSCamera;
	player_combat_links.inputSystem = &m_inputSystem;
	player_combat_links.onHitStopRequested = [this](float hitStopSeconds){
		m_timeScale.RequestHitStop(hitStopSeconds);
	};

	// 参照先の登録に失敗したら、生成済みのResourceを含めて終了して失敗を返す。
	if(!m_playerCombat.Initialize(player_combat_links)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneのプレイヤー戦闘の初期化に失敗しました。");
		Finalize();
		return false;
	}

	// 射撃に使う参照を、ShotControllerへ渡す。参照先の登録に失敗したら、生成済みのResourceを含めて終了して失敗を返す。
	SHOT_CONTROLLER_LINKS shot_links{};
	shot_links.playerController = &m_playerController;
	shot_links.camera = &m_FPSCamera;
	shot_links.inputSystem = &m_inputSystem;
	shot_links.collisionWorld = &m_collisionWorld;
	shot_links.enemyManager = &m_enemyController.GetEnemyManager();
	shot_links.playerCombat = &m_playerCombat;
	shot_links.collisionDebugView = &m_collisionDebugView;
	if(!m_shotController.Initialize(shot_links)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの射撃の初期化に失敗しました。");
		Finalize();
		return false;
	}

	// 現在の部屋とウェーブの生成に失敗したら、残っているSceneのResourceも解放して初期化失敗を返す。
	if(!TrySpawnCurrentWave()){
		Finalize();
		return false;
	}

	// Resource生成とObject生成が成功してからCaptureとBGMを変更し、false返却時には変更しない。
	m_inputSystem.SetMouseCaptureEnabled(true);
	m_audioSystem.PlayGameBgm();
	return true;
}

// 遷移入力とCapture状態を先に処理し、操作可能な場合だけ視点・移動・戦闘を更新する。
void GameScene::Update(float deltaTime)
{
	// F2が押された瞬間はタイトルへの遷移を予約し、同じフレームのCapture切替や移動を行わず戻る。
	if(m_inputSystem.IsKeyTriggered(VK_F2)){
		m_sceneManager.RequestSceneChange<TitleScene>();
		return;
	}

	// F1が押された瞬間は、ゲーム操作とImGui操作に使うCapture状態を反転する。
	if(m_inputSystem.IsKeyTriggered(VK_F1)){
		m_inputSystem.SetMouseCaptureEnabled(!m_inputSystem.IsMouseCaptureEnabled());
	}

	// F3が押された瞬間は、Collision可視化の表示を反転する。Capture解除中も切り替えられるよう、Captureの判定より前に処理する。
	if(m_inputSystem.IsKeyTriggered(VK_F3)){
		m_collisionDebugView.ToggleVisible();
	}

	// F4では最初の部屋から作り直す。旧ウェーブと、旧ウェーブの弾・範囲の予告は、同じ更新の生成処理で置き換わる。
	if(m_inputSystem.IsKeyTriggered(VK_F4)){
		m_roomController.Initialize();
		m_hasWaveSpawnFailed = false;
		m_enemyController.ResetAttackState();
		m_playerCombat.ClearSlideKnockdownRecord();
	}

	// F5が押された瞬間は、HPバーと低HP警告の確認のため、プレイヤーへ確認用のダメージを与える。ヒットストップや怯みは与えない。
	if(m_inputSystem.IsKeyTriggered(VK_F5)){
		m_playerHealth.ApplyDamage(COMBAT_TABLE::ns_DEBUG_DAMAGE);
	}

	// F6が押された瞬間は、プレイヤーのHPを満タンへ戻す。
	if(m_inputSystem.IsKeyTriggered(VK_F6)){
		m_playerHealth.HealFull();
	}

	// F7が押された瞬間は、回避とジャスト回避の確認のため、一定時間後に当たる攻撃を予告する。敵の攻撃の実装までの暫定の操作。
	if(m_inputSystem.IsKeyTriggered(VK_F7)){
		m_dodgeJudge.StartIncomingAttack(COMBAT_TABLE::ns_SIMULATED_ATTACK_DELAY_SECONDS);
	}

	// F8が押された瞬間は、回復アイテムを1個増やす。入手の実装までの暫定の操作。上限を超えない。
	if(m_inputSystem.IsKeyTriggered(VK_F8)){
		m_healItems.Add(1);
	}

	// F9が押された瞬間は、デバッグ用に、一撃で敵を倒す設定を切り替える。Capture解除中も切り替えられるよう、Captureの判定より前に処理する。
	if(m_inputSystem.IsKeyTriggered(VK_F9)){
		m_playerCombat.ToggleOneHitKill();
	}

	// F11が押された瞬間は、デバッグ用に、現在のウェーブの敵を全て倒して、次のウェーブまたは出口の開放へ進める。
	if(m_inputSystem.IsKeyTriggered(VK_F11)){
		SkipCurrentWave();
	}

	// ヒットストップとスローの残り時間を実時間で進め、プレイヤー側の時間と、敵・判定側の時間を求める。ヒットストップ中は、どちらも0になる。
	// スロー中は、敵・判定側の時間だけが遅くなる。プレイヤーと武器は、スローの影響を受けない。
	const TIME_SCALE_DELTAS time_deltas = m_timeScale.Advance(deltaTime);
	const float gameplay_delta_time = time_deltas.gameplayDeltaTime;
	const float world_delta_time = time_deltas.worldDeltaTime;

	// 確認用の攻撃の判定、敵の怯み・転倒、回復アイテムの間隔、表示用の残り時間を、Captureに関係なく進める。表示用は実時間で、0未満にしない。
	UpdateDodgeJudge(world_delta_time);
	m_enemyController.GetEnemyManager().Update(world_delta_time);

	// 生成待ちなら現在のウェーブを生成する。失敗した場合は再試行せず、F4で最初からやり直すまで敵なしで待つ。
	if(m_roomController.GetPhase() == ROOM_PHASE::e_NEEDS_SPAWN && !m_hasWaveSpawnFailed){
		m_hasWaveSpawnFailed = !TrySpawnCurrentWave();
	}
	// 生存中の敵がいなければウェーブまたは部屋を進める。生成待ちの間は、空の敵一覧を全滅と扱わない。
	m_roomController.Update(m_enemyController.GetEnemyManager().HasLivingEnemies());

	// 部屋の進行状態に合わせて、出口を開閉する。出口開放中と攻略完了後は開き、それ以外は閉じる。F4や部屋の移動で戻った場合も、この更新で閉じる。
	const ROOM_PHASE room_phase = m_roomController.GetPhase();
	m_exitDoor.SetOpen(room_phase == ROOM_PHASE::e_EXIT_OPEN || room_phase == ROOM_PHASE::e_COMPLETE,m_collisionWorld);

	m_healItems.Update(gameplay_delta_time);
	m_collisionDebugView.Update(deltaTime);
	m_shotController.UpdateHitMarker(deltaTime);

	// 反動によるカメラのずれを、実時間で目標へ滑らかに近づけ、射撃をやめて少し経つと、元の視線へ戻す。Captureに関係なく進める。
	m_FPSCamera.UpdateRecoil(deltaTime);

	// マウスを解放している間は、カメラ・移動・戦闘の操作更新をまとめて停止し、ここで戻る。
	if(!m_inputSystem.IsMouseCaptureEnabled())return;

	// 視線を先に更新し、その向きで、蹴れる敵がいるかを調べてから、プレイヤーを更新する。ヒットストップ中も視線は動かせる。
	m_FPSCamera.Update(m_inputSystem);
	const bool can_kick = m_enemyController.GetEnemyManager().FindMeleeTarget(m_FPSCamera.GetPosition(),m_FPSCamera.GetForward(),true) != nullptr;
	m_playerController.Update(gameplay_delta_time,m_inputSystem,m_FPSCamera,m_collisionWorld,can_kick);
	m_FPSCamera.SetPosition(m_playerController.GetPosition());

	// 移動後の位置で、出口の通過を判定する。通過した場合は、プレイヤーが開始位置へ戻るため、以降の更新は新しい位置から行う。
	UpdateExitPass();

	// 現在の動作の状態を取得する。
	const PLAYER_ACTION_STATE action_state = m_playerController.GetActionState();
	// 殴打を始めた更新では、リロードをキャンセルする。弾は補充しない。殴打の開始は、1回前の状態が殴打ではなかったことで検出する。
	if(action_state == PLAYER_ACTION_STATE::e_PUNCH && m_previousActionState != PLAYER_ACTION_STATE::e_PUNCH){
		m_shotController.CancelReload();
	}
	m_previousActionState = action_state;

	// 動作の状態と構えに応じた視野角の目標値へ、滑らかに近づける。
	m_cameraFov.Update(deltaTime,action_state,m_playerController.IsAiming());

	// 近接攻撃・接触・回復・射撃を、この順に更新する。
	m_playerCombat.Update(gameplay_delta_time);
	m_shotController.Update(gameplay_delta_time);
	// 当フレームの回避・移動・敵への怯みを反映してから、敵の行動と、敵の弾の命中を判定する。
	m_enemyController.Update(world_delta_time);

	// 更新後のカメラから、Collision可視化の視線のRayを更新する。非表示の間は判定を行わない。
	m_collisionDebugView.UpdateRay(m_FPSCamera,m_collisionWorld);
}

// 現在の描画先サイズと視点から3D描画とHUDを行い、操作状態をデバッグUIへ表示する。
void GameScene::Draw()
{
	// 描画先の現在サイズをpixel単位で取得する。
	const unsigned int render_width = m_graphicsSystem.GetRenderWidth();
	const unsigned int render_height = m_graphicsSystem.GetRenderHeight();
	// 最小化中などで幅または高さが0なら、アスペクト比の除算と描画を行わず戻る。
	if(render_width == 0 || render_height == 0)return;

	// 現在サイズの比率で左手系の透視投影行列を作り、カメラからView行列を取得する。視野角は動作の状態を反映した現在値を使う。
	// 投影行列の生成関数と角度変換は、MathTypes.hのusing宣言により修飾なしで呼ぶ。
	const float aspect_ratio = static_cast<float>(render_width) / static_cast<float>(render_height);
	const XMMATRIX projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_cameraFov.GetFovDegrees()),aspect_ratio,ns_NEAR_CLIP,ns_FAR_CLIP);
	const XMMATRIX view_matrix = m_FPSCamera.GetViewMatrix();

	// 床・壁・範囲の予告・出口・敵・敵の弾はOPAQUE Passで描画するため、描画前にPassを設定する。
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);

	// 生成済みの全Objectを、表の順（床→左壁→右壁→手前壁→奥壁）に描画する。
	for(const SceneObject& object : m_stageObjects){
		object.Draw(m_graphicsSystem,view_matrix,projection_matrix);
	}

	// 床の上に、範囲の予告を描画する。床の後に描き、敵や出口より先に描くことで、敵の体が予告の上に重なる。
	m_enemyController.DrawAreaTelegraph(m_graphicsSystem,view_matrix,projection_matrix);
	// 続けて出口を描画する。
	m_exitDoor.Draw(m_graphicsSystem,view_matrix,projection_matrix);
	// 続けて全枠の敵を出現順に描画し、その後に飛行中の敵の弾を描画する。死亡枠はEnemy::Drawが描画を省略する。
	m_enemyController.DrawEnemies(m_graphicsSystem,view_matrix,projection_matrix);
	// 敵と弾の後に、表示中ならCollisionの線を重ねる。
	m_collisionDebugView.Draw(m_graphicsSystem,m_collisionWorld,m_FPSCamera,view_matrix,projection_matrix);

	// 3Dの上に画像のHUDを重ねるため、深度比較なし・アルファ合成の画面UI用Passへ切り替え、現在の値をHUDへ渡して描画する。
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);
	GAME_HUD_STATE hud_state{};
	hud_state.currentHP = m_playerHealth.GetCurrentHP();
	hud_state.maxHP = m_playerHealth.GetMaxHP();
	hud_state.healItemCount = m_healItems.GetCount();
	hud_state.magazineAmmo = m_shotController.GetWeaponSystem().GetMagazineAmmo();
	hud_state.magazineSize = m_shotController.GetWeaponSystem().GetMagazineSize();
	hud_state.reserveAmmo = m_shotController.GetWeaponSystem().GetReserveAmmo();
	hud_state.isReloading = m_shotController.GetWeaponSystem().IsReloading();
	hud_state.reloadProgress = m_shotController.GetWeaponSystem().GetReloadProgress();
	hud_state.isAiming = m_playerController.IsAiming();
	m_gameHud.Draw(m_graphicsSystem,hud_state);

	// デバッグUIに表示する値を集めて渡す。デバッグUIはゲームの状態を変更しない。
	GAME_DEBUG_INFO debug_info{};
	debug_info.playerPosition = m_playerController.GetPosition();
	debug_info.playerHP = m_playerHealth.GetCurrentHP();
	debug_info.playerMaxHP = m_playerHealth.GetMaxHP();
	debug_info.actionState = m_playerController.GetActionState();
	debug_info.isInvincible = m_playerController.IsInvincible();
	debug_info.isPlayerFlinching = m_playerController.IsFlinching();
	debug_info.healItemCount = m_healItems.GetCount();
	debug_info.roomNumber = m_roomController.GetRoomNumber();
	debug_info.waveNumber = m_roomController.GetWaveNumber();
	debug_info.roomPhase = m_roomController.GetPhase();
	debug_info.hasWaveSpawnFailed = m_hasWaveSpawnFailed;
	debug_info.isOneHitKillEnabled = m_playerCombat.IsOneHitKillEnabled();
	debug_info.isEnemyAlive = m_enemyController.GetEnemyManager().GetFirstEnemy().IsAlive();
	debug_info.enemyHP = m_enemyController.GetEnemyManager().GetFirstEnemy().GetCurrentHP();
	debug_info.enemyMaxHP = m_enemyController.GetEnemyManager().GetFirstEnemy().GetMaxHP();
	debug_info.isEnemyFlinching = m_enemyController.GetEnemyManager().GetFirstEnemy().IsFlinching();
	debug_info.isEnemyKnockedDown = m_enemyController.GetEnemyManager().GetFirstEnemy().IsKnockedDown();
	debug_info.isHitMarkerActive = m_shotController.IsHitMarkerActive();
	debug_info.isCaptureEnabled = m_inputSystem.IsMouseCaptureEnabled();
	debug_info.isHitStopActive = m_timeScale.IsHitStopActive();
	debug_info.isSlowMotionActive = m_timeScale.IsSlowMotionActive();
	debug_info.hasIncomingAttack = m_dodgeJudge.HasIncomingAttack();
	debug_info.secondsUntilHit = m_dodgeJudge.GetSecondsUntilHit();
	debug_info.lastJudgeResult = m_lastJudgeResult;
	debug_info.isCollisionDebugVisible = m_collisionDebugView.IsVisible();
	debug_info.hasRayHit = m_collisionDebugView.HasRayHit();
	debug_info.rayHitLayer = m_collisionDebugView.GetRayHitInfo().layer;
	debug_info.rayHitDistance = m_collisionDebugView.GetRayHit().distance;
	GameDebugUi::Draw(debug_info);
}

// Sceneが所有する敵・敵の弾・範囲の予告・出口・Object・共有Renderer・HUD・Collision可視化・Collisionを終了する。
void GameScene::Finalize()
{
	// 遷移先のInit後に呼ばれても、遷移先が設定したCaptureやBGMを上書きしない。
	// 敵・敵の弾・範囲の予告・出口・Objectは、Collisionと共有Rendererを借用しているため、それらを解放する前に先に終了する。
	m_shotController.Uninit();
	m_playerCombat.Uninit();
	m_enemyController.Uninit();
	m_exitDoor.Uninit(m_collisionWorld);
	m_stageObjects.clear();
	m_modelRegistry.Uninit();

	// HUDと線描画Rendererを解放し、登録したColliderを空にする。
	m_gameHud.Uninit();
	m_collisionDebugView.Uninit();
	m_collisionWorld.Clear();
}

//========= 更新の補助関数=========
// 確認用の攻撃が当たる瞬間に、無敵かどうかと回避の開始からの経過時間で判定し、結果を反映する。
void GameScene::UpdateDodgeJudge(float worldDeltaTime)
{
	// 予告した攻撃が当たる瞬間だけ、結果が返る。それ以外の更新では、何も返らない。
	const DODGE_JUDGE_RESULT judge_result = m_dodgeJudge.Update(worldDeltaTime,m_playerController.IsInvincible(),
																															m_playerController.GetSecondsSinceDodgeStart());
	// 判定結果が出た場合だけ、表示用に記録する。
	if(judge_result != DODGE_JUDGE_RESULT::e_NONE){
		m_lastJudgeResult = judge_result;
	}

	// 命中した場合は、ダメージ・怯み・ヒットストップを与える。
	if(judge_result == DODGE_JUDGE_RESULT::e_HIT){
		m_playerCombat.ApplyHitReaction(COMBAT_TABLE::ns_SIMULATED_ATTACK_DAMAGE,COMBAT_TABLE::ns_DAMAGE_HIT_STOP_SECONDS);
	}
	// ジャスト回避の場合は、スローを開始する。連続した場合は、時間を数え直す。
	else if(judge_result == DODGE_JUDGE_RESULT::e_JUST_DODGED){
		m_timeScale.StartSlowMotion();
	}
}

// 敵の攻撃イベントを、プレイヤーへの被弾・スロー・表示用の記録へ反映する。
void GameScene::ApplyEnemyAttackEvent(const ENEMY_ATTACK_EVENT& attackEvent)
{
	// 表示用の記録が求められた場合だけ、直近の判定結果を更新する。接近型の結果は求めない。
	if(attackEvent.shouldRecordResult){
		m_lastJudgeResult = attackEvent.result;
	}

	// 無敵でなく命中した場合は、ダメージ・怯み・ヒットストップを与える。
	if(attackEvent.result == DODGE_JUDGE_RESULT::e_HIT){
		m_playerCombat.ApplyHitReaction(attackEvent.damage,COMBAT_TABLE::ns_DAMAGE_HIT_STOP_SECONDS);
	}
	// 実際に回避中のジャスト回避の場合は、スローを開始する。連続した場合は、時間を数え直す。
	else if(attackEvent.shouldStartSlowMotion){
		m_timeScale.StartSlowMotion();
	}
}

// 現在の部屋とウェーブをEnemyControllerへ生成させ、成功した場合だけ転倒の記録と進行状態を新しい編成に合わせる。
bool GameScene::TrySpawnCurrentWave()
{
	// 登録に失敗した場合は、EnemyControllerが失敗を記録し、EnemyManagerが生成済みの枠を解除して空にする。進行管理へ生成完了を通知せず、失敗を返す。
	if(!m_enemyController.TrySpawnWave(m_roomController.GetRoomNumber(),m_roomController.GetWaveNumber()))return false;

	// 新しい敵がスライディングの転倒記録を引き継がないよう初期化し、全滅判定を開始する。
	m_playerCombat.ClearSlideKnockdownRecord();
	m_roomController.NotifyWaveSpawned();
	return true;
}

// 生きている全ての敵を、残りHPと同じダメージで倒し、敵の予告・弾・範囲の予告を消す。
void GameScene::SkipCurrentWave()
{
	// 全枠を先頭から調べ、生きている敵だけを倒す。倒された敵のColliderは、ApplyDamageの中で解除される。
	for(Enemy& enemy : m_enemyController.GetEnemyManager().GetEnemies()){
		// 死亡中の敵は、倒す対象がないため次の枠へ進む。
		if(!enemy.IsAlive())continue;

		enemy.ApplyDamage(enemy.GetCurrentHP(),m_collisionWorld);
	}

	// 倒された敵の予告・飛行中の弾・範囲の予告を消す。全滅の判定は、次の更新で進行管理が行う。
	m_enemyController.ResetAttackState();
}

// 通過範囲に入ったプレイヤーを、次の部屋の開始位置へ移す。
void GameScene::UpdateExitPass()
{
	// 出口が閉じている、またはプレイヤーが通過範囲の外にいる場合は、何もしない。
	if(!m_exitDoor.IsInPassZone(m_playerController.GetPosition()))return;

	// 進行管理が出口の通過を受理しなかった場合は、何もしない。最終部屋の通過後や、受理済みの重複した通知では受理されない。
	if(!m_roomController.TryAdvanceRoom())return;

	// 次の部屋へ進んだので、プレイヤーを開始位置へ戻し、カメラ位置を合わせる。出口は、次の更新の進行状態の同期で閉じる。
	m_playerController.Initialize(ns_PLAYER_START_POSITION);
	m_FPSCamera.SetPosition(m_playerController.GetPosition());
}