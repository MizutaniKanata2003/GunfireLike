#include "../H/GameScene.h"

//========= Windows インクルード=========
#include <windows.h>

//========= 外部ライブラリ インクルード=========
#include <DirectXMath.h>
#include "imgui.h"

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/H/SceneManager.h"
#include "Scene/TitleScene/H/TitleScene.h"

namespace
{
	//========= Projection設定=========
	// 視野角。単位は度。
	constexpr float ns_FOV_DEGREES = 60.0f;
	// 近距離Clip面。
	constexpr float ns_NEAR_CLIP = 0.1f;
	// 遠距離Clip面。
	constexpr float ns_FAR_CLIP = 1000.0f;
}

GameScene::GameScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem)
	: m_sceneManager(sceneManager),m_inputSystem(inputSystem),m_graphicsSystem(graphicsSystem),m_audioSystem(audioSystem)
{}

void GameScene::Initialize()
{
	m_debugPlayer = DebugPlayer{};
	m_fpsCamera = FpsCamera{};
	m_stageField.Initialize();

	// DebugPlayerの既存初期Zは床の外側なので、初回移動のclampを先に適用する。
	m_debugPlayer.Update(0.0f,m_inputSystem,m_fpsCamera);
	m_fpsCamera.SetPosition(m_debugPlayer.GetPosition());
}

bool GameScene::Init()
{
	if(!m_basicMeshRenderer.Initialize(m_graphicsSystem)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの基本Mesh初期化に失敗しました。");
		Finalize();
		return false;
	}

	// Resource生成に成功してからFPS操作とBGMを有効にする。
	m_inputSystem.SetMouseCaptureEnabled(true);
	m_audioSystem.PlayGameBgm();
	return true;
}

void GameScene::Update(float deltaTime)
{
	if(m_inputSystem.IsKeyTriggered(VK_F2)){
		m_sceneManager.RequestSceneChange<TitleScene>();
		return;
	}

	if(m_inputSystem.IsKeyTriggered(VK_F1)){
		m_inputSystem.SetMouseCaptureEnabled(!m_inputSystem.IsMouseCaptureEnabled());
	}

	// マウスを解放している間は、テスト画面の移動を止める。
	if(!m_inputSystem.IsMouseCaptureEnabled())return;

	m_fpsCamera.Update(m_inputSystem);
	m_debugPlayer.Update(deltaTime,m_inputSystem,m_fpsCamera);
	m_fpsCamera.SetPosition(m_debugPlayer.GetPosition());
}

void GameScene::Draw()
{
	const unsigned int render_width = m_graphicsSystem.GetRenderWidth();
	const unsigned int render_height = m_graphicsSystem.GetRenderHeight();
	if(render_width == 0 || render_height == 0)return;

	const float aspect_ratio = static_cast<float>(render_width) / static_cast<float>(render_height);
	const DirectX::XMMATRIX projection_matrix = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(ns_FOV_DEGREES),
																																						aspect_ratio,ns_NEAR_CLIP,ns_FAR_CLIP);
	const DirectX::XMMATRIX view_matrix = m_fpsCamera.GetViewMatrix();

	m_graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);
	m_stageField.Draw(m_basicMeshRenderer,m_graphicsSystem,view_matrix,projection_matrix);

	// Debug UIは値の表示だけを行い、Draw内ではScene遷移を予約しない。
	const DirectX::XMFLOAT3& player_position = m_debugPlayer.GetPosition();
	ImGui::Begin("Minimal Game Scene");
	ImGui::TextUnformatted("WASD: Move / Mouse: Look");
	ImGui::TextUnformatted("F1: Toggle mouse capture / F2: Title / Escape: Exit");
	ImGui::Text("Position: %.2f, %.2f, %.2f",player_position.x,player_position.y,player_position.z);
	ImGui::Text("Mouse capture: %s",m_inputSystem.IsMouseCaptureEnabled() ? "ON" : "OFF");
	ImGui::End();
}

void GameScene::Finalize()
{
	// 次Sceneが設定したCaptureやBGMは変更しない。
	m_basicMeshRenderer.Uninit();
}