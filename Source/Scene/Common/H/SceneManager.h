#pragma once

#include <memory>

#include "IScene.h"
#include "GameProgress.h"

#include "Framework/2D/H/FadeOverlay.h"

class AudioSystem;
class GraphicsSystem;
class InputSystem;

class SceneManager final
{
public:
	~SceneManager();

	void Initialize(
		InputSystem& inputSystem,
		GraphicsSystem& graphicsSystem,
		AudioSystem& audioSystem );

	void Update(
		float deltaTime );

	void Draw();

	void Uninit();

	template<class TScene>
	void RequestSceneChange();

	// 新しいゲーム開始時に進捗を初期化する。
	void StartNewGame();

	// Sceneをまたいで維持するゲーム進捗を返す。
	[[nodiscard]] GameProgress& GetGameProgress();

	[[nodiscard]] const GameProgress&
		GetGameProgress() const;

private:
	void ApplySceneChange();

	InputSystem* m_InputSystem = nullptr;

	GraphicsSystem* m_GraphicsSystem = nullptr;

	AudioSystem* m_AudioSystem = nullptr;

	std::unique_ptr<IScene> m_CurrentScene;

	std::unique_ptr<IScene> m_NextScene;

	GameProgress m_GameProgress{};

	// シーン遷移直後にフェードインを表示する。
	FadeOverlay m_FadeOverlay;
};

template<class TScene>
void SceneManager::RequestSceneChange()
{
	m_NextScene = std::make_unique<TScene>(
		*this,
		*m_InputSystem,
		*m_GraphicsSystem,
		*m_AudioSystem );
}