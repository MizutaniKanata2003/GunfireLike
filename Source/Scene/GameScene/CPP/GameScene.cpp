#include "../H/GameScene.h"

// テストSceneの初期化、入力に応じた状態更新、設定表から生成した床・壁とCollision可視化・デバッグUIの描画、Resource終了を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_F1・VK_F2・VK_F3などの仮想キーコードで、Capture切替・タイトル遷移・可視化切替・入力確認表示のキーを指定する。

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
#include "Scene/TitleScene/H/TitleScene.h"

// このSceneの透視投影、Collision可視化、入力確認表示に使用する固定設定をまとめる。
namespace
{
	//========= Projection設定=========
	// 垂直方向の視野角。単位は度。行列生成時にラジアンへ変換する。
	constexpr float ns_FOV_DEGREES = 60.0f;
	// View空間での近距離Clip面の距離。単位はワールド距離単位。これより近い物は描画されない。
	constexpr float ns_NEAR_CLIP = 0.1f;
	// View空間での遠距離Clip面の距離。単位はワールド距離単位。これより遠い物は描画されない。
	constexpr float ns_FAR_CLIP = 1000.0f;

	//========= Collisionテスト設定=========
	// 動作確認用に配置する、敵レイヤーの球。中心はワールド座標(0,1,6)で、床の中央付近の正面。半径は1。
	constexpr COLLISION_SPHERE ns_TEST_TARGET_SPHERE{{0.0f,1.0f,6.0f},1.0f};
	// 視線のRayを調べる最大距離。単位はワールド距離単位。ステージの奥行き（約20）より十分長い値を使う。
	constexpr float ns_DEBUG_RAY_MAX_DISTANCE = 100.0f;

	//========= Rayの表示設定=========
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
	// Rayの表示線の色。黄色。
	constexpr XMFLOAT4 ns_DEBUG_RAY_COLOR{1.0f,1.0f,0.0f,1.0f};
	// 命中点の十字の色。白。
	constexpr XMFLOAT4 ns_DEBUG_HIT_CROSS_COLOR{1.0f,1.0f,1.0f,1.0f};
	// 命中点の法線の線の色。水色。
	constexpr XMFLOAT4 ns_DEBUG_NORMAL_COLOR{0.0f,1.0f,1.0f,1.0f};

	//========= 入力確認表示の型=========
	// 入力確認表示に並べるキー1個分の設定。
	struct INPUT_DEBUG_KEY
	{
		// 画面に表示するキーの名前。静的な文字列定数を指す。
		const char* label;
		// InputSystemへ渡す0〜255の仮想キーコード。マウスボタンもキー配列から取得できるか確認する対象。
		unsigned char keyCode;
	};

	//========= 入力確認表示の対象キー=========
	// α版の操作表で使うキーと、左右を区別するShift・Ctrlを並べる。表示順は表の順。
	// 総称のShift・Ctrlと左右別の値を両方並べ、どちらが取得できるかを実機で確認する。
	constexpr INPUT_DEBUG_KEY ns_INPUT_DEBUG_KEYS[]
	{
		{"Left click",VK_LBUTTON},{"Right click",VK_RBUTTON},{"Shift (any)",VK_SHIFT},{"Shift (left)",VK_LSHIFT},
		{"Ctrl (any)",VK_CONTROL},{"Ctrl (left)",VK_LCONTROL},{"Space",VK_SPACE},{"Tab",VK_TAB},
		{"W",'W'},{"A",'A'},{"S",'S'},{"D",'D'},{"R",'R'},{"F",'F'},{"E",'E'},{"Q",'Q'},{"1",'1'},{"2",'2'}
	};
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
	// プレイヤー・カメラ・入力確認表示・Collisionの状態を既定へ戻し、前回の生成Objectを空にする。Rendererの生成と表からのObject生成はInitで行う。
	m_debugPlayer = DebugPlayer{};
	m_FPSCamera = FpsCamera{};
	m_debugWheelTotal = 0;
	m_stageObjects.clear();
	m_collisionWorld.Clear();
	m_isCollisionDebugVisible = false;
	m_hasDebugRayHit = false;

