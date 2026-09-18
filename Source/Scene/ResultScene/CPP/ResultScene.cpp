#include "../H/ResultScene.h"

//========= C++標準ライブラリ インクルード=========
#include <cwchar>
#include <string>

//========= Windows インクルード=========
#include <windows.h>

//========= DirectX インクルード=========
#include <DirectXColors.h>

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/H/GameProgress.h"
#include "Scene/Common/H/SceneManager.h"
#include "Scene/TitleScene/H/TitleScene.h"

namespace
{
	//========= 入力定数=========
	// タイトル画面へ戻る操作に使用するキー。
	constexpr unsigned char RESULT_RETURN_TO_TITLE_KEY = VK_RETURN;

	//========= 画面サイズ定数=========
	// Result画面UIの基準画面サイズ。
	constexpr float RESULT_SCREEN_WIDTH = 1280.0f;
	constexpr float RESULT_SCREEN_HEIGHT = 720.0f;

	//========= パネル描画定数=========
	// Result画面のメインパネル位置とサイズ。
	constexpr float RESULT_PANEL_X = 250.0f;
	constexpr float RESULT_PANEL_Y = 70.0f;
	constexpr float RESULT_PANEL_WIDTH = 780.0f;
	constexpr float RESULT_PANEL_HEIGHT = 590.0f;

	//========= ボタン描画定数=========
	// タイトルへ戻るボタンの位置とサイズ。
	constexpr float RESULT_RETURN_BUTTON_X = 450.0f;
	constexpr float RESULT_RETURN_BUTTON_Y = 570.0f;
	constexpr float RESULT_RETURN_BUTTON_WIDTH = 380.0f;
	constexpr float RESULT_RETURN_BUTTON_HEIGHT = 60.0f;

	// ボタンの外枠の太さ。
	constexpr float RESULT_BUTTON_BORDER = 3.0f;

	//========= 背景・パネル色定数=========
	// Result画面の背景色。
	constexpr float RESULT_BACKGROUND_RED = 0.075f;
	constexpr float RESULT_BACKGROUND_GREEN = 0.018f;
	constexpr float RESULT_BACKGROUND_BLUE = 0.13f;

	// Result画面上部の発光色。
	constexpr float RESULT_TOP_GLOW_RED = 0.38f;
	constexpr float RESULT_TOP_GLOW_GREEN = 0.16f;
	constexpr float RESULT_TOP_GLOW_BLUE = 0.05f;

	// メインパネル外枠の色。
	constexpr float RESULT_PANEL_BORDER_RED = 0.92f;
	constexpr float RESULT_PANEL_BORDER_GREEN = 0.62f;
	constexpr float RESULT_PANEL_BORDER_BLUE = 0.12f;

	// メインパネルの色。
	constexpr float RESULT_PANEL_RED = 0.18f;
	constexpr float RESULT_PANEL_GREEN = 0.05f;
	constexpr float RESULT_PANEL_BLUE = 0.25f;

	// タイトルへ戻るボタンの色。
	constexpr float RESULT_BUTTON_RED = 0.72f;
	constexpr float RESULT_BUTTON_GREEN = 0.35f;
	constexpr float RESULT_BUTTON_BLUE = 0.05f;

	//========= Alpha定数=========
	// Result画面の各Quadに使用するAlpha値。
	constexpr float FULLY_OPAQUE_ALPHA = 1.0f;
	constexpr float RESULT_TOP_GLOW_ALPHA = 0.50f;
	constexpr float RESULT_PANEL_BORDER_ALPHA = 0.95f;
	constexpr float RESULT_PANEL_ALPHA = 0.97f;

	//========= 文字描画定数=========
	// タイトルへ戻るボタンの文字Scale。
	constexpr float RESULT_RETURN_BUTTON_TEXT_SCALE = 0.95f;
}

// ResultSceneが使用するSceneManagerとFramework Systemを登録する。
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

