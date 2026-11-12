#include "../H/GameScene.h"

// テストSceneの初期化、入力に応じた状態更新、床・壁とデバッグUIの描画、Resource終了を実装する。

//========= Windows インクルード=========
#include <windows.h>

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
#include "Scene/TitleScene/H/TitleScene.h"

// このSceneの透視投影に使用する固定設定をまとめる。
namespace
{
	//========= Projection設定=========
	// 垂直方向の視野角。単位は度。行列生成時にラジアンへ変換する。
	constexpr float ns_FOV_DEGREES = 60.0f;
	// View空間での近距離Clip面の距離。
	constexpr float ns_NEAR_CLIP = 0.1f;
	// View空間での遠距離Clip面の距離。
	constexpr float ns_FAR_CLIP = 1000.0f;
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
	// プレイヤーとカメラを既定状態へ戻し、床と壁のTransformを初期化する。
	m_debugPlayer = DebugPlayer{};
	m_FPSCamera = FpsCamera{};
	m_stageField.Initialize();

	// 0秒の更新でも位置のclampは適用されるため、初回描画前に床範囲内へ補正する。
	// 補正後にカメラ位置を合わせ、プレイヤーと視点の開始位置を一致させる。
	m_debugPlayer.Update(0.0f,m_inputSystem,m_FPSCamera);
	m_FPSCamera.SetPosition(m_debugPlayer.GetPosition());
}

// 描画Resourceの初期化成功後にだけ、ゲーム操作とBGMへ切り替える。
bool GameScene::Init()
{
	// Mesh初期化がfalseを返した場合は、部分Resourceを終了して管理側へ失敗を返す。
	if(!m_basicMeshRenderer.Initialize(m_graphicsSystem)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの基本Mesh初期化に失敗しました。");
		Finalize();
		return false;
	}

	// Resource生成が成功してからCaptureとBGMを変更し、false返却時には変更しない。
	m_inputSystem.SetMouseCaptureEnabled(true);
	m_audioSystem.PlayGameBgm();
	return true;
}

// 遷移入力とCapture状態を先に処理し、操作可能な場合だけ視点と移動を更新する。
void GameScene::Update(float deltaTime)
{
	// タイトルへの予約を最優先し、同じフレームのCapture切替や移動は行わない。
	if(m_inputSystem.IsKeyTriggered(VK_F2)){
		m_sceneManager.RequestSceneChange<TitleScene>();
		return;
	}

	// F1のトリガー入力で、ゲーム操作とImGui操作に使うCapture状態を切り替える。
	if(m_inputSystem.IsKeyTriggered(VK_F1)){
		m_inputSystem.SetMouseCaptureEnabled(!m_inputSystem.IsMouseCaptureEnabled());
	}

	// マウスを解放している間は、カメラとプレイヤーの操作更新をまとめて停止する。
	if(!m_inputSystem.IsMouseCaptureEnabled())return;

	// 視線を先に更新してその向きで移動し、移動後のプレイヤー位置へカメラを追従させる。
	m_FPSCamera.Update(m_inputSystem);
	m_debugPlayer.Update(deltaTime,m_inputSystem,m_FPSCamera);
	m_FPSCamera.SetPosition(m_debugPlayer.GetPosition());
}

// 現在の描画先サイズと視点から3D描画を行い、操作状態をデバッグUIへ表示する。
void GameScene::Draw()
{
	// 最小化中などのサイズ0を除外し、描画とアスペクト比の除算を行わない。
	const unsigned int render_width = m_graphicsSystem.GetRenderWidth();
	const unsigned int render_height = m_graphicsSystem.GetRenderHeight();
	if(render_width == 0 || render_height == 0)return;

	// 現在サイズの比率で左手系の透視投影行列を作り、カメラからView行列を取得する。
	// Projection生成関数は共通usingへ追加せず、このファイルではDirectX修飾を維持する。
	const float aspect_ratio = static_cast<float>(render_width) / static_cast<float>(render_height);
	const XMMATRIX projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(ns_FOV_DEGREES),aspect_ratio,ns_NEAR_CLIP,ns_FAR_CLIP);
	const XMMATRIX view_matrix = m_FPSCamera.GetViewMatrix();

	// Fieldの描画契約に合わせてOPAQUE Passを設定し、床と4面の壁を描画する。
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);
	m_stageField.Draw(m_basicMeshRenderer,m_graphicsSystem,view_matrix,projection_matrix);

	// UIは操作案内と値の表示だけに限定し、Draw内ではScene予約やゲーム進行を変更しない。
	const XMFLOAT3& player_position = m_debugPlayer.GetPosition();
	ImGui::Begin("Minimal Game Scene");
	ImGui::TextUnformatted("WASD: Move / Mouse: Look");
	ImGui::TextUnformatted("F1: Toggle mouse capture / F2: Title / Escape: Exit");
	ImGui::Text("Position: %.2f, %.2f, %.2f",player_position.x,player_position.y,player_position.z);
	ImGui::Text("Mouse capture: %s",m_inputSystem.IsMouseCaptureEnabled() ? "ON" : "OFF");
	ImGui::End();
}

// Sceneが所有するMesh描画Resourceだけを終了する。
void GameScene::Finalize()
{
	// 遷移先のInit後に呼ばれても、遷移先が設定したCaptureやBGMを上書きしない。
	m_basicMeshRenderer.Uninit();
}