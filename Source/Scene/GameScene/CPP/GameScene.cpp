#include "../H/GameScene.h"

// テストSceneの初期化、入力に応じた状態更新、射撃・近接攻撃・回避・回復・被弾の反映、部屋ごとの敵と敵の弾と範囲の予告と出口、設定表から生成した床・壁・画像のHUD・デバッグUIの描画、Resource終了を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_F1〜VK_F9とVK_F11の仮想キーコードで、Capture切替・タイトル遷移・可視化切替・最初の部屋からの再開・確認操作・デバッグ操作のキーを指定する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで残り時間を0未満にせず、std::minで視野角の補間割合を1以下に制限する。
#include <cmath> // std::sqrtで射撃型・範囲攻撃型とプレイヤーの水平距離を求め、std::fabsで範囲の中心からのずれの大きさを求める。
#include <cstdint> // std::uint32_tで、スライディングで転倒させた敵の記録のビットを扱う。
#include <vector> // 敵の弾の判定結果を、std::vectorで受け取る。

//========= 外部ライブラリ インクルード=========
#include <DirectXMath.h>

//========= Debug インクルード=========
#include "Debug/H/GameDebugUi.h"

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/Collision/H/CollisionMath.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/GameScene/H/EnemyApproach.h"
#include "Scene/GameScene/H/EnemyAreaBehavior.h"
#include "Scene/GameScene/H/EnemyRangedBehavior.h"
#include "Scene/Common/H/SceneManager.h"
#include "Scene/Common/H/SceneObjectFactory.h"
#include "Scene/Common/H/StageLayout.h"
#include "Scene/GameScene/H/CombatResolver.h"
#include "Scene/GameScene/H/CombatTable.h"
#include "Scene/GameScene/H/WeaponTable.h"
#include "Scene/TitleScene/H/TitleScene.h"

// このSceneの視野角、開始位置、射撃の表示に使用する固定設定をまとめる。戦闘の数値はCombatTableにある。
namespace
{
	//========= Projection設定=========
	// 通常時の垂直方向の視野角。単位は度。行列生成時にラジアンへ変換する。
	constexpr float ns_FOV_DEGREES = 60.0f;
	// 構えている時の垂直方向の視野角。単位は度。通常より狭くして、拡大して見える。企画書で未確定の仮値。
	constexpr float ns_AIM_FOV_DEGREES = 45.0f;
	// ダッシュ中の垂直方向の視野角。単位は度。通常より広くして、速さを出す。仮値。
	constexpr float ns_DASH_FOV_DEGREES = 66.0f;
	// スライディング中の垂直方向の視野角。単位は度。ダッシュよりさらに広くして、滑る速さと低さを出し、しゃがみとの違いを分かりやすくする。仮値。
	constexpr float ns_SLIDE_FOV_DEGREES = 78.0f;
	// しゃがみ中の垂直方向の視野角。単位は度。通常より少し狭くして、慎重な状態を示し、スライディングとの違いを分かりやすくする。仮値。
	constexpr float ns_CROUCH_FOV_DEGREES = 56.0f;
	// 視野角が目標値へ近づく速さ。1秒あたりに差の何倍を縮めるかを表し、大きいほど速い。仮値。
	constexpr float ns_FOV_SMOOTHING_RATE = 12.0f;
	// View空間での近距離Clip面の距離。単位はワールド距離単位。これより近い物は描画されない。
	constexpr float ns_NEAR_CLIP = 0.1f;
	// View空間での遠距離Clip面の距離。単位はワールド距離単位。これより遠い物は描画されない。
	constexpr float ns_FAR_CLIP = 1000.0f;

	//========= 配置の設定=========
	// プレイヤーの開始位置。ワールド座標(0,1.6,0)で、床の手前寄りの中央。Yは立っている時のカメラの高さ。壁の内側にある。
	constexpr XMFLOAT3 ns_PLAYER_START_POSITION{0.0f,1.6f,0.0f};

	//========= 射撃の表示設定=========
	// 命中しない場合に、弾道の線を伸ばす長さ。単位はワールド距離単位。
	constexpr float ns_SHOT_TRACE_DISPLAY_LENGTH = 30.0f;
	// 命中を示す表示を続ける時間。単位は秒。
	constexpr float ns_HIT_MARKER_SECONDS = 0.3f;
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
	m_weaponSystem.Initialize(WEAPON_TABLE::ns_ASSAULT_RIFLE);
	m_fovDegrees = ns_FOV_DEGREES;

	// 進行状態を第1部屋の第1ウェーブへ戻し、前回の生成失敗とデバッグ設定を引き継がない。
	m_roomController.Initialize();
	m_hasWaveSpawnFailed = false;
	m_isOneHitKillEnabled = false;

	// 判定・時間・接触・表示の状態を既定へ戻し、前回の生成ObjectとColliderを空にする。Rendererの生成、表からのObject生成、出口と敵の配置はInitで行う。
	m_dodgeJudge.Initialize();
	ResetEnemyMelee();
	m_enemyProjectiles.Clear();
	m_areaTelegraph.Clear();
	m_lastJudgeResult = DODGE_JUDGE_RESULT::e_NONE;
	m_slowMotionRemainingSeconds = 0.0f;
	m_hitStopRemainingSeconds = 0.0f;
	m_contactCooldownRemainingSeconds = 0.0f;
	m_slideKnockedDownEnemyMask = 0;
	m_previousActionState = PLAYER_ACTION_STATE::e_NORMAL;
	m_hitMarkerRemainingSeconds = 0.0f;
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

