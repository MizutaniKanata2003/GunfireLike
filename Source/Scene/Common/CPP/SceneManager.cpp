#include "../H/SceneManager.h"

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/GameScene/H/GameScene.h"
#include "Scene/TitleScene/H/TitleScene.h"

namespace
{
	//========= Scene遷移定数=========
		// Scene遷移時のフェードにかける秒数。
	constexpr float SCENE_FADE_DURATION_SECONDS = 0.35f;
}

// SceneManagerが保持するSceneを安全に破棄する。
SceneManager::~SceneManager()
{
	Uninit();
}

// SceneManagerが使用するFramework Systemを登録し、TitleSceneを開始する。
void SceneManager::Initialize(
	InputSystem& inputSystem,
	GraphicsSystem& graphicsSystem,
	AudioSystem& audioSystem )
{
	// 二重初期化時に既存SceneとFramework参照を安全に解除する。
	Uninit();

	// Scene生成に必要なFramework Systemへの非所有参照を登録する。
	m_InputSystem = &inputSystem;
	m_GraphicsSystem = &graphicsSystem;
	m_AudioSystem = &audioSystem;

	StartNewGame();

	// TitleSceneを生成して初期化し、開始時のフェードインを開始する。
	RequestSceneChange<TitleScene>();
	ApplySceneChange();
}

// Scene更新、フェード更新、予約済みSceneの切替を行う。
void SceneManager::Update( float deltaTime )
{
	// フェードアウト中は現在Sceneを更新せず、黒画面への遷移中にゲーム操作が進むことを防ぐ。
	if ( !m_FadeOverlay.IsPlaying() && m_CurrentScene ) m_CurrentScene->Update( deltaTime );

	// 次Sceneが予約され、フェード中でなければ黒へのフェードアウトを開始する。
	if ( m_NextScene && !m_FadeOverlay.IsPlaying() ) m_FadeOverlay.StartFadeOut( SCENE_FADE_DURATION_SECONDS );

	m_FadeOverlay.Update( deltaTime );

	// 画面が黒くなった瞬間にだけ予約済みSceneを現在Sceneへ反映する。
	if ( m_NextScene && m_FadeOverlay.IsFadeOutFinished() ) ApplySceneChange();
}

// 現在Sceneとフェードを描画する。
void SceneManager::Draw()
{
	if ( m_CurrentScene ) m_CurrentScene->Draw();

	m_FadeOverlay.Draw();
}

// 現在Sceneと予約済みSceneを終了し、参照先を解除する。
void SceneManager::Uninit()
{
	// まだ開始していない次Sceneは初期化していないため、そのまま破棄する。
	m_NextScene.reset();

	// 現在Sceneがある場合だけ、Resource解放後に破棄する。
	if ( m_CurrentScene )
	{
		m_CurrentScene->Uninit();
		m_CurrentScene.reset();
	}

	// 所有しないFramework Systemへの参照を解除する。
	m_InputSystem = nullptr;
	m_GraphicsSystem = nullptr;
	m_AudioSystem = nullptr;
}

// 予約済みSceneを現在Sceneへ反映し、初期化とフェードインを行う。
void SceneManager::ApplySceneChange()
{
	if ( !m_NextScene ) return;

	// 現在SceneのResourceを解放してから破棄する。
	if ( m_CurrentScene )
	{
		m_CurrentScene->Uninit();
		m_CurrentScene.reset();
	}

	// 予約済みSceneを現在Sceneへ移し、初期化する。
	m_CurrentScene = std::move( m_NextScene );
	m_CurrentScene->Initialize();

	// 深い初期化に失敗した場合、Resourceを解放して現在Sceneを破棄する。
	if ( !m_CurrentScene->Init() )
	{
		m_CurrentScene->Uninit();
		m_CurrentScene.reset();
		return;
	}

	// 実際にSceneを切り替えた直後にフェードインを開始する。
	m_FadeOverlay.StartFadeIn( SCENE_FADE_DURATION_SECONDS );
}