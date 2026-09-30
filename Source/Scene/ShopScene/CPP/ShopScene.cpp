#include "../H/ShopScene.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>

//========= DirectX インクルード=========
#include <DirectXColors.h>
#include <DirectXMath.h>

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Input/H/InputSystem.h"
#include "Framework/Etc/H/Logger.h"

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

	//========= ゲート定数=========
	// GateのScaleとRay判定設定。
	constexpr float SHOP_GATE_SCALE_X = 1.2f;
	constexpr float SHOP_GATE_SCALE_Y = 1.4f;
	constexpr float SHOP_GATE_SCALE_Z = 0.35f;
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
ShopScene::ShopScene ( SceneManager& sceneManager, InputSystem& inputSystem, GraphicsSystem& graphicsSystem, AudioSystem& audioSystem )
	: m_SceneManager ( sceneManager ), m_InputSystem ( inputSystem ), m_GraphicsSystem ( graphicsSystem ), m_AudioSystem ( audioSystem )
{}

// ShopのPlayer、Camera、Renderer、HUD、BGMを初期化する。
void ShopScene::Initialize ()
{
	// Shop内のアニメーション、照準対象、購入メッセージ状態を初期化する。
	m_AnimationTime = {};
	m_AimedTarget = InteractionTarget::e_NONE;
	m_ShowPurchaseSuccess = {};
	m_ShowPurchaseFailure = {};
	m_InteractionMessageTimer = {};

	InitializeGates ();

	// ShopではFPS視点操作を使用し、Shop用BGMを再生する。
	m_InputSystem.SetMouseCaptureEnabled ( true );
	m_AudioSystem.PlayShopBgm ();
}

// 3D、HUD、文字描画に使用する深い描画Resourceを初期化する。
bool ShopScene::Init ()
{
	// Shopの3D描画に使用する基本Mesh Rendererを初期化する。
	const bool isBasicMeshRendererInitialized = m_BasicMeshRenderer.Initialize ( m_GraphicsSystem );

	if ( !isBasicMeshRendererInitialized )
	{
		Logger::Write ( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"ShopSceneのBasicMeshRenderer初期化失敗" );
		Finalize ();
		return false;
	}

	// ShopのHUD Quad描画Resourceを初期化する。
	const bool isHudRendererInitialized = m_HudRenderer.Initialize ( m_GraphicsSystem );

	if ( !isHudRendererInitialized )
	{
		Logger::Write ( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"ShopSceneのHudRenderer初期化失敗" );
		Finalize ();
		return false;
	}

	// ShopのHUD文字描画Resourceを初期化する。
	const bool isHudTextRendererInitialized = m_HudTextRenderer.Initialize ( m_GraphicsSystem );

	if ( !isHudTextRendererInitialized )
	{
		Logger::Write ( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"ShopSceneのHudTextRenderer初期化失敗" );
		Finalize ();
		return false;
	}

	return true;
}

// Shop内の操作、カメラ、Player、選択対象、メッセージ表示時間を更新する。
void ShopScene::Update ( float deltaTime )
{
	UpdateMouseCapture ();
	UpdateAnimation ( deltaTime );
	UpdateInteractionMessage ( deltaTime );
	UpdatePlayerAndCamera ( deltaTime );
	UpdateAimedInteractionTarget ();
	UpdateInteraction ();
}