	// 敵の弾の描画に使うCube Rendererを借用する。借用できなければ、生成済みのResourceを含めて終了して失敗を返す。
	if(!m_enemyProjectiles.Initialize(m_modelRegistry)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの敵の弾の初期化に失敗しました。");
		Finalize();
		return false;
	}

	// 範囲の予告の描画に使うCube Rendererを借用する。借用できなければ、生成済みのResourceを含めて終了して失敗を返す。
	if(!m_areaTelegraph.Initialize(m_modelRegistry)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの範囲の予告の初期化に失敗しました。");
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
		ResetEnemyMelee();
		m_enemyProjectiles.Clear();
		m_areaTelegraph.Clear();
		m_slideKnockedDownEnemyMask = 0;
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
		m_isOneHitKillEnabled = !m_isOneHitKillEnabled;
	}

	// F11が押された瞬間は、デバッグ用に、現在のウェーブの敵を全て倒して、次のウェーブまたは出口の開放へ進める。
	if(m_inputSystem.IsKeyTriggered(VK_F11)){
		SkipCurrentWave();
	}

	// ヒットストップ中かを、この更新の開始時点の残り時間で決め、残り時間を実時間で減らす。ヒットストップ中は、プレイヤーと敵の時間を止める。
	const bool is_hit_stopped = m_hitStopRemainingSeconds > 0.0f;
	m_hitStopRemainingSeconds = std::max(0.0f,m_hitStopRemainingSeconds - deltaTime);

	// スローの残り時間を実時間で減らし、スロー中ならプレイヤー以外の時間の進みを遅くする。プレイヤーと武器は、スローの影響を受けない。
	m_slowMotionRemainingSeconds = std::max(0.0f,m_slowMotionRemainingSeconds - deltaTime);
	const float slowed_delta_time = m_slowMotionRemainingSeconds > 0.0f ? deltaTime * COMBAT_TABLE::ns_SLOW_MOTION_SCALE : deltaTime;

	// プレイヤー側の時間と、敵・判定側の時間を決める。ヒットストップ中は、どちらも0にする。
	const float gameplay_delta_time = is_hit_stopped ? 0.0f : deltaTime;
	const float world_delta_time = is_hit_stopped ? 0.0f : slowed_delta_time;

	// 確認用の攻撃の判定、敵の怯み・転倒、回復アイテムの間隔、表示用の残り時間を、Captureに関係なく進める。表示用は実時間で、0未満にしない。
	UpdateDodgeJudge(world_delta_time);
	m_enemyManager.Update(world_delta_time);

	// 生成待ちなら現在のウェーブを生成する。失敗した場合は再試行せず、F4で最初からやり直すまで敵なしで待つ。
	if(m_roomController.GetPhase() == ROOM_PHASE::e_NEEDS_SPAWN && !m_hasWaveSpawnFailed){
		m_hasWaveSpawnFailed = !TrySpawnCurrentWave();
	}
	// 生存中の敵がいなければウェーブまたは部屋を進める。生成待ちの間は、空の敵一覧を全滅と扱わない。
	m_roomController.Update(m_enemyManager.HasLivingEnemies());

	// 部屋の進行状態に合わせて、出口を開閉する。出口開放中と攻略完了後は開き、それ以外は閉じる。F4や部屋の移動で戻った場合も、この更新で閉じる。
	const ROOM_PHASE room_phase = m_roomController.GetPhase();
	m_exitDoor.SetOpen(room_phase == ROOM_PHASE::e_EXIT_OPEN || room_phase == ROOM_PHASE::e_COMPLETE,m_collisionWorld);

	m_healItems.Update(gameplay_delta_time);
	m_collisionDebugView.Update(deltaTime);
	m_hitMarkerRemainingSeconds = std::max(0.0f,m_hitMarkerRemainingSeconds - deltaTime);

	// 反動によるカメラのずれを、実時間で目標へ滑らかに近づけ、射撃をやめて少し経つと、元の視線へ戻す。Captureに関係なく進める。
	m_FPSCamera.UpdateRecoil(deltaTime);

	// マウスを解放している間は、カメラ・移動・戦闘の操作更新をまとめて停止し、ここで戻る。
	if(!m_inputSystem.IsMouseCaptureEnabled())return;

	// 視線を先に更新し、その向きで、蹴れる敵がいるかを調べてから、プレイヤーを更新する。ヒットストップ中も視線は動かせる。
	m_FPSCamera.Update(m_inputSystem);
	const bool can_kick = m_enemyManager.FindMeleeTarget(m_FPSCamera.GetPosition(),m_FPSCamera.GetForward(),true) != nullptr;
	m_playerController.Update(gameplay_delta_time,m_inputSystem,m_FPSCamera,m_collisionWorld,can_kick);
	m_FPSCamera.SetPosition(m_playerController.GetPosition());

	// 移動後の位置で、出口の通過を判定する。通過した場合は、プレイヤーが開始位置へ戻るため、以降の更新は新しい位置から行う。
	UpdateExitPass();

	// 現在の動作の状態を取得する。
	const PLAYER_ACTION_STATE action_state = m_playerController.GetActionState();
	// 殴打を始めた更新では、リロードをキャンセルする。弾は補充しない。殴打の開始は、1回前の状態が殴打ではなかったことで検出する。
	if(action_state == PLAYER_ACTION_STATE::e_PUNCH && m_previousActionState != PLAYER_ACTION_STATE::e_PUNCH){
		m_weaponSystem.CancelReload();
	}
	m_previousActionState = action_state;

