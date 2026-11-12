#include "../H/TitleScene.h"

//========= Windows インクルード=========
#include <windows.h>

//========= 外部ライブラリ インクルード=========
#include <DirectXColors.h>

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/H/SceneManager.h"
#include "Scene/GameScene/H/GameScene.h"

TitleScene::TitleScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem)
	: m_sceneManager(sceneManager),m_inputSystem(inputSystem),m_graphicsSystem(graphicsSystem),m_audioSystem(audioSystem)
{}

void TitleScene::Initialize()
{
	// このSceneにはResource生成前に初期化する進捗状態はない。
}

bool TitleScene::Init()
{
	if(!m_hudTextRenderer.Initialize(m_graphicsSystem)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"TitleSceneの文字描画初期化に失敗しました。");
		Finalize();
		return false;
	}

	// Resource生成に成功してから共有Systemの状態を変更する。
	m_inputSystem.SetMouseCaptureEnabled(false);
	m_audioSystem.PlayTitleBgm();
	return true;
}

void TitleScene::Update(float deltaTime)
{
	// タイトルには時間経過による更新がない。
	static_cast<void>(deltaTime);

	if(m_inputSystem.IsKeyTriggered(VK_RETURN)){
		m_sceneManager.RequestSceneChange<GameScene>();
	}
}

void TitleScene::Draw()
{
	if(m_graphicsSystem.GetRenderWidth() == 0 || m_graphicsSystem.GetRenderHeight() == 0)return;

	m_graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);
	m_hudTextRenderer.Begin();
	m_hudTextRenderer.DrawText(L"GUNFIRE LIKE",DirectX::XMFLOAT2{400.0f,230.0f},DirectX::Colors::Gold,1.5f);
	m_hudTextRenderer.DrawText(L"Enter: テスト画面へ",DirectX::XMFLOAT2{400.0f,350.0f},DirectX::Colors::White,0.9f);
	m_hudTextRenderer.DrawText(L"Escape: 終了",DirectX::XMFLOAT2{400.0f,400.0f},DirectX::Colors::White,0.8f);
	m_hudTextRenderer.End();
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);
}

void TitleScene::Finalize()
{
	m_hudTextRenderer.Uninit();
}