// Shopの3D空間、強化Object、ゲート、HUDを描画する。
void ShopScene::Draw ()
{
	// 3D描画に使用する現在のRender Targetサイズを取得する。
	const unsigned int renderWidth = m_GraphicsSystem.GetRenderWidth ();
	const unsigned int renderHeight = m_GraphicsSystem.GetRenderHeight ();

	if ( renderWidth == 0 || renderHeight == 0 ) return;

	// 現在のRender Target比率に合わせたProjection、View、Camera座標を取得する。
	const float projectionAspectRatio = static_cast<float>( renderWidth ) / static_cast<float>( renderHeight );
	const DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH ( DirectX::XMConvertToRadians ( SHOP_FIELD_FOV_DEGREES ),
																				  projectionAspectRatio, SHOP_NEAR_CLIP, SHOP_FAR_CLIP );

	const DirectX::XMMATRIX viewMatrix = m_FpsCamera.GetViewMatrix ();
	const DirectX::XMFLOAT3 cameraPosition = m_FpsCamera.GetPosition ();
	// HUD表示に使用するゲーム進捗とPlayer能力を取得する。
	GameProgress& progress = m_SceneManager.GetGameProgress ();
	const PlayerStats& playerStats = progress.GetPlayerStats ();

	DrawSky ( viewMatrix, projectionMatrix, cameraPosition );
	DrawOpaqueField ( viewMatrix, projectionMatrix );
	DrawUpgradeObjects ( viewMatrix, projectionMatrix );
	DrawGates ( viewMatrix, projectionMatrix );
	DrawHud ( progress, playerStats );
}

// Shopで使用した描画リソースを終了する。
void ShopScene::Finalize ()
{
	m_HudTextRenderer.Uninit ();
	m_HudRenderer.Uninit ();
	m_BasicMeshRenderer.Uninit ();
}

// Challenge GateとTitle GateのTransform、色、Raycast設定を初期化する。
void ShopScene::InitializeGates ()
{
	const DirectX::XMFLOAT3 gateScale { SHOP_GATE_SCALE_X,SHOP_GATE_SCALE_Y,SHOP_GATE_SCALE_Z };

	m_ChallengeGate.Initialize ( ShopGate::GateType::e_CHALLENGE, DirectX::XMFLOAT3 { SHOP_CHALLENGE_GATE_X,SHOP_CHALLENGE_GATE_Y,SHOP_CHALLENGE_GATE_Z },
								gateScale, DirectX::XMFLOAT4 { 0.10f,0.85f,1.0f,1.0f }, SHOP_GATE_HIT_SPHERE_RADIUS );
	m_TitleGate.Initialize ( ShopGate::GateType::e_TITLE, DirectX::XMFLOAT3 { SHOP_TITLE_GATE_X,SHOP_TITLE_GATE_Y,SHOP_TITLE_GATE_Z },
							gateScale, DirectX::XMFLOAT4 { 0.85f,0.30f,1.0f,1.0f }, SHOP_GATE_HIT_SPHERE_RADIUS );
}

// F1キーによるFPSマウスキャプチャ切替を処理する。
void ShopScene::UpdateMouseCapture ()
{
	if ( m_InputSystem.IsKeyTriggered ( SHOP_TOGGLE_MOUSE_CAPTURE_KEY ) )
	{
		m_InputSystem.SetMouseCaptureEnabled ( !m_InputSystem.IsMouseCaptureEnabled () );
	}
}

// 強化ObjectとGateの回転・浮遊に使用する時間を更新する。
void ShopScene::UpdateAnimation ( float deltaTime )
{
	m_AnimationTime += deltaTime;

	m_ChallengeGate.Update ( deltaTime );
	m_TitleGate.Update ( deltaTime );
}

// 購入結果Messageの表示時間と表示状態を更新する。
void ShopScene::UpdateInteractionMessage ( float deltaTime )
{
	if ( m_InteractionMessageTimer <= 0.0f ) return;

	m_InteractionMessageTimer = std::max ( 0.0f, m_InteractionMessageTimer - deltaTime );

	if ( m_InteractionMessageTimer > 0.0f ) return;

	m_ShowPurchaseSuccess = false;
	m_ShowPurchaseFailure = false;
}

// FPS Camera、DebugPlayer、Camera追従位置を更新する。
void ShopScene::UpdatePlayerAndCamera ( float deltaTime )
{
	m_FpsCamera.Update ( m_InputSystem );
	m_DebugPlayer.Update ( deltaTime, m_InputSystem, m_FpsCamera );
	m_FpsCamera.SetPosition ( m_DebugPlayer.GetPosition () );
}

// Eキーによる現在照準中の操作対象との相互作用を処理する。
void ShopScene::UpdateInteraction ()
{
	if ( !m_InputSystem.IsKeyTriggered ( SHOP_USE_INTERACTION_KEY ) ) return;

	TryInteractWithTarget ( m_AimedTarget );
}

