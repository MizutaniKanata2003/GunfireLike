#include "../H/ShopScene.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>

//========= DirectX インクルード=========
#include <DirectXCollision.h>
#include <DirectXColors.h>

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/H/SceneManager.h"
#include "Scene/GameScene/H/GameScene.h"
#include "Scene/TitleScene/H/TitleScene.h"

namespace
{
	//========= 入力定数=========
	// Shop内の操作とFPSマウスキャプチャ切替に使用するキー。
	constexpr unsigned char SHOP_USE_INTERACTION_KEY = 'E';
	constexpr unsigned char SHOP_TOGGLE_MOUSE_CAPTURE_KEY = VK_F1;

	//========= Camera設定定数=========
	// Shopの3D描画に使用するProjection設定。
	constexpr float SHOP_ASPECT_RATIO = 1280.0f / 720.0f;
	constexpr float SHOP_NEAR_CLIP = 0.1f;
	constexpr float SHOP_FAR_CLIP = 1000.0f;
	constexpr float SHOP_FIELD_FOV_DEGREES = 60.0f;

	//========= フィールド定数=========
	// 床のScaleとワールド座標。
	constexpr float SHOP_FLOOR_SCALE_X = 18.0f;
	constexpr float SHOP_FLOOR_SCALE_Y = 0.2f;
	constexpr float SHOP_FLOOR_SCALE_Z = 18.0f;
	constexpr float SHOP_FLOOR_POSITION_X = 0.0f;
	constexpr float SHOP_FLOOR_POSITION_Y = -0.6f;
	constexpr float SHOP_FLOOR_POSITION_Z = 7.0f;

	// 壁のサイズと配置に使用する値。
	constexpr float SHOP_WALL_THICKNESS = 0.2f;
	constexpr float SHOP_WALL_HEIGHT = 3.2f;
	constexpr float SHOP_WALL_LENGTH = 18.0f;
	constexpr float SHOP_LEFT_WALL_X = -9.0f;
	constexpr float SHOP_RIGHT_WALL_X = 9.0f;
	constexpr float SHOP_NEAR_WALL_Z = -2.0f;
	constexpr float SHOP_FAR_WALL_Z = 16.0f;
	constexpr float SHOP_WALL_CENTER_X = 0.0f;
	constexpr float SHOP_WALL_CENTER_Y = 1.1f;
	constexpr float SHOP_WALL_CENTER_Z = 7.0f;

	//========= Skybox定数=========
	// 簡易SkyboxのScaleと単色表示色。
	constexpr float SHOP_SKYBOX_SCALE = 250.0f;
	constexpr float SHOP_SKY_COLOR_RED = 0.08f;
	constexpr float SHOP_SKY_COLOR_GREEN = 0.18f;
	constexpr float SHOP_SKY_COLOR_BLUE = 0.42f;
	constexpr float SHOP_SKY_COLOR_ALPHA = 1.0f;

	//========= 強化Object定数=========
	// 強化ObjectのScale、アニメーション、Ray判定設定。
	constexpr float SHOP_UPGRADE_ORB_SCALE = 0.65f;
	constexpr float SHOP_UPGRADE_ORB_ROTATION_SPEED = 1.6f;
	constexpr float SHOP_UPGRADE_ORB_FLOAT_SPEED = 2.0f;
	constexpr float SHOP_UPGRADE_ORB_FLOAT_HEIGHT = 0.18f;
	constexpr float SHOP_UPGRADE_AIM_MAX_DISTANCE = 8.0f;
	constexpr float SHOP_UPGRADE_HIT_SPHERE_RADIUS = 0.80f;

	// 強化Objectの共通Y・Z座標と各ObjectのX座標。
	constexpr float SHOP_UPGRADE_ORB_BASE_Y = 1.1f;
	constexpr float SHOP_UPGRADE_ORB_Z = 7.0f;
	constexpr float SHOP_MAX_HP_ORB_X = -4.5f;
	constexpr float SHOP_GUN_DAMAGE_ORB_X = -1.5f;
	constexpr float SHOP_SPECIAL_UNLOCK_ORB_X = 1.5f;
	constexpr float SHOP_SPECIAL_COOLDOWN_ORB_X = 4.5f;

