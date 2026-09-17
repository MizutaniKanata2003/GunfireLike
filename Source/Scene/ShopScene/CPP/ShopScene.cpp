#include "../H/ShopScene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>

#include <DirectXCollision.h>
#include <DirectXColors.h>

#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/Input/H/InputSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"

#include "Scene/Common/H/SceneManager.h"

#include "Scene/GameScene/H/GameScene.h"
#include "Scene/TitleScene/H/TitleScene.h"

namespace
{
	constexpr float SHOP_INTERACTION_MESSAGE_DURATION =
		1.5f;

	constexpr float SHOP_NEAR_CLIP =
		0.1f;

	constexpr float SHOP_FAR_CLIP =
		1000.0f;

	constexpr float SHOP_FIELD_FOV_DEGREES =
		60.0f;
}

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

void ShopScene::Initialize()
{
	m_DebugPlayer.Initialize();
	m_FpsCamera.Initialize();

	m_BasicMeshRenderer.Initialize(
		m_GraphicsSystem );

	m_HudRenderer.Initialize(
		m_GraphicsSystem );

	m_HudTextRenderer.Initialize(
		m_GraphicsSystem );

	m_AnimationTime = 0.0f;

	m_AimedTarget =
		InteractionTarget::e_NONE;

	m_ShowPurchaseSuccess = false;
	m_ShowPurchaseFailure = false;

	m_InteractionMessageTimer = 0.0f;

	m_InputSystem.SetMouseCaptureEnabled(
		true );

	m_AudioSystem.PlayShopBgm();
}

bool ShopScene::Init()
{
	return true;
}

void ShopScene::Update(
	float deltaTime )
{
	if ( m_InputSystem.IsKeyTriggered(
		SHOP_TOGGLE_MOUSE_CAPTURE_KEY ) )
	{
		m_InputSystem.SetMouseCaptureEnabled(
			!m_InputSystem.
			IsMouseCaptureEnabled() );
	}

	m_AnimationTime +=
		deltaTime;

	if ( m_InteractionMessageTimer > 0.0f )
	{
		m_InteractionMessageTimer =
			std::max(
				0.0f,
				m_InteractionMessageTimer -
				deltaTime );

		if ( m_InteractionMessageTimer <=
			0.0f )
		{
			m_ShowPurchaseSuccess = false;
			m_ShowPurchaseFailure = false;
		}
	}

	m_FpsCamera.Update(
		m_InputSystem );

	m_DebugPlayer.Update(
		deltaTime,
		m_InputSystem,
		m_FpsCamera );

	m_FpsCamera.SetPosition(
		m_DebugPlayer.GetPosition() );

	m_AimedTarget =
		GetAimedInteractionTarget();

	if ( m_InputSystem.IsKeyTriggered(
		SHOP_USE_INTERACTION_KEY ) )
	{
		TryInteractWithTarget(
			m_AimedTarget );
	}
}

