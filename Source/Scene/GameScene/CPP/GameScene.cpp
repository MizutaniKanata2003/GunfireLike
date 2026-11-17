#include "../H/GameScene.h"

// テストSceneの初期化、入力に応じた状態更新、射撃の反映、設定表から生成した床・壁と敵・Collision可視化・デバッグUIの描画、Resource終了を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_F1〜VK_F4の仮想キーコードで、Capture切替・タイトル遷移・可視化切替・敵の再配置のキーを指定する。

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

// このSceneの視野角、開始位置、Collision可視化、射撃の表示に使用する固定設定をまとめる。
namespace
{
	//========= Projection設定=========
	// 構えていない時の垂直方向の視野角。単位は度。行列生成時にラジアンへ変換する。
	constexpr float ns_FOV_DEGREES = 60.0f;
	// 構えている時の垂直方向の視野角。単位は度。通常より狭くして、拡大して見える。企画書で未確定の仮値。
	constexpr float ns_AIM_FOV_DEGREES = 45.0f;
	// 視野角が目標値へ近づく速さ。1秒あたりに差の何倍を縮めるかを表し、大きいほど速い。仮値。
	constexpr float ns_FOV_SMOOTHING_RATE = 12.0f;
	// View空間での近距離Clip面の距離。単位はワールド距離単位。これより近い物は描画されない。
	constexpr float ns_NEAR_CLIP = 0.1f;
	// View空間での遠距離Clip面の距離。単位はワールド距離単位。これより遠い物は描画されない。
	constexpr float ns_FAR_CLIP = 1000.0f;

	//========= 配置の設定=========
	// プレイヤーの開始位置。ワールド座標(0,1.6,0)で、床の手前寄りの中央。Yはカメラの高さ。壁の内側にある。
	constexpr XMFLOAT3 ns_PLAYER_START_POSITION{0.0f,1.6f,0.0f};
	// 検証用の敵の位置。ワールド座標(0,1,6)で、プレイヤー開始位置の正面。
	constexpr XMFLOAT3 ns_ENEMY_POSITION{0.0f,1.0f,6.0f};
	// 検証用の敵のColliderへ付ける識別値。命中結果のuserDataと照合する。
	constexpr std::uint32_t ns_ENEMY_ID = 1;

	//========= 視線のRay表示設定=========
	// 視線のRayを調べる最大距離。単位はワールド距離単位。ステージの奥行き（約20）より十分長い値を使う。
	constexpr float ns_DEBUG_RAY_MAX_DISTANCE = 100.0f;
	// 命中しない場合に、Rayの表示線を伸ばす長さ。単位はワールド距離単位。
	constexpr float ns_DEBUG_RAY_DISPLAY_LENGTH = 30.0f;
	// Rayの表示線の始点を、カメラ位置から右方向へずらす量。画面中央の1点に線が重ならないようにする。単位はワールド距離単位。
	constexpr float ns_DEBUG_RAY_OFFSET_RIGHT = 0.25f;
	// Rayの表示線の始点を、カメラ位置から下方向（-Y）へずらす量。単位はワールド距離単位。
	constexpr float ns_DEBUG_RAY_OFFSET_DOWN = 0.3f;
	// 命中点の十字の、中心から端までの長さ。単位はワールド距離単位。
	constexpr float ns_DEBUG_HIT_CROSS_HALF_SIZE = 0.2f;
	// 命中点から伸ばす法線の線の長さ。単位はワールド距離単位。
	constexpr float ns_DEBUG_NORMAL_LENGTH = 0.5f;
	// 視線のRayの表示線の色。黄色。
	constexpr XMFLOAT4 ns_DEBUG_RAY_COLOR{1.0f,1.0f,0.0f,1.0f};
	// 命中点の十字の色。白。
	constexpr XMFLOAT4 ns_DEBUG_HIT_CROSS_COLOR{1.0f,1.0f,1.0f,1.0f};
	// 命中点の法線の線の色。水色。
	constexpr XMFLOAT4 ns_DEBUG_NORMAL_COLOR{0.0f,1.0f,1.0f,1.0f};