// Shopの全強化Object設定を読み取り専用で返す。
const std::array<ShopScene::ShopUpgradeData, 4>& ShopScene::GetUpgradeData ()
{
	static const std::array<ShopUpgradeData, 4> upgradeData
	{
	ShopUpgradeData
	{
		InteractionTarget::e_MAX_HP_UPGRADE,
		UpgradeType::e_MAX_HP,
		DirectX::XMFLOAT3{ -4.5f,1.1f,7.0f },
		DirectX::XMFLOAT4{ 0.15f,1.0f,0.25f,1.0f }
	},
	ShopUpgradeData
	{
		InteractionTarget::e_GUN_DAMAGE_UPGRADE,
		UpgradeType::e_GUN_DAMAGE,
		DirectX::XMFLOAT3{ -1.5f,1.1f,7.0f },
		DirectX::XMFLOAT4{ 1.0f,0.20f,0.12f,1.0f }
	},
	ShopUpgradeData
	{
		InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE,
		UpgradeType::e_UNLOCK_SPECIAL_ATTACK,
		DirectX::XMFLOAT3{ 1.5f,1.1f,7.0f },
		DirectX::XMFLOAT4{ 0.75f,0.25f,1.0f,1.0f }
	},
	ShopUpgradeData
	{
		InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE,
		UpgradeType::e_SPECIAL_ATTACK_COOLDOWN,
		DirectX::XMFLOAT3{ 4.5f,1.1f,7.0f },
		DirectX::XMFLOAT4{ 0.15f,0.65f,1.0f,1.0f }
	}
	};

	return upgradeData;
}

// 指定した操作対象に対応する強化Object設定を返す。対応しない場合はnullptrを返す。
const ShopScene::ShopUpgradeData* ShopScene::FindUpgradeData ( InteractionTarget target ) const
{
	const std::array<ShopUpgradeData, 4>& upgradeData = GetUpgradeData ();

	for ( const ShopUpgradeData& data : upgradeData )
	{
		if ( data.interactionTarget == target ) return &data;
	}

	return nullptr;
}

// カメラ中央のRayが当たる最も近い操作対象を返す。
ShopScene::InteractionTarget ShopScene::GetAimedInteractionTarget () const
{
	// Rayの始点と視線方向をCameraから取得する。
	const DirectX::XMFLOAT3 rayOrigin = m_FpsCamera.GetPosition ();
	const DirectX::XMFLOAT3 rayDirection = m_FpsCamera.GetForward ();

	// 現在見つかっている最も近い対象と距離を保持する。
	InteractionTarget nearestTarget = InteractionTarget::e_NONE;
	float nearestDistance = SHOP_GATE_AIM_MAX_DISTANCE;

	// 各強化ObjectへのRayとSphereの交差判定を行う。
	const std::array<ShopUpgradeData, 4>& upgradeData = GetUpgradeData ();

	for ( const ShopUpgradeData& data : upgradeData )
	{
		const float floatingOffset = std::sinf ( m_AnimationTime * SHOP_UPGRADE_ORB_FLOAT_SPEED + data.position.x ) * SHOP_UPGRADE_ORB_FLOAT_HEIGHT;
		const DirectX::XMFLOAT3 sphereCenter { data.position.x,data.position.y + floatingOffset,data.position.z };
		const RaycastResult raycastResult = m_CombatSystem.RaycastSphere ( rayOrigin, rayDirection, sphereCenter,
																		  SHOP_UPGRADE_HIT_SPHERE_RADIUS, SHOP_UPGRADE_AIM_MAX_DISTANCE );
		if ( !raycastResult.isHit || raycastResult.hitDistance >= nearestDistance ) continue;

		nearestDistance = raycastResult.hitDistance;
		nearestTarget = data.interactionTarget;
	}

	// チャレンジゲートへのRaycast判定を行う。
	const RaycastResult challengeGateRaycastResult = m_ChallengeGate.Raycast ( m_CombatSystem, rayOrigin, rayDirection, SHOP_GATE_AIM_MAX_DISTANCE );

	if ( challengeGateRaycastResult.isHit && challengeGateRaycastResult.hitDistance < nearestDistance )
	{
		nearestDistance = challengeGateRaycastResult.hitDistance;
		nearestTarget = InteractionTarget::e_CHALLENGE_GATE;
	}

	// タイトルゲートへのRaycast判定を行う。
	const RaycastResult titleGateRaycastResult = m_TitleGate.Raycast ( m_CombatSystem, rayOrigin, rayDirection, SHOP_GATE_AIM_MAX_DISTANCE );

	if ( titleGateRaycastResult.isHit && titleGateRaycastResult.hitDistance < nearestDistance )
	{
		nearestTarget = InteractionTarget::e_TITLE_GATE;
	}

	return nearestTarget;
}