void ShopScene::Draw()
{
	const GameProgress& progress =
		m_SceneManager.GetGameProgress();

	const PlayerStats& playerStats =
		progress.GetPlayerStats();

	const DirectX::XMMATRIX projectionMatrix =
		DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XMConvertToRadians(
		SHOP_FIELD_FOV_DEGREES ),
			SHOP_ASPECT_RATIO,
			SHOP_NEAR_CLIP,
			SHOP_FAR_CLIP );

	const DirectX::XMMATRIX viewMatrix =
		m_FpsCamera.GetViewMatrix();

	const DirectX::XMFLOAT3 cameraPosition =
		m_FpsCamera.GetPosition();

	const DirectX::XMMATRIX skyboxWorldMatrix =
		DirectX::XMMatrixScaling(
			SHOP_SKYBOX_SCALE,
			SHOP_SKYBOX_SCALE,
			SHOP_SKYBOX_SCALE ) *
		DirectX::XMMatrixTranslation(
			cameraPosition.x,
			cameraPosition.y,
			cameraPosition.z );

	m_GraphicsSystem.SetDepthTestEnabled(
		false );

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		skyboxWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4(
		SHOP_SKY_COLOR_RED,
		SHOP_SKY_COLOR_GREEN,
		SHOP_SKY_COLOR_BLUE,
		SHOP_SKY_COLOR_ALPHA ),
		DirectX::XMFLOAT2(
		1.0f,
		1.0f ),
		BasicMeshRenderer::TextureType::Color );

	m_GraphicsSystem.SetDepthTestEnabled(
		true );

	const DirectX::XMMATRIX floorWorldMatrix =
		DirectX::XMMatrixScaling(
			SHOP_FLOOR_SCALE_X,
			SHOP_FLOOR_SCALE_Y,
			SHOP_FLOOR_SCALE_Z ) *
		DirectX::XMMatrixTranslation(
			SHOP_FLOOR_POSITION_X,
			SHOP_FLOOR_POSITION_Y,
			SHOP_FLOOR_POSITION_Z );

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		floorWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4(
		1.0f,
		1.0f,
		1.0f,
		1.0f ),
		DirectX::XMFLOAT2(
		10.0f,
		10.0f ),
		BasicMeshRenderer::TextureType::Floor );

	const DirectX::XMFLOAT4 wallColor =
	{
		0.25f,
		0.30f,
		0.38f,
		1.0f
	};

	const DirectX::XMMATRIX leftWallWorldMatrix =
		DirectX::XMMatrixScaling(
			SHOP_WALL_THICKNESS,
			SHOP_WALL_HEIGHT,
			SHOP_WALL_LENGTH ) *
		DirectX::XMMatrixTranslation(
			SHOP_LEFT_WALL_X,
			SHOP_WALL_CENTER_Y,
			SHOP_WALL_CENTER_Z );

	const DirectX::XMMATRIX rightWallWorldMatrix =
		DirectX::XMMatrixScaling(
			SHOP_WALL_THICKNESS,
			SHOP_WALL_HEIGHT,
			SHOP_WALL_LENGTH ) *
		DirectX::XMMatrixTranslation(
			SHOP_RIGHT_WALL_X,
			SHOP_WALL_CENTER_Y,
			SHOP_WALL_CENTER_Z );

	const DirectX::XMMATRIX nearWallWorldMatrix =
		DirectX::XMMatrixScaling(
			SHOP_WALL_LENGTH,
			SHOP_WALL_HEIGHT,
			SHOP_WALL_THICKNESS ) *
		DirectX::XMMatrixTranslation(
			SHOP_WALL_CENTER_X,
			SHOP_WALL_CENTER_Y,
			SHOP_NEAR_WALL_Z );

	const DirectX::XMMATRIX farWallWorldMatrix =
		DirectX::XMMatrixScaling(
			SHOP_WALL_LENGTH,
			SHOP_WALL_HEIGHT,
			SHOP_WALL_THICKNESS ) *
		DirectX::XMMatrixTranslation(
			SHOP_WALL_CENTER_X,
			SHOP_WALL_CENTER_Y,
			SHOP_FAR_WALL_Z );

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		leftWallWorldMatrix,
		viewMatrix,
		projectionMatrix,
		wallColor,
		DirectX::XMFLOAT2(
		1.0f,
		1.0f ),
		BasicMeshRenderer::TextureType::Wall );

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		rightWallWorldMatrix,
		viewMatrix,
		projectionMatrix,
		wallColor,
		DirectX::XMFLOAT2(
		1.0f,
		1.0f ),
		BasicMeshRenderer::TextureType::Wall );

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		nearWallWorldMatrix,
		viewMatrix,
		projectionMatrix,
		wallColor,
		DirectX::XMFLOAT2(
		1.0f,
		1.0f ),
		BasicMeshRenderer::TextureType::Wall );

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		farWallWorldMatrix,
		viewMatrix,
		projectionMatrix,
		wallColor,
		DirectX::XMFLOAT2(
		1.0f,
		1.0f ),
		BasicMeshRenderer::TextureType::Wall );

	constexpr std::array<
		InteractionTarget,
		4 > upgradeTargets =
	{
		InteractionTarget::e_MAX_HP_UPGRADE,
		InteractionTarget::e_GUN_DAMAGE_UPGRADE,
		InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE,
		InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE
	};

	for ( const InteractionTarget target :
	upgradeTargets )
	{
		const DirectX::XMFLOAT3 basePosition =
			GetUpgradeObjectPosition(
				target );

		const float floatingOffset =
			std::sinf(
				m_AnimationTime *
				SHOP_UPGRADE_ORB_FLOAT_SPEED +
				basePosition.x ) *
			SHOP_UPGRADE_ORB_FLOAT_HEIGHT;

		const bool isAimed =
			target ==
			m_AimedTarget;

		const float scale =
			isAimed
			? SHOP_UPGRADE_ORB_SCALE * 1.20f
			: SHOP_UPGRADE_ORB_SCALE;

		DirectX::XMFLOAT4 color =
			GetUpgradeObjectColor(
				target );

		if ( isAimed )
		{
			color.x =
				std::min(
					1.0f,
					color.x + 0.25f );

			color.y =
				std::min(
					1.0f,
					color.y + 0.25f );

			color.z =
				std::min(
					1.0f,
					color.z + 0.25f );
		}

		// 現段階はテクスチャなしの単色Cube。
		// Sphereメッシュ追加後、このDrawCubeをDrawSphereへ置き換える。
		const DirectX::XMMATRIX upgradeWorldMatrix =
			DirectX::XMMatrixScaling(
				scale,
				scale,
				scale ) *
			DirectX::XMMatrixRotationY(
				m_AnimationTime *
				SHOP_UPGRADE_ORB_ROTATION_SPEED ) *
			DirectX::XMMatrixRotationX(
				m_AnimationTime *
				0.7f ) *
			DirectX::XMMatrixTranslation(
				basePosition.x,
				basePosition.y +
				floatingOffset,
				basePosition.z );

		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			upgradeWorldMatrix,
			viewMatrix,
			projectionMatrix,
			color,
			DirectX::XMFLOAT2(
			1.0f,
			1.0f ),
			BasicMeshRenderer::TextureType::Color );
	}

	const float gateRotation =
		m_AnimationTime *
		SHOP_GATE_ROTATION_SPEED;

	const bool isChallengeGateAimed =
		m_AimedTarget ==
		InteractionTarget::e_CHALLENGE_GATE;

	const bool isTitleGateAimed =
		m_AimedTarget ==
		InteractionTarget::e_TITLE_GATE;

	const float challengeGateScale =
		isChallengeGateAimed
		? 1.10f
		: 1.0f;

	const float titleGateScale =
		isTitleGateAimed
		? 1.10f
		: 1.0f;

	const DirectX::XMMATRIX challengeGateWorldMatrix =
		DirectX::XMMatrixScaling(
			SHOP_GATE_SCALE_X *
			challengeGateScale,
			SHOP_GATE_SCALE_Y *
			challengeGateScale,
			SHOP_GATE_SCALE_Z *
			challengeGateScale ) *
		DirectX::XMMatrixRotationY(
			gateRotation ) *
		DirectX::XMMatrixTranslation(
			SHOP_CHALLENGE_GATE_X,
			SHOP_CHALLENGE_GATE_Y,
			SHOP_CHALLENGE_GATE_Z );

	const DirectX::XMMATRIX titleGateWorldMatrix =
		DirectX::XMMatrixScaling(
			SHOP_GATE_SCALE_X *
			titleGateScale,
			SHOP_GATE_SCALE_Y *
			titleGateScale,
			SHOP_GATE_SCALE_Z *
			titleGateScale ) *
		DirectX::XMMatrixRotationY(
			gateRotation ) *
		DirectX::XMMatrixTranslation(
			SHOP_TITLE_GATE_X,
			SHOP_TITLE_GATE_Y,
			SHOP_TITLE_GATE_Z );

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		challengeGateWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4(
		0.10f,
		0.85f,
		1.0f,
		1.0f ),
		DirectX::XMFLOAT2(
		1.0f,
		1.0f ),
		BasicMeshRenderer::TextureType::Color );

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		titleGateWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4(
		0.85f,
		0.30f,
		1.0f,
		1.0f ),
		DirectX::XMFLOAT2(
		1.0f,
		1.0f ),
		BasicMeshRenderer::TextureType::Color );

	m_GraphicsSystem.SetDepthTestEnabled(
		false );

	m_HudRenderer.DrawCrosshair(
		m_GraphicsSystem );

	m_HudTextRenderer.Begin();

	wchar_t statusText[ 128 ]{};

	swprintf_s(
		statusText,
		L"所持金: %d G   最大HP: %.0f   攻撃力: %.0f",
		progress.GetCurrency(),
		playerStats.maxHp,
		playerStats.gunDamage );

	m_HudTextRenderer.DrawText(
		statusText,
		DirectX::XMFLOAT2(
		SHOP_HUD_STATUS_X,
		SHOP_HUD_STATUS_Y ),
		DirectX::Colors::Gold,
		SHOP_HUD_TEXT_SCALE );

	if ( m_AimedTarget ==
		InteractionTarget::e_MAX_HP_UPGRADE )
	{
		const int cost =
			progress.GetUpgradeCost(
				UpgradeType::e_MAX_HP );

		wchar_t hintText[ 128 ]{};

		swprintf_s(
			hintText,
			L"E: 最大HPを25上げる  （%d G）",
			cost );

		m_HudTextRenderer.DrawText(
			hintText,
			DirectX::XMFLOAT2(
			SHOP_HUD_HINT_X,
			SHOP_HUD_HINT_Y ),
			DirectX::Colors::Lime,
			SHOP_HUD_TEXT_SCALE );
	}
	else if ( m_AimedTarget ==
		InteractionTarget::e_GUN_DAMAGE_UPGRADE )
	{
		const int cost =
			progress.GetUpgradeCost(
				UpgradeType::e_GUN_DAMAGE );

		wchar_t hintText[ 128 ]{};

		swprintf_s(
			hintText,
			L"E: 攻撃力を5上げる  （%d G）",
			cost );

		m_HudTextRenderer.DrawText(
			hintText,
			DirectX::XMFLOAT2(
			SHOP_HUD_HINT_X,
			SHOP_HUD_HINT_Y ),
			DirectX::Colors::Orange,
			SHOP_HUD_TEXT_SCALE );
	}
	else if ( m_AimedTarget ==
		InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE )
	{
		if ( playerStats.isSpecialAttackUnlocked )
		{
			m_HudTextRenderer.DrawText(
				L"特殊攻撃は解放済みです",
				DirectX::XMFLOAT2(
				SHOP_HUD_HINT_X,
				SHOP_HUD_HINT_Y ),
				DirectX::Colors::Violet,
				SHOP_HUD_TEXT_SCALE );
		}
		else
		{
			const int cost =
				progress.GetUpgradeCost(
					UpgradeType::
					e_UNLOCK_SPECIAL_ATTACK );

			wchar_t hintText[ 128 ]{};

			swprintf_s(
				hintText,
				L"E: 特殊攻撃を解放する  （%d G）",
				cost );

			m_HudTextRenderer.DrawText(
				hintText,
				DirectX::XMFLOAT2(
				SHOP_HUD_HINT_X,
				SHOP_HUD_HINT_Y ),
				DirectX::Colors::Violet,
				SHOP_HUD_TEXT_SCALE );
		}
	}
	else if ( m_AimedTarget ==
		InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE )
	{
		if ( !playerStats.isSpecialAttackUnlocked )
		{
			m_HudTextRenderer.DrawText(
				L"先に特殊攻撃を解放してください",
				DirectX::XMFLOAT2(
				SHOP_HUD_HINT_X,
				SHOP_HUD_HINT_Y ),
				DirectX::Colors::Yellow,
				SHOP_HUD_TEXT_SCALE );
		}
		else
		{
			const int cost =
				progress.GetUpgradeCost(
					UpgradeType::
					e_SPECIAL_ATTACK_COOLDOWN );

			wchar_t hintText[ 128 ]{};

			swprintf_s(
				hintText,
				L"E: 特殊攻撃の待機時間を1秒短縮  （%d G）",
				cost );

			m_HudTextRenderer.DrawText(
				hintText,
				DirectX::XMFLOAT2(
				SHOP_HUD_HINT_X,
				SHOP_HUD_HINT_Y ),
				DirectX::Colors::Cyan,
				SHOP_HUD_TEXT_SCALE );
		}
	}
	else if ( m_AimedTarget ==
		InteractionTarget::e_CHALLENGE_GATE )
	{
		m_HudTextRenderer.DrawText(
			L"E: 現在のステージに挑戦",
			DirectX::XMFLOAT2(
			SHOP_HUD_HINT_X,
			SHOP_HUD_HINT_Y ),
			DirectX::Colors::Cyan,
			SHOP_HUD_TEXT_SCALE );
	}
	else if ( m_AimedTarget ==
		InteractionTarget::e_TITLE_GATE )
	{
		m_HudTextRenderer.DrawText(
			L"E: タイトルへ戻る",
			DirectX::XMFLOAT2(
			SHOP_HUD_HINT_X,
			SHOP_HUD_HINT_Y ),
			DirectX::Colors::Violet,
			SHOP_HUD_TEXT_SCALE );
	}
	else
	{
		m_HudTextRenderer.DrawText(
			L"強化オブジェクトまたはゲートに照準を合わせてEキー",
			DirectX::XMFLOAT2(
			255.0f,
			SHOP_HUD_HINT_Y ),
			DirectX::Colors::White,
			0.68f );
	}

	if ( m_ShowPurchaseSuccess )
	{
		m_HudTextRenderer.DrawText(
			L"強化に成功しました！",
			DirectX::XMFLOAT2(
			525.0f,
			120.0f ),
			DirectX::Colors::Lime,
			1.0f );
	}
	else if ( m_ShowPurchaseFailure )
	{
		m_HudTextRenderer.DrawText(
			L"ゴールド不足、または強化できません",
			DirectX::XMFLOAT2(
			400.0f,
			120.0f ),
			DirectX::Colors::Red,
			0.85f );
	}

	m_HudTextRenderer.DrawText(
		L"WASD: 移動   マウス: 視点移動   E: 調べる   F1: マウス固定切替",
		DirectX::XMFLOAT2(
		210.0f,
		680.0f ),
		DirectX::Colors::White,
		0.60f );

	m_HudTextRenderer.End();

	m_GraphicsSystem.SetDepthTestEnabled(
		true );

	m_GraphicsSystem.SetAlphaBlendEnabled(
		false );
}