	// 動作の状態に応じた視野角の目標値へ、滑らかに近づける。スライディングが最も広く、次にダッシュ。構えは狭く、しゃがみは少し狭い。それ以外は通常。
	// 1回の更新で差を超えないよう、近づく割合は1以下に制限する。
	float target_fov_degrees = ns_FOV_DEGREES;
	// スライディング中は、視野角を最も広げる。
	if(action_state == PLAYER_ACTION_STATE::e_SLIDE){
		target_fov_degrees = ns_SLIDE_FOV_DEGREES;
	}
	// ダッシュ中は、視野角を広げる。
	else if(action_state == PLAYER_ACTION_STATE::e_DASH){
		target_fov_degrees = ns_DASH_FOV_DEGREES;
	}
	// 構えている間は、視野角を狭める。しゃがみ中でも、構えを優先する。
	else if(m_playerController.IsAiming()){
		target_fov_degrees = ns_AIM_FOV_DEGREES;
	}
	// しゃがみ中は、視野角を少し狭める。
	else if(action_state == PLAYER_ACTION_STATE::e_CROUCH){
		target_fov_degrees = ns_CROUCH_FOV_DEGREES;
	}
	const float fov_blend = std::min(1.0f,ns_FOV_SMOOTHING_RATE * deltaTime);
	m_fovDegrees += (target_fov_degrees - m_fovDegrees) * fov_blend;

	// 近接攻撃・接触・回復・射撃を、この順に更新する。
	UpdateMelee();
	UpdateContact(gameplay_delta_time);
	UpdateHeal();
	UpdateShooting(gameplay_delta_time);
	// 当フレームの回避・移動・敵への怯みを反映してから、敵の行動と、敵の弾の命中を判定する。
	UpdateEnemyActions(world_delta_time);
	UpdateEnemyProjectiles(world_delta_time);

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
	const XMMATRIX projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_fovDegrees),aspect_ratio,ns_NEAR_CLIP,ns_FAR_CLIP);
	const XMMATRIX view_matrix = m_FPSCamera.GetViewMatrix();

	// 床・壁・範囲の予告・出口・敵・敵の弾はOPAQUE Passで描画するため、描画前にPassを設定する。
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);

	// 生成済みの全Objectを、表の順（床→左壁→右壁→手前壁→奥壁）に描画する。
	for(const SceneObject& object : m_stageObjects){
		object.Draw(m_graphicsSystem,view_matrix,projection_matrix);
	}

	// 床の上に、範囲の予告を描画する。床の後に描き、敵や出口より先に描くことで、敵の体が予告の上に重なる。
	m_areaTelegraph.Draw(m_graphicsSystem,view_matrix,projection_matrix);
	// 続けて出口を描画する。
	m_exitDoor.Draw(m_graphicsSystem,view_matrix,projection_matrix);
	// 続けて全枠の敵を出現順に描画する。死亡枠はEnemy::Drawが描画を省略する。
	for(const Enemy& enemy : m_enemyManager.GetEnemies()){
		enemy.Draw(m_graphicsSystem,view_matrix,projection_matrix);
	}
	// 敵の後に、飛行中の敵の弾を描画し、続けて表示中ならCollisionの線を重ねる。
	m_enemyProjectiles.Draw(m_graphicsSystem,view_matrix,projection_matrix);
	m_collisionDebugView.Draw(m_graphicsSystem,m_collisionWorld,m_FPSCamera,view_matrix,projection_matrix);

	// 3Dの上に画像のHUDを重ねるため、深度比較なし・アルファ合成の画面UI用Passへ切り替え、現在の値をHUDへ渡して描画する。
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);
	GAME_HUD_STATE hud_state{};
	hud_state.currentHP = m_playerHealth.GetCurrentHP();
	hud_state.maxHP = m_playerHealth.GetMaxHP();
	hud_state.healItemCount = m_healItems.GetCount();
	hud_state.magazineAmmo = m_weaponSystem.GetMagazineAmmo();
	hud_state.magazineSize = m_weaponSystem.GetMagazineSize();
	hud_state.reserveAmmo = m_weaponSystem.GetReserveAmmo();
	hud_state.isReloading = m_weaponSystem.IsReloading();
	hud_state.reloadProgress = m_weaponSystem.GetReloadProgress();
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
	debug_info.isOneHitKillEnabled = m_isOneHitKillEnabled;
	debug_info.isEnemyAlive = m_enemyManager.GetFirstEnemy().IsAlive();
	debug_info.enemyHP = m_enemyManager.GetFirstEnemy().GetCurrentHP();
	debug_info.enemyMaxHP = m_enemyManager.GetFirstEnemy().GetMaxHP();
	debug_info.isEnemyFlinching = m_enemyManager.GetFirstEnemy().IsFlinching();
	debug_info.isEnemyKnockedDown = m_enemyManager.GetFirstEnemy().IsKnockedDown();
	debug_info.isHitMarkerActive = m_hitMarkerRemainingSeconds > 0.0f;
	debug_info.isCaptureEnabled = m_inputSystem.IsMouseCaptureEnabled();
	debug_info.isHitStopActive = m_hitStopRemainingSeconds > 0.0f;
	debug_info.isSlowMotionActive = m_slowMotionRemainingSeconds > 0.0f;
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
	m_enemyManager.Clear(m_collisionWorld);
	m_enemyProjectiles.Uninit();
	m_areaTelegraph.Uninit();
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
		ApplyPlayerHitReaction(COMBAT_TABLE::ns_SIMULATED_ATTACK_DAMAGE,COMBAT_TABLE::ns_DAMAGE_HIT_STOP_SECONDS);
	}
	// ジャスト回避の場合は、スローを開始する。連続した場合は、時間を数え直す。
	else if(judge_result == DODGE_JUDGE_RESULT::e_JUST_DODGED){
		m_slowMotionRemainingSeconds = COMBAT_TABLE::ns_SLOW_MOTION_SECONDS;
	}
}