	//========= ゲート定数=========
	// ゲートのScale、回転速度、Ray判定設定。
	constexpr float SHOP_GATE_SCALE_X = 1.2f;
	constexpr float SHOP_GATE_SCALE_Y = 1.4f;
	constexpr float SHOP_GATE_SCALE_Z = 0.35f;
	constexpr float SHOP_GATE_ROTATION_SPEED = 1.4f;
	constexpr float SHOP_GATE_AIM_MAX_DISTANCE = 10.0f;
	constexpr float SHOP_GATE_HIT_SPHERE_RADIUS = 1.5f;

	// チャレンジゲートとタイトルゲートのワールド座標。
	constexpr float SHOP_CHALLENGE_GATE_X = 0.0f;
	constexpr float SHOP_CHALLENGE_GATE_Y = 1.2f;
	constexpr float SHOP_CHALLENGE_GATE_Z = 13.0f;
	constexpr float SHOP_TITLE_GATE_X = -6.5f;
	constexpr float SHOP_TITLE_GATE_Y = 1.2f;
	constexpr float SHOP_TITLE_GATE_Z = 11.0f;

	//========= HUD定数=========
	// HUD文字のScaleと表示座標。
	constexpr float SHOP_HUD_TEXT_SCALE = 0.80f;
	constexpr float SHOP_HUD_HINT_X = 315.0f;
	constexpr float SHOP_HUD_HINT_Y = 590.0f;
	constexpr float SHOP_HUD_STATUS_X = 30.0f;
	constexpr float SHOP_HUD_STATUS_Y = 30.0f;

	//========= メッセージ表示定数=========
	// 購入結果メッセージを表示する秒数。
	constexpr float SHOP_INTERACTION_MESSAGE_DURATION = 1.5f;
}