void ShopScene::Uninit()
{
	m_HudTextRenderer.Uninit();
	m_HudRenderer.Uninit();
	m_BasicMeshRenderer.Uninit();
}

ShopScene::InteractionTarget
ShopScene::GetAimedInteractionTarget() const
{
	const DirectX::XMFLOAT3 rayOrigin =
		m_FpsCamera.GetPosition();

	const DirectX::XMFLOAT3 forward =
		m_FpsCamera.GetForward();

	const DirectX::XMVECTOR rayOriginVector =
		DirectX::XMLoadFloat3(
			&rayOrigin );

	const DirectX::XMVECTOR rayDirection =
		DirectX::XMVector3Normalize(
			DirectX::XMLoadFloat3(
		&forward ) );

	InteractionTarget nearestTarget =
		InteractionTarget::e_NONE;

	float nearestDistance =
		SHOP_UPGRADE_AIM_MAX_DISTANCE;

	constexpr std::array<
		InteractionTarget,
		4 > upgradeTargets =
	{
		InteractionTarget::e_MAX_HP_UPGRADE,
		InteractionTarget::e_GUN_DAMAGE_UPGRADE,
		InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE,
		InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE
	};

	for ( const InteractionTarget target :
	upgradeTargets )
	{
		const DirectX::XMFLOAT3 basePosition =
			GetUpgradeObjectPosition(
				target );

		const float floatingOffset =
			std::sinf(
				m_AnimationTime *
				SHOP_UPGRADE_ORB_FLOAT_SPEED +
				basePosition.x ) *
			SHOP_UPGRADE_ORB_FLOAT_HEIGHT;

		const DirectX::XMFLOAT3 sphereCenter =
		{
			basePosition.x,
			basePosition.y +
			floatingOffset,
			basePosition.z
		};

		const DirectX::BoundingSphere sphere(
			sphereCenter,
			SHOP_UPGRADE_HIT_SPHERE_RADIUS );

		float hitDistance = 0.0f;

		if ( sphere.Intersects(
			rayOriginVector,
			rayDirection,
			hitDistance ) &&
			hitDistance <= nearestDistance )
		{
			nearestDistance = hitDistance;
			nearestTarget = target;
		}
	}

	const DirectX::BoundingSphere challengeGateSphere(
		DirectX::XMFLOAT3(
		SHOP_CHALLENGE_GATE_X,
		SHOP_CHALLENGE_GATE_Y,
		SHOP_CHALLENGE_GATE_Z ),
		SHOP_GATE_HIT_SPHERE_RADIUS );

	float challengeGateDistance = 0.0f;

	if ( challengeGateSphere.Intersects(
		rayOriginVector,
		rayDirection,
		challengeGateDistance ) &&
		challengeGateDistance <=
		SHOP_GATE_AIM_MAX_DISTANCE &&
		challengeGateDistance <
		nearestDistance )
	{
		nearestDistance =
			challengeGateDistance;

		nearestTarget =
			InteractionTarget::e_CHALLENGE_GATE;
	}

	const DirectX::BoundingSphere titleGateSphere(
		DirectX::XMFLOAT3(
		SHOP_TITLE_GATE_X,
		SHOP_TITLE_GATE_Y,
		SHOP_TITLE_GATE_Z ),
		SHOP_GATE_HIT_SPHERE_RADIUS );

	float titleGateDistance = 0.0f;

	if ( titleGateSphere.Intersects(
		rayOriginVector,
		rayDirection,
		titleGateDistance ) &&
		titleGateDistance <=
		SHOP_GATE_AIM_MAX_DISTANCE &&
		titleGateDistance <
		nearestDistance )
	{
		nearestTarget =
			InteractionTarget::e_TITLE_GATE;
	}

	return nearestTarget;
}