	//========= 射撃の表示設定=========
	// 直近の弾道を表示し続ける時間。単位は秒。連射中は撃つたびに数え直す。
	constexpr float ns_SHOT_TRACE_SECONDS = 0.15f;
	// 命中を示す表示を続ける時間。単位は秒。
	constexpr float ns_HIT_MARKER_SECONDS = 0.3f;
	// 弾道の線の色。橙。
	constexpr XMFLOAT4 ns_SHOT_TRACE_COLOR{1.0f,0.5f,0.0f,1.0f};
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
	// プレイヤー・カメラ・武器・視野角・表示状態を既定へ戻し、前回の生成ObjectとColliderを空にする。
	// Rendererの生成、表からのObject生成、敵の配置はInitで行う。
	m_playerController.Initialize(ns_PLAYER_START_POSITION);
	m_FPSCamera = FpsCamera{};
	m_weaponSystem.Initialize(WEAPON_TABLE::ns_ASSAULT_RIFLE);
	m_fovDegrees = ns_FOV_DEGREES;
	m_stageObjects.clear();
	m_collisionWorld.Clear();
	m_isCollisionDebugVisible = false;
	m_hasDebugRayHit = false;
	m_shotTraceRemainingSeconds = 0.0f;
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

	// Collision可視化に使う線描画Rendererの初期化失敗を検出したら、生成済みのRendererを含めて終了して失敗を返す。
	if(!m_debugLineRenderer.Initialize(m_graphicsSystem)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの線描画Renderer初期化に失敗しました。");
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
		m_isCollisionDebugVisible = !m_isCollisionDebugVisible;
	}

	// F4が押された瞬間は、検証用の敵を満タンのHPで同じ位置へ再配置する。Capture解除中も使えるよう、Captureの判定より前に処理する。
	if(m_inputSystem.IsKeyTriggered(VK_F4)){
		m_enemy.Respawn(m_collisionWorld);
	}

	// 敵の怯みと、表示用の残り時間は、Captureに関係なく進める。残り時間は0未満にしない。
	m_enemy.Update(deltaTime);
	m_shotTraceRemainingSeconds = std::max(0.0f,m_shotTraceRemainingSeconds - deltaTime);
	m_hitMarkerRemainingSeconds = std::max(0.0f,m_hitMarkerRemainingSeconds - deltaTime);

	// マウスを解放している間は、カメラ・移動・射撃の操作更新をまとめて停止し、ここで戻る。
	if(!m_inputSystem.IsMouseCaptureEnabled())return;

	// 視線を先に更新してその向きで移動し、移動後のプレイヤー位置へカメラを追従させる。射撃はこの位置と向きから発射する。
	m_FPSCamera.Update(m_inputSystem);
	m_playerController.Update(deltaTime,m_inputSystem,m_FPSCamera,m_collisionWorld);
	m_FPSCamera.SetPosition(m_playerController.GetPosition());

	// 構えの状態に応じて、視野角の目標値へ滑らかに近づける。1回の更新で差を超えないよう、近づく割合は1以下に制限する。
	const float target_fov_degrees = m_playerController.IsAiming() ? ns_AIM_FOV_DEGREES : ns_FOV_DEGREES;
	const float fov_blend = std::min(1.0f,ns_FOV_SMOOTHING_RATE * deltaTime);
	m_fovDegrees += (target_fov_degrees - m_fovDegrees) * fov_blend;

	// 射撃を更新する。リロード・連射間隔・弾薬の処理と、命中判定は武器が行い、ダメージの反映だけをここで行う。
	const WEAPON_UPDATE_RESULT weapon_result = m_weaponSystem.Update(deltaTime,m_inputSystem,m_playerController.IsAiming(),m_FPSCamera,
																																	 m_collisionWorld);

	// 弾を発射した場合は、命中点または表示用の長さまでを、弾道の終点として記録して表示時間を数え直す。
	if(weapon_result.didFire){
		const XMFLOAT3& shot_origin = weapon_result.shotRay.origin;
		const XMFLOAT3& shot_direction = weapon_result.shotRay.direction;
		// 命中した場合は、終点を命中点にする。
		if(weapon_result.didHit){
			m_shotTraceEnd = weapon_result.rayHit.point;
		}
		// 命中しなかった場合は、弾の方向へ表示用の長さだけ進んだ点を終点にする。
		else{
			m_shotTraceEnd = XMFLOAT3{shot_origin.x + shot_direction.x * ns_DEBUG_RAY_DISPLAY_LENGTH,
				shot_origin.y + shot_direction.y * ns_DEBUG_RAY_DISPLAY_LENGTH,
				shot_origin.z + shot_direction.z * ns_DEBUG_RAY_DISPLAY_LENGTH};
		}
		m_shotTraceRemainingSeconds = ns_SHOT_TRACE_SECONDS;
	}