// Result画面の入力設定、HUD、文字、BGMを初期化する。
void ResultScene::Initialize()
{
	// Result画面ではFPSマウスキャプチャを解除する。
	m_InputSystem.SetMouseCaptureEnabled( false );

	// Result画面で使用するHUD Quadと文字描画を初期化する。
	m_HudRenderer.Initialize( m_GraphicsSystem );
	m_HudTextRenderer.Initialize( m_GraphicsSystem );

	// Result画面のBGMを再生する。
	m_AudioSystem.PlayResultBgm();
}

// Result画面の入力を更新する。
void ResultScene::Update( float deltaTime )
{
	if ( m_InputSystem.IsKeyTriggered( RESULT_RETURN_TO_TITLE_KEY ) ) ReturnToTitle();
}

// 最終結果、背景、タイトルへ戻るボタンを描画する。
void ResultScene::Draw()
{
	// SceneManagerが所有するゲーム進捗と最終Player能力を取得する。
	const GameProgress& progress = m_SceneManager.GetGameProgress();
	const PlayerStats& playerStats = progress.GetPlayerStats();

	// Result画面のQuadと文字を画面固定で描画する。
	m_GraphicsSystem.SetDepthTestEnabled( false );

	// 背景、上部発光、メインパネルを描画する。
	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		0.0f,
		0.0f,
		RESULT_SCREEN_WIDTH,
		RESULT_SCREEN_HEIGHT,
		DirectX::XMFLOAT4{
			RESULT_BACKGROUND_RED,
			RESULT_BACKGROUND_GREEN,
			RESULT_BACKGROUND_BLUE,
			FULLY_OPAQUE_ALPHA } );
	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		0.0f,
		0.0f,
		RESULT_SCREEN_WIDTH,
		170.0f,
		DirectX::XMFLOAT4{
			RESULT_TOP_GLOW_RED,
			RESULT_TOP_GLOW_GREEN,
			RESULT_TOP_GLOW_BLUE,
			RESULT_TOP_GLOW_ALPHA } );
	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		RESULT_PANEL_X - 4.0f,
		RESULT_PANEL_Y - 4.0f,
		RESULT_PANEL_WIDTH + 8.0f,
		RESULT_PANEL_HEIGHT + 8.0f,
		DirectX::XMFLOAT4{
			RESULT_PANEL_BORDER_RED,
			RESULT_PANEL_BORDER_GREEN,
			RESULT_PANEL_BORDER_BLUE,
			RESULT_PANEL_BORDER_ALPHA } );
	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		RESULT_PANEL_X,
		RESULT_PANEL_Y,
		RESULT_PANEL_WIDTH,
		RESULT_PANEL_HEIGHT,
		DirectX::XMFLOAT4{
			RESULT_PANEL_RED,
			RESULT_PANEL_GREEN,
			RESULT_PANEL_BLUE,
			RESULT_PANEL_ALPHA } );

	// タイトルへ戻るボタンの外枠と本体を描画する。
	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		RESULT_RETURN_BUTTON_X - RESULT_BUTTON_BORDER,
		RESULT_RETURN_BUTTON_Y - RESULT_BUTTON_BORDER,
		RESULT_RETURN_BUTTON_WIDTH + RESULT_BUTTON_BORDER * 2.0f,
		RESULT_RETURN_BUTTON_HEIGHT + RESULT_BUTTON_BORDER * 2.0f,
		DirectX::XMFLOAT4{ 1.0f, 0.80f, 0.25f, FULLY_OPAQUE_ALPHA } );
	m_HudRenderer.DrawQuad(
		m_GraphicsSystem,
		RESULT_RETURN_BUTTON_X,
		RESULT_RETURN_BUTTON_Y,
		RESULT_RETURN_BUTTON_WIDTH,
		RESULT_RETURN_BUTTON_HEIGHT,
		DirectX::XMFLOAT4{
			RESULT_BUTTON_RED,
			RESULT_BUTTON_GREEN,
			RESULT_BUTTON_BLUE,
			FULLY_OPAQUE_ALPHA } );

	// ボタン内文字の位置計算に使用する文字列と描画サイズを取得する。
	const std::wstring returnButtonText{ L"タイトルへ戻る" };
	const DirectX::XMFLOAT2 returnTextSize = m_HudTextRenderer.MeasureText( returnButtonText, RESULT_RETURN_BUTTON_TEXT_SCALE );

	// ボタン中央へ文字を配置する座標を計算する。
	const DirectX::XMFLOAT2 returnTextPosition
	{
		RESULT_RETURN_BUTTON_X + ( RESULT_RETURN_BUTTON_WIDTH - returnTextSize.x ) * 0.5f,
		RESULT_RETURN_BUTTON_Y + ( RESULT_RETURN_BUTTON_HEIGHT - returnTextSize.y ) * 0.5f
	};

	// Result画面のタイトルと最終結果を描画する。
	m_HudTextRenderer.Begin();
	m_HudTextRenderer.DrawText(
		L"ゲームクリア！",
		DirectX::XMFLOAT2{ 485.0f, 115.0f },
		DirectX::Colors::Gold,
		1.90f );
	m_HudTextRenderer.DrawText(
		L"最終結果",
		DirectX::XMFLOAT2{ 565.0f, 200.0f },
		DirectX::Colors::White,
		1.0f );

	// 各最終結果を文字列へ変換して描画する。
	wchar_t text[ 128 ]{};

	swprintf_s( text, L"クリア時間: %.1f 秒", progress.GetTotalPlayTime() );
	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2{ 445.0f, 265.0f },
		DirectX::Colors::White,
		0.90f );

	swprintf_s( text, L"総ダメージ: %.0f", progress.GetTotalDamageDealt() );
	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2{ 445.0f, 305.0f },
		DirectX::Colors::White,
		0.90f );

	swprintf_s( text, L"死亡回数: %d", progress.GetTotalDeaths() );
	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2{ 445.0f, 345.0f },
		DirectX::Colors::White,
		0.90f );

	swprintf_s( text, L"撃破数: %d", progress.GetTotalEnemiesDefeated() );
	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2{ 445.0f, 385.0f },
		DirectX::Colors::White,
		0.90f );

	swprintf_s( text, L"最終ゴールド: %d G", progress.GetCurrency() );
	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2{ 445.0f, 435.0f },
		DirectX::Colors::Gold,
		0.90f );

	swprintf_s(
		text,
		L"最終HP: %.0f   最終攻撃力: %.0f",
		playerStats.maxHp,
		playerStats.gunDamage );
	m_HudTextRenderer.DrawText(
		text,
		DirectX::XMFLOAT2{ 410.0f, 485.0f },
		DirectX::Colors::LightGray,
		0.78f );

	// ボタン文字とタイトルへ戻る操作説明を描画する。
	m_HudTextRenderer.DrawText(
		returnButtonText,
		returnTextPosition,
		DirectX::Colors::White,
		RESULT_RETURN_BUTTON_TEXT_SCALE );
	m_HudTextRenderer.DrawText(
		L"Enterキーでタイトルへ戻る",
		DirectX::XMFLOAT2{ 500.0f, 680.0f },
		DirectX::Colors::LightGray,
		0.65f );
	m_HudTextRenderer.End();

	// 次の3D描画へ影響を残さないようDepth TestとAlpha Blendを戻す。
	m_GraphicsSystem.SetDepthTestEnabled( true );
	m_GraphicsSystem.SetAlphaBlendEnabled( false );
}

// Result画面で使用したHUDと文字描画リソースを解放する。
void ResultScene::Uninit()
{
	m_HudTextRenderer.Uninit();
	m_HudRenderer.Uninit();
}

// タイトル画面へのScene遷移を予約する。
void ResultScene::ReturnToTitle()
{
	m_AudioSystem.PlayWarpSe();
	m_SceneManager.RequestSceneChange<TitleScene>();
}