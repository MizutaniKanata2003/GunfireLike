#include "../H/TitleScene.h"

#include <windows.h>

#include <DirectXColors.h>

#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/Input/H/InputSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"

#include "Scene/Common/H/SceneManager.h"

#include "Scene/ShopScene/H/ShopScene.h"

namespace
{
	constexpr float TITLE_BACKGROUND_RED = 0.015f;
	constexpr float TITLE_BACKGROUND_GREEN = 0.035f;
	constexpr float TITLE_BACKGROUND_BLUE = 0.090f;

	constexpr float TITLE_TOP_GLOW_RED = 0.05f;
	constexpr float TITLE_TOP_GLOW_GREEN = 0.20f;
	constexpr float TITLE_TOP_GLOW_BLUE = 0.42f;

	constexpr float TITLE_PANEL_RED = 0.035f;
	constexpr float TITLE_PANEL_GREEN = 0.09f;
	constexpr float TITLE_PANEL_BLUE = 0.19f;

	constexpr float TITLE_BORDER_RED = 0.10f;
	constexpr float TITLE_BORDER_GREEN = 0.58f;
	constexpr float TITLE_BORDER_BLUE = 0.92f;

	constexpr float TITLE_START_BUTTON_RED = 0.06f;
	constexpr float TITLE_START_BUTTON_GREEN = 0.48f;
	constexpr float TITLE_START_BUTTON_BLUE = 0.78f;

	constexpr float TITLE_EXIT_BUTTON_RED = 0.42f;
	constexpr float TITLE_EXIT_BUTTON_GREEN = 0.08f;
	constexpr float TITLE_EXIT_BUTTON_BLUE = 0.12f;

	constexpr float TITLE_BUTTON_BORDER = 3.0f;
}

TitleScene::TitleScene(
	SceneManager& sceneManager,
	InputSystem& inputSystem,
	GraphicsSystem& graphicsSystem,
	AudioSystem& audioSystem )
	: m_SceneManager( sceneManager )
	, m_InputSystem( inputSystem )
	, m_GraphicsSystem( graphicsSystem )
	, m_AudioSystem( audioSystem )
{
}

void TitleScene::Initialize()
{
	m_InputSystem.SetMouseCaptureEnabled(
		false );

	m_HudRenderer.Initialize(
		m_GraphicsSystem );

	m_HudTextRenderer.Initialize(
		m_GraphicsSystem );

	m_AudioSystem.PlayTitleBgm();
}

bool TitleScene::Init()
{
	return true;
}

void TitleScene::Update(
	float )
{
	if ( m_InputSystem.IsKeyTriggered(
		TITLE_START_GAME_KEY ) )
	{
		StartGame();
		return;
	}

	if ( m_InputSystem.IsKeyTriggered(
		TITLE_EXIT_KEY ) )
	{
		PostQuitMessage( 0 );
	}
}

