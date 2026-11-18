#include "../H/GameScene.h"

// テストSceneの初期化、入力に応じた状態更新、射撃・回避の反映、設定表から生成した床・壁と敵・画像のHUD・デバッグUIの描画、Resource終了を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_F1〜VK_F7の仮想キーコードで、Capture切替・タイトル遷移・可視化切替・敵の再配置・HPと回避の確認操作のキーを指定する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで表示用の残り時間を0未満にせず、std::minで視野角の補間割合を1以下に制限する。
#include <cstdint> // std::uint32_tで、敵のColliderへ付ける識別値を保持する。

//========= 外部ライブラリ インクルード=========
#include <DirectXMath.h>
#include "imgui.h"

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/H/SceneManager.h"
#include "Scene/Common/H/SceneObjectFactory.h"
#include "Scene/Common/H/StageLayout.h"
#include "Scene/GameScene/H/WeaponTable.h"
#include "Scene/TitleScene/H/TitleScene.h"

// このSceneの視野角、開始位置、HP、確認用の攻撃、スロー、射撃の表示に使用する固定設定と、表示用の補助関数をまとめる。
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

	//========= HPの設定=========
	// プレイヤーの最大HP。企画書で未確定のため、検証用の仮値。
	constexpr float ns_PLAYER_MAX_HP = 100.0f;
	// F5で与える確認用のダメージ量。HPバーと低HP警告の表示確認に使う。
	constexpr float ns_DEBUG_DAMAGE = 10.0f;

	//========= 確認用の攻撃とスロー=========
	// F7で発生させる確認用の攻撃が、予告から当たるまでの時間。単位は秒。回避の入力を間に合わせられる長さの仮値。
	constexpr float ns_SIMULATED_ATTACK_DELAY_SECONDS = 0.8f;
	// 確認用の攻撃が命中した場合に、プレイヤーへ与えるダメージ。
	constexpr float ns_SIMULATED_ATTACK_DAMAGE = 10.0f;
	// ジャスト回避で、スローを続ける時間。単位は実時間の秒。企画書で未確定の仮値。
	constexpr float ns_SLOW_MOTION_SECONDS = 1.5f;
	// スロー中の、プレイヤー以外の時間の進む速さ。1が通常で、小さいほど遅い。仮値。
	constexpr float ns_SLOW_MOTION_SCALE = 0.3f;

	//========= 射撃の表示設定=========
	// 命中しない場合に、弾道の線を伸ばす長さ。単位はワールド距離単位。
	constexpr float ns_SHOT_TRACE_DISPLAY_LENGTH = 30.0f;
	// 命中を示す表示を続ける時間。単位は秒。
	constexpr float ns_HIT_MARKER_SECONDS = 0.3f;

	//========= 補助関数=========
	// 動作の状態を、デバッグUIに表示する名前へ変換する。
	// actionState: 変換する動作の状態。
	// 戻り値: 状態の名前。静的な文字列定数。未定義の値ではUNKNOWN。
	const char* GetActionStateName(PLAYER_ACTION_STATE actionState)
	{
		// 状態ごとに名前を選ぶ。
		switch(actionState){
			// 通常。
		case PLAYER_ACTION_STATE::e_NORMAL:
		return "NORMAL";

		// ダッシュ。
		case PLAYER_ACTION_STATE::e_DASH:
		return "DASH";

		// しゃがみ。
		case PLAYER_ACTION_STATE::e_CROUCH:
		return "CROUCH";

		// スライディング。
		case PLAYER_ACTION_STATE::e_SLIDE:
		return "SLIDE";

		// 回避。
		case PLAYER_ACTION_STATE::e_DODGE:
		return "DODGE";
		}

		// 未定義の値は、不明として扱う。
		return "UNKNOWN";
	}

	// 回避の判定結果を、デバッグUIに表示する名前へ変換する。
	// judgeResult: 変換する判定結果。
	// 戻り値: 結果の名前。静的な文字列定数。未定義の値ではUNKNOWN。
	const char* GetJudgeResultName(DODGE_JUDGE_RESULT judgeResult)
	{
		// 結果ごとに名前を選ぶ。
		switch(judgeResult){
			// まだ判定がない。
		case DODGE_JUDGE_RESULT::e_NONE:
		return "-";

		// 命中した。
		case DODGE_JUDGE_RESULT::e_HIT:
		return "HIT (player damaged)";

		// 無敵で避けた。
		case DODGE_JUDGE_RESULT::e_DODGED:
		return "DODGED";

		// ジャスト回避。
		case DODGE_JUDGE_RESULT::e_JUST_DODGED:
		return "JUST DODGE!";
		}

		// 未定義の値は、不明として扱う。
		return "UNKNOWN";
	}
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
	// プレイヤー・HP・カメラ・武器・視野角・回避の判定・スロー・表示状態を既定へ戻し、前回の生成ObjectとColliderを空にする。
	// Rendererの生成、表からのObject生成、敵の配置はInitで行う。
	m_playerController.Initialize(ns_PLAYER_START_POSITION);
	m_playerHealth.Initialize(ns_PLAYER_MAX_HP);
	m_FPSCamera = FpsCamera{};
	m_weaponSystem.Initialize(WEAPON_TABLE::ns_ASSAULT_RIFLE);
	m_fovDegrees = ns_FOV_DEGREES;
	m_dodgeJudge.Initialize();
	m_lastJudgeResult = DODGE_JUDGE_RESULT::e_NONE;
	m_slowMotionRemainingSeconds = 0.0f;
	m_stageObjects.clear();
	m_collisionWorld.Clear();
	m_hitMarkerRemainingSeconds = 0.0f;

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