// 全枠を出現順に、種類に応じた更新へ振り分け、更新の後に予告中の表示を敵へ反映する。
void GameScene::UpdateEnemyActions(float worldDeltaTime)
{
	// 全枠を先頭から更新する。死亡枠や行動不能の敵は、枠ごとの更新の中で予告を中断する。
	for(std::size_t slot_index = 0; slot_index < m_enemyMeleeJudges.size(); ++slot_index){
		// 敵の種類ごとに、対応する更新を呼ぶ。
		switch(m_enemyManager.GetEnemies()[slot_index].GetKind()){
			// 接近型は、追従と近接の攻撃予告・命中を更新する。
		case ENEMY_KIND::e_MELEE:
		UpdateEnemyMeleeSlot(slot_index,worldDeltaTime);
		break;

		// 射撃型は、距離維持の移動と、発射の予告と、弾の生成を更新する。
		case ENEMY_KIND::e_RANGED:
		UpdateEnemyRangedSlot(slot_index,worldDeltaTime);
		break;

		// 範囲攻撃型は、接近と、床の範囲の予告と、予告の終わりの範囲の判定を更新する。
		case ENEMY_KIND::e_AREA:
		UpdateEnemyAreaSlot(slot_index,worldDeltaTime);
		break;
		}

		// 予告の状態を、その敵の表示へ反映する。死亡・怯みなどで予告が中断された場合は、解除される。
		m_enemyManager.GetEnemies()[slot_index].SetTelegraphing(m_enemyMeleeJudges[slot_index].HasIncomingAttack());
	}
}

// 接近型の敵だけ、予告中は追従を止め、予告が終わったフレームで一度だけ命中を判定する。
void GameScene::UpdateEnemyMeleeSlot(std::size_t slotIndex,float worldDeltaTime)
{
	// この枠の敵と、その敵専用の予告・再攻撃待ちを参照する。
	Enemy& enemy = m_enemyManager.GetEnemies()[slotIndex];
	JustDodgeJudge& judge = m_enemyMeleeJudges[slotIndex];
	float& recovery_seconds = m_enemyMeleeRecoverySeconds[slotIndex];

	// 戦闘不能になった敵の予告は、時間停止中でも取り消して持ち越さない。
	if(!enemy.IsAlive() || enemy.IsFlinching() || enemy.IsKnockedDown() || m_playerHealth.IsDead()){
		// 予告中断時だけ再攻撃待ちを設定し、行動不能が続いても毎回は設定しない。
		if(judge.HasIncomingAttack()){
			judge.Initialize();
			recovery_seconds = COMBAT_TABLE::ns_ENEMY_MELEE_RECOVERY_SECONDS;
		}
		return;
	}

	// ヒットストップで敵側の時間が進まない場合は、新しい予告も始めない。
	if(worldDeltaTime <= 0.0f)return;

	// 追従の停止距離と同じXZ平面で測る。二乗同士を比較し、平方根を取らずに範囲を求める。
	const XMFLOAT3 enemy_position = enemy.GetBodySphere().center;
	const XMFLOAT3& player_position = m_playerController.GetPosition();
	const float offset_x = player_position.x - enemy_position.x;
	const float offset_z = player_position.z - enemy_position.z;
	const float reach = COMBAT_TABLE::ns_ENEMY_MELEE_REACH;
	const bool is_in_reach = offset_x * offset_x + offset_z * offset_z <= reach * reach;

	// 予告中は追従せず、判定が返るまで敵側の時間で予告を進める。
	if(judge.HasIncomingAttack()){
		// 回避以外の無敵も被弾を防ぐが、回避開始時刻としては使わない。
		const bool is_dodging = m_playerController.GetActionState() == PLAYER_ACTION_STATE::e_DODGE;
		const float dodge_age_seconds = is_dodging ? m_playerController.GetSecondsSinceDodgeStart() : COMBAT_TABLE::ns_ENEMY_MELEE_RECOVERY_SECONDS;
		const bool is_invincible = m_playerController.IsInvincible();
		const DODGE_JUDGE_RESULT result = judge.Update(worldDeltaTime,is_invincible,dodge_age_seconds);
		// 予告が続いている間は、命中も再攻撃待ちの開始も行わない。
		if(result == DODGE_JUDGE_RESULT::e_NONE)return;

		// 予告を消費したので、範囲外の空振りでも再攻撃待ちに入る。
		recovery_seconds = COMBAT_TABLE::ns_ENEMY_MELEE_RECOVERY_SECONDS;
		// 予告終了時点で範囲外なら、被弾もジャスト回避のスローも発生しない。
		if(!is_in_reach)return;

		// 範囲内で無敵でなければ、既存の被弾反応にHP・怯み・ヒットストップを任せる。
		if(result == DODGE_JUDGE_RESULT::e_HIT){
			ApplyPlayerHitReaction(COMBAT_TABLE::ns_ENEMY_MELEE_DAMAGE,COMBAT_TABLE::ns_DAMAGE_HIT_STOP_SECONDS);
		}
		// 実際に回避中のジャスト回避だけがスローを開始する。
		else if(is_dodging && result == DODGE_JUDGE_RESULT::e_JUST_DODGED){
			m_slowMotionRemainingSeconds = COMBAT_TABLE::ns_SLOW_MOTION_SECONDS;
		}
		return;
	}

	// 再攻撃待ちは敵側の時間で減らす。残り時間が負にならないようにする。
	recovery_seconds = std::max(0.0f,recovery_seconds - worldDeltaTime);
	// 射程外では追従だけを行い、予告の開始は次の更新で判定する。
	if(!is_in_reach){
		ENEMY_APPROACH::Update(enemy,player_position,worldDeltaTime,m_collisionWorld);
		return;
	}

	// 射程内では待ち時間が終わってから、次の攻撃を予告する。
	if(recovery_seconds <= 0.0f){
		judge.StartIncomingAttack(COMBAT_TABLE::ns_ENEMY_MELEE_WINDUP_SECONDS);
	}
}