// ShopSceneが使用するSceneManagerとFramework Systemを登録する。
ShopScene::ShopScene(
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

// ShopのPlayer、Camera、Renderer、HUD、BGMを初期化する。
void ShopScene::Initialize()
{
	// Shop内で使用する3D・2D描画リソースを初期化する。
	m_BasicMeshRenderer.Initialize( m_GraphicsSystem );
	m_HudRenderer.Initialize( m_GraphicsSystem );
	m_HudTextRenderer.Initialize( m_GraphicsSystem );

	// Shop内のアニメーション、照準対象、購入メッセージ状態を初期化する。
	m_AnimationTime = {};
	m_AimedTarget = InteractionTarget::e_NONE;
	m_ShowPurchaseSuccess = {};
	m_ShowPurchaseFailure = {};
	m_InteractionMessageTimer = {};

	// ShopではFPS視点操作を使用し、Shop用BGMを再生する。
	m_InputSystem.SetMouseCaptureEnabled( true );
	m_AudioSystem.PlayShopBgm();
}

// Shop内の操作、カメラ、Player、選択対象、メッセージ表示時間を更新する。
void ShopScene::Update( float deltaTime )
{
	// F1キーでFPSマウスキャプチャの有効・無効を切り替える。
	if ( m_InputSystem.IsKeyTriggered( SHOP_TOGGLE_MOUSE_CAPTURE_KEY ) )
	{
		m_InputSystem.SetMouseCaptureEnabled( !m_InputSystem.IsMouseCaptureEnabled() );
	}

	// 強化Objectとゲートのアニメーション時間を進める。
	m_AnimationTime += deltaTime;

	// 購入結果メッセージの表示時間を更新する。
	if ( m_InteractionMessageTimer > 0.0f )
	{
		m_InteractionMessageTimer = std::max( 0.0f, m_InteractionMessageTimer - deltaTime );

		if ( m_InteractionMessageTimer <= 0.0f )
		{
			m_ShowPurchaseSuccess = false;
			m_ShowPurchaseFailure = false;
		}
	}

	// FPS CameraとCamera基準のDebugPlayer位置を更新する。
	m_FpsCamera.Update( m_InputSystem );
	m_DebugPlayer.Update( deltaTime, m_InputSystem, m_FpsCamera );
	m_FpsCamera.SetPosition( m_DebugPlayer.GetPosition() );

	// カメラ中央のRayが照準している操作対象を更新する。
	m_AimedTarget = GetAimedInteractionTarget();

	// Eキーが押された場合、現在照準している対象と相互作用する。
	if ( m_InputSystem.IsKeyTriggered( SHOP_USE_INTERACTION_KEY ) )
	{
		TryInteractWithTarget( m_AimedTarget );
	}
}

// Shopの3D空間、強化Object、ゲート、HUDを描画する。
void ShopScene::Draw()
{
	// SceneManagerが所有するゲーム進捗と現在のPlayer能力を取得する。
	GameProgress& progress = m_SceneManager.GetGameProgress();
	const PlayerStats& playerStats = progress.GetPlayerStats();

	// 3D描画に使用するCamera行列とSkybox追従用Camera座標を取得する。
	const DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
		DirectX::XMConvertToRadians( SHOP_FIELD_FOV_DEGREES ),
		SHOP_ASPECT_RATIO,
		SHOP_NEAR_CLIP,
		SHOP_FAR_CLIP );
	const DirectX::XMMATRIX viewMatrix = m_FpsCamera.GetViewMatrix();
	const DirectX::XMFLOAT3 cameraPosition = m_FpsCamera.GetPosition();

	// Camera位置に追従する単色Skyboxを描画する。
	const DirectX::XMMATRIX skyboxWorldMatrix =
		DirectX::XMMatrixScaling( SHOP_SKYBOX_SCALE, SHOP_SKYBOX_SCALE, SHOP_SKYBOX_SCALE ) *
		DirectX::XMMatrixTranslation( cameraPosition.x, cameraPosition.y, cameraPosition.z );

	m_GraphicsSystem.SetDepthTestEnabled( false );
	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		skyboxWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4{
			SHOP_SKY_COLOR_RED,
			SHOP_SKY_COLOR_GREEN,
			SHOP_SKY_COLOR_BLUE,
			SHOP_SKY_COLOR_ALPHA },
			DirectX::XMFLOAT2{ 1.0f, 1.0f },
			BasicMeshRenderer::TextureType::Color );
	m_GraphicsSystem.SetDepthTestEnabled( true );

	// 床のWorld行列を作成してFloor Textureで描画する。
	const DirectX::XMMATRIX floorWorldMatrix =
		DirectX::XMMatrixScaling( SHOP_FLOOR_SCALE_X, SHOP_FLOOR_SCALE_Y, SHOP_FLOOR_SCALE_Z ) *
		DirectX::XMMatrixTranslation(
			SHOP_FLOOR_POSITION_X,
			SHOP_FLOOR_POSITION_Y,
			SHOP_FLOOR_POSITION_Z );

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		floorWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f },
		DirectX::XMFLOAT2{ 10.0f, 10.0f },
		BasicMeshRenderer::TextureType::Floor );

	// 壁の色と4面のWorld行列を作成する。
	const DirectX::XMFLOAT4 wallColor{ 0.25f, 0.30f, 0.38f, 1.0f };

	const DirectX::XMMATRIX leftWallWorldMatrix =
		DirectX::XMMatrixScaling( SHOP_WALL_THICKNESS, SHOP_WALL_HEIGHT, SHOP_WALL_LENGTH ) *
		DirectX::XMMatrixTranslation( SHOP_LEFT_WALL_X, SHOP_WALL_CENTER_Y, SHOP_WALL_CENTER_Z );
	const DirectX::XMMATRIX rightWallWorldMatrix =
		DirectX::XMMatrixScaling( SHOP_WALL_THICKNESS, SHOP_WALL_HEIGHT, SHOP_WALL_LENGTH ) *
		DirectX::XMMatrixTranslation( SHOP_RIGHT_WALL_X, SHOP_WALL_CENTER_Y, SHOP_WALL_CENTER_Z );
	const DirectX::XMMATRIX nearWallWorldMatrix =
		DirectX::XMMatrixScaling( SHOP_WALL_LENGTH, SHOP_WALL_HEIGHT, SHOP_WALL_THICKNESS ) *
		DirectX::XMMatrixTranslation( SHOP_WALL_CENTER_X, SHOP_WALL_CENTER_Y, SHOP_NEAR_WALL_Z );
	const DirectX::XMMATRIX farWallWorldMatrix =
		DirectX::XMMatrixScaling( SHOP_WALL_LENGTH, SHOP_WALL_HEIGHT, SHOP_WALL_THICKNESS ) *
		DirectX::XMMatrixTranslation( SHOP_WALL_CENTER_X, SHOP_WALL_CENTER_Y, SHOP_FAR_WALL_Z );

	// 4面の壁をWall Textureで描画する。
	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		leftWallWorldMatrix,
		viewMatrix,
		projectionMatrix,
		wallColor,
		DirectX::XMFLOAT2{ 1.0f, 1.0f },
		BasicMeshRenderer::TextureType::Wall );
	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		rightWallWorldMatrix,
		viewMatrix,
		projectionMatrix,
		wallColor,
		DirectX::XMFLOAT2{ 1.0f, 1.0f },
		BasicMeshRenderer::TextureType::Wall );
	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		nearWallWorldMatrix,
		viewMatrix,
		projectionMatrix,
		wallColor,
		DirectX::XMFLOAT2{ 1.0f, 1.0f },
		BasicMeshRenderer::TextureType::Wall );
	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		farWallWorldMatrix,
		viewMatrix,
		projectionMatrix,
		wallColor,
		DirectX::XMFLOAT2{ 1.0f, 1.0f },
		BasicMeshRenderer::TextureType::Wall );

	// 強化Objectとして描画する対象を定義する。
	constexpr std::array<InteractionTarget, 4> upgradeTargets
	{
		InteractionTarget::e_MAX_HP_UPGRADE,
		InteractionTarget::e_GUN_DAMAGE_UPGRADE,
		InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE,
		InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE
	};

	// 各強化Objectを浮遊・回転させ、照準中は少し大きく明るく描画する。
	for ( const InteractionTarget target : upgradeTargets )
	{
		const DirectX::XMFLOAT3 basePosition = GetUpgradeObjectPosition( target );
		const float floatingOffset = std::sinf(
			m_AnimationTime * SHOP_UPGRADE_ORB_FLOAT_SPEED + basePosition.x ) *
			SHOP_UPGRADE_ORB_FLOAT_HEIGHT;
		const bool isAimed = target == m_AimedTarget;
		const float scale = isAimed ?
			SHOP_UPGRADE_ORB_SCALE * 1.20f :
			SHOP_UPGRADE_ORB_SCALE;

		DirectX::XMFLOAT4 color = GetUpgradeObjectColor( target );

		if ( isAimed )
		{
			color.x = std::min( 1.0f, color.x + 0.25f );
			color.y = std::min( 1.0f, color.y + 0.25f );
			color.z = std::min( 1.0f, color.z + 0.25f );
		}

		// 現段階はテクスチャなしの単色Cubeを使用する。
		// Sphereメッシュ追加後、このDrawCubeをDrawSphereへ置き換える。
		const DirectX::XMMATRIX upgradeWorldMatrix =
			DirectX::XMMatrixScaling( scale, scale, scale ) *
			DirectX::XMMatrixRotationY( m_AnimationTime * SHOP_UPGRADE_ORB_ROTATION_SPEED ) *
			DirectX::XMMatrixRotationX( m_AnimationTime * 0.7f ) *
			DirectX::XMMatrixTranslation(
				basePosition.x,
				basePosition.y + floatingOffset,
				basePosition.z );

		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			upgradeWorldMatrix,
			viewMatrix,
			projectionMatrix,
			color,
			DirectX::XMFLOAT2{ 1.0f, 1.0f },
			BasicMeshRenderer::TextureType::Color );
	}

	// 2つのゲートの回転角、照準状態、拡大率を計算する。
	const float gateRotation = m_AnimationTime * SHOP_GATE_ROTATION_SPEED;
	const bool isChallengeGateAimed = m_AimedTarget == InteractionTarget::e_CHALLENGE_GATE;
	const bool isTitleGateAimed = m_AimedTarget == InteractionTarget::e_TITLE_GATE;
	const float challengeGateScale = isChallengeGateAimed ? 1.10f : 1.0f;
	const float titleGateScale = isTitleGateAimed ? 1.10f : 1.0f;

	// チャレンジゲートとタイトルゲートのWorld行列を作成する。
	const DirectX::XMMATRIX challengeGateWorldMatrix =
		DirectX::XMMatrixScaling(
			SHOP_GATE_SCALE_X * challengeGateScale,
			SHOP_GATE_SCALE_Y * challengeGateScale,
			SHOP_GATE_SCALE_Z * challengeGateScale ) *
		DirectX::XMMatrixRotationY( gateRotation ) *
		DirectX::XMMatrixTranslation(
			SHOP_CHALLENGE_GATE_X,
			SHOP_CHALLENGE_GATE_Y,
			SHOP_CHALLENGE_GATE_Z );
	const DirectX::XMMATRIX titleGateWorldMatrix =
		DirectX::XMMatrixScaling(
			SHOP_GATE_SCALE_X * titleGateScale,
			SHOP_GATE_SCALE_Y * titleGateScale,
			SHOP_GATE_SCALE_Z * titleGateScale ) *
		DirectX::XMMatrixRotationY( gateRotation ) *
		DirectX::XMMatrixTranslation(
			SHOP_TITLE_GATE_X,
			SHOP_TITLE_GATE_Y,
			SHOP_TITLE_GATE_Z );

	// Challenge用とTitle用の単色ゲートを描画する。
	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		challengeGateWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4{ 0.10f, 0.85f, 1.0f, 1.0f },
		DirectX::XMFLOAT2{ 1.0f, 1.0f },
		BasicMeshRenderer::TextureType::Color );
	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		titleGateWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4{ 0.85f, 0.30f, 1.0f, 1.0f },
		DirectX::XMFLOAT2{ 1.0f, 1.0f },
		BasicMeshRenderer::TextureType::Color );

	// HUDを画面固定で描画するためDepth Testを無効化する。
	m_GraphicsSystem.SetDepthTestEnabled( false );
	m_HudRenderer.DrawCrosshair( m_GraphicsSystem );
	m_HudTextRenderer.Begin();

	// 所持金とPlayer能力を画面左上へ表示する。
	wchar_t statusText[ 128 ]{};
	swprintf_s(
		statusText,
		L"所持金: %d G   最大HP: %.0f   攻撃力: %.0f",
		progress.GetCurrency(),
		playerStats.maxHp,
		playerStats.gunDamage );

	m_HudTextRenderer.DrawText(
		statusText,
		DirectX::XMFLOAT2{ SHOP_HUD_STATUS_X, SHOP_HUD_STATUS_Y },
		DirectX::Colors::Gold,
		SHOP_HUD_TEXT_SCALE );

	// 照準中の強化Objectまたはゲートに応じた操作説明を表示する。
	if ( m_AimedTarget == InteractionTarget::e_MAX_HP_UPGRADE )
	{
		const int cost = progress.GetUpgradeCost( UpgradeType::e_MAX_HP );
		wchar_t hintText[ 128 ]{};

		swprintf_s( hintText, L"E: 最大HPを25上げる  （%d G）", cost );

		m_HudTextRenderer.DrawText(
			hintText,
			DirectX::XMFLOAT2{ SHOP_HUD_HINT_X, SHOP_HUD_HINT_Y },
			DirectX::Colors::Lime,
			SHOP_HUD_TEXT_SCALE );
	}
	else if ( m_AimedTarget == InteractionTarget::e_GUN_DAMAGE_UPGRADE )
	{
		const int cost = progress.GetUpgradeCost( UpgradeType::e_GUN_DAMAGE );
		wchar_t hintText[ 128 ]{};

		swprintf_s( hintText, L"E: 攻撃力を5上げる  （%d G）", cost );

		m_HudTextRenderer.DrawText(
			hintText,
			DirectX::XMFLOAT2{ SHOP_HUD_HINT_X, SHOP_HUD_HINT_Y },
			DirectX::Colors::Orange,
			SHOP_HUD_TEXT_SCALE );
	}
	else if ( m_AimedTarget == InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE )
	{
		if ( playerStats.isSpecialAttackUnlocked )
		{
			m_HudTextRenderer.DrawText(
				L"特殊攻撃は解放済みです",
				DirectX::XMFLOAT2{ SHOP_HUD_HINT_X, SHOP_HUD_HINT_Y },
				DirectX::Colors::Violet,
				SHOP_HUD_TEXT_SCALE );
		}
		else
		{
			const int cost = progress.GetUpgradeCost( UpgradeType::e_UNLOCK_SPECIAL_ATTACK );
			wchar_t hintText[ 128 ]{};

			swprintf_s( hintText, L"E: 特殊攻撃を解放する  （%d G）", cost );

			m_HudTextRenderer.DrawText(
				hintText,
				DirectX::XMFLOAT2{ SHOP_HUD_HINT_X, SHOP_HUD_HINT_Y },
				DirectX::Colors::Violet,
				SHOP_HUD_TEXT_SCALE );
		}
	}
	else if ( m_AimedTarget == InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE )
	{
		if ( !playerStats.isSpecialAttackUnlocked )
		{
			m_HudTextRenderer.DrawText(
				L"先に特殊攻撃を解放してください",
				DirectX::XMFLOAT2{ SHOP_HUD_HINT_X, SHOP_HUD_HINT_Y },
				DirectX::Colors::Yellow,
				SHOP_HUD_TEXT_SCALE );
		}
		else
		{
			const int cost = progress.GetUpgradeCost( UpgradeType::e_SPECIAL_ATTACK_COOLDOWN );
			wchar_t hintText[ 128 ]{};

			swprintf_s( hintText, L"E: 特殊攻撃の待機時間を1秒短縮  （%d G）", cost );

			m_HudTextRenderer.DrawText(
				hintText,
				DirectX::XMFLOAT2{ SHOP_HUD_HINT_X, SHOP_HUD_HINT_Y },
				DirectX::Colors::Cyan,
				SHOP_HUD_TEXT_SCALE );
		}
	}
	else if ( m_AimedTarget == InteractionTarget::e_CHALLENGE_GATE )
	{
		m_HudTextRenderer.DrawText(
			L"E: 現在のステージに挑戦",
			DirectX::XMFLOAT2{ SHOP_HUD_HINT_X, SHOP_HUD_HINT_Y },
			DirectX::Colors::Cyan,
			SHOP_HUD_TEXT_SCALE );
	}
	else if ( m_AimedTarget == InteractionTarget::e_TITLE_GATE )
	{
		m_HudTextRenderer.DrawText(
			L"E: タイトルへ戻る",
			DirectX::XMFLOAT2{ SHOP_HUD_HINT_X, SHOP_HUD_HINT_Y },
			DirectX::Colors::Violet,
			SHOP_HUD_TEXT_SCALE );
	}
	else
	{
		m_HudTextRenderer.DrawText(
			L"強化オブジェクトまたはゲートに照準を合わせてEキー",
			DirectX::XMFLOAT2{ 255.0f, SHOP_HUD_HINT_Y },
			DirectX::Colors::White,
			0.68f );
	}

	// 強化購入後の成功または失敗メッセージを画面上部へ表示する。
	if ( m_ShowPurchaseSuccess )
	{
		m_HudTextRenderer.DrawText(
			L"強化に成功しました！",
			DirectX::XMFLOAT2{ 525.0f, 120.0f },
			DirectX::Colors::Lime,
			1.0f );
	}
	else if ( m_ShowPurchaseFailure )
	{
		m_HudTextRenderer.DrawText(
			L"ゴールド不足、または強化できません",
			DirectX::XMFLOAT2{ 400.0f, 120.0f },
			DirectX::Colors::Red,
			0.85f );
	}

	// Shop内の基本操作を画面下部へ表示する。
	m_HudTextRenderer.DrawText(
		L"WASD: 移動   マウス: 視点移動   E: 調べる   F1: マウス固定切替",
		DirectX::XMFLOAT2{ 210.0f, 680.0f },
		DirectX::Colors::White,
		0.60f );
	m_HudTextRenderer.End();

	// 次の3D描画へ影響を残さないようDepth TestとAlpha Blendを戻す。
	m_GraphicsSystem.SetDepthTestEnabled( true );
	m_GraphicsSystem.SetAlphaBlendEnabled( false );
}

