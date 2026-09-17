#include "../H/ResultScene.h"

#include <cwchar>
#include <string>
#include <DirectXColors.h>

#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/Input/H/InputSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"

#include "Scene/Common/H/SceneManager.h"
#include "Scene/Common/H/GameProgress.h"

#include "Scene/TitleScene/H/TitleScene.h"

namespace
{
	constexpr float RESULT_BACKGROUND_RED = 0.075f;
	constexpr float RESULT_BACKGROUND_GREEN = 0.018f;
	constexpr float RESULT_BACKGROUND_BLUE = 0.13f;

	constexpr float RESULT_TOP_GLOW_RED = 0.38f;
	constexpr float RESULT_TOP_GLOW_GREEN = 0.16f;
	constexpr float RESULT_TOP_GLOW_BLUE = 0.05f;

	constexpr float RESULT_PANEL_BORDER_RED = 0.92f;
	constexpr float RESULT_PANEL_BORDER_GREEN = 0.62f;
	constexpr float RESULT_PANEL_BORDER_BLUE = 0.12f;

	constexpr float RESULT_PANEL_RED = 0.18f;
	constexpr float RESULT_PANEL_GREEN = 0.05f;
	constexpr float RESULT_PANEL_BLUE = 0.25f;

	constexpr float RESULT_BUTTON_RED = 0.72f;
	constexpr float RESULT_BUTTON_GREEN = 0.35f;
	constexpr float RESULT_BUTTON_BLUE = 0.05f;

	constexpr float RESULT_BUTTON_BORDER = 3.0f;
}

