#include "../H/GameScene.h"

// テストSceneの初期化、入力に応じた状態更新、射撃・近接攻撃・回避・回復・被弾の反映、設定表から生成した床・壁と敵・画像のHUD・デバッグUIの描画、Resource終了を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_F1〜VK_F8の仮想キーコードで、Capture切替・タイトル遷移・可視化切替・敵の再配置・確認操作のキーを指定する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで残り時間を0未満にせず、std::minで視野角の補間割合を1以下に制限する。
#include <cstdint> // std::uint32_tで、敵のColliderへ付ける識別値を保持する。

//========= 外部ライブラリ インクルード=========
#include <DirectXMath.h>

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/Collision/H/CollisionMath.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/H/SceneManager.h"
#include "Scene/Common/H/SceneObjectFactory.h"
#include "Scene/Common/H/StageLayout.h"
#include "Scene/GameScene/H/CombatResolver.h"
#include "Scene/GameScene/H/CombatTable.h"
#include "Scene/GameScene/H/GameDebugUi.h"
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
	// 視野角が目標値へ近づく速さ。1秒あたりに差の何倍を縮めるかを表し、大きいほど速い。仮値。
	constexpr float ns_FOV_SMOOTHING_RATE = 12.0f;
	// View空間での近距離Clip面の距離。単位はワールド距離単位。これより近い物は描画されない。
	constexpr float ns_NEAR_CLIP = 0.1f;
	// View空間での遠距離Clip面の距離。単位はワールド距離単位。これより遠い物は描画されない。
	constexpr float ns_FAR_CLIP = 1000.0f;

	//========= 配置の設定=========
	// プレイヤーの開始位置。ワールド座標(0,1.6,0)で、床の手前寄りの中央。Yは立っている時のカメラの高さ。壁の内側にある。
	constexpr XMFLOAT3 ns_PLAYER_START_POSITION{0.0f,1.6f,0.0f};
	// 検証用の敵の位置。ワールド座標(0,1,6)で、プレイヤー開始位置の正面。
	constexpr XMFLOAT3 ns_ENEMY_POSITION{0.0f,1.0f,6.0f};
	// 検証用の敵のColliderへ付ける識別値。命中結果のuserDataと照合する。
	constexpr std::uint32_t ns_ENEMY_ID = 1;

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

	// 判定・時間・接触・表示の状態を既定へ戻し、前回の生成ObjectとColliderを空にする。Rendererの生成、表からのObject生成、敵の配置はInitで行う。
	m_dodgeJudge.Initialize();
	m_lastJudgeResult = DODGE_JUDGE_RESULT::e_NONE;
	m_slowMotionRemainingSeconds = 0.0f;
	m_hitStopRemainingSeconds = 0.0f;
	m_contactCooldownRemainingSeconds = 0.0f;
	m_hasSlideKnockedDownEnemy = false;
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

	// 検証用の敵を配置し、球のColliderを登録する。Cube Rendererを借用できなければ、生成済みのResourceを含めて終了して失敗を返す。
	if(!m_enemy.Initialize(m_modelRegistry,m_collisionWorld,ns_ENEMY_POSITION,ns_ENEMY_ID)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの敵の配置に失敗しました。");
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

	// F4が押された瞬間は、検証用の敵を満タンのHPで同じ位置へ再配置する。
	if(m_inputSystem.IsKeyTriggered(VK_F4)){
		m_enemy.Respawn(m_collisionWorld);
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
	m_enemy.Update(world_delta_time);
	m_healItems.Update(gameplay_delta_time);
	m_collisionDebugView.Update(deltaTime);
	m_hitMarkerRemainingSeconds = std::max(0.0f,m_hitMarkerRemainingSeconds - deltaTime);

	// マウスを解放している間は、カメラ・移動・戦闘の操作更新をまとめて停止し、ここで戻る。
	if(!m_inputSystem.IsMouseCaptureEnabled())return;

	// 視線を先に更新し、その向きで、蹴れる敵がいるかを調べてから、プレイヤーを更新する。ヒットストップ中も視線は動かせる。
	m_FPSCamera.Update(m_inputSystem);
	const bool can_kick = CombatResolver::CanKickEnemy(m_FPSCamera.GetPosition(),m_FPSCamera.GetForward(),m_enemy);
	m_playerController.Update(gameplay_delta_time,m_inputSystem,m_FPSCamera,m_collisionWorld,can_kick);
	m_FPSCamera.SetPosition(m_playerController.GetPosition());

	// 現在の動作の状態を取得する。
	const PLAYER_ACTION_STATE action_state = m_playerController.GetActionState();
	// 殴打を始めた更新では、リロードをキャンセルする。弾は補充しない。殴打の開始は、1回前の状態が殴打ではなかったことで検出する。
	if(action_state == PLAYER_ACTION_STATE::e_PUNCH && m_previousActionState != PLAYER_ACTION_STATE::e_PUNCH){
		m_weaponSystem.CancelReload();
	}
	m_previousActionState = action_state;

	// 動作の状態に応じた視野角の目標値へ、滑らかに近づける。ダッシュは広く、構えは狭く、それ以外は通常。
	// 1回の更新で差を超えないよう、近づく割合は1以下に制限する。
	float target_fov_degrees = ns_FOV_DEGREES;
	// ダッシュ中は、視野角を広げる。
	if(action_state == PLAYER_ACTION_STATE::e_DASH){
		target_fov_degrees = ns_DASH_FOV_DEGREES;
	}
	// 構えている間は、視野角を狭める。
	else if(m_playerController.IsAiming()){
		target_fov_degrees = ns_AIM_FOV_DEGREES;
	}
	const float fov_blend = std::min(1.0f,ns_FOV_SMOOTHING_RATE * deltaTime);
	m_fovDegrees += (target_fov_degrees - m_fovDegrees) * fov_blend;

	// 近接攻撃・接触・回復・射撃を、この順に更新する。
	UpdateMelee();
	UpdateContact(gameplay_delta_time);
	UpdateHeal();
	UpdateShooting(gameplay_delta_time);

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

	// 床・壁・敵はOPAQUE Passで描画するため、描画前にPassを設定する。
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);

	// 生成済みの全Objectを、表の順（床→左壁→右壁→手前壁→奥壁）に描画する。
	for(const SceneObject& object : m_stageObjects){
		object.Draw(m_graphicsSystem,view_matrix,projection_matrix);
	}

	// 地形の後に、検証用の敵を描画する。死亡している間は何も描かれない。表示中なら、その後にCollisionの線を重ねる。
	m_enemy.Draw(m_graphicsSystem,view_matrix,projection_matrix);
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
	debug_info.isEnemyAlive = m_enemy.IsAlive();
	debug_info.enemyHP = m_enemy.GetCurrentHP();
	debug_info.enemyMaxHP = m_enemy.GetMaxHP();
	debug_info.isEnemyFlinching = m_enemy.IsFlinching();
	debug_info.isEnemyKnockedDown = m_enemy.IsKnockedDown();
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

// Sceneが所有する敵・Object・共有Renderer・HUD・Collision可視化・Collisionを終了する。
void GameScene::Finalize()
{
	// 遷移先のInit後に呼ばれても、遷移先が設定したCaptureやBGMを上書きしない。
	// 敵とObjectは、Collisionと共有Rendererを借用しているため、それらを解放する前に先に終了する。
	m_enemy.Uninit(m_collisionWorld);
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

	// 当たる瞬間に、敵が射程と範囲の外なら、空振りとして何もしない。
	if(!CombatResolver::IsEnemyInMeleeReach(m_FPSCamera.GetPosition(),m_FPSCamera.GetForward(),m_enemy))return;

	// 蹴りはダメージだけを与える。殴打はダメージを与えた後に、転倒させる。ダメージで死亡した場合、転倒は何も起こらない。
	if(melee_type == PLAYER_MELEE_TYPE::e_KICK){
		m_enemy.ApplyDamage(COMBAT_TABLE::ns_KICK_DAMAGE,m_collisionWorld);
	}
	// 殴打の場合は、ダメージを与えた後に転倒させる。
	else{
		m_enemy.ApplyDamage(COMBAT_TABLE::ns_PUNCH_DAMAGE,m_collisionWorld);
		m_enemy.ApplyKnockdown();
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
		m_hasSlideKnockedDownEnemy = false;
	}

	// 敵が死亡している、または体が重なっていない場合は、接触していないため何もしない。
	if(!m_enemy.IsAlive() || !CollisionMath::IntersectSphereSphere(m_playerController.GetBodySphere(),m_enemy.GetBodySphere()))return;

	// スライディング中は、まだ転倒させていない場合だけ、敵を転倒させる。同じスライディングで同じ敵を繰り返し転倒させず、接触の反応もない。
	if(action_state == PLAYER_ACTION_STATE::e_SLIDE){
		// 転倒させられた場合だけ、このスライディングでは転倒させた記録を付ける。すでに転倒中なら、何もしない。
		if(!m_hasSlideKnockedDownEnemy && m_enemy.ApplyKnockdown()){
			m_hasSlideKnockedDownEnemy = true;
		}
		return;
	}

	// 殴打中・蹴り中・無敵中・敵が転倒中・反応の間隔が残っている間は、接触の反応を与えない。
	if(action_state == PLAYER_ACTION_STATE::e_PUNCH || action_state == PLAYER_ACTION_STATE::e_KICK || m_playerController.IsInvincible() ||
		 m_enemy.IsKnockedDown() || m_contactCooldownRemainingSeconds > 0.0f)return;

	// 条件を満たした接触は、ダメージなしの怯みとヒットストップを与え、次の反応までの間隔を設定する。
	ApplyPlayerHitReaction(0.0f,COMBAT_TABLE::ns_CONTACT_HIT_STOP_SECONDS);
	m_contactCooldownRemainingSeconds = COMBAT_TABLE::ns_CONTACT_COOLDOWN_SECONDS;
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

	// 弾が敵のColliderへ命中した場合だけ、敵へダメージを与え、命中の表示時間を数え直す。地形への命中ではダメージを与えない。
	if(weapon_result.didHit && weapon_result.hitInfo.layer == COLLISION_LAYER::ns_ENEMY && weapon_result.hitInfo.userData == m_enemy.GetEnemyId()){
		m_enemy.ApplyDamage(WEAPON_TABLE::ns_ASSAULT_RIFLE.damage,m_collisionWorld);
		m_hitMarkerRemainingSeconds = ns_HIT_MARKER_SECONDS;
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