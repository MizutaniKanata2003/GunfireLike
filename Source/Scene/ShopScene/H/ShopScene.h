#pragma once

#include <array>
#include <windows.h>

#include "Debug/H/DebugPlayer.h"

#include "Framework/2D/H/HudRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

#include "Framework/3D/H/BasicMeshRenderer.h"

#include "Framework/Input/H/FpsCamera.h"

#include "Scene/Common/H/IScene.h"

class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

namespace
{
	constexpr unsigned char SHOP_USE_INTERACTION_KEY =
		'E';

	constexpr unsigned char SHOP_TOGGLE_MOUSE_CAPTURE_KEY =
		VK_F1;

	constexpr float SHOP_ASPECT_RATIO =
		1280.0f / 720.0f;

	// フィールド。
	constexpr float SHOP_FLOOR_SCALE_X = 18.0f;
	constexpr float SHOP_FLOOR_SCALE_Y = 0.2f;
	constexpr float SHOP_FLOOR_SCALE_Z = 18.0f;

	constexpr float SHOP_FLOOR_POSITION_X = 0.0f;
	constexpr float SHOP_FLOOR_POSITION_Y = -0.6f;
	constexpr float SHOP_FLOOR_POSITION_Z = 7.0f;

	constexpr float SHOP_WALL_THICKNESS = 0.2f;
	constexpr float SHOP_WALL_HEIGHT = 3.2f;
	constexpr float SHOP_WALL_LENGTH = 18.0f;

	constexpr float SHOP_LEFT_WALL_X = -9.0f;
	constexpr float SHOP_RIGHT_WALL_X = 9.0f;
	constexpr float SHOP_NEAR_WALL_Z = -2.0f;
	constexpr float SHOP_FAR_WALL_Z = 16.0f;

	constexpr float SHOP_WALL_CENTER_Y = 1.1f;
	constexpr float SHOP_WALL_CENTER_X = 0.0f;
	constexpr float SHOP_WALL_CENTER_Z = 7.0f;

	// 簡易Skybox。
	constexpr float SHOP_SKYBOX_SCALE = 250.0f;

	constexpr float SHOP_SKY_COLOR_RED = 0.08f;
	constexpr float SHOP_SKY_COLOR_GREEN = 0.18f;
	constexpr float SHOP_SKY_COLOR_BLUE = 0.42f;
	constexpr float SHOP_SKY_COLOR_ALPHA = 1.0f;

	// 強化オーブ。
	constexpr float SHOP_UPGRADE_ORB_SCALE = 0.65f;
	constexpr float SHOP_UPGRADE_ORB_ROTATION_SPEED = 1.6f;
	constexpr float SHOP_UPGRADE_ORB_FLOAT_SPEED = 2.0f;
	constexpr float SHOP_UPGRADE_ORB_FLOAT_HEIGHT = 0.18f;

	constexpr float SHOP_UPGRADE_ORB_BASE_Y = 1.1f;
	constexpr float SHOP_UPGRADE_ORB_Z = 7.0f;

	constexpr float SHOP_MAX_HP_ORB_X = -4.5f;
	constexpr float SHOP_GUN_DAMAGE_ORB_X = -1.5f;
	constexpr float SHOP_SPECIAL_UNLOCK_ORB_X = 1.5f;
	constexpr float SHOP_SPECIAL_COOLDOWN_ORB_X = 4.5f;

	constexpr float SHOP_UPGRADE_AIM_MAX_DISTANCE = 8.0f;
	constexpr float SHOP_UPGRADE_HIT_SPHERE_RADIUS = 0.80f;

	// ゲート。
	constexpr float SHOP_GATE_SCALE_X = 1.2f;
	constexpr float SHOP_GATE_SCALE_Y = 1.4f;
	constexpr float SHOP_GATE_SCALE_Z = 0.35f;

	constexpr float SHOP_GATE_ROTATION_SPEED = 1.4f;

	constexpr float SHOP_GATE_AIM_MAX_DISTANCE = 10.0f;
	constexpr float SHOP_GATE_HIT_SPHERE_RADIUS = 1.5f;

	constexpr float SHOP_CHALLENGE_GATE_X = 0.0f;
	constexpr float SHOP_CHALLENGE_GATE_Y = 1.2f;
	constexpr float SHOP_CHALLENGE_GATE_Z = 13.0f;

	constexpr float SHOP_TITLE_GATE_X = -6.5f;
	constexpr float SHOP_TITLE_GATE_Y = 1.2f;
	constexpr float SHOP_TITLE_GATE_Z = 11.0f;

	// HUD。
	constexpr float SHOP_HUD_TEXT_SCALE = 0.80f;
	constexpr float SHOP_HUD_HINT_X = 315.0f;
	constexpr float SHOP_HUD_HINT_Y = 590.0f;

	constexpr float SHOP_HUD_STATUS_X = 30.0f;
	constexpr float SHOP_HUD_STATUS_Y = 30.0f;
}

class ShopScene final : public IScene
{
public:
	ShopScene(
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
	enum class InteractionTarget
	{
		e_NONE,
		e_MAX_HP_UPGRADE,
		e_GUN_DAMAGE_UPGRADE,
		e_SPECIAL_UNLOCK_UPGRADE,
		e_SPECIAL_COOLDOWN_UPGRADE,
		e_CHALLENGE_GATE,
		e_TITLE_GATE
	};

	[[nodiscard]] InteractionTarget
		GetAimedInteractionTarget() const;

	void TryInteractWithTarget(
		InteractionTarget target );

	[[nodiscard]] DirectX::XMFLOAT3
		GetUpgradeObjectPosition(
			InteractionTarget target ) const;

	[[nodiscard]] DirectX::XMFLOAT4
		GetUpgradeObjectColor(
			InteractionTarget target ) const;

	SceneManager& m_SceneManager;
	InputSystem& m_InputSystem;
	GraphicsSystem& m_GraphicsSystem;
	AudioSystem& m_AudioSystem;

	DebugPlayer m_DebugPlayer;
	FpsCamera m_FpsCamera;

	BasicMeshRenderer m_BasicMeshRenderer;
	HudRenderer m_HudRenderer;
	HudTextRenderer m_HudTextRenderer;

	float m_AnimationTime = 0.0f;

	InteractionTarget m_AimedTarget =
		InteractionTarget::e_NONE;

	bool m_ShowPurchaseSuccess = false;
	bool m_ShowPurchaseFailure = false;

	float m_InteractionMessageTimer = 0.0f;
};