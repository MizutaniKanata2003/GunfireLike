#pragma once

#include <array>
#include <windows.h>

#include "Debug/H/DebugPlayer.h"

#include "Framework/Input/H/FpsCamera.h"

#include "Scene/Common/H/EnemyHealth.h"
#include "Scene/Common/H/PlayerHealth.h"
#include "Scene/Common/H/StageData.h"
#include "Scene/Common/H/IScene.h"

#include "Framework/2D/H/HudRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/3D/H/ObjModelRenderer.h"

class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

namespace
{
	constexpr float TEST_PLAYER_DAMAGE = 10.0f;

	constexpr int PREVIOUS_STAGE_OFFSET = 1;
	constexpr int NEXT_STAGE_OFFSET = 1;

	constexpr int TUTORIAL_STAGE_NUMBER =
		StageConstants::FIRST_STAGE_NUMBER;

	// ----- Input -----
	constexpr unsigned char USE_GATE_KEY = 'E';
	constexpr unsigned char SHOOT_ENEMY_KEY = VK_LBUTTON;
	constexpr unsigned char SPECIAL_ATTACK_KEY = 'Q';

	// ----- Enemy model -----
	constexpr float ENEMY_BASE_X = 0.0f;

	// player.objのローカル下端はY=約1.0。
	// スケール0.50なら、地面に置くためY=-0.50付近へ移動する。
	constexpr float ENEMY_BASE_Y = -0.45f;

	constexpr float ENEMY_BASE_Z = 8.0f;

	// 敵を小さくする。
	constexpr float ENEMY_MODEL_SCALE = 0.50f;

	// 小さくしたモデルに合わせて浮遊量も小さくする。
	constexpr float ENEMY_FLOAT_HEIGHT = 0.10f;

	constexpr float ENEMY_FLOAT_SPEED = 2.0f;
	constexpr float ENEMY_ROTATION_SPEED = 1.5f;

	// モデル中心付近。
	constexpr float ENEMY_HIT_CENTER_Y_OFFSET = 0.50f;

	// 見た目に合わせて当たり判定も小さくする。
	constexpr float ENEMY_HIT_SPHERE_RADIUS = 0.55f;

	constexpr float SHOOT_MAX_DISTANCE = 30.0f;

	// ----- Sky dome -----
	constexpr float SKY_DOME_SCALE = 150.0f;
	constexpr float SKY_DOME_Y_OFFSET = 0.0f;

	// ----- Enemy attacks -----
	constexpr float ENEMY_NORMAL_ATTACK_RANGE = 3.0f;

	constexpr float ENEMY_NORMAL_ATTACK_RANGE_SQUARED =
		ENEMY_NORMAL_ATTACK_RANGE *
		ENEMY_NORMAL_ATTACK_RANGE;

	constexpr float ENEMY_NORMAL_ATTACK_INTERVAL = 1.2f;

	constexpr float ENEMY_SPECIAL_ATTACK_DAMAGE_MULTIPLIER =
		2.0f;

	// ----- Player special attack -----
	constexpr float PLAYER_SPECIAL_ATTACK_RANGE = 8.0f;

	constexpr float PLAYER_SPECIAL_ATTACK_RANGE_SQUARED =
		PLAYER_SPECIAL_ATTACK_RANGE *
		PLAYER_SPECIAL_ATTACK_RANGE;

	constexpr float PLAYER_SPECIAL_ATTACK_DAMAGE_MULTIPLIER =
		3.0f;

	// ----- Enemy HP billboard -----
	constexpr float HP_BAR_WIDTH = 2.0f;
	constexpr float HP_BAR_HEIGHT = 0.18f;

	// HPバーは敵モデルの上端より少し上。
	constexpr float HP_BAR_Y_OFFSET = 1.10f;


	constexpr float HP_BAR_BACKGROUND_DEPTH = 0.02f;
	constexpr float HP_BAR_FOREGROUND_DEPTH = 0.02f;

	constexpr float HP_BAR_FOREGROUND_CAMERA_OFFSET =
		0.08f;

	// ----- 3D stage gates -----
	constexpr float GATE_HEIGHT = 1.4f;
	constexpr float GATE_WIDTH = 1.2f;
	constexpr float GATE_DEPTH = 0.35f;

	constexpr float GATE_ROTATION_SPEED = 1.4f;
	constexpr float GATE_COLOR_ALPHA = 1.0f;

	constexpr float GATE_INTERACTION_RADIUS = 2.0f;

	constexpr float GATE_INTERACTION_RADIUS_SQUARED =
		GATE_INTERACTION_RADIUS *
		GATE_INTERACTION_RADIUS;

	constexpr float PREVIOUS_GATE_POSITION_X = -6.5f;
	constexpr float PREVIOUS_GATE_POSITION_Y = 1.2f;
	constexpr float PREVIOUS_GATE_POSITION_Z = 10.0f;

