#include "../H/TitleScene.h"

//========= Windows インクルード=========
#include <windows.h>

//========= DirectX インクルード=========
#include <DirectXColors.h>

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/H/SceneManager.h"
#include "Scene/ShopScene/H/ShopScene.h"

namespace
{
	//========= 画面サイズ定数=========
	// タイトル画面UIの基準画面サイズ。
	constexpr float TITLE_SCREEN_WIDTH = 1280.0f;
	constexpr float TITLE_SCREEN_HEIGHT = 720.0f;

	//========= パネル描画定数=========
	// タイトル画面のメインパネル位置とサイズ。
	constexpr float TITLE_MAIN_PANEL_X = 190.0f;
	constexpr float TITLE_MAIN_PANEL_Y = 90.0f;
	constexpr float TITLE_MAIN_PANEL_WIDTH = 900.0f;
	constexpr float TITLE_MAIN_PANEL_HEIGHT = 560.0f;

	//========= ボタン描画定数=========
	// ゲーム開始ボタンの位置とサイズ。
	constexpr float TITLE_START_BUTTON_X = 470.0f;
	constexpr float TITLE_START_BUTTON_Y = 480.0f;
	constexpr float TITLE_START_BUTTON_WIDTH = 340.0f;
	constexpr float TITLE_START_BUTTON_HEIGHT = 62.0f;

	// 終了ボタンの位置とサイズ。
	constexpr float TITLE_EXIT_BUTTON_X = 470.0f;
	constexpr float TITLE_EXIT_BUTTON_Y = 555.0f;
	constexpr float TITLE_EXIT_BUTTON_WIDTH = 340.0f;
	constexpr float TITLE_EXIT_BUTTON_HEIGHT = 52.0f;

	// ボタンの外枠の太さ。
	constexpr float TITLE_BUTTON_BORDER = 3.0f;

	//========= 入力定数=========
	// ゲーム開始と終了に使用するキーコード。
	constexpr unsigned char TITLE_START_GAME_KEY = VK_RETURN;
	constexpr unsigned char TITLE_EXIT_KEY = VK_ESCAPE;

	//========= 背景・パネル色定数=========
	// タイトル画面の背景色。
	constexpr float TITLE_BACKGROUND_RED = 0.015f;
	constexpr float TITLE_BACKGROUND_GREEN = 0.035f;
	constexpr float TITLE_BACKGROUND_BLUE = 0.090f;

	// タイトル画面上部の発光色。
	constexpr float TITLE_TOP_GLOW_RED = 0.05f;
	constexpr float TITLE_TOP_GLOW_GREEN = 0.20f;
	constexpr float TITLE_TOP_GLOW_BLUE = 0.42f;

	// メインパネルの色。
	constexpr float TITLE_PANEL_RED = 0.035f;
	constexpr float TITLE_PANEL_GREEN = 0.09f;
	constexpr float TITLE_PANEL_BLUE = 0.19f;

	// メインパネル外枠の色。
	constexpr float TITLE_BORDER_RED = 0.10f;
	constexpr float TITLE_BORDER_GREEN = 0.58f;
	constexpr float TITLE_BORDER_BLUE = 0.92f;

	// ゲーム開始ボタンの色。
	constexpr float TITLE_START_BUTTON_RED = 0.06f;
	constexpr float TITLE_START_BUTTON_GREEN = 0.48f;
	constexpr float TITLE_START_BUTTON_BLUE = 0.78f;

	// 終了ボタンの色。
	constexpr float TITLE_EXIT_BUTTON_RED = 0.42f;
	constexpr float TITLE_EXIT_BUTTON_GREEN = 0.08f;
	constexpr float TITLE_EXIT_BUTTON_BLUE = 0.12f;

	//========= Alpha定数=========
	// タイトル画面の各Quadに使用するAlpha値。
	constexpr float FULLY_OPAQUE_ALPHA = 1.0f;
	constexpr float TITLE_TOP_GLOW_ALPHA = 0.65f;
	constexpr float TITLE_BORDER_ALPHA = 0.90f;
	constexpr float TITLE_PANEL_ALPHA = 0.96f;

	//========= 文字描画定数=========
	// ボタン内文字の描画Scale。
	constexpr float TITLE_START_BUTTON_TEXT_SCALE = 1.0f;
	constexpr float TITLE_EXIT_BUTTON_TEXT_SCALE = 0.90f;
}

// TitleSceneが使用するSceneManagerとFramework Systemを登録する。
TitleScene::TitleScene( SceneManager& sceneManager, InputSystem& inputSystem, GraphicsSystem& graphicsSystem, AudioSystem& audioSystem )
	: m_SceneManager( sceneManager )
	, m_InputSystem( inputSystem )
	, m_GraphicsSystem( graphicsSystem )
	, m_AudioSystem( audioSystem )
{
}