	// 0秒の更新でも位置のclampは適用されるため、初回描画前に床範囲内へ補正する。
	// 補正後にカメラ位置を合わせ、プレイヤーと視点の開始位置を一致させる。
	m_debugPlayer.Update(0.0f,m_inputSystem,m_FPSCamera);
	m_FPSCamera.SetPosition(m_debugPlayer.GetPosition());
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

	// 動作確認用の球Colliderを敵レイヤーで登録する。識別用の値は使わないため0にする。
	m_collisionWorld.AddSphere(ns_TEST_TARGET_SPHERE,COLLISION_LAYER::ns_ENEMY,0);

	// Resource生成とObject生成が成功してからCaptureとBGMを変更し、false返却時には変更しない。
	m_inputSystem.SetMouseCaptureEnabled(true);
	m_audioSystem.PlayGameBgm();
	return true;
}

// 遷移入力とCapture状態を先に処理し、操作可能な場合だけ視点と移動を更新する。
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

	// マウスを解放している間は、カメラとプレイヤーの操作更新をまとめて停止し、ここで戻る。
	if(!m_inputSystem.IsMouseCaptureEnabled())return;

	// 入力確認用に、この更新までに蓄積したホイール回転量を取り出して累計へ加える。視点・移動の更新には影響しない。
	m_debugWheelTotal += m_inputSystem.ConsumeMouseWheelDelta();

	// 視線を先に更新してその向きで移動し、移動後のプレイヤー位置へカメラを追従させる。
	m_FPSCamera.Update(m_inputSystem);
	m_debugPlayer.Update(deltaTime,m_inputSystem,m_FPSCamera);
	m_FPSCamera.SetPosition(m_debugPlayer.GetPosition());

	// Collision可視化の表示中だけ、移動後のカメラ位置から視線方向へRayを飛ばし、最も近い命中を調べる。表示しない間は判定を行わない。
	if(m_isCollisionDebugVisible){
		m_debugRay.origin = m_FPSCamera.GetPosition();
		m_debugRay.direction = m_FPSCamera.GetForward();
		m_hasDebugRayHit = m_collisionWorld.Raycast(m_debugRay,ns_DEBUG_RAY_MAX_DISTANCE,COLLISION_LAYER::ns_ALL,m_debugRayHit,m_debugRayHitInfo);
	}
}