// Shopで使用した描画リソースを解放する。
void ShopScene::Uninit()
{
	m_HudTextRenderer.Uninit();
	m_HudRenderer.Uninit();
	m_BasicMeshRenderer.Uninit();
}

// カメラ中央のRayが当たる最も近い操作対象を返す。
ShopScene::InteractionTarget ShopScene::GetAimedInteractionTarget() const
{
	// Rayの始点と正規化した視線方向をCameraから取得する。
	const DirectX::XMFLOAT3 rayOrigin = m_FpsCamera.GetPosition();
	const DirectX::XMFLOAT3 forward = m_FpsCamera.GetForward();
	const DirectX::XMVECTOR rayOriginVector = DirectX::XMLoadFloat3( &rayOrigin );
	const DirectX::XMVECTOR rayDirection = DirectX::XMVector3Normalize(
		DirectX::XMLoadFloat3( &forward ) );

	// 現在見つかっている最も近い対象と距離を保持する。
	InteractionTarget nearestTarget = InteractionTarget::e_NONE;
	float nearestDistance = SHOP_GATE_AIM_MAX_DISTANCE;

	// Ray判定する強化Objectの対象一覧。
	constexpr std::array<InteractionTarget, 4> upgradeTargets
	{
		InteractionTarget::e_MAX_HP_UPGRADE,
		InteractionTarget::e_GUN_DAMAGE_UPGRADE,
		InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE,
		InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE
	};

	// 各強化ObjectへのRayとSphereの交差判定を行う。
	for ( const InteractionTarget target : upgradeTargets )
	{
		const DirectX::XMFLOAT3 basePosition = GetUpgradeObjectPosition( target );
		const float floatingOffset = std::sinf(
			m_AnimationTime * SHOP_UPGRADE_ORB_FLOAT_SPEED + basePosition.x ) *
			SHOP_UPGRADE_ORB_FLOAT_HEIGHT;
		const DirectX::XMFLOAT3 sphereCenter
		{
			basePosition.x,
			basePosition.y + floatingOffset,
			basePosition.z
		};

		const DirectX::BoundingSphere sphere( sphereCenter, SHOP_UPGRADE_HIT_SPHERE_RADIUS );
		float hitDistance{};

		if ( sphere.Intersects( rayOriginVector, rayDirection, hitDistance ) &&
			hitDistance <= SHOP_UPGRADE_AIM_MAX_DISTANCE &&
			hitDistance < nearestDistance )
		{
			nearestDistance = hitDistance;
			nearestTarget = target;
		}
	}

	// チャレンジゲートへのRayとSphereの交差判定を行う。
	const DirectX::BoundingSphere challengeGateSphere(
		DirectX::XMFLOAT3{
			SHOP_CHALLENGE_GATE_X,
			SHOP_CHALLENGE_GATE_Y,
			SHOP_CHALLENGE_GATE_Z },
			SHOP_GATE_HIT_SPHERE_RADIUS );
	float challengeGateDistance{};

	if ( challengeGateSphere.Intersects( rayOriginVector, rayDirection, challengeGateDistance ) &&
		challengeGateDistance <= SHOP_GATE_AIM_MAX_DISTANCE &&
		challengeGateDistance < nearestDistance )
	{
		nearestDistance = challengeGateDistance;
		nearestTarget = InteractionTarget::e_CHALLENGE_GATE;
	}

	// タイトルゲートへのRayとSphereの交差判定を行う。
	const DirectX::BoundingSphere titleGateSphere(
		DirectX::XMFLOAT3{
			SHOP_TITLE_GATE_X,
			SHOP_TITLE_GATE_Y,
			SHOP_TITLE_GATE_Z },
			SHOP_GATE_HIT_SPHERE_RADIUS );
	float titleGateDistance{};

	if ( titleGateSphere.Intersects( rayOriginVector, rayDirection, titleGateDistance ) &&
		titleGateDistance <= SHOP_GATE_AIM_MAX_DISTANCE &&
		titleGateDistance < nearestDistance )
	{
		nearestTarget = InteractionTarget::e_TITLE_GATE;
	}

	return nearestTarget;
}