// 指定した操作対象に応じて強化購入またはScene遷移を実行する。
void ShopScene::TryInteractWithTarget ( InteractionTarget target )
{
	// SceneManagerが所有するゲーム進捗を取得する。
	GameProgress& progress = m_SceneManager.GetGameProgress ();

	const ShopUpgradeData* upgradeData = FindUpgradeData ( target );

	if ( upgradeData != nullptr )
	{
		const bool isPurchaseSuccessful = progress.TryPurchaseUpgrade ( upgradeData->upgradeType );

		if ( isPurchaseSuccessful )
		{
			m_AudioSystem.PlayPurchaseSe ();
			m_ShowPurchaseSuccess = true;
			m_ShowPurchaseFailure = false;
			m_InteractionMessageTimer = SHOP_INTERACTION_MESSAGE_DURATION;
			return;
		}

		m_ShowPurchaseSuccess = false;
		m_ShowPurchaseFailure = true;
		m_InteractionMessageTimer = SHOP_INTERACTION_MESSAGE_DURATION;

		return;
	}

	switch ( target )
	{
		case InteractionTarget::e_CHALLENGE_GATE:
			// GameSceneへ遷移する前にFPSマウスキャプチャを解除する。
			m_AudioSystem.PlayWarpSe ();
			m_InputSystem.SetMouseCaptureEnabled ( false );
			m_SceneManager.RequestSceneChange<GameScene> ();
			return;

		case InteractionTarget::e_TITLE_GATE:
			// TitleSceneへ遷移する前にFPSマウスキャプチャを解除する。
			m_AudioSystem.PlayWarpSe ();
			m_InputSystem.SetMouseCaptureEnabled ( false );
			m_SceneManager.RequestSceneChange<TitleScene> ();
			return;

		case InteractionTarget::e_NONE:
		default:
			return;
	}
}

// Camera位置に追従する単色SkyboxをSky Passで描画する。
void ShopScene::DrawSky ( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix, const DirectX::XMFLOAT3& cameraPosition )
{
	const DirectX::XMMATRIX skyboxWorldMatrix =
		DirectX::XMMatrixScaling ( SHOP_SKYBOX_SCALE, SHOP_SKYBOX_SCALE, SHOP_SKYBOX_SCALE ) *
		DirectX::XMMatrixTranslation ( cameraPosition.x, cameraPosition.y, cameraPosition.z );

	m_GraphicsSystem.SetRenderPass ( e_RenderPass::e_SKY );

	m_BasicMeshRenderer.DrawCube ( m_GraphicsSystem, skyboxWorldMatrix, viewMatrix, projectionMatrix,
								  DirectX::XMFLOAT4 { SHOP_SKY_COLOR_RED,SHOP_SKY_COLOR_GREEN,SHOP_SKY_COLOR_BLUE,SHOP_SKY_COLOR_ALPHA },
								  DirectX::XMFLOAT2 { 1.0f,1.0f }, BasicMeshRenderer::TextureType::Color );

	m_GraphicsSystem.SetRenderPass ( e_RenderPass::e_OPAQUE );
}