void TitleScene::Draw()
{
	m_GraphicsSystem.SetDepthTestEnabled(
		false );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		0.0f,
		0.0f,
		TITLE_SCREEN_WIDTH,
		TITLE_SCREEN_HEIGHT,
		DirectX::XMFLOAT4(
		TITLE_BACKGROUND_RED,
		TITLE_BACKGROUND_GREEN,
		TITLE_BACKGROUND_BLUE,
		1.0f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		0.0f,
		0.0f,
		TITLE_SCREEN_WIDTH,
		180.0f,
		DirectX::XMFLOAT4(
		TITLE_TOP_GLOW_RED,
		TITLE_TOP_GLOW_GREEN,
		TITLE_TOP_GLOW_BLUE,
		0.65f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		TITLE_MAIN_PANEL_X - 4.0f,
		TITLE_MAIN_PANEL_Y - 4.0f,
		TITLE_MAIN_PANEL_WIDTH + 8.0f,
		TITLE_MAIN_PANEL_HEIGHT + 8.0f,
		DirectX::XMFLOAT4(
		TITLE_BORDER_RED,
		TITLE_BORDER_GREEN,
		TITLE_BORDER_BLUE,
		0.90f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		TITLE_MAIN_PANEL_X,
		TITLE_MAIN_PANEL_Y,
		TITLE_MAIN_PANEL_WIDTH,
		TITLE_MAIN_PANEL_HEIGHT,
		DirectX::XMFLOAT4(
		TITLE_PANEL_RED,
		TITLE_PANEL_GREEN,
		TITLE_PANEL_BLUE,
		0.96f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		TITLE_START_BUTTON_X -
		TITLE_BUTTON_BORDER,
		TITLE_START_BUTTON_Y -
		TITLE_BUTTON_BORDER,
		TITLE_START_BUTTON_WIDTH +
		TITLE_BUTTON_BORDER * 2.0f,
		TITLE_START_BUTTON_HEIGHT +
		TITLE_BUTTON_BORDER * 2.0f,
		DirectX::XMFLOAT4(
		0.35f,
		0.85f,
		1.0f,
		1.0f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		TITLE_START_BUTTON_X,
		TITLE_START_BUTTON_Y,
		TITLE_START_BUTTON_WIDTH,
		TITLE_START_BUTTON_HEIGHT,
		DirectX::XMFLOAT4(
		TITLE_START_BUTTON_RED,
		TITLE_START_BUTTON_GREEN,
		TITLE_START_BUTTON_BLUE,
		1.0f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		TITLE_EXIT_BUTTON_X -
		TITLE_BUTTON_BORDER,
		TITLE_EXIT_BUTTON_Y -
		TITLE_BUTTON_BORDER,
		TITLE_EXIT_BUTTON_WIDTH +
		TITLE_BUTTON_BORDER * 2.0f,
		TITLE_EXIT_BUTTON_HEIGHT +
		TITLE_BUTTON_BORDER * 2.0f,
		DirectX::XMFLOAT4(
		0.95f,
		0.25f,
		0.30f,
		1.0f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		TITLE_EXIT_BUTTON_X,
		TITLE_EXIT_BUTTON_Y,
		TITLE_EXIT_BUTTON_WIDTH,
		TITLE_EXIT_BUTTON_HEIGHT,
		DirectX::XMFLOAT4(
		TITLE_EXIT_BUTTON_RED,
		TITLE_EXIT_BUTTON_GREEN,
		TITLE_EXIT_BUTTON_BLUE,
		1.0f ) );

	m_HudTextRenderer.Begin();

	const std::wstring startButtonText =
		L"ゲーム開始";

	const std::wstring exitButtonText =
		L"終了";

	constexpr float startButtonTextScale =
		1.0f;

	constexpr float exitButtonTextScale =
		0.90f;

	const DirectX::XMFLOAT2 startTextSize =
		m_HudTextRenderer.MeasureText(
			startButtonText,
			startButtonTextScale );

	const DirectX::XMFLOAT2 exitTextSize =
		m_HudTextRenderer.MeasureText(
			exitButtonText,
			exitButtonTextScale );

	const DirectX::XMFLOAT2 startTextPosition =
	{
		TITLE_START_BUTTON_X +
		( TITLE_START_BUTTON_WIDTH -
		startTextSize.x ) *
		0.5f,

		TITLE_START_BUTTON_Y +
		( TITLE_START_BUTTON_HEIGHT -
		startTextSize.y ) *
		0.5f
	};

	const DirectX::XMFLOAT2 exitTextPosition =
	{
		TITLE_EXIT_BUTTON_X +
		( TITLE_EXIT_BUTTON_WIDTH -
		exitTextSize.x ) *
		0.5f,

		TITLE_EXIT_BUTTON_Y +
		( TITLE_EXIT_BUTTON_HEIGHT -
		exitTextSize.y ) *
		0.5f
	};

	m_HudTextRenderer.DrawText(
		L"GUNFIRE LIKE DX11",
		DirectX::XMFLOAT2(
		320.0f,
		155.0f ),
		DirectX::Colors::Gold,
		1.85f );

	m_HudTextRenderer.DrawText(
		L"5 ステージ サバイバル FPS",
		DirectX::XMFLOAT2(
		430.0f,
		245.0f ),
		DirectX::Colors::White,
		1.0f );

	m_HudTextRenderer.DrawText(
		L"敵を倒してゴールドを集め、装備を強化しよう。",
		DirectX::XMFLOAT2(
		310.0f,
		305.0f ),
		DirectX::Colors::LightGray,
		0.72f );

	m_HudTextRenderer.DrawText(
		L"WASD: 移動   マウス: 視点移動   左クリック: 射撃",
		DirectX::XMFLOAT2(
		340.0f,
		365.0f ),
		DirectX::Colors::White,
		0.70f );

	m_HudTextRenderer.DrawText(
		L"E: 調べる   Q: 特殊攻撃   F1: マウス固定切替",
		DirectX::XMFLOAT2(
		355.0f,
		400.0f ),
		DirectX::Colors::White,
		0.70f );

	m_HudTextRenderer.DrawText(
		startButtonText,
		startTextPosition,
		DirectX::Colors::White,
		startButtonTextScale );

	m_HudTextRenderer.DrawText(
		exitButtonText,
		exitTextPosition,
		DirectX::Colors::White,
		exitButtonTextScale );

	m_HudTextRenderer.DrawText(
		L"Enterキーでゲーム開始",
		DirectX::XMFLOAT2(
		515.0f,
		670.0f ),
		DirectX::Colors::LightGray,
		0.70f );

	m_HudTextRenderer.End();

	m_GraphicsSystem.SetDepthTestEnabled(
		true );

	m_GraphicsSystem.SetAlphaBlendEnabled(
		false );
}

void TitleScene::Uninit()
{
	m_HudTextRenderer.Uninit();
	m_HudRenderer.Uninit();
}

void TitleScene::StartGame()
{
	m_AudioSystem.PlayWarpSe();

	m_SceneManager.StartNewGame();

	m_SceneManager.
		RequestSceneChange<ShopScene>();
}