// 遷移入力とCapture状態を先に処理し、操作可能な場合だけ視点・移動・射撃を更新する。
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

	// F4が押された瞬間は、検証用の敵を満タンのHPで同じ位置へ再配置する。Capture解除中も使えるよう、Captureの判定より前に処理する。
	if(m_inputSystem.IsKeyTriggered(VK_F4)){
		m_enemy.Respawn(m_collisionWorld);
	}

	// F5が押された瞬間は、HPバーと低HP警告の確認のため、プレイヤーへ確認用のダメージを与える。被弾の実装までの暫定の操作。
	if(m_inputSystem.IsKeyTriggered(VK_F5)){
		m_playerHealth.ApplyDamage(ns_DEBUG_DAMAGE);
	}

	// F6が押された瞬間は、プレイヤーのHPを満タンへ戻す。
	if(m_inputSystem.IsKeyTriggered(VK_F6)){
		m_playerHealth.HealFull();
	}

	// F7が押された瞬間は、回避とジャスト回避の確認のため、一定時間後に当たる攻撃を予告する。敵の攻撃の実装までの暫定の操作。
	if(m_inputSystem.IsKeyTriggered(VK_F7)){
		m_dodgeJudge.StartIncomingAttack(ns_SIMULATED_ATTACK_DELAY_SECONDS);
	}

	// スローの残り時間を実時間で減らし、スロー中ならプレイヤー以外の時間の進みを遅くする。プレイヤーと武器は、実時間のまま更新する。
	m_slowMotionRemainingSeconds = std::max(0.0f,m_slowMotionRemainingSeconds - deltaTime);
	const float world_delta_time = m_slowMotionRemainingSeconds > 0.0f ? deltaTime * ns_SLOW_MOTION_SCALE : deltaTime;

	// 予告した攻撃が当たる瞬間に、無敵かどうかと回避の開始からの経過時間で判定する。Captureに関係なく進める。
	const DODGE_JUDGE_RESULT judge_result = m_dodgeJudge.Update(world_delta_time,m_playerController.IsInvincible(),
																															m_playerController.GetSecondsSinceDodgeStart());
	// 判定結果が出た場合だけ、表示用に記録する。
	if(judge_result != DODGE_JUDGE_RESULT::e_NONE){
		m_lastJudgeResult = judge_result;
	}
	// 命中した場合は、プレイヤーへ確認用のダメージを与える。
	if(judge_result == DODGE_JUDGE_RESULT::e_HIT){
		m_playerHealth.ApplyDamage(ns_SIMULATED_ATTACK_DAMAGE);
	}
	// ジャスト回避の場合は、スローを開始する。連続した場合は、時間を数え直す。
	else if(judge_result == DODGE_JUDGE_RESULT::e_JUST_DODGED){
		m_slowMotionRemainingSeconds = ns_SLOW_MOTION_SECONDS;
	}

	// 敵の怯みはスローを反映した時間で、表示用の残り時間は実時間で、Captureに関係なく進める。残り時間は0未満にしない。
	m_enemy.Update(world_delta_time);
	m_collisionDebugView.Update(deltaTime);
	m_hitMarkerRemainingSeconds = std::max(0.0f,m_hitMarkerRemainingSeconds - deltaTime);

	// マウスを解放している間は、カメラ・移動・射撃の操作更新をまとめて停止し、ここで戻る。
	if(!m_inputSystem.IsMouseCaptureEnabled())return;

	// 視線を先に更新してその向きで移動し、移動後のプレイヤー位置へカメラを追従させる。射撃はこの位置と向きから発射する。
	m_FPSCamera.Update(m_inputSystem);
	m_playerController.Update(deltaTime,m_inputSystem,m_FPSCamera,m_collisionWorld);
	m_FPSCamera.SetPosition(m_playerController.GetPosition());

	// 動作の状態に応じた視野角の目標値へ、滑らかに近づける。ダッシュは広く、構えは狭く、それ以外は通常。
	// 1回の更新で差を超えないよう、近づく割合は1以下に制限する。
	float target_fov_degrees = ns_FOV_DEGREES;
	// ダッシュ中は、視野角を広げる。
	if(m_playerController.GetActionState() == PLAYER_ACTION_STATE::e_DASH){
		target_fov_degrees = ns_DASH_FOV_DEGREES;
	}
	// 構えている間は、視野角を狭める。
	else if(m_playerController.IsAiming()){
		target_fov_degrees = ns_AIM_FOV_DEGREES;
	}
	const float fov_blend = std::min(1.0f,ns_FOV_SMOOTHING_RATE * deltaTime);
	m_fovDegrees += (target_fov_degrees - m_fovDegrees) * fov_blend;

	// 射撃を更新する。リロード・連射間隔・弾薬の処理と、命中判定は武器が行い、ダメージの反映だけをここで行う。回避中は射撃できない。
	const WEAPON_UPDATE_RESULT weapon_result = m_weaponSystem.Update(deltaTime,m_inputSystem,m_playerController.IsAiming(),
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

	// 地形の後に、検証用の敵を描画する。死亡している間は何も描かれない。
	m_enemy.Draw(m_graphicsSystem,view_matrix,projection_matrix);

	// 表示中なら、3Dの描画の後に、Colliderの線とRay・弾道の線を重ねて描画する。非表示の間は何も描かれない。
	m_collisionDebugView.Draw(m_graphicsSystem,m_collisionWorld,m_FPSCamera,view_matrix,projection_matrix);

	// 3Dの上に画像のHUDを重ねるため、深度比較なし・アルファ合成の画面UI用Passへ切り替え、現在の値をHUDへ渡して描画する。
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);
	GAME_HUD_STATE hud_state{};
	hud_state.currentHP = m_playerHealth.GetCurrentHP();
	hud_state.maxHP = m_playerHealth.GetMaxHP();
	hud_state.magazineAmmo = m_weaponSystem.GetMagazineAmmo();
	hud_state.magazineSize = m_weaponSystem.GetMagazineSize();
	hud_state.reserveAmmo = m_weaponSystem.GetReserveAmmo();
	hud_state.isReloading = m_weaponSystem.IsReloading();
	hud_state.reloadProgress = m_weaponSystem.GetReloadProgress();
	hud_state.isAiming = m_playerController.IsAiming();
	m_gameHud.Draw(m_graphicsSystem,hud_state);

	// デバッグUIは操作案内と値の表示だけに限定し、Draw内ではScene予約やゲーム進行を変更しない。
	const XMFLOAT3& player_position = m_playerController.GetPosition();
	ImGui::Begin("Minimal Game Scene");
	ImGui::TextUnformatted("WASD: Move / Mouse: Look / Left click: Shoot / Right click (hold): Aim / R: Reload");
	ImGui::TextUnformatted("Shift (hold): Dash / Ctrl (hold): Crouch / Ctrl while dashing: Slide / Space + WASD: Dodge");
	ImGui::TextUnformatted("F1: Mouse capture / F2: Title / F3: Collision debug / F4: Respawn enemy / F5: Damage / F6: Heal / F7: Incoming attack");
	ImGui::Text("Position: %.2f, %.2f, %.2f",player_position.x,player_position.y,player_position.z);
	ImGui::Text("Mouse capture: %s",m_inputSystem.IsMouseCaptureEnabled() ? "ON" : "OFF");
	ImGui::Text("Collision debug: %s",m_collisionDebugView.IsVisible() ? "ON" : "OFF");
	ImGui::Text("Player HP: %.0f / %.0f",m_playerHealth.GetCurrentHP(),m_playerHealth.GetMaxHP());

	// 動作の状態・無敵・スローと、確認用の攻撃の状態を表示する。
	ImGui::Separator();
	ImGui::Text("Action: %s%s",GetActionStateName(m_playerController.GetActionState()),m_playerController.IsInvincible() ? " (INVINCIBLE)" : "");
	ImGui::Text("Slow motion: %s",m_slowMotionRemainingSeconds > 0.0f ? "ON" : "OFF");
	// 攻撃を予告中の場合だけ、当たるまでの残り時間を表示する。
	if(m_dodgeJudge.HasIncomingAttack()){
		ImGui::Text("Incoming attack in: %.2f s",m_dodgeJudge.GetSecondsUntilHit());
	}
	ImGui::Text("Last attack result: %s",GetJudgeResultName(m_lastJudgeResult));

	// 敵の状態を表示する。死亡中は、再配置の案内を出す。
	ImGui::Separator();
	// 生きている場合は、HPと怯みの有無を表示する。
	if(m_enemy.IsAlive()){
		ImGui::Text("Enemy HP: %.0f / %.0f%s",m_enemy.GetCurrentHP(),m_enemy.GetMaxHP(),m_enemy.IsFlinching() ? " (Flinch)" : "");
	}
	// 死亡している場合は、F4で再配置できることを表示する。
	else{
		ImGui::TextUnformatted("Enemy: defeated (F4: respawn)");
	}

	// 直近の命中の表示時間が残っている間だけ、命中した旨を表示する。
	if(m_hitMarkerRemainingSeconds > 0.0f){
		ImGui::TextUnformatted("HIT!");
	}

	// 可視化の表示中でRayが命中している場合だけ、命中したColliderのレイヤーのbitと距離を表示する。
	if(m_collisionDebugView.IsVisible() && m_collisionDebugView.HasRayHit()){
		ImGui::Text("Ray hit: layer bit=%u distance=%.2f",m_collisionDebugView.GetRayHitInfo().layer,m_collisionDebugView.GetRayHit().distance);
	}
	ImGui::End();
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