// 予告中は移動せず、予告が終わったフレームで弾を生成する。予告以外では、距離を保つ。
void GameScene::UpdateEnemyRangedSlot(std::size_t slotIndex,float worldDeltaTime)
{
	// この枠の敵と、その敵専用の予告・再攻撃待ちを参照する。
	Enemy& enemy = m_enemyManager.GetEnemies()[slotIndex];
	JustDodgeJudge& judge = m_enemyMeleeJudges[slotIndex];
	float& recovery_seconds = m_enemyMeleeRecoverySeconds[slotIndex];

	// 戦闘不能になった敵の予告は、時間停止中でも取り消して持ち越さない。
	if(!enemy.IsAlive() || enemy.IsFlinching() || enemy.IsKnockedDown() || m_playerHealth.IsDead()){
		// 予告中断時だけ再攻撃待ちを設定し、行動不能が続いても毎回は設定しない。
		if(judge.HasIncomingAttack()){
			judge.Initialize();
			recovery_seconds = ENEMY_RANGED::ns_RECOVERY_SECONDS;
		}
		return;
	}

	// ヒットストップで敵側の時間が進まない場合は、移動も新しい予告も始めない。
	if(worldDeltaTime <= 0.0f)return;

	// 敵とプレイヤーの水平距離を求める。高さは使わない。
	const XMFLOAT3 enemy_position = enemy.GetBodySphere().center;
	const XMFLOAT3& player_position = m_playerController.GetPosition();
	const float offset_x = player_position.x - enemy_position.x;
	const float offset_z = player_position.z - enemy_position.z;
	const float distance = std::sqrt(offset_x * offset_x + offset_z * offset_z);

	// 予告中はその場に立ち止まり、予告の終わりだけ、発射のタイミングに達した旨を受け取る。
	if(judge.HasIncomingAttack()){
		// 射撃型は回避の判定を使わないため、無敵でない状態と、十分に大きい回避からの経過時間を渡し、予告の終わりだけを検出する。
		const DODGE_JUDGE_RESULT result = judge.Update(worldDeltaTime,false,ENEMY_RANGED::ns_UNUSED_DODGE_AGE_SECONDS);
		// 予告が続いている間は、何も起こさない。
		if(result == DODGE_JUDGE_RESULT::e_NONE)return;

		// 予告の終わりで、再攻撃待ちに入り、予告の終わりのプレイヤーの位置へ向けて、弾を1つ生成する。弾は、以後追尾しない。
		recovery_seconds = ENEMY_RANGED::ns_RECOVERY_SECONDS;
		m_enemyProjectiles.Spawn(enemy_position,player_position);
		return;
	}

	// 再攻撃待ちは敵側の時間で減らす。残り時間が負にならないようにする。
	recovery_seconds = std::max(0.0f,recovery_seconds - worldDeltaTime);

	// 遠すぎる間は近づき、近すぎる間は離れる。移動した更新では、予告を始めない。
	if(distance > ENEMY_RANGED::ns_FAR_DISTANCE){
		ENEMY_RANGED::Move(enemy,player_position,1.0f,worldDeltaTime,m_collisionWorld);
		return;
	}
	// 近すぎる場合は、プレイヤーから離れる。
	if(distance < ENEMY_RANGED::ns_NEAR_DISTANCE){
		ENEMY_RANGED::Move(enemy,player_position,-1.0f,worldDeltaTime,m_collisionWorld);
		return;
	}

	// 適切な距離にいて、再攻撃待ちが終わっていれば、立ち止まったまま、次の攻撃を予告する。
	if(recovery_seconds <= 0.0f){
		judge.StartIncomingAttack(ENEMY_RANGED::ns_WINDUP_SECONDS);
	}
}