// Floorと4面のWallをOpaque Passで描画する。
void ShopScene::DrawOpaqueField ( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix )
{
	const DirectX::XMMATRIX floorWorldMatrix =
		DirectX::XMMatrixScaling ( SHOP_FLOOR_SCALE_X, SHOP_FLOOR_SCALE_Y, SHOP_FLOOR_SCALE_Z ) *
		DirectX::XMMatrixTranslation ( SHOP_FLOOR_POSITION_X, SHOP_FLOOR_POSITION_Y, SHOP_FLOOR_POSITION_Z );

	m_BasicMeshRenderer.DrawCube ( m_GraphicsSystem, floorWorldMatrix, viewMatrix, projectionMatrix,
								  DirectX::XMFLOAT4 { 1.0f,1.0f,1.0f,1.0f }, DirectX::XMFLOAT2 { 10.0f,10.0f },
								  BasicMeshRenderer::TextureType::Floor );

	const DirectX::XMFLOAT4 wallColor { 0.25f,0.30f,0.38f,1.0f };

	const DirectX::XMMATRIX leftWallWorldMatrix =
		DirectX::XMMatrixScaling ( SHOP_WALL_THICKNESS, SHOP_WALL_HEIGHT, SHOP_WALL_LENGTH ) *
		DirectX::XMMatrixTranslation ( SHOP_LEFT_WALL_X, SHOP_WALL_CENTER_Y, SHOP_WALL_CENTER_Z );

	const DirectX::XMMATRIX rightWallWorldMatrix =
		DirectX::XMMatrixScaling ( SHOP_WALL_THICKNESS, SHOP_WALL_HEIGHT, SHOP_WALL_LENGTH ) *
		DirectX::XMMatrixTranslation ( SHOP_RIGHT_WALL_X, SHOP_WALL_CENTER_Y, SHOP_WALL_CENTER_Z );

	const DirectX::XMMATRIX nearWallWorldMatrix =
		DirectX::XMMatrixScaling ( SHOP_WALL_LENGTH, SHOP_WALL_HEIGHT, SHOP_WALL_THICKNESS ) *
		DirectX::XMMatrixTranslation ( SHOP_WALL_CENTER_X, SHOP_WALL_CENTER_Y, SHOP_NEAR_WALL_Z );

	const DirectX::XMMATRIX farWallWorldMatrix =
		DirectX::XMMatrixScaling ( SHOP_WALL_LENGTH, SHOP_WALL_HEIGHT, SHOP_WALL_THICKNESS ) *
		DirectX::XMMatrixTranslation ( SHOP_WALL_CENTER_X, SHOP_WALL_CENTER_Y, SHOP_FAR_WALL_Z );

	m_BasicMeshRenderer.DrawCube ( m_GraphicsSystem, leftWallWorldMatrix, viewMatrix, projectionMatrix, wallColor,
								  DirectX::XMFLOAT2 { 1.0f,1.0f }, BasicMeshRenderer::TextureType::Wall );

	m_BasicMeshRenderer.DrawCube ( m_GraphicsSystem, rightWallWorldMatrix, viewMatrix, projectionMatrix, wallColor,
								  DirectX::XMFLOAT2 { 1.0f,1.0f }, BasicMeshRenderer::TextureType::Wall );

	m_BasicMeshRenderer.DrawCube ( m_GraphicsSystem, nearWallWorldMatrix, viewMatrix, projectionMatrix, wallColor,
								  DirectX::XMFLOAT2 { 1.0f,1.0f }, BasicMeshRenderer::TextureType::Wall );

	m_BasicMeshRenderer.DrawCube ( m_GraphicsSystem, farWallWorldMatrix, viewMatrix, projectionMatrix, wallColor,
								  DirectX::XMFLOAT2 { 1.0f,1.0f }, BasicMeshRenderer::TextureType::Wall );
}