// 現在の描画先サイズと視点から3D描画を行い、操作状態と入力確認をデバッグUIへ表示する。
void GameScene::Draw()
{
	// 描画先の現在サイズをpixel単位で取得する。
	const unsigned int render_width = m_graphicsSystem.GetRenderWidth();
	const unsigned int render_height = m_graphicsSystem.GetRenderHeight();
	// 最小化中などで幅または高さが0なら、アスペクト比の除算と描画を行わず戻る。
	if(render_width == 0 || render_height == 0)return;

	// 現在サイズの比率で左手系の透視投影行列を作り、カメラからView行列を取得する。
	// 投影行列の生成関数と角度変換は、MathTypes.hのusing宣言により修飾なしで呼ぶ。
	const float aspect_ratio = static_cast<float>(render_width) / static_cast<float>(render_height);
	const XMMATRIX projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(ns_FOV_DEGREES),aspect_ratio,ns_NEAR_CLIP,ns_FAR_CLIP);
	const XMMATRIX view_matrix = m_FPSCamera.GetViewMatrix();

	// 床と壁はOPAQUE Passで描画するため、描画前にPassを設定する。
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);

	// 生成済みの全Objectを、表の順（床→左壁→右壁→手前壁→奥壁）に描画する。
	for(const SceneObject& object : m_stageObjects){
		object.Draw(m_graphicsSystem,view_matrix,projection_matrix);
	}

	// Collision可視化が有効なら、Objectの描画後に、Colliderの線とRayの線を重ねて描画する。
	if(m_isCollisionDebugVisible){
		DrawCollisionDebug(view_matrix,projection_matrix);
	}

	// UIは操作案内と値の表示だけに限定し、Draw内ではScene予約やゲーム進行を変更しない。
	const XMFLOAT3& player_position = m_debugPlayer.GetPosition();
	ImGui::Begin("Minimal Game Scene");
	ImGui::TextUnformatted("WASD: Move / Mouse: Look");
	ImGui::TextUnformatted("F1: Toggle mouse capture / F2: Title / F3: Collision debug / Escape: Exit");
	ImGui::Text("Position: %.2f, %.2f, %.2f",player_position.x,player_position.y,player_position.z);
	ImGui::Text("Mouse capture: %s",m_inputSystem.IsMouseCaptureEnabled() ? "ON" : "OFF");
	ImGui::Text("Collision debug: %s",m_isCollisionDebugVisible ? "ON" : "OFF");

	// 可視化の表示中でRayが命中している場合だけ、命中したColliderのレイヤーのbitと距離を表示する。
	if(m_isCollisionDebugVisible && m_hasDebugRayHit){
		ImGui::Text("Ray hit: layer bit=%u distance=%.2f",m_debugRayHitInfo.layer,m_debugRayHit.distance);
	}

	// 入力確認用に、各キーの押下状態とホイールの累計を、読み取り専用の一覧として追加する。
	ImGui::Separator();
	ImGui::TextUnformatted("Input check");
	// 表の全キーを先頭から末尾まで走査し、現在の押下状態をONまたはOFFで1行ずつ表示する。
	for(const INPUT_DEBUG_KEY& debug_key : ns_INPUT_DEBUG_KEYS){
		ImGui::Text("%s: %s",debug_key.label,m_inputSystem.IsKeyPressed(debug_key.keyCode) ? "ON" : "OFF");
	}
	ImGui::Text("Mouse wheel total: %d",m_debugWheelTotal);
	ImGui::End();
}

// Sceneが所有するObject・共有Renderer・線描画Renderer・Collisionを終了する。
void GameScene::Finalize()
{
	// 遷移先のInit後に呼ばれても、遷移先が設定したCaptureやBGMを上書きしない。
	// Objectが借用しているRendererを解放する前に、Objectを先に破棄する。
	m_stageObjects.clear();
	m_modelRegistry.Uninit();

	// 線描画Rendererを解放し、登録したColliderと蓄えた線分を空にする。
	m_debugLineRenderer.Uninit();
	m_collisionWorld.Clear();
	m_collisionDebugDrawer.Clear();
}

//========= 補助関数=========
// 登録中のColliderと視線のRayの結果を線分へ変換し、深度比較なしで描画する。
void GameScene::DrawCollisionDebug(const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix)
{
	// 前フレームの線を捨て、登録中の全Colliderをレイヤーごとの色で追加する。
	m_collisionDebugDrawer.Clear();
	m_collisionDebugDrawer.AddWorld(m_collisionWorld);

	// Rayの表示線の始点を求める。カメラ位置のままでは画面中央の1点に重なり線が見えないため、右方向と下方向（-Y）へずらす。
	// 右方向はカメラの右ベクトルに距離を掛け、下方向はワールドのY座標から引く。
	const XMFLOAT3& camera_position = m_FPSCamera.GetPosition();
	const XMFLOAT3 camera_right = m_FPSCamera.GetRight();
	const XMFLOAT3 line_start{camera_position.x + camera_right.x * ns_DEBUG_RAY_OFFSET_RIGHT,
		camera_position.y + camera_right.y * ns_DEBUG_RAY_OFFSET_RIGHT - ns_DEBUG_RAY_OFFSET_DOWN,
		camera_position.z + camera_right.z * ns_DEBUG_RAY_OFFSET_RIGHT};

	// 表示線の終点を決める。
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

	// 壁面と同じ位置のAABBが深度比較でちらつかないよう、深度比較を切って描画し、描画後に既定へ戻す。
	m_graphicsSystem.SetDepthTestEnabled(false);
	m_collisionDebugDrawer.Draw(m_graphicsSystem,m_debugLineRenderer,viewMatrix,projectionMatrix);
	m_graphicsSystem.SetDepthTestEnabled(true);
}