// 予告中は移動せず、予告の終わりに、固定した範囲の内にいるプレイヤーへ判定する。予告以外では、攻撃を始める距離まで近づく。
void GameScene::UpdateEnemyAreaSlot(std::size_t slotIndex,float worldDeltaTime)
{
	// この枠の敵と、その敵専用の予告・再攻撃待ちを参照する。
	Enemy& enemy = m_enemyManager.GetEnemies()[slotIndex];
	JustDodgeJudge& judge = m_enemyMeleeJudges[slotIndex];
	float& recovery_seconds = m_enemyMeleeRecoverySeconds[slotIndex];

	// 戦闘不能になった敵の予告と、床の範囲の表示は、時間停止中でも取り消して持ち越さない。
	if(!enemy.IsAlive() || enemy.IsFlinching() || enemy.IsKnockedDown() || m_playerHealth.IsDead()){
		// 予告中断時だけ再攻撃待ちを設定し、行動不能が続いても毎回は設定しない。
		if(judge.HasIncomingAttack()){
			judge.Initialize();
			recovery_seconds = ENEMY_AREA::ns_RECOVERY_SECONDS;
		}
		m_areaTelegraph.End(slotIndex);
		return;
	}

	// ヒットストップで敵側の時間が進まない場合は、移動も新しい予告も始めない。
	if(worldDeltaTime <= 0.0f)return;

	// 敵とプレイヤーの位置を取得する。範囲の判定にも、接近の判断にも使う。
	const XMFLOAT3 enemy_position = enemy.GetBodySphere().center;
	const XMFLOAT3& player_position = m_playerController.GetPosition();

	// 予告中はその場に立ち止まり、予告の終わりだけを受け取る。範囲の中心は、予告を始めた時点で固定した位置のままにする。
	if(judge.HasIncomingAttack()){
		// 回避以外の無敵も被弾を防ぐが、回避開始時刻としては使わない。
		const bool is_dodging = m_playerController.GetActionState() == PLAYER_ACTION_STATE::e_DODGE;
		const float dodge_age_seconds = is_dodging ? m_playerController.GetSecondsSinceDodgeStart() : ENEMY_AREA::ns_NON_DODGE_AGE_SECONDS;
		const DODGE_JUDGE_RESULT result = judge.Update(worldDeltaTime,m_playerController.IsInvincible(),dodge_age_seconds);
		// 予告が続いている間は、範囲を表示したまま、何も起こさない。
		if(result == DODGE_JUDGE_RESULT::e_NONE)return;

		// 予告の終わりで、再攻撃待ちに入り、固定した範囲の中心を取り出して、範囲の表示を終える。
		recovery_seconds = ENEMY_AREA::ns_RECOVERY_SECONDS;
		const XMFLOAT3 area_center = m_areaTelegraph.GetCenter(slotIndex);
		m_areaTelegraph.End(slotIndex);

		// 範囲は正方形で、高さを使わない。中心からX・Z方向とも半分の大きさ以内なら、範囲の内とする。しゃがんでも範囲の内にいれば、避けられない。
		const bool is_in_area = std::fabs(player_position.x - area_center.x) <= AREA_TELEGRAPH::ns_HALF_SIZE &&
			std::fabs(player_position.z - area_center.z) <= AREA_TELEGRAPH::ns_HALF_SIZE;
		// 範囲の外にいる場合は、被弾もジャスト回避のスローも発生しない。
		if(!is_in_area)return;

		// 範囲の内にいた場合の結果を、表示用に記録する。
		m_lastJudgeResult = result;
		// 無敵でなければ、命中として、ダメージ・怯み・ヒットストップを与える。
		if(result == DODGE_JUDGE_RESULT::e_HIT){
			ApplyPlayerHitReaction(ENEMY_AREA::ns_DAMAGE,COMBAT_TABLE::ns_DAMAGE_HIT_STOP_SECONDS);
		}
		// 実際に回避中のジャスト回避だけがスローを開始する。
		else if(is_dodging && result == DODGE_JUDGE_RESULT::e_JUST_DODGED){
			m_slowMotionRemainingSeconds = COMBAT_TABLE::ns_SLOW_MOTION_SECONDS;
		}
		return;
	}

	// 再攻撃待ちは敵側の時間で減らす。残り時間が負にならないようにする。
	recovery_seconds = std::max(0.0f,recovery_seconds - worldDeltaTime);

	// 敵とプレイヤーの水平距離を求める。高さは使わない。
	const float offset_x = player_position.x - enemy_position.x;
	const float offset_z = player_position.z - enemy_position.z;
	const float distance = std::sqrt(offset_x * offset_x + offset_z * offset_z);

	// 攻撃を始める距離より遠い間は近づく。近づいた更新では、予告を始めない。
	if(distance > ENEMY_AREA::ns_ENGAGE_DISTANCE){
		ENEMY_AREA::Approach(enemy,player_position,worldDeltaTime,m_collisionWorld);
		return;
	}

	// 攻撃を始める距離の内で、再攻撃待ちが終わっていれば、プレイヤーのいる位置へ範囲を固定して予告を始める。範囲は、プレイヤーが動いても追わない。
	if(recovery_seconds <= 0.0f){
		m_areaTelegraph.Begin(slotIndex,player_position);
		judge.StartIncomingAttack(ENEMY_AREA::ns_WINDUP_SECONDS);
	}
}

// 弾を進め、プレイヤーへ命中・回避した弾の結果を、ダメージ・スロー・表示用の記録へ反映する。
void GameScene::UpdateEnemyProjectiles(float worldDeltaTime)
{
	// 回避の動作中だけ、回避の開始からの経過時間がジャスト回避の判定に使われる。蹴り・殴打の無敵は、通常の回避として扱う。
	const bool is_dodging = m_playerController.GetActionState() == PLAYER_ACTION_STATE::e_DODGE;

	// 弾を進め、今回の更新で命中・回避した弾ごとの判定結果を受け取る。
	std::vector<DODGE_JUDGE_RESULT> results;
	m_enemyProjectiles.Update(worldDeltaTime,m_collisionWorld,m_playerController.GetBodySphere(),m_playerController.IsInvincible(),is_dodging,
														m_playerController.GetSecondsSinceDodgeStart(),results);

	// 受け取った結果を、出た順に反映する。
	for(const DODGE_JUDGE_RESULT result : results){
		m_lastJudgeResult = result;

		// 無敵でなく命中した場合は、ダメージ・怯み・ヒットストップを与える。
		if(result == DODGE_JUDGE_RESULT::e_HIT){
			ApplyPlayerHitReaction(ENEMY_PROJECTILE::ns_DAMAGE,COMBAT_TABLE::ns_DAMAGE_HIT_STOP_SECONDS);
		}
		// ジャスト回避の場合は、スローを開始する。連続した場合は、時間を数え直す。
		else if(result == DODGE_JUDGE_RESULT::e_JUST_DODGED){
			m_slowMotionRemainingSeconds = COMBAT_TABLE::ns_SLOW_MOTION_SECONDS;
		}
	}
}

