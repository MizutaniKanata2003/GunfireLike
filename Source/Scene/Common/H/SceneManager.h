#pragma once

//========= C++標準ライブラリ インクルード=========
#include <memory>

//========= Scene インクルード=========
#include "GameProgress.h"
#include "IScene.h"

//========= Framework インクルード=========
#include "Framework/2D/H/FadeOverlay.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;

// Sceneの生成、更新、描画、遷移、ゲーム進捗を管理する。
class SceneManager final
{
public:
	//========= 生成・破棄関数=========
	// SceneManagerが保持するSceneを安全に終了する。
	~SceneManager();

	//========= ライフサイクル関数=========
	// SceneManagerが使用するFramework Systemを登録し、TitleSceneを開始する。
	void Initialize( InputSystem& inputSystem, GraphicsSystem& graphicsSystem, AudioSystem& audioSystem );
	// Scene更新、フェード更新、予約済みSceneの切替を行う。
	void Update( float deltaTime );
	// 現在Sceneとフェードを描画する。
	void Draw();
	// 現在Sceneと予約済みSceneを終了し、参照先を解除する。
	void Finalize();

	//========= Scene遷移関数=========
	// 指定したSceneを次の遷移先として予約する。
	template<class TScene>
	void RequestSceneChange()
	{
		m_NextScene = std::make_unique<TScene>( *this, *m_InputSystem, *m_GraphicsSystem, *m_AudioSystem );
	}

	//========= ゲーム進捗操作関数=========
	// 新しいゲーム開始時に進捗を初期化する。
	void StartNewGame()
	{
		m_GameProgress.Initialize();
	}

	//========= Getter関数=========
	// Sceneをまたいで維持するゲーム進捗を返す。
	[[nodiscard]] GameProgress& GetGameProgress()
	{
		return m_GameProgress;
	}

	// Sceneをまたいで維持する読み取り専用のゲーム進捗を返す。
	[[nodiscard]] const GameProgress& GetGameProgress() const
	{
		return m_GameProgress;
	}

private:
	//========= 補助関数=========
	// 予約済みSceneを現在Sceneへ反映し、初期化とフェードインを行う。
	void ApplySceneChange();

	//========= Framework参照=========
	// Scene生成時に渡すInputSystemへの非所有参照。
	InputSystem* m_InputSystem{};
	// Scene生成時に渡すGraphicsSystemへの非所有参照。
	GraphicsSystem* m_GraphicsSystem{};
	// Scene生成時に渡すAudioSystemへの非所有参照。
	AudioSystem* m_AudioSystem{};

	//========= Scene管理=========
	// 現在更新・描画するScene。
	std::unique_ptr<IScene> m_CurrentScene{};
	// 次の遷移時に現在Sceneへ切り替えるScene。
	std::unique_ptr<IScene> m_NextScene{};

	//========= ゲーム進捗=========
	// Sceneをまたいで維持するゲーム進捗。
	GameProgress m_GameProgress{};

	//========= Scene遷移演出=========
	// Scene遷移時に黒画面のフェードを表示する。
	FadeOverlay m_FadeOverlay{};
};