void ShopScene::TryInteractWithTarget(
	InteractionTarget target )
{
	GameProgress& progress =
		m_SceneManager.GetGameProgress();

	bool isPurchaseAttempted = false;
	bool isPurchaseSuccessful = false;

	switch ( target )
	{
		case InteractionTarget::e_MAX_HP_UPGRADE:
		isPurchaseAttempted = true;

		isPurchaseSuccessful =
			progress.TryPurchaseUpgrade(
				UpgradeType::e_MAX_HP );

		break;

		case InteractionTarget::e_GUN_DAMAGE_UPGRADE:
		isPurchaseAttempted = true;

		isPurchaseSuccessful =
			progress.TryPurchaseUpgrade(
				UpgradeType::e_GUN_DAMAGE );

		break;

		case InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE:
		isPurchaseAttempted = true;

		isPurchaseSuccessful =
			progress.TryPurchaseUpgrade(
				UpgradeType::
				e_UNLOCK_SPECIAL_ATTACK );

		break;

		case InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE:
		isPurchaseAttempted = true;

		isPurchaseSuccessful =
			progress.TryPurchaseUpgrade(
				UpgradeType::
				e_SPECIAL_ATTACK_COOLDOWN );

		break;

		case InteractionTarget::e_CHALLENGE_GATE:
		m_AudioSystem.PlayWarpSe();

		m_InputSystem.SetMouseCaptureEnabled(
			false );

		m_SceneManager.
			RequestSceneChange<GameScene>();

		return;

		case InteractionTarget::e_TITLE_GATE:
		m_AudioSystem.PlayWarpSe();

		m_InputSystem.SetMouseCaptureEnabled(
			false );

		m_SceneManager.
			RequestSceneChange<TitleScene>();

		return;

		case InteractionTarget::e_NONE:
		default:
		return;
	}

	if ( isPurchaseSuccessful )
	{
		m_AudioSystem.PlayPurchaseSe();

		m_ShowPurchaseSuccess = true;
		m_ShowPurchaseFailure = false;

		m_InteractionMessageTimer =
			SHOP_INTERACTION_MESSAGE_DURATION;
	}
	else if ( isPurchaseAttempted )
	{
		m_ShowPurchaseSuccess = false;
		m_ShowPurchaseFailure = true;

		m_InteractionMessageTimer =
			SHOP_INTERACTION_MESSAGE_DURATION;
	}
}