// 全枠の予告と再攻撃待ちを初期状態へ戻す。
void GameScene::ResetEnemyMelee()
{
	// 全枠の攻撃予告を破棄し、再配置後の敵が旧予告で攻撃しないようにする。
	for(JustDodgeJudge& judge : m_enemyMeleeJudges){
		judge.Initialize();
	}
	m_enemyMeleeRecoverySeconds.fill(0.0f);
}

// 現在の部屋とウェーブを生成し、成功した場合だけ敵の攻撃状態と進行状態を新しい編成に合わせる。
bool GameScene::TrySpawnCurrentWave()
{
	// 登録に失敗した場合は、EnemyManagerが生成済みの枠を解除して空にする。進行管理へ生成完了を通知せず、失敗を返す。
	if(!m_enemyManager.SpawnWave(m_roomController.GetRoomNumber(),m_roomController.GetWaveNumber(),m_modelRegistry,m_collisionWorld)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの部屋ウェーブの生成に失敗しました。");
		return false;
	}

	// 新しい敵が旧予告・旧ウェーブの弾と範囲の予告・スライディングの転倒記録を引き継がないよう初期化し、全滅判定を開始する。
	ResetEnemyMelee();
	m_enemyProjectiles.Clear();
	m_areaTelegraph.Clear();
	m_slideKnockedDownEnemyMask = 0;
	m_roomController.NotifyWaveSpawned();
	return true;
}

// 生きている全ての敵を、残りHPと同じダメージで倒し、敵の予告・弾・範囲の予告を消す。
void GameScene::SkipCurrentWave()
{
	// 全枠を先頭から調べ、生きている敵だけを倒す。倒された敵のColliderは、ApplyDamageの中で解除される。
	for(Enemy& enemy : m_enemyManager.GetEnemies()){
		// 死亡中の敵は、倒す対象がないため次の枠へ進む。
		if(!enemy.IsAlive())continue;

		enemy.ApplyDamage(enemy.GetCurrentHP(),m_collisionWorld);
	}

	// 倒された敵の予告・飛行中の弾・範囲の予告を消す。全滅の判定は、次の更新で進行管理が行う。
	ResetEnemyMelee();
	m_enemyProjectiles.Clear();
	m_areaTelegraph.Clear();
}

