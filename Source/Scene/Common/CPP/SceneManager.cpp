#include "../H/SceneManager.h"

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
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

// SceneManagerが保持するSceneを安全に終了する。
SceneManager::~SceneManager()
{
	Finalize();
}

// SceneManagerが使用するFramework Systemを登録し、TitleSceneを開始する。
void SceneManager::Initialize( InputSystem& inputSystem, GraphicsSystem& graphicsSystem, AudioSystem& audioSystem )
{
	// 二重初期化時に既存SceneとFramework参照を安全に解除する。
	Finalize();

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
	if ( !m_FadeOverlay.IsPlaying() && m_CurrentScene )m_CurrentScene->Update( deltaTime );

	// 次Sceneが予約され、フェード中でなければ黒へのフェードアウトを開始する。
	if ( m_NextScene && !m_FadeOverlay.IsPlaying() )m_FadeOverlay.StartFadeOut( SCENE_FADE_DURATION_SECONDS );

	m_FadeOverlay.Update( deltaTime );

	// 画面が黒くなった瞬間にだけ予約済みSceneを現在Sceneへ反映する。
	if ( m_NextScene && m_FadeOverlay.IsFadeOutFinished() )ApplySceneChange();

}

// 現在Sceneとフェードを描画する。
void SceneManager::Draw()
{
	if ( m_CurrentScene )m_CurrentScene->Draw();

	m_FadeOverlay.Draw();
}

// 現在Sceneと予約済みSceneを終了し、参照先を解除する。
void SceneManager::Finalize()
{
	// まだ開始していない次Sceneは初期化していないため、そのまま破棄する。
	m_NextScene.reset();

	// 現在Sceneがある場合だけ、下位Resourceを終了してから破棄する。
	if ( m_CurrentScene )
	{
		m_CurrentScene->Finalize();
		m_CurrentScene.reset();
	}

	// SceneManagerが所有するフェード描画Resourceを終了する。
	m_FadeOverlay.Uninit();

	// 所有しないFramework Systemへの参照を解除する。
	m_InputSystem = nullptr;
	m_GraphicsSystem = nullptr;
	m_AudioSystem = nullptr;
}

// 予約済みSceneを初期化し、成功した場合だけ現在Sceneへ反映してフェードインを開始する。
void SceneManager::ApplySceneChange()
{
	if ( !m_NextScene )return;

	// 予約済みSceneを一時保持し、初期化成功まで現在Sceneを維持する。
	std::unique_ptr<IScene> nextScene = std::move( m_NextScene );

	// Scene固有の状態と失敗し得るResourceを順に初期化する。
	nextScene->Initialize();

	const bool isNextSceneInitialized = nextScene->Init();
	if ( !isNextSceneInitialized )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"次Scene初期化失敗。現在Sceneを維持します。" );

		nextScene->Finalize();
		m_FadeOverlay.StartFadeIn( SCENE_FADE_DURATION_SECONDS );
		return;
	}

	// 次Sceneの初期化成功後に現在Sceneの下位Resourceを終了して破棄する。
	if ( m_CurrentScene )
	{
		m_CurrentScene->Finalize();
		m_CurrentScene.reset();
	}

	// 初期化済みの次Sceneを現在Sceneとして反映する。
	m_CurrentScene = std::move( nextScene );

	// 実際にSceneを切り替えた直後にフェードインを開始する。
	m_FadeOverlay.StartFadeIn( SCENE_FADE_DURATION_SECONDS );
}