DirectX::XMFLOAT3
ShopScene::GetUpgradeObjectPosition(
	InteractionTarget target ) const
{
	switch ( target )
	{
		case InteractionTarget::e_MAX_HP_UPGRADE:
		return DirectX::XMFLOAT3(
			SHOP_MAX_HP_ORB_X,
			SHOP_UPGRADE_ORB_BASE_Y,
			SHOP_UPGRADE_ORB_Z );

		case InteractionTarget::e_GUN_DAMAGE_UPGRADE:
		return DirectX::XMFLOAT3(
			SHOP_GUN_DAMAGE_ORB_X,
			SHOP_UPGRADE_ORB_BASE_Y,
			SHOP_UPGRADE_ORB_Z );

		case InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE:
		return DirectX::XMFLOAT3(
			SHOP_SPECIAL_UNLOCK_ORB_X,
			SHOP_UPGRADE_ORB_BASE_Y,
			SHOP_UPGRADE_ORB_Z );

		case InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE:
		return DirectX::XMFLOAT3(
			SHOP_SPECIAL_COOLDOWN_ORB_X,
			SHOP_UPGRADE_ORB_BASE_Y,
			SHOP_UPGRADE_ORB_Z );

		case InteractionTarget::e_NONE:
		case InteractionTarget::e_CHALLENGE_GATE:
		case InteractionTarget::e_TITLE_GATE:
		default:
		return DirectX::XMFLOAT3(
			0.0f,
			0.0f,
			0.0f );
	}
}

DirectX::XMFLOAT4
ShopScene::GetUpgradeObjectColor(
	InteractionTarget target ) const
{
	switch ( target )
	{
		case InteractionTarget::e_MAX_HP_UPGRADE:
		return DirectX::XMFLOAT4(
			0.15f,
			1.0f,
			0.25f,
			1.0f );

		case InteractionTarget::e_GUN_DAMAGE_UPGRADE:
		return DirectX::XMFLOAT4(
			1.0f,
			0.20f,
			0.12f,
			1.0f );

		case InteractionTarget::e_SPECIAL_UNLOCK_UPGRADE:
		return DirectX::XMFLOAT4(
			0.75f,
			0.25f,
			1.0f,
			1.0f );

		case InteractionTarget::e_SPECIAL_COOLDOWN_UPGRADE:
		return DirectX::XMFLOAT4(
			0.15f,
			0.65f,
			1.0f,
			1.0f );

		case InteractionTarget::e_NONE:
		case InteractionTarget::e_CHALLENGE_GATE:
		case InteractionTarget::e_TITLE_GATE:
		default:
		return DirectX::XMFLOAT4(
			1.0f,
			1.0f,
			1.0f,
			1.0f );
	}
}