ResultScene::ResultScene(
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

void ResultScene::Initialize()
{
	m_InputSystem.SetMouseCaptureEnabled(
		false );

	m_HudRenderer.Initialize(
		m_GraphicsSystem );

	m_HudTextRenderer.Initialize(
		m_GraphicsSystem );

	m_AudioSystem.PlayResultBgm();
}

bool ResultScene::Init()
{
	return true;
}

void ResultScene::Update(
	float )
{
	if ( m_InputSystem.IsKeyTriggered(
		RESULT_RETURN_TO_TITLE_KEY ) )
	{
		ReturnToTitle();
	}
}

void ResultScene::Draw()
{
	const GameProgress& progress =
		m_SceneManager.GetGameProgress();

	const PlayerStats& playerStats =
		progress.GetPlayerStats();

	m_GraphicsSystem.SetDepthTestEnabled(
		false );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		0.0f,
		0.0f,
		RESULT_SCREEN_WIDTH,
		RESULT_SCREEN_HEIGHT,
		DirectX::XMFLOAT4(
		RESULT_BACKGROUND_RED,
		RESULT_BACKGROUND_GREEN,
		RESULT_BACKGROUND_BLUE,
		1.0f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		0.0f,
		0.0f,
		RESULT_SCREEN_WIDTH,
		170.0f,
		DirectX::XMFLOAT4(
		RESULT_TOP_GLOW_RED,
		RESULT_TOP_GLOW_GREEN,
		RESULT_TOP_GLOW_BLUE,
		0.50f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		RESULT_PANEL_X - 4.0f,
		RESULT_PANEL_Y - 4.0f,
		RESULT_PANEL_WIDTH + 8.0f,
		RESULT_PANEL_HEIGHT + 8.0f,
		DirectX::XMFLOAT4(
		RESULT_PANEL_BORDER_RED,
		RESULT_PANEL_BORDER_GREEN,
		RESULT_PANEL_BORDER_BLUE,
		0.95f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		RESULT_PANEL_X,
		RESULT_PANEL_Y,
		RESULT_PANEL_WIDTH,
		RESULT_PANEL_HEIGHT,
		DirectX::XMFLOAT4(
		RESULT_PANEL_RED,
		RESULT_PANEL_GREEN,
		RESULT_PANEL_BLUE,
		0.97f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		RESULT_RETURN_BUTTON_X -
		RESULT_BUTTON_BORDER,
		RESULT_RETURN_BUTTON_Y -
		RESULT_BUTTON_BORDER,
		RESULT_RETURN_BUTTON_WIDTH +
		RESULT_BUTTON_BORDER * 2.0f,
		RESULT_RETURN_BUTTON_HEIGHT +
		RESULT_BUTTON_BORDER * 2.0f,
		DirectX::XMFLOAT4(
		1.0f,
		0.80f,
		0.25f,
		1.0f ) );

	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		RESULT_RETURN_BUTTON_X,
		RESULT_RETURN_BUTTON_Y,
		RESULT_RETURN_BUTTON_WIDTH,
		RESULT_RETURN_BUTTON_HEIGHT,
		DirectX::XMFLOAT4(
		RESULT_BUTTON_RED,
		RESULT_BUTTON_GREEN,
		RESULT_BUTTON_BLUE,
		1.0f ) );

	m_HudTextRenderer.Begin();

	const std::wstring returnButtonText =
		L"タイトルへ戻る";

	constexpr float returnButtonTextScale =
		0.95f;

	const DirectX::XMFLOAT2 returnTextSize =
		m_HudTextRenderer.MeasureText(
			returnButtonText,
			returnButtonTextScale );

	const DirectX::XMFLOAT2 returnTextPosition =
	{
		RESULT_RETURN_BUTTON_X +
		( RESULT_RETURN_BUTTON_WIDTH -
		returnTextSize.x ) *
		0.5f,

		RESULT_RETURN_BUTTON_Y +
		( RESULT_RETURN_BUTTON_HEIGHT -
		returnTextSize.y ) *
		0.5f
	};

	m_HudTextRenderer.DrawText(
		L"ゲームクリア！",
		DirectX::XMFLOAT2(
		485.0f,
		115.0f ),
		DirectX::Colors::Gold,
		1.90f );

	m_HudTextRenderer.DrawText(
		L"最終結果",
		DirectX::XMFLOAT2(
		565.0f,
		200.0f ),
		DirectX::Colors::White,
		1.0f );

	wchar_t text[ 128 ]{};

	swprintf_s(
		text,
		L"クリア時間: %.1f 秒",
		progress.GetTotalPlayTime() );

	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2(
		445.0f,
		265.0f ),
		DirectX::Colors::White,
		0.90f );

	swprintf_s(
		text,
		L"総ダメージ: %.0f",
		progress.GetTotalDamageDealt() );

	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2(
		445.0f,
		305.0f ),
		DirectX::Colors::White,
		0.90f );

	swprintf_s(
		text,
		L"死亡回数: %d",
		progress.GetTotalDeaths() );

	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2(
		445.0f,
		345.0f ),
		DirectX::Colors::White,
		0.90f );

	swprintf_s(
		text,
		L"撃破数: %d",
		progress.GetTotalEnemiesDefeated() );

	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2(
		445.0f,
		385.0f ),
		DirectX::Colors::White,
		0.90f );

	swprintf_s(
		text,
		L"最終ゴールド: %d G",
		progress.GetCurrency() );

	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2(
		445.0f,
		435.0f ),
		DirectX::Colors::Gold,
		0.90f );

	swprintf_s(
		text,
		L"最終HP: %.0f   最終攻撃力: %.0f",
		playerStats.maxHp,
		playerStats.gunDamage );

	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2(
		410.0f,
		485.0f ),
		DirectX::Colors::LightGray,
		0.78f );

	m_HudTextRenderer.DrawText(
		returnButtonText,
		returnTextPosition,
		DirectX::Colors::White,
		returnButtonTextScale );

	m_HudTextRenderer.DrawText(
		L"Enterキーでタイトルへ戻る",
		DirectX::XMFLOAT2(
		500.0f,
		680.0f ),
		DirectX::Colors::LightGray,
		0.65f );

	m_HudTextRenderer.End();

	m_GraphicsSystem.SetDepthTestEnabled(
		true );

	m_GraphicsSystem.SetAlphaBlendEnabled(
		false );
}

void ResultScene::Uninit()
{
	m_HudTextRenderer.Uninit();
	m_HudRenderer.Uninit();
}

void ResultScene::ReturnToTitle()
{
	m_AudioSystem.PlayWarpSe();

	m_SceneManager.
		RequestSceneChange<TitleScene>();
}