// 一撃で倒す設定が有効なら敵の残りHPを、無効なら基本のダメージを返す。
float GameScene::GetPlayerAttackDamage(float baseDamage,const Enemy& target) const
{
	return m_isOneHitKillEnabled ? target.GetCurrentHP() : baseDamage;
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

// 被弾・接触の反応として、ダメージ（あれば）・怯み・ヒットストップを与える。
void GameScene::ApplyPlayerHitReaction(float damage,float hitStopSeconds)
{
	// ダメージが正の場合だけHPを減らす。接触は0のため、HPは変わらない。
	if(damage > 0.0f){
		m_playerHealth.ApplyDamage(damage);
	}

	// プレイヤーを怯ませ、ヒットストップを設定する。すでに残っている場合は、長い方を使う。
	m_playerController.ApplyFlinch(COMBAT_TABLE::ns_PLAYER_FLINCH_SECONDS);
	m_hitStopRemainingSeconds = std::max(m_hitStopRemainingSeconds,hitStopSeconds);
}

// 近接攻撃の当たりを取り出し、範囲内の敵へ効果を与える。
void GameScene::UpdateMelee()
{
	// 当たりの種類の出力先を用意する。
	PLAYER_MELEE_TYPE melee_type{};
	// 当たりが発生していなければ、何もしない。
	if(!m_playerController.TryConsumeMeleeImpact(melee_type))return;

	// 当たる瞬間に、視点から射程と範囲の内で最も近い生存中の敵を選ぶ。いなければ空振りとして何もしない。
	Enemy* const target_enemy = m_enemyManager.FindMeleeTarget(m_FPSCamera.GetPosition(),m_FPSCamera.GetForward(),false);
	// 対象がいない場合は、空振りとして効果を与えずに戻る。
	if(target_enemy == nullptr)return;

	// 蹴りはダメージだけを与える。殴打はダメージを与えた後に、転倒させる。ダメージで死亡した場合、転倒は何も起こらない。
	if(melee_type == PLAYER_MELEE_TYPE::e_KICK){
		target_enemy->ApplyDamage(GetPlayerAttackDamage(COMBAT_TABLE::ns_KICK_DAMAGE,*target_enemy),m_collisionWorld);
	}
	// 殴打の場合は、ダメージを与えた後に転倒させる。範囲攻撃型は、転倒しない。
	else{
		target_enemy->ApplyDamage(GetPlayerAttackDamage(COMBAT_TABLE::ns_PUNCH_DAMAGE,*target_enemy),m_collisionWorld);
		target_enemy->ApplyKnockdown();
	}

	// 当たった手応えとして、短いヒットストップを与える。
	m_hitStopRemainingSeconds = std::max(m_hitStopRemainingSeconds,COMBAT_TABLE::ns_MELEE_HIT_STOP_SECONDS);
}

// 敵との接触を調べ、スライディングなら転倒、それ以外は条件を満たす場合に接触の反応を与える。
void GameScene::UpdateContact(float gameplayDeltaTime)
{
	// 接触の反応の間隔を進める。0未満にはしない。
	m_contactCooldownRemainingSeconds = std::max(0.0f,m_contactCooldownRemainingSeconds - gameplayDeltaTime);

	// 現在の動作の状態を取得する。
	const PLAYER_ACTION_STATE action_state = m_playerController.GetActionState();
	// スライディングでなくなったら、次のスライディングで再び転倒させられるよう、記録を戻す。
	if(action_state != PLAYER_ACTION_STATE::e_SLIDE){
		m_slideKnockedDownEnemyMask = 0;
	}

	// 全枠を出現順に調べ、体が重なった敵ごとに、スライディングの転倒か接触の反応を判定する。
	for(Enemy& enemy : m_enemyManager.GetEnemies()){
		// 死亡中、または体が重なっていない敵は、接触していないため次の枠へ進む。
		if(!enemy.IsAlive() || !CollisionMath::IntersectSphereSphere(m_playerController.GetBodySphere(),enemy.GetBodySphere()))continue;

		// スライディング中は、この敵をまだ転倒させていない場合だけ転倒させる。接触の反応は与えず、他の敵の判定を続ける。範囲攻撃型は、転倒しない。
		if(action_state == PLAYER_ACTION_STATE::e_SLIDE){
			const std::uint32_t enemy_bit = 1u << enemy.GetEnemyId();
			// 未記録で、実際に転倒させられた場合だけ、この敵への記録を付ける。すでに転倒中や、転倒しない種類なら何もしない。
			if((m_slideKnockedDownEnemyMask & enemy_bit) == 0 && enemy.ApplyKnockdown()){
				m_slideKnockedDownEnemyMask |= enemy_bit;
			}
			continue;
		}

		// 殴打中・蹴り中・無敵中・敵が転倒中・反応の間隔が残っている間は、接触の反応を与えない。
		if(action_state == PLAYER_ACTION_STATE::e_PUNCH || action_state == PLAYER_ACTION_STATE::e_KICK || m_playerController.IsInvincible() ||
			 enemy.IsKnockedDown() || m_contactCooldownRemainingSeconds > 0.0f)continue;

		// 条件を満たした接触は、ダメージなしの怯みとヒットストップを与え、間隔を設定する。以後の枠は、間隔が残るため反応しない。
		ApplyPlayerHitReaction(0.0f,COMBAT_TABLE::ns_CONTACT_HIT_STOP_SECONDS);
		m_contactCooldownRemainingSeconds = COMBAT_TABLE::ns_CONTACT_COOLDOWN_SECONDS;
	}
}

// 射撃・リロードを更新し、敵へ命中していればダメージを与える。
void GameScene::UpdateShooting(float gameplayDeltaTime)
{
	// 射撃を更新する。リロード・連射間隔・弾薬の処理と、命中判定は武器が行い、ダメージの反映だけをここで行う。回避・蹴り・殴打の間は射撃できない。
	const WEAPON_UPDATE_RESULT weapon_result = m_weaponSystem.Update(gameplayDeltaTime,m_inputSystem,m_playerController.IsAiming(),
																																	 m_playerController.IsShootingAllowed(),m_FPSCamera,m_collisionWorld);

	// 弾を発射した場合は、弾道の終点を可視化へ伝える。命中していれば命中点、していなければ弾の方向へ表示用の長さだけ進んだ点。
	if(weapon_result.didFire){
		const XMFLOAT3& shot_origin = weapon_result.shotRay.origin;
		const XMFLOAT3& shot_direction = weapon_result.shotRay.direction;
		// 命中した場合は、終点を命中点にする。
		if(weapon_result.didHit){
			m_collisionDebugView.NotifyShot(weapon_result.rayHit.point);
		}
		// 命中しなかった場合は、弾の方向へ表示用の長さだけ進んだ点を終点にする。
		else{
			m_collisionDebugView.NotifyShot(XMFLOAT3{shot_origin.x + shot_direction.x * ns_SHOT_TRACE_DISPLAY_LENGTH,
																							 shot_origin.y + shot_direction.y * ns_SHOT_TRACE_DISPLAY_LENGTH,
																							 shot_origin.z + shot_direction.z * ns_SHOT_TRACE_DISPLAY_LENGTH});
		}
	}

	// 敵レイヤーに命中した場合だけ、Colliderの識別値からダメージを与える相手を探す。
	if(weapon_result.didHit && weapon_result.hitInfo.layer == COLLISION_LAYER::ns_ENEMY){
		Enemy* const hit_enemy = m_enemyManager.FindLivingEnemy(weapon_result.hitInfo.userData);
		// 対応する生存中の敵が見つかった場合だけ、ダメージと命中表示を反映する。
		if(hit_enemy != nullptr){
			hit_enemy->ApplyDamage(GetPlayerAttackDamage(WEAPON_TABLE::ns_ASSAULT_RIFLE.damage,*hit_enemy),m_collisionWorld);
			m_hitMarkerRemainingSeconds = ns_HIT_MARKER_SECONDS;
		}
	}
}

// Qを押した瞬間に、HPが満タンでなく、回復アイテムを使えれば、HPを回復する。動作の状態に関係なく使え、他の動作は中断しない。
void GameScene::UpdateHeal()
{
	// Qを押した瞬間でなければ、何もしない。
	if(!m_inputSystem.IsKeyTriggered('Q'))return;

	// HPが満タンなら、使っても効果がないため、アイテムを消費しない。
	if(m_playerHealth.IsFull())return;

	// アイテムを使えた（所持数があり、連続使用の間隔が空いている）場合だけ、HPを回復する。
	if(m_healItems.TryUse(COMBAT_TABLE::ns_HEAL_COOLDOWN_SECONDS)){
		m_playerHealth.Heal(COMBAT_TABLE::ns_HEAL_AMOUNT);
	}
}