#pragma once

#include <windows.h>

#include "Framework/2D/H/HudRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

#include "Scene/Common/H/IScene.h"

class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

namespace
{
	constexpr float RESULT_SCREEN_WIDTH = 1280.0f;
	constexpr float RESULT_SCREEN_HEIGHT = 720.0f;

	constexpr float RESULT_PANEL_X = 250.0f;
	constexpr float RESULT_PANEL_Y = 70.0f;
	constexpr float RESULT_PANEL_WIDTH = 780.0f;
	constexpr float RESULT_PANEL_HEIGHT = 590.0f;

	constexpr float RESULT_RETURN_BUTTON_X = 450.0f;
	constexpr float RESULT_RETURN_BUTTON_Y = 570.0f;
	constexpr float RESULT_RETURN_BUTTON_WIDTH = 380.0f;
	constexpr float RESULT_RETURN_BUTTON_HEIGHT = 60.0f;

	constexpr unsigned char RESULT_RETURN_TO_TITLE_KEY =
		VK_RETURN;
}

class ResultScene final : public IScene
{
public:
	ResultScene(
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
	void ReturnToTitle();

	SceneManager& m_SceneManager;
	InputSystem& m_InputSystem;
	GraphicsSystem& m_GraphicsSystem;
	AudioSystem& m_AudioSystem;

	HudRenderer m_HudRenderer;
	HudTextRenderer m_HudTextRenderer;
};