// 強化Objectを浮遊・回転・照準状態に応じてOpaque Passで描画する。
void ShopScene::DrawUpgradeObjects ( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix )
{
	const std::array<ShopUpgradeData, 4>& upgradeData = GetUpgradeData ();

	for ( const ShopUpgradeData& data : upgradeData )
	{
		const float floatingOffset = std::sinf ( m_AnimationTime * SHOP_UPGRADE_ORB_FLOAT_SPEED + data.position.x ) * SHOP_UPGRADE_ORB_FLOAT_HEIGHT;

		const bool isAimed = data.interactionTarget == m_AimedTarget;
		const float scale = isAimed ? SHOP_UPGRADE_ORB_SCALE * 1.20f : SHOP_UPGRADE_ORB_SCALE;
		DirectX::XMFLOAT4 color = data.color;

		if ( isAimed )
		{
			color.x = std::min ( 1.0f, color.x + 0.25f );
			color.y = std::min ( 1.0f, color.y + 0.25f );
			color.z = std::min ( 1.0f, color.z + 0.25f );
		}

		// 現段階はテクスチャなしの単色Cubeを使用する。
		// Sphereメッシュ追加後、このDrawCubeをDrawSphereへ置き換える。
		const DirectX::XMMATRIX upgradeWorldMatrix =
			DirectX::XMMatrixScaling ( scale, scale, scale ) *
			DirectX::XMMatrixRotationY ( m_AnimationTime * SHOP_UPGRADE_ORB_ROTATION_SPEED ) *
			DirectX::XMMatrixRotationX ( m_AnimationTime * 0.7f ) *
			DirectX::XMMatrixTranslation ( data.position.x, data.position.y + floatingOffset, data.position.z );

		m_BasicMeshRenderer.DrawCube ( m_GraphicsSystem, upgradeWorldMatrix, viewMatrix, projectionMatrix, color,
									  DirectX::XMFLOAT2 { 1.0f,1.0f }, BasicMeshRenderer::TextureType::Color );
	}
}

// Challenge GateとTitle GateをOpaque Passで描画する。
void ShopScene::DrawGates ( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix )
{
	const bool isChallengeGateAimed = m_AimedTarget == InteractionTarget::e_CHALLENGE_GATE;
	const bool isTitleGateAimed = m_AimedTarget == InteractionTarget::e_TITLE_GATE;

	m_ChallengeGate.Draw ( m_BasicMeshRenderer, m_GraphicsSystem, viewMatrix, projectionMatrix, isChallengeGateAimed );
	m_TitleGate.Draw ( m_BasicMeshRenderer, m_GraphicsSystem, viewMatrix, projectionMatrix, isTitleGateAimed );
}