// 指定した操作対象に応じて強化購入またはScene遷移を実行する。
void ShopScene::TryInteractWithTarget( InteractionTarget target )
{
	// SceneManagerが所有するゲーム進捗を取得する。
	GameProgress& progress = m_SceneManager.GetGameProgress();

	// 強化購入を試行したかと、その結果を保持する。
	bool isPurchaseAttempted{};
	bool isPurchaseSuccessful{};

	switch ( target )
	{
		case InteractionTarget::e_MAX_HP_UPGRADE:
		isPurchaseAttempted = true;
		isPurchaseSuccessful = progress.TryPurchaseUpgrade( UpgradeType::e_MAX_HP );
		break;

		case InteractionTarget::e_GUN_DAMAGE_UPGRADE:
		isPurchaseAttempted = true;
		isPurchaseSuccessful = progress.TryPurchaseUpgrade( UpgradeType::e_GUN_DAMAGE );
		break;

		case InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE:
		isPurchaseAttempted = true;
		isPurchaseSuccessful = progress.TryPurchaseUpgrade( UpgradeType::e_UNLOCK_SPECIAL_ATTACK );
		break;

		case InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE:
		isPurchaseAttempted = true;
		isPurchaseSuccessful = progress.TryPurchaseUpgrade( UpgradeType::e_SPECIAL_ATTACK_COOLDOWN );
		break;

		case InteractionTarget::e_CHALLENGE_GATE:
		// GameSceneへ遷移する前にFPSマウスキャプチャを解除する。
		m_AudioSystem.PlayWarpSe();
		m_InputSystem.SetMouseCaptureEnabled( false );
		m_SceneManager.RequestSceneChange<GameScene>();
		return;

		case InteractionTarget::e_TITLE_GATE:
		// TitleSceneへ遷移する前にFPSマウスキャプチャを解除する。
		m_AudioSystem.PlayWarpSe();
		m_InputSystem.SetMouseCaptureEnabled( false );
		m_SceneManager.RequestSceneChange<TitleScene>();
		return;

		case InteractionTarget::e_NONE:
		default:
		return;
	}

	// 強化購入結果に応じてSEとメッセージ表示状態を更新する。
	if ( isPurchaseSuccessful )
	{
		m_AudioSystem.PlayPurchaseSe();
		m_ShowPurchaseSuccess = true;
		m_ShowPurchaseFailure = false;
		m_InteractionMessageTimer = SHOP_INTERACTION_MESSAGE_DURATION;
		return;
	}

	if ( isPurchaseAttempted )
	{
		m_ShowPurchaseSuccess = false;
		m_ShowPurchaseFailure = true;
		m_InteractionMessageTimer = SHOP_INTERACTION_MESSAGE_DURATION;
	}
}