// タイトル画面の入力設定、HUD、文字、BGMを初期化する。
void TitleScene::Initialize()
{
	// タイトル画面ではImGui操作を優先するため、FPSマウスキャプチャを解除する。
	m_InputSystem.SetMouseCaptureEnabled( false );

	// タイトル画面で使用するHUD Quadと文字描画を初期化する。
	m_HudRenderer.Initialize( m_GraphicsSystem );
	m_HudTextRenderer.Initialize( m_GraphicsSystem );

	// タイトル画面のBGMを再生する。
	m_AudioSystem.PlayTitleBgm();
}

// タイトル画面の入力を更新する。
void TitleScene::Update( float deltaTime )
{
	if ( m_InputSystem.IsKeyTriggered( TITLE_START_GAME_KEY ) )
	{
		StartGame();
		return;
	}

	if ( m_InputSystem.IsKeyTriggered( TITLE_EXIT_KEY ) ) PostQuitMessage( 0 );
}

// タイトル画面の背景、操作説明、ボタンを描画する。
void TitleScene::Draw()
{
	// タイトル画面のQuadと文字を画面固定で描画する。
	m_GraphicsSystem.SetDepthTestEnabled( false );

	// 背景、上部発光、メインパネルを描画する。
	m_HudRenderer.DrawQuad(
	m_GraphicsSystem,
	0.0f,
	0.0f,
	TITLE_SCREEN_WIDTH,
	TITLE_SCREEN_HEIGHT,
	DirectX::XMFLOAT4{ TITLE_BACKGROUND_RED,TITLE_BACKGROUND_GREEN,TITLE_BACKGROUND_BLUE,FULLY_OPAQUE_ALPHA } );

	m_HudRenderer.DrawQuad(
	m_GraphicsSystem,
	0.0f,
	0.0f,
	TITLE_SCREEN_WIDTH,
	180.0f,
	DirectX::XMFLOAT4{ TITLE_TOP_GLOW_RED,TITLE_TOP_GLOW_GREEN,TITLE_TOP_GLOW_BLUE,TITLE_TOP_GLOW_ALPHA } );

	m_HudRenderer.DrawQuad(
	m_GraphicsSystem,
	TITLE_MAIN_PANEL_X - 4.0f,
	TITLE_MAIN_PANEL_Y - 4.0f,
	TITLE_MAIN_PANEL_WIDTH + 8.0f,
	TITLE_MAIN_PANEL_HEIGHT + 8.0f,
	DirectX::XMFLOAT4{ TITLE_BORDER_RED,TITLE_BORDER_GREEN,TITLE_BORDER_BLUE,TITLE_BORDER_ALPHA } );

	m_HudRenderer.DrawQuad(
	m_GraphicsSystem,
	TITLE_MAIN_PANEL_X,
	TITLE_MAIN_PANEL_Y,
	TITLE_MAIN_PANEL_WIDTH,
	TITLE_MAIN_PANEL_HEIGHT,
	DirectX::XMFLOAT4{ TITLE_PANEL_RED,TITLE_PANEL_GREEN,TITLE_PANEL_BLUE,TITLE_PANEL_ALPHA } );

	// ゲーム開始ボタンと終了ボタンを描画する。
	m_HudRenderer.DrawQuad(
	m_GraphicsSystem,
	TITLE_START_BUTTON_X - TITLE_BUTTON_BORDER,
	TITLE_START_BUTTON_Y - TITLE_BUTTON_BORDER,
	TITLE_START_BUTTON_WIDTH + TITLE_BUTTON_BORDER * 2.0f,
	TITLE_START_BUTTON_HEIGHT + TITLE_BUTTON_BORDER * 2.0f,
	DirectX::XMFLOAT4{ 0.35f, 0.85f, 1.0f, FULLY_OPAQUE_ALPHA } );

	m_HudRenderer.DrawQuad(
	m_GraphicsSystem,
	TITLE_START_BUTTON_X,
	TITLE_START_BUTTON_Y,
	TITLE_START_BUTTON_WIDTH,
	TITLE_START_BUTTON_HEIGHT,
	DirectX::XMFLOAT4{ TITLE_START_BUTTON_RED,TITLE_START_BUTTON_GREEN,TITLE_START_BUTTON_BLUE,FULLY_OPAQUE_ALPHA } );

	m_HudRenderer.DrawQuad(
	m_GraphicsSystem,
	TITLE_EXIT_BUTTON_X - TITLE_BUTTON_BORDER,
	TITLE_EXIT_BUTTON_Y - TITLE_BUTTON_BORDER,
	TITLE_EXIT_BUTTON_WIDTH + TITLE_BUTTON_BORDER * 2.0f,
	TITLE_EXIT_BUTTON_HEIGHT + TITLE_BUTTON_BORDER * 2.0f,
	DirectX::XMFLOAT4{ 0.95f, 0.25f, 0.30f, FULLY_OPAQUE_ALPHA } );

	m_HudRenderer.DrawQuad(
	m_GraphicsSystem,
	TITLE_EXIT_BUTTON_X,
	TITLE_EXIT_BUTTON_Y,
	TITLE_EXIT_BUTTON_WIDTH,
	TITLE_EXIT_BUTTON_HEIGHT,
	DirectX::XMFLOAT4{ TITLE_EXIT_BUTTON_RED,TITLE_EXIT_BUTTON_GREEN,TITLE_EXIT_BUTTON_BLUE,FULLY_OPAQUE_ALPHA } );

	// ボタン内文字の位置計算に使用する文字列と描画サイズを取得する。
	const std::wstring startButtonText{ L"ゲーム開始" };
	const std::wstring exitButtonText{ L"終了" };

	const DirectX::XMFLOAT2 startTextSize = m_HudTextRenderer.MeasureText( startButtonText, TITLE_START_BUTTON_TEXT_SCALE );
	const DirectX::XMFLOAT2 exitTextSize = m_HudTextRenderer.MeasureText( exitButtonText, TITLE_EXIT_BUTTON_TEXT_SCALE );

	// ボタン中央へ文字を配置する座標を計算する。
	const DirectX::XMFLOAT2 startTextPosition
	{
	TITLE_START_BUTTON_X + ( TITLE_START_BUTTON_WIDTH - startTextSize.x ) * 0.5f,
	TITLE_START_BUTTON_Y + ( TITLE_START_BUTTON_HEIGHT - startTextSize.y ) * 0.5f
	};
	const DirectX::XMFLOAT2 exitTextPosition
	{
	TITLE_EXIT_BUTTON_X + ( TITLE_EXIT_BUTTON_WIDTH - exitTextSize.x ) * 0.5f,
	TITLE_EXIT_BUTTON_Y + ( TITLE_EXIT_BUTTON_HEIGHT - exitTextSize.y ) * 0.5f
	};

	// タイトル、説明文、操作説明、ボタン文字を描画する。
	m_HudTextRenderer.Begin();

	m_HudTextRenderer.DrawText(
	L"GUNFIRE LIKE DX11",
	DirectX::XMFLOAT2{ 320.0f, 155.0f },
	DirectX::Colors::Gold,
	1.85f );

	m_HudTextRenderer.DrawText(
	L"5 ステージ サバイバル FPS",
	DirectX::XMFLOAT2{ 430.0f, 245.0f },
	DirectX::Colors::White,
	1.0f );

	m_HudTextRenderer.DrawText(
	L"敵を倒してゴールドを集め、装備を強化しよう。",
	DirectX::XMFLOAT2{ 310.0f, 305.0f },
	DirectX::Colors::LightGray,
	0.72f );

	m_HudTextRenderer.DrawText(
	L"WASD: 移動   マウス: 視点移動   左クリック: 射撃",
	DirectX::XMFLOAT2{ 340.0f, 365.0f },
	DirectX::Colors::White,
	0.70f );

	m_HudTextRenderer.DrawText(
	L"E: 調べる   Q: 特殊攻撃   F1: マウス固定切替",
	DirectX::XMFLOAT2{ 355.0f, 400.0f },
	DirectX::Colors::White,
	0.70f );

	m_HudTextRenderer.DrawText(
	startButtonText,
	startTextPosition,
	DirectX::Colors::White,
	TITLE_START_BUTTON_TEXT_SCALE );

	m_HudTextRenderer.DrawText(
	exitButtonText,
	exitTextPosition,
	DirectX::Colors::White,
	TITLE_EXIT_BUTTON_TEXT_SCALE );

	m_HudTextRenderer.DrawText(
	L"Enterキーでゲーム開始",
	DirectX::XMFLOAT2{ 515.0f, 670.0f },
	DirectX::Colors::LightGray,
	0.70f );

	m_HudTextRenderer.End();

	// 次の3D描画へ影響を残さないようDepth TestとAlpha Blendを戻す。
	m_GraphicsSystem.SetDepthTestEnabled( true );
	m_GraphicsSystem.SetAlphaBlendEnabled( false );
}

// タイトル画面で使用したHUDと文字描画リソースを終了する。
void TitleScene::Finalize()
{
	m_HudTextRenderer.Uninit();
	m_HudRenderer.Uninit();
}

// 新しいゲームを開始してShopSceneへの遷移を予約する。
void TitleScene::StartGame()
{
	// Scene遷移に使用するSEを再生し、ゲーム進捗を初期化する。
	m_AudioSystem.PlayWarpSe();
	m_SceneManager.StartNewGame();

	m_SceneManager.RequestSceneChange<ShopScene>();
}