// Shop HUD、照準、所持金、操作案内、購入結果MessageをScreen UI Passで描画する。
void ShopScene::DrawHud ( GameProgress& progress, const PlayerStats& playerStats )
{
	m_GraphicsSystem.SetRenderPass ( e_RenderPass::e_SCREEN_UI );
	m_HudRenderer.DrawCrosshair ( m_GraphicsSystem );
	m_HudTextRenderer.Begin ();

	wchar_t statusText[128] {};

	swprintf_s ( statusText, L"所持金: %d G   最大HP: %.0f   攻撃力: %.0f", progress.GetCurrency (), playerStats.maxHp, playerStats.gunDamage );

	m_HudTextRenderer.DrawText ( statusText, DirectX::XMFLOAT2 { SHOP_HUD_STATUS_X,SHOP_HUD_STATUS_Y }, DirectX::Colors::Gold, SHOP_HUD_TEXT_SCALE );

	if ( m_AimedTarget == InteractionTarget::e_MAX_HP_UPGRADE )
	{
		const int cost = progress.GetUpgradeCost ( UpgradeType::e_MAX_HP );

		wchar_t hintText[128] {};

		swprintf_s ( hintText, L"E: 最大HPを25上げる  （%d G）", cost );

		m_HudTextRenderer.DrawText ( hintText, DirectX::XMFLOAT2 { SHOP_HUD_HINT_X,SHOP_HUD_HINT_Y }, DirectX::Colors::Lime, SHOP_HUD_TEXT_SCALE );
	}
	else if ( m_AimedTarget == InteractionTarget::e_GUN_DAMAGE_UPGRADE )
	{
		const int cost = progress.GetUpgradeCost ( UpgradeType::e_GUN_DAMAGE );

		wchar_t hintText[128] {};

		swprintf_s ( hintText, L"E: 攻撃力を5上げる  （%d G）", cost );

		m_HudTextRenderer.DrawText ( hintText, DirectX::XMFLOAT2 { SHOP_HUD_HINT_X,SHOP_HUD_HINT_Y }, DirectX::Colors::Orange, SHOP_HUD_TEXT_SCALE );
	}
	else if ( m_AimedTarget == InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE )
	{
		if ( playerStats.isSpecialAttackUnlocked )
		{
			m_HudTextRenderer.DrawText ( L"特殊攻撃は解放済みです", DirectX::XMFLOAT2 { SHOP_HUD_HINT_X,SHOP_HUD_HINT_Y }, DirectX::Colors::Violet, SHOP_HUD_TEXT_SCALE );
		}
		else
		{
			const int cost = progress.GetUpgradeCost ( UpgradeType::e_UNLOCK_SPECIAL_ATTACK );

			wchar_t hintText[128] {};

			swprintf_s ( hintText, L"E: 特殊攻撃を解放する  （%d G）", cost );

			m_HudTextRenderer.DrawText ( hintText, DirectX::XMFLOAT2 { SHOP_HUD_HINT_X,SHOP_HUD_HINT_Y }, DirectX::Colors::Violet, SHOP_HUD_TEXT_SCALE );
		}
	}
	else if ( m_AimedTarget == InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE )
	{
		if ( !playerStats.isSpecialAttackUnlocked )
		{
			m_HudTextRenderer.DrawText ( L"先に特殊攻撃を解放してください", DirectX::XMFLOAT2 { SHOP_HUD_HINT_X,SHOP_HUD_HINT_Y }, DirectX::Colors::Yellow, SHOP_HUD_TEXT_SCALE );
		}
		else
		{
			const int cost = progress.GetUpgradeCost ( UpgradeType::e_SPECIAL_ATTACK_COOLDOWN );

			wchar_t hintText[128] {};

			swprintf_s ( hintText, L"E: 特殊攻撃の待機時間を1秒短縮  （%d G）", cost );

			m_HudTextRenderer.DrawText ( hintText, DirectX::XMFLOAT2 { SHOP_HUD_HINT_X,SHOP_HUD_HINT_Y }, DirectX::Colors::Cyan, SHOP_HUD_TEXT_SCALE );
		}
	}
	else if ( m_AimedTarget == InteractionTarget::e_CHALLENGE_GATE )
	{
		m_HudTextRenderer.DrawText ( L"E: 現在のステージに挑戦", DirectX::XMFLOAT2 { SHOP_HUD_HINT_X,SHOP_HUD_HINT_Y }, DirectX::Colors::Cyan, SHOP_HUD_TEXT_SCALE );
	}
	else if ( m_AimedTarget == InteractionTarget::e_TITLE_GATE )
	{
		m_HudTextRenderer.DrawText ( L"E: タイトルへ戻る", DirectX::XMFLOAT2 { SHOP_HUD_HINT_X,SHOP_HUD_HINT_Y }, DirectX::Colors::Violet, SHOP_HUD_TEXT_SCALE );
	}
	else
	{
		m_HudTextRenderer.DrawText ( L"強化オブジェクトまたはゲートに照準を合わせてEキー", DirectX::XMFLOAT2 { 255.0f,SHOP_HUD_HINT_Y }, DirectX::Colors::White, 0.68f );
	}

	if ( m_ShowPurchaseSuccess )
	{
		m_HudTextRenderer.DrawText ( L"強化に成功しました！", DirectX::XMFLOAT2 { 525.0f,120.0f }, DirectX::Colors::Lime, 1.0f );
	}
	else if ( m_ShowPurchaseFailure )
	{
		m_HudTextRenderer.DrawText ( L"ゴールド不足、または強化できません", DirectX::XMFLOAT2 { 400.0f,120.0f }, DirectX::Colors::Red, 0.85f );
	}

	m_HudTextRenderer.DrawText ( L"WASD: 移動   マウス: 視点移動   E: 調べる   F1: マウス固定切替", DirectX::XMFLOAT2 { 210.0f,680.0f }, DirectX::Colors::White, 0.60f );

	m_HudTextRenderer.End ();
	m_GraphicsSystem.SetRenderPass ( e_RenderPass::e_OPAQUE );
}