// 指定した強化Objectのワールド座標を返す。
DirectX::XMFLOAT3 ShopScene::GetUpgradeObjectPosition( InteractionTarget target ) const
{
	switch ( target )
	{
		case InteractionTarget::e_MAX_HP_UPGRADE:
		return DirectX::XMFLOAT3{
			SHOP_MAX_HP_ORB_X,
			SHOP_UPGRADE_ORB_BASE_Y,
			SHOP_UPGRADE_ORB_Z };

		case InteractionTarget::e_GUN_DAMAGE_UPGRADE:
		return DirectX::XMFLOAT3{
			SHOP_GUN_DAMAGE_ORB_X,
			SHOP_UPGRADE_ORB_BASE_Y,
			SHOP_UPGRADE_ORB_Z };

		case InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE:
		return DirectX::XMFLOAT3{
			SHOP_SPECIAL_UNLOCK_ORB_X,
			SHOP_UPGRADE_ORB_BASE_Y,
			SHOP_UPGRADE_ORB_Z };

		case InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE:
		return DirectX::XMFLOAT3{
			SHOP_SPECIAL_COOLDOWN_ORB_X,
			SHOP_UPGRADE_ORB_BASE_Y,
			SHOP_UPGRADE_ORB_Z };

		case InteractionTarget::e_NONE:
		case InteractionTarget::e_CHALLENGE_GATE:
		case InteractionTarget::e_TITLE_GATE:
		default:
		return {};
	}
}

// 指定した強化Objectの表示色を返す。
DirectX::XMFLOAT4 ShopScene::GetUpgradeObjectColor( InteractionTarget target ) const
{
	switch ( target )
	{
		case InteractionTarget::e_MAX_HP_UPGRADE:
		return DirectX::XMFLOAT4{ 0.15f, 1.0f, 0.25f, 1.0f };

		case InteractionTarget::e_GUN_DAMAGE_UPGRADE:
		return DirectX::XMFLOAT4{ 1.0f, 0.20f, 0.12f, 1.0f };

		case InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE:
		return DirectX::XMFLOAT4{ 0.75f, 0.25f, 1.0f, 1.0f };

		case InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE:
		return DirectX::XMFLOAT4{ 0.15f, 0.65f, 1.0f, 1.0f };

		case InteractionTarget::e_NONE:
		case InteractionTarget::e_CHALLENGE_GATE:
		case InteractionTarget::e_TITLE_GATE:
		default:
		return DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
	}
}