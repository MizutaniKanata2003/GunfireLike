#include "../H/SceneManager.h"
#include "Scene/GameScene/H/GameScene.h"
#include "Scene/TitleScene/H/TitleScene.h"
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/Input/H/InputSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"

namespace
{
	constexpr float SCENE_FADE_DURATION_SECONDS = 0.35f;
}

SceneManager::~SceneManager() = default;

void SceneManager::Initialize(
	InputSystem& inputSystem,
	GraphicsSystem& graphicsSystem,
	AudioSystem& audioSystem )
{
	m_InputSystem = &inputSystem;
	m_GraphicsSystem = &graphicsSystem;
	m_AudioSystem = &audioSystem;

	StartNewGame();

	RequestSceneChange<TitleScene>();
	ApplySceneChange();

	m_FadeOverlay.StartFadeIn(
		SCENE_FADE_DURATION_SECONDS );
}

void SceneManager::Update(
	float deltaTime )
{
	// フェードアウト中は、現在シーンを更新しない。
	// 黒画面へ遷移中にゲーム操作が進むことを防ぐ。
	if ( !m_FadeOverlay.IsPlaying() &&
		m_CurrentScene )
	{
		m_CurrentScene->Update(
			deltaTime );
	}

	// RequestSceneChange()が呼ばれるとm_NextSceneが入る。
	// まだフェードを開始していなければ、黒へのフェードアウトを始める。
	if ( m_NextScene &&
		!m_FadeOverlay.IsPlaying() )
	{
		m_FadeOverlay.StartFadeOut(
			SCENE_FADE_DURATION_SECONDS );
	}

	m_FadeOverlay.Update(
		deltaTime );

	// 黒くなった瞬間にだけ、予約済みの次シーンを反映する。
	if ( m_NextScene &&
		m_FadeOverlay.IsFadeOutFinished() )
	{
		ApplySceneChange();
	}
}

void SceneManager::Draw()
{
	if ( m_CurrentScene )
	{
		m_CurrentScene->Draw();
	}

	m_FadeOverlay.Draw();
}

void SceneManager::Uninit()
{
	m_NextScene.reset();

	if ( m_CurrentScene )
	{
		m_CurrentScene->Uninit();
		m_CurrentScene.reset();
	}

	m_InputSystem = nullptr;
	m_GraphicsSystem = nullptr;
	m_AudioSystem = nullptr;
}

void SceneManager::ApplySceneChange()
{
	if ( !m_NextScene )
	{
		return;
	}

	if ( m_CurrentScene )
	{
		m_CurrentScene->Uninit();
		m_CurrentScene.reset();
	}

	m_CurrentScene = std::move(
		m_NextScene );

	m_CurrentScene->Initialize();
	m_CurrentScene->Init();

	// 実際にシーンを切り替えた直後にフェードインを開始する。
	m_FadeOverlay.StartFadeIn(
		SCENE_FADE_DURATION_SECONDS );
}

void SceneManager::StartNewGame()
{
	m_GameProgress.Initialize();
}

GameProgress& SceneManager::GetGameProgress()
{
	return m_GameProgress;
}

const GameProgress&
SceneManager::GetGameProgress() const
{
	return m_GameProgress;
}