	constexpr float NEXT_GATE_POSITION_X = 0.0f;
	constexpr float NEXT_GATE_POSITION_Y = 1.2f;
	constexpr float NEXT_GATE_POSITION_Z = 13.0f;

	constexpr float SHOP_GATE_POSITION_X = 6.5f;
	constexpr float SHOP_GATE_POSITION_Y = 1.2f;
	constexpr float SHOP_GATE_POSITION_Z = 10.0f;

	// ----- Gun / bullets -----
	constexpr float GUN_POSITION_X = 0.55f;
	constexpr float GUN_POSITION_Y = -0.38f;
	constexpr float GUN_POSITION_Z = 1.20f;

	constexpr float GUN_BODY_SCALE_X = 0.24f;
	constexpr float GUN_BODY_SCALE_Y = 0.16f;
	constexpr float GUN_BODY_SCALE_Z = 0.65f;

	constexpr float GUN_BARREL_SCALE_X = 0.09f;
	constexpr float GUN_BARREL_SCALE_Y = 0.09f;
	constexpr float GUN_BARREL_SCALE_Z = 0.55f;

	constexpr float GUN_BARREL_OFFSET_Z = 0.55f;

	constexpr float MUZZLE_FLASH_SCALE = 0.20f;
	constexpr float MUZZLE_FLASH_OFFSET_Z = 1.10f;
	constexpr float MUZZLE_FLASH_DURATION = 0.08f;

	constexpr float BULLET_SPEED = 28.0f;
	constexpr float BULLET_LIFETIME = 1.20f;
	constexpr float BULLET_SCALE = 0.08f;

	constexpr float BULLET_SPAWN_OFFSET_X = 0.55f;
	constexpr float BULLET_SPAWN_OFFSET_Y = -0.34f;
	constexpr float BULLET_SPAWN_OFFSET_Z = 1.85f;

	constexpr int MAX_BULLET_COUNT = 16;

	// ----- Debug UI -----
	constexpr float GATE_BUTTON_WIDTH = 240.0f;
	constexpr float GATE_BUTTON_HEIGHT = 32.0f;

	constexpr const char* PREVIOUS_STAGE_GATE_LABEL =
		"Previous Stage Gate";

	constexpr const char* NEXT_STAGE_GATE_LABEL =
		"Next Stage Gate";

	constexpr const char* SHOP_GATE_LABEL =
		"Shop Gate";

	// ----- HUD -----
	constexpr float HUD_GATE_HINT_X = 440.0f;
	constexpr float HUD_GATE_HINT_Y = 590.0f;

	constexpr float HUD_TUTORIAL_X = 40.0f;
	constexpr float HUD_TUTORIAL_Y = 80.0f;

	constexpr float HUD_MESSAGE_SCALE = 0.85f;
	constexpr float HUD_TUTORIAL_SCALE = 0.65f;
}

class GameScene final : public IScene
{
public:
	GameScene(
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
	struct Bullet
	{
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 direction{};
		float remainingLifetime = 0.0f;
		bool isActive = false;
	};

	[[nodiscard]] bool IsPlayerNearGate(
		float gatePositionX,
		float gatePositionZ ) const;

	SceneManager& m_SceneManager;
	InputSystem& m_InputSystem;
	GraphicsSystem& m_GraphicsSystem;
	AudioSystem& m_AudioSystem;

	DebugPlayer m_DebugPlayer;
	FpsCamera m_FpsCamera;

	PlayerHealth m_PlayerHealth;
	EnemyHealth m_EnemyHealth;

	BasicMeshRenderer m_BasicMeshRenderer;

	ObjModelRenderer m_SkyDomeRenderer;
	ObjModelRenderer m_EnemyModelRenderer;

	HudRenderer m_HudRenderer;
	HudTextRenderer m_HudTextRenderer;

	float m_EnemyAnimationTime = 0.0f;
	float m_GateAnimationTime = 0.0f;

	float m_EnemyNormalAttackTimer = 0.0f;
	float m_EnemySpecialAttackTimer = 0.0f;
	float m_SpecialAttackCooldownTimer = 0.0f;
	float m_MuzzleFlashTimer = 0.0f;

	bool m_IsGameClear = false;
	bool m_IsResultSceneRequested = false;

	bool m_IsLastShotHit = false;
	bool m_IsLastNormalAttackHit = false;
	bool m_IsLastSpecialAttackHit = false;
	bool m_IsLastPlayerSpecialAttackHit = false;

	bool m_HasPlayedLowHpSe = false;
	bool m_HasPlayedEnemyDefeatSe = false;

	std::array<Bullet, MAX_BULLET_COUNT>
		m_Bullets{};
};