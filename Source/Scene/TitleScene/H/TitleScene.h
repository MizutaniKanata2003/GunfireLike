#pragma once

#include <windows.h>

#include "Scene/Common/H/IScene.h"
#include "Framework/2D/H/HudRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

namespace
{
	constexpr float TITLE_SCREEN_WIDTH = 1280.0f;
	constexpr float TITLE_SCREEN_HEIGHT = 720.0f;

	constexpr float TITLE_MAIN_PANEL_X = 190.0f;
	constexpr float TITLE_MAIN_PANEL_Y = 90.0f;
	constexpr float TITLE_MAIN_PANEL_WIDTH = 900.0f;
	constexpr float TITLE_MAIN_PANEL_HEIGHT = 560.0f;

	constexpr float TITLE_START_BUTTON_X = 470.0f;
	constexpr float TITLE_START_BUTTON_Y = 480.0f;
	constexpr float TITLE_START_BUTTON_WIDTH = 340.0f;
	constexpr float TITLE_START_BUTTON_HEIGHT = 62.0f;

	constexpr float TITLE_EXIT_BUTTON_X = 470.0f;
	constexpr float TITLE_EXIT_BUTTON_Y = 555.0f;
	constexpr float TITLE_EXIT_BUTTON_WIDTH = 340.0f;
	constexpr float TITLE_EXIT_BUTTON_HEIGHT = 52.0f;

	constexpr unsigned char TITLE_START_GAME_KEY =
		VK_RETURN;

	constexpr unsigned char TITLE_EXIT_KEY =
		VK_ESCAPE;
}

class TitleScene final : public IScene
{
public:
	TitleScene(
		SceneManager& sceneManager,
		InputSystem& inputSystem,
		GraphicsSystem& graphicsSystem,
		AudioSystem& audioSystem );

	void Initialize() override;
	bool Init() override;
	void Update( float deltaTime ) override;
	void Draw() override;
	void Uninit() override;

private:
	void StartGame();

	SceneManager& m_SceneManager;
	InputSystem& m_InputSystem;
	GraphicsSystem& m_GraphicsSystem;
	AudioSystem& m_AudioSystem;

	HudRenderer m_HudRenderer;
	HudTextRenderer m_HudTextRenderer;
};