	// 弾が敵のColliderへ命中した場合だけ、敵へダメージを与え、命中の表示時間を数え直す。地形への命中ではダメージを与えない。
	if(weapon_result.didHit && weapon_result.hitInfo.layer == COLLISION_LAYER::ns_ENEMY && weapon_result.hitInfo.userData == m_enemy.GetEnemyId()){
		m_enemy.ApplyDamage(WEAPON_TABLE::ns_ASSAULT_RIFLE.damage,m_collisionWorld);
		m_hitMarkerRemainingSeconds = ns_HIT_MARKER_SECONDS;
	}

	// Collision可視化の表示中だけ、移動後のカメラ位置から視線方向へRayを飛ばし、最も近い命中を調べる。表示しない間は判定を行わない。
	if(m_isCollisionDebugVisible){
		m_debugRay.origin = m_FPSCamera.GetPosition();
		m_debugRay.direction = m_FPSCamera.GetForward();
		m_hasDebugRayHit = m_collisionWorld.Raycast(m_debugRay,ns_DEBUG_RAY_MAX_DISTANCE,COLLISION_LAYER::ns_ALL,m_debugRayHit,m_debugRayHitInfo);
	}
}

// 現在の描画先サイズと視点から3D描画を行い、操作状態をデバッグUIへ表示する。
void GameScene::Draw()
{
	// 描画先の現在サイズをpixel単位で取得する。
	const unsigned int render_width = m_graphicsSystem.GetRenderWidth();
	const unsigned int render_height = m_graphicsSystem.GetRenderHeight();
	// 最小化中などで幅または高さが0なら、アスペクト比の除算と描画を行わず戻る。
	if(render_width == 0 || render_height == 0)return;

	// 現在サイズの比率で左手系の透視投影行列を作り、カメラからView行列を取得する。視野角は構えの状態を反映した現在値を使う。
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

	// Collision可視化が有効なら、Objectの描画後に、Colliderの線とRay・弾道の線を重ねて描画する。
	if(m_isCollisionDebugVisible){
		DrawCollisionDebug(view_matrix,projection_matrix);
	}

	// UIは操作案内と値の表示だけに限定し、Draw内ではScene予約やゲーム進行を変更しない。
	const XMFLOAT3& player_position = m_playerController.GetPosition();
	ImGui::Begin("Minimal Game Scene");
	ImGui::TextUnformatted("WASD: Move / Mouse: Look / Left click: Shoot / Right click (hold): Aim / R: Reload");
	ImGui::TextUnformatted("F1: Toggle mouse capture / F2: Title / F3: Collision debug / F4: Respawn enemy / Escape: Exit");
	ImGui::Text("Position: %.2f, %.2f, %.2f",player_position.x,player_position.y,player_position.z);
	ImGui::Text("Mouse capture: %s",m_inputSystem.IsMouseCaptureEnabled() ? "ON" : "OFF");
	ImGui::Text("Collision debug: %s",m_isCollisionDebugVisible ? "ON" : "OFF");

	// 構えと弾薬の状態を表示する。
	ImGui::Separator();
	ImGui::Text("Aiming: %s",m_playerController.IsAiming() ? "ON" : "OFF");
	ImGui::Text("Ammo: %d / %d (reserve %d)",m_weaponSystem.GetMagazineAmmo(),m_weaponSystem.GetMagazineSize(),m_weaponSystem.GetReserveAmmo());
	// リロード中の場合だけ、進み具合を百分率で表示する。
	if(m_weaponSystem.IsReloading()){
		ImGui::Text("Reloading: %.0f%%",m_weaponSystem.GetReloadProgress() * 100.0f);
	}

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
	if(m_isCollisionDebugVisible && m_hasDebugRayHit){
		ImGui::Text("Ray hit: layer bit=%u distance=%.2f",m_debugRayHitInfo.layer,m_debugRayHit.distance);
	}
	ImGui::End();
}

// Sceneが所有する敵・Object・共有Renderer・線描画Renderer・Collisionを終了する。
void GameScene::Finalize()
{
	// 遷移先のInit後に呼ばれても、遷移先が設定したCaptureやBGMを上書きしない。
	// 敵とObjectは、Collisionと共有Rendererを借用しているため、それらを解放する前に先に終了する。
	m_enemy.Uninit(m_collisionWorld);
	m_stageObjects.clear();
	m_modelRegistry.Uninit();

	// 線描画Rendererを解放し、登録したColliderと蓄えた線分を空にする。
	m_debugLineRenderer.Uninit();
	m_collisionWorld.Clear();
	m_collisionDebugDrawer.Clear();
}

//========= 補助関数=========
// 登録中のColliderと視線のRay・直近の弾道を線分へ変換し、深度比較なしで描画する。
void GameScene::DrawCollisionDebug(const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix)
{
	// 前フレームの線を捨て、登録中の全Colliderをレイヤーごとの色で追加する。
	m_collisionDebugDrawer.Clear();
	m_collisionDebugDrawer.AddWorld(m_collisionWorld);

	// 線の表示の始点を求める。カメラ位置のままでは画面中央の1点に重なり線が見えないため、右方向と下方向（-Y）へずらす。
	// 右方向はカメラの右ベクトルに距離を掛け、下方向はワールドのY座標から引く。
	const XMFLOAT3& camera_position = m_FPSCamera.GetPosition();
	const XMFLOAT3 camera_right = m_FPSCamera.GetRight();
	const XMFLOAT3 line_start{camera_position.x + camera_right.x * ns_DEBUG_RAY_OFFSET_RIGHT,
		camera_position.y + camera_right.y * ns_DEBUG_RAY_OFFSET_RIGHT - ns_DEBUG_RAY_OFFSET_DOWN,
		camera_position.z + camera_right.z * ns_DEBUG_RAY_OFFSET_RIGHT};

	// 視線のRayの表示線の終点を決める。
	XMFLOAT3 line_end{};
	// 命中している場合は、終点を命中点にして、命中点の十字と、命中面の法線の線も追加する。
	if(m_hasDebugRayHit){
		line_end = m_debugRayHit.point;
		m_collisionDebugDrawer.AddCross(m_debugRayHit.point,ns_DEBUG_HIT_CROSS_HALF_SIZE,ns_DEBUG_HIT_CROSS_COLOR);
		m_collisionDebugDrawer.AddLine(m_debugRayHit.point,
																	 XMFLOAT3{m_debugRayHit.point.x + m_debugRayHit.normal.x * ns_DEBUG_NORMAL_LENGTH,
																						m_debugRayHit.point.y + m_debugRayHit.normal.y * ns_DEBUG_NORMAL_LENGTH,
																						m_debugRayHit.point.z + m_debugRayHit.normal.z * ns_DEBUG_NORMAL_LENGTH},
																	 ns_DEBUG_NORMAL_COLOR);
	}
	// 命中していない場合は、カメラ位置から視線方向へ表示用の長さだけ進んだ点を終点にする。
	else{
		const XMFLOAT3 camera_forward = m_FPSCamera.GetForward();
		line_end = XMFLOAT3{camera_position.x + camera_forward.x * ns_DEBUG_RAY_DISPLAY_LENGTH,
			camera_position.y + camera_forward.y * ns_DEBUG_RAY_DISPLAY_LENGTH,
			camera_position.z + camera_forward.z * ns_DEBUG_RAY_DISPLAY_LENGTH};
	}
	m_collisionDebugDrawer.AddLine(line_start,line_end,ns_DEBUG_RAY_COLOR);

	// 直近の弾道の表示時間が残っている間だけ、同じ始点から弾の終点までの線を橙色で追加する。ばらつきのある弾道を確認できる。
	if(m_shotTraceRemainingSeconds > 0.0f){
		m_collisionDebugDrawer.AddLine(line_start,m_shotTraceEnd,ns_SHOT_TRACE_COLOR);
	}

	// 壁面と同じ位置のAABBが深度比較でちらつかないよう、深度比較を切って描画し、描画後に既定へ戻す。
	m_graphicsSystem.SetDepthTestEnabled(false);
	m_collisionDebugDrawer.Draw(m_graphicsSystem,m_debugLineRenderer,viewMatrix,projectionMatrix);
	m_graphicsSystem.SetDepthTestEnabled(true);
}