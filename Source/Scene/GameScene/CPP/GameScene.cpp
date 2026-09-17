#include "../H/GameScene.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

#include <DirectXCollision.h>
#include <DirectXColors.h>

#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/Input/H/InputSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"

#include "Scene/Common/H/StageData.h"
#include "Scene/Common/H/SceneManager.h"

#include "Scene/ResultScene/H/ResultScene.h"
#include "Scene/ShopScene/H/ShopScene.h"


#include "imgui.h"

namespace
{
	constexpr float LOW_HP_RATIO_THRESHOLD = 0.25f;

	constexpr float PROJECTION_ASPECT_RATIO =
		1280.0f / 720.0f;

	constexpr float PROJECTION_FOV_DEGREES = 60.0f;
	constexpr float PROJECTION_NEAR_Z = 0.1f;
	constexpr float PROJECTION_FAR_Z = 1000.0f;

	constexpr const wchar_t* SKY_OBJ_PATH = L"Assets\\Models\\Sky\\sky.obj";

	constexpr const wchar_t* SKY_TEXTURE_PATH = L"Assets\\Models\\Sky\\sky.jpg";

	constexpr const wchar_t* ENEMY_OBJ_PATH = L"Assets\\Models\\Enemy\\player.obj";
}

GameScene::GameScene(
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

void GameScene::Initialize()
{
	m_DebugPlayer.Initialize();
	m_FpsCamera.Initialize();

	const StageData& stageData =
		m_SceneManager.GetGameProgress().
		GetCurrentStageData();

	m_PlayerHealth.Initialize( m_SceneManager.GetGameProgress().GetPlayerStats().maxHp );

	m_EnemyHealth.Initialize( stageData.enemyMaxHp );

	m_IsGameClear = false;
	m_IsResultSceneRequested = false;

	m_IsLastShotHit = false;
	m_IsLastNormalAttackHit = false;
	m_IsLastSpecialAttackHit = false;
	m_IsLastPlayerSpecialAttackHit = false;

	m_HasPlayedLowHpSe = false;
	m_HasPlayedEnemyDefeatSe = false;

	m_EnemyAnimationTime = 0.0f;
	m_GateAnimationTime = 0.0f;

	m_EnemyNormalAttackTimer = 0.0f;
	m_EnemySpecialAttackTimer = 0.0f;
	m_SpecialAttackCooldownTimer = 0.0f;
	m_MuzzleFlashTimer = 0.0f;

	const bool is_BasicMeshRendererInitialized = m_BasicMeshRenderer.Initialize( m_GraphicsSystem );

	if ( !is_BasicMeshRendererInitialized ) OutputDebugStringW( L"[GameScene] BasicMeshRenderer初期化失敗\n" );

	const bool is_SkyInitialized = m_SkyDomeRenderer.Initialize( m_GraphicsSystem, SKY_OBJ_PATH, SKY_TEXTURE_PATH );

	if ( !is_SkyInitialized ) OutputDebugStringW( L"[GameScene] Sky初期化失敗\n" );

	const bool is_EnemyInitialized = m_EnemyModelRenderer.Initialize( m_GraphicsSystem, ENEMY_OBJ_PATH, L"" );

	if ( !is_EnemyInitialized ) OutputDebugStringW( L"[GameScene] Enemy初期化失敗\n" );

	const bool is_HudRendererInitialized = m_HudRenderer.Initialize( m_GraphicsSystem );

	if ( !is_HudRendererInitialized ) OutputDebugStringW( L"[GameScene] HudRenderer初期化失敗\n" );

	const bool is_HudTextRendererInitialized = m_HudTextRenderer.Initialize( m_GraphicsSystem );

	if ( !is_HudTextRendererInitialized ) OutputDebugStringW( L"[GameScene] HudTextRenderer初期化失敗\n" );

	m_InputSystem.SetMouseCaptureEnabled( true );
	m_AudioSystem.PlayGameBgm();
}

bool GameScene::Init()
{
	return true;
}

void GameScene::Update(
	float deltaTime )
{
	if ( m_InputSystem.IsKeyTriggered( VK_F2 ) )
	{
		m_AudioSystem.PlayWarpSe();

		m_InputSystem.SetMouseCaptureEnabled(
			false );

		m_SceneManager.RequestSceneChange<ShopScene>();
		return;
	}

	if ( m_PlayerHealth.IsDead() )
	{
		GameProgress& progress =
			m_SceneManager.GetGameProgress();

		progress.AddDeath();
		progress.ApplyDeathCurrencyPenalty();

		m_AudioSystem.PlayWarpSe();

		m_InputSystem.SetMouseCaptureEnabled(
			false );

		m_SceneManager.RequestSceneChange<ShopScene>();
		return;
	}

	GameProgress& progress =
		m_SceneManager.GetGameProgress();

	progress.Update( deltaTime );

	m_SpecialAttackCooldownTimer =
		std::max(
			0.0f,
			m_SpecialAttackCooldownTimer -
			deltaTime );

	m_MuzzleFlashTimer =
		std::max(
			0.0f,
			m_MuzzleFlashTimer -
			deltaTime );

	const float playerMaxHp =
		m_PlayerHealth.GetMaxHp();

	const float playerCurrentHp =
		m_PlayerHealth.GetCurrentHp();

	const bool isLowHealth =
		playerMaxHp > 0.0f &&
		playerCurrentHp <=
		playerMaxHp *
		LOW_HP_RATIO_THRESHOLD;

	if ( isLowHealth &&
		!m_HasPlayedLowHpSe )
	{
		m_AudioSystem.PlayLowHpSe();
		m_HasPlayedLowHpSe = true;
	}
	else if ( !isLowHealth )
	{
		m_HasPlayedLowHpSe = false;
	}

	const StageData& stageData =
		progress.GetCurrentStageData();

	const DirectX::XMFLOAT3 playerPosition =
		m_DebugPlayer.GetPosition();

	const float deltaX =
		playerPosition.x -
		ENEMY_BASE_X;

	const float deltaZ =
		playerPosition.z -
		ENEMY_BASE_Z;

	const float playerToEnemyDistanceSquared =
		deltaX * deltaX +
		deltaZ * deltaZ;

	if ( !m_EnemyHealth.IsDead() &&
		!m_IsResultSceneRequested )
	{
		m_EnemyNormalAttackTimer +=
			deltaTime;

		if ( m_EnemyNormalAttackTimer >=
			ENEMY_NORMAL_ATTACK_INTERVAL )
		{
			m_EnemyNormalAttackTimer = 0.0f;

			m_IsLastNormalAttackHit =
				playerToEnemyDistanceSquared <=
				ENEMY_NORMAL_ATTACK_RANGE_SQUARED;

			if ( m_IsLastNormalAttackHit )
			{
				m_PlayerHealth.TakeDamage(
					stageData.enemyDamage );

				m_AudioSystem.PlayDamageSe();
			}
		}

		m_EnemySpecialAttackTimer +=
			deltaTime;

		if ( m_EnemySpecialAttackTimer >=
			stageData.specialAttackInterval )
		{
			m_EnemySpecialAttackTimer = 0.0f;

			const float specialAttackRangeSquared =
				stageData.specialAttackHitboxRadius *
				stageData.specialAttackHitboxRadius;

			m_IsLastSpecialAttackHit =
				playerToEnemyDistanceSquared <=
				specialAttackRangeSquared;

			if ( m_IsLastSpecialAttackHit )
			{
				m_PlayerHealth.TakeDamage(
					stageData.enemyDamage *
					ENEMY_SPECIAL_ATTACK_DAMAGE_MULTIPLIER );

				m_AudioSystem.PlayDamageSe();
			}
		}
	}

	if ( m_InputSystem.IsKeyTriggered(
		SHOOT_ENEMY_KEY ) &&
		!m_EnemyHealth.IsDead() &&
		!m_IsResultSceneRequested )
	{
		m_AudioSystem.PlayGunSe();

		m_MuzzleFlashTimer =
			MUZZLE_FLASH_DURATION;

		const DirectX::XMMATRIX inverseViewMatrix =
			DirectX::XMMatrixInverse(
				nullptr,
				m_FpsCamera.GetViewMatrix() );

		const DirectX::XMVECTOR bulletSpawnCameraSpace =
			DirectX::XMVectorSet(
				BULLET_SPAWN_OFFSET_X,
				BULLET_SPAWN_OFFSET_Y,
				BULLET_SPAWN_OFFSET_Z,
				1.0f );

		const DirectX::XMVECTOR bulletSpawnWorldSpace =
			DirectX::XMVector3TransformCoord(
				bulletSpawnCameraSpace,
				inverseViewMatrix );

		DirectX::XMFLOAT3 bulletSpawnPosition{};

		DirectX::XMStoreFloat3(
			&bulletSpawnPosition,
			bulletSpawnWorldSpace );

		const DirectX::XMFLOAT3 bulletDirection =
			m_FpsCamera.GetForward();

		for ( Bullet& bullet : m_Bullets )
		{
			if ( !bullet.isActive )
			{
				bullet.position =
					bulletSpawnPosition;

				bullet.direction =
					bulletDirection;

				bullet.remainingLifetime =
					BULLET_LIFETIME;

				bullet.isActive = true;
				break;
			}
		}

		const DirectX::XMFLOAT3 rayOrigin =
			m_FpsCamera.GetPosition();

		const DirectX::XMFLOAT3 cameraForward =
			m_FpsCamera.GetForward();

		const DirectX::XMVECTOR rayDirection =
			DirectX::XMVector3Normalize(
				DirectX::XMLoadFloat3(
			&cameraForward ) );

		const float enemyFloatOffset =
			std::sinf(
				m_EnemyAnimationTime *
				ENEMY_FLOAT_SPEED ) *
			ENEMY_FLOAT_HEIGHT;

		const DirectX::XMFLOAT3 enemyHitPosition =
		{
			ENEMY_BASE_X,
			ENEMY_BASE_Y +
			enemyFloatOffset +
			ENEMY_HIT_CENTER_Y_OFFSET,
			ENEMY_BASE_Z
		};

		const DirectX::BoundingSphere enemyHitSphere(
			enemyHitPosition,
			ENEMY_HIT_SPHERE_RADIUS );

		float hitDistance = 0.0f;

		m_IsLastShotHit =
			enemyHitSphere.Intersects(
				DirectX::XMLoadFloat3(
			&rayOrigin ),
				rayDirection,
				hitDistance ) &&
			hitDistance <=
			SHOOT_MAX_DISTANCE;

		if ( m_IsLastShotHit )
		{
			const float actualDamage =
				m_EnemyHealth.TakeDamage(
					progress.GetPlayerStats().
					gunDamage );

			progress.AddDamageReward(
				actualDamage );
		}
	}

	if ( m_InputSystem.IsKeyTriggered(
		SPECIAL_ATTACK_KEY ) &&
		!m_EnemyHealth.IsDead() &&
		!m_IsResultSceneRequested &&
		progress.GetPlayerStats().
		isSpecialAttackUnlocked &&
		m_SpecialAttackCooldownTimer <=
		0.0f )
	{
		m_AudioSystem.PlaySpecialSe();

		m_IsLastPlayerSpecialAttackHit =
			playerToEnemyDistanceSquared <=
			PLAYER_SPECIAL_ATTACK_RANGE_SQUARED;

		m_SpecialAttackCooldownTimer =
			progress.GetPlayerStats().
			specialAttackCooldown;

		if ( m_IsLastPlayerSpecialAttackHit )
		{
			const float actualDamage =
				m_EnemyHealth.TakeDamage(
					progress.GetPlayerStats().
					gunDamage *
					PLAYER_SPECIAL_ATTACK_DAMAGE_MULTIPLIER );

			progress.AddDamageReward(
				actualDamage );
		}
	}

	for ( Bullet& bullet : m_Bullets )
	{
		if ( !bullet.isActive )
		{
			continue;
		}

		bullet.position.x +=
			bullet.direction.x *
			BULLET_SPEED *
			deltaTime;

		bullet.position.y +=
			bullet.direction.y *
			BULLET_SPEED *
			deltaTime;

		bullet.position.z +=
			bullet.direction.z *
			BULLET_SPEED *
			deltaTime;

		bullet.remainingLifetime -=
			deltaTime;

		if ( bullet.remainingLifetime <=
			0.0f )
		{
			bullet.isActive = false;
		}
	}

	if ( !m_EnemyHealth.IsDead() )
	{
		m_EnemyAnimationTime +=
			deltaTime;
	}

	m_GateAnimationTime +=
		deltaTime;

	if ( m_EnemyHealth.IsDead() &&
		!m_IsResultSceneRequested )
	{
		if ( !m_HasPlayedEnemyDefeatSe )
		{
			m_AudioSystem.PlayEnemyDefeatSe();
			m_HasPlayedEnemyDefeatSe = true;
		}

		progress.AddEnemyDefeat();

		const bool isFirstClear =
			progress.MarkCurrentStageCleared();

		if ( isFirstClear )
		{
			progress.AddStageClearReward();
		}

		if ( isFirstClear &&
			progress.GetCurrentStage() ==
			StageConstants::MAX_STAGE_COUNT )
		{
			m_IsGameClear = true;
			m_IsResultSceneRequested = true;
		}
	}

	if ( m_InputSystem.IsKeyTriggered( VK_F1 ) )
	{
		m_InputSystem.SetMouseCaptureEnabled(
			!m_InputSystem.
			IsMouseCaptureEnabled() );
	}

	const int currentStage =
		progress.GetCurrentStage();

	const bool isPreviousGateAvailable =
		currentStage >
		StageConstants::FIRST_STAGE_NUMBER;

	const bool isNextGateAvailable =
		currentStage <
		StageConstants::MAX_STAGE_COUNT &&
		progress.IsStageCleared(
			currentStage );

	if ( m_InputSystem.IsKeyTriggered(
		USE_GATE_KEY ) )
	{
		if ( isPreviousGateAvailable &&
			IsPlayerNearGate(
			 PREVIOUS_GATE_POSITION_X,
			 PREVIOUS_GATE_POSITION_Z ) &&
			progress.TrySetCurrentStage(
			 currentStage -
			 PREVIOUS_STAGE_OFFSET ) )
		{
			m_AudioSystem.PlayWarpSe();

			m_InputSystem.SetMouseCaptureEnabled(
				false );

			m_SceneManager.
				RequestSceneChange<GameScene>();

			return;
		}

		if ( isNextGateAvailable &&
			IsPlayerNearGate(
			 NEXT_GATE_POSITION_X,
			 NEXT_GATE_POSITION_Z ) &&
			progress.TrySetCurrentStage(
			 currentStage +
			 NEXT_STAGE_OFFSET ) )
		{
			m_AudioSystem.PlayWarpSe();

			m_InputSystem.SetMouseCaptureEnabled(
				false );

			m_SceneManager.
				RequestSceneChange<GameScene>();

			return;
		}

		if ( IsPlayerNearGate(
			SHOP_GATE_POSITION_X,
			SHOP_GATE_POSITION_Z ) )
		{
			m_AudioSystem.PlayWarpSe();

			m_InputSystem.SetMouseCaptureEnabled(
				false );

			m_SceneManager.
				RequestSceneChange<ShopScene>();

			return;
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
}

void GameScene::Draw()
{
	GameProgress& progress =
		m_SceneManager.GetGameProgress();

	const PlayerStats& playerStats =
		progress.GetPlayerStats();

	const StageData& stageData =
		progress.GetCurrentStageData();

	const int currentStage =
		progress.GetCurrentStage();

	const bool isPreviousGateAvailable =
		currentStage >
		StageConstants::FIRST_STAGE_NUMBER;

	const bool isNextGateAvailable =
		currentStage <
		StageConstants::MAX_STAGE_COUNT &&
		progress.IsStageCleared(
			currentStage );

	enum class GateDestination
	{
		e_NONE,
		e_PREVIOUS_STAGE,
		e_NEXT_STAGE,
		e_SHOP
	};

	GateDestination gateDestination =
		GateDestination::e_NONE;

	const DirectX::XMMATRIX projectionMatrix =
		DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XMConvertToRadians(
		PROJECTION_FOV_DEGREES ),
			PROJECTION_ASPECT_RATIO,
			PROJECTION_NEAR_Z,
			PROJECTION_FAR_Z );

	const DirectX::XMMATRIX viewMatrix =
		m_FpsCamera.GetViewMatrix();

	const DirectX::XMFLOAT3 cameraPosition =
		m_FpsCamera.GetPosition();

	const float enemyFloatOffset =
		std::sinf(
			m_EnemyAnimationTime *
			ENEMY_FLOAT_SPEED ) *
		ENEMY_FLOAT_HEIGHT;

	const float enemyY =
		ENEMY_BASE_Y +
		enemyFloatOffset;

	// スカイドームはカメラ追従にする。
	const DirectX::XMMATRIX skyDomeWorldMatrix =
		DirectX::XMMatrixScaling(
			SKY_DOME_SCALE,
			SKY_DOME_SCALE,
			SKY_DOME_SCALE ) *
		DirectX::XMMatrixTranslation(
			cameraPosition.x,
			cameraPosition.y +
			SKY_DOME_Y_OFFSET,
			cameraPosition.z );

	m_GraphicsSystem.SetAlphaBlendEnabled(
		false );

	m_GraphicsSystem.SetDepthTestEnabled(
		false );

	m_SkyDomeRenderer.Draw(
		m_GraphicsSystem,
		skyDomeWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4(
		1.0f,
		1.0f,
		1.0f,
		1.0f ) );

	m_GraphicsSystem.SetDepthTestEnabled(
		true );

	// 敵はテクスチャなし、単色で描画する。
	if ( !m_EnemyHealth.IsDead() )
	{
		const DirectX::XMMATRIX enemyWorldMatrix =
			DirectX::XMMatrixScaling(
				ENEMY_MODEL_SCALE,
				ENEMY_MODEL_SCALE,
				ENEMY_MODEL_SCALE ) *
			DirectX::XMMatrixRotationY(
				m_EnemyAnimationTime *
				ENEMY_ROTATION_SPEED ) *
			DirectX::XMMatrixTranslation(
				ENEMY_BASE_X,
				enemyY,
				ENEMY_BASE_Z );

		m_EnemyModelRenderer.Draw(
			m_GraphicsSystem,
			enemyWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4(
			0.18f,
			0.95f,
			0.28f,
			1.0f ) );
	}

	const DirectX::XMMATRIX floorWorldMatrix =
		DirectX::XMMatrixScaling(
			20.0f,
			0.2f,
			20.0f ) *
		DirectX::XMMatrixTranslation(
			0.0f,
			-0.6f,
			8.0f );

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
		1.0f,
		1.0f,
		1.0f,
		1.0f
	};

	const DirectX::XMMATRIX leftWallWorldMatrix =
		DirectX::XMMatrixScaling(
			0.2f,
			3.2f,
			20.0f ) *
		DirectX::XMMatrixTranslation(
			-10.0f,
			1.1f,
			8.0f );

	const DirectX::XMMATRIX rightWallWorldMatrix =
		DirectX::XMMatrixScaling(
			0.2f,
			3.2f,
			20.0f ) *
		DirectX::XMMatrixTranslation(
			10.0f,
			1.1f,
			8.0f );

	const DirectX::XMMATRIX nearWallWorldMatrix =
		DirectX::XMMatrixScaling(
			20.0f,
			3.2f,
			0.2f ) *
		DirectX::XMMatrixTranslation(
			0.0f,
			1.1f,
			-2.0f );

	const DirectX::XMMATRIX farWallWorldMatrix =
		DirectX::XMMatrixScaling(
			20.0f,
			3.2f,
			0.2f ) *
		DirectX::XMMatrixTranslation(
			0.0f,
			1.1f,
			18.0f );

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

	const float gateRotationAngle =
		m_GateAnimationTime *
		GATE_ROTATION_SPEED;

	const DirectX::XMMATRIX previousGateWorldMatrix =
		DirectX::XMMatrixScaling(
			GATE_WIDTH,
			GATE_HEIGHT,
			GATE_DEPTH ) *
		DirectX::XMMatrixRotationY(
			gateRotationAngle ) *
		DirectX::XMMatrixTranslation(
			PREVIOUS_GATE_POSITION_X,
			PREVIOUS_GATE_POSITION_Y,
			PREVIOUS_GATE_POSITION_Z );

	const DirectX::XMMATRIX nextGateWorldMatrix =
		DirectX::XMMatrixScaling(
			GATE_WIDTH,
			GATE_HEIGHT,
			GATE_DEPTH ) *
		DirectX::XMMatrixRotationY(
			gateRotationAngle ) *
		DirectX::XMMatrixTranslation(
			NEXT_GATE_POSITION_X,
			NEXT_GATE_POSITION_Y,
			NEXT_GATE_POSITION_Z );

	const DirectX::XMMATRIX shopGateWorldMatrix =
		DirectX::XMMatrixScaling(
			GATE_WIDTH,
			GATE_HEIGHT,
			GATE_DEPTH ) *
		DirectX::XMMatrixRotationY(
			gateRotationAngle ) *
		DirectX::XMMatrixTranslation(
			SHOP_GATE_POSITION_X,
			SHOP_GATE_POSITION_Y,
			SHOP_GATE_POSITION_Z );

	if ( isPreviousGateAvailable )
	{
		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			previousGateWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4(
			0.85f,
			0.30f,
			1.0f,
			GATE_COLOR_ALPHA ),
			DirectX::XMFLOAT2(
			1.0f,
			1.0f ),
			BasicMeshRenderer::TextureType::Color );
	}

	if ( isNextGateAvailable )
	{
		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			nextGateWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4(
			0.10f,
			0.85f,
			1.0f,
			GATE_COLOR_ALPHA ),
			DirectX::XMFLOAT2(
			1.0f,
			1.0f ),
			BasicMeshRenderer::TextureType::Color );
	}

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		shopGateWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4(
		1.0f,
		0.75f,
		0.10f,
		GATE_COLOR_ALPHA ),
		DirectX::XMFLOAT2(
		1.0f,
		1.0f ),
		BasicMeshRenderer::TextureType::Color );

	const DirectX::XMMATRIX inverseViewMatrix =
		DirectX::XMMatrixInverse(
			nullptr,
			viewMatrix );

	const DirectX::XMMATRIX gunBodyWorldMatrix =
		DirectX::XMMatrixScaling(
			GUN_BODY_SCALE_X,
			GUN_BODY_SCALE_Y,
			GUN_BODY_SCALE_Z ) *
		DirectX::XMMatrixTranslation(
			GUN_POSITION_X,
			GUN_POSITION_Y,
			GUN_POSITION_Z ) *
		inverseViewMatrix;

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		gunBodyWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4(
		0.12f,
		0.12f,
		0.14f,
		1.0f ),
		DirectX::XMFLOAT2(
		1.0f,
		1.0f ),
		BasicMeshRenderer::TextureType::Color );

	const DirectX::XMMATRIX gunBarrelWorldMatrix =
		DirectX::XMMatrixScaling(
			GUN_BARREL_SCALE_X,
			GUN_BARREL_SCALE_Y,
			GUN_BARREL_SCALE_Z ) *
		DirectX::XMMatrixTranslation(
			GUN_POSITION_X,
			GUN_POSITION_Y +
			0.04f,
			GUN_POSITION_Z +
			GUN_BARREL_OFFSET_Z ) *
		inverseViewMatrix;

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		gunBarrelWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4(
		0.30f,
		0.32f,
		0.36f,
		1.0f ),
		DirectX::XMFLOAT2(
		1.0f,
		1.0f ),
		BasicMeshRenderer::TextureType::Color );

	if ( m_MuzzleFlashTimer > 0.0f )
	{
		const float muzzleFlashAlpha =
			m_MuzzleFlashTimer /
			MUZZLE_FLASH_DURATION;

		const DirectX::XMMATRIX muzzleFlashWorldMatrix =
			DirectX::XMMatrixScaling(
				MUZZLE_FLASH_SCALE,
				MUZZLE_FLASH_SCALE,
				MUZZLE_FLASH_SCALE ) *
			DirectX::XMMatrixTranslation(
				GUN_POSITION_X,
				GUN_POSITION_Y +
				0.04f,
				GUN_POSITION_Z +
				MUZZLE_FLASH_OFFSET_Z ) *
			inverseViewMatrix;

		m_GraphicsSystem.SetAlphaBlendEnabled(
			true );

		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			muzzleFlashWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4(
			1.0f,
			0.65f,
			0.05f,
			muzzleFlashAlpha ),
			DirectX::XMFLOAT2(
			1.0f,
			1.0f ),
			BasicMeshRenderer::TextureType::Color );

		m_GraphicsSystem.SetAlphaBlendEnabled(
			false );
	}

	m_GraphicsSystem.SetAlphaBlendEnabled(
		true );

	for ( const Bullet& bullet :
		m_Bullets )
	{
		if ( !bullet.isActive )
		{
			continue;
		}

		const float bulletAlpha =
			std::clamp(
				bullet.remainingLifetime /
				BULLET_LIFETIME,
				0.0f,
				1.0f );

		const DirectX::XMMATRIX bulletWorldMatrix =
			DirectX::XMMatrixScaling(
				BULLET_SCALE,
				BULLET_SCALE,
				BULLET_SCALE ) *
			DirectX::XMMatrixTranslation(
				bullet.position.x,
				bullet.position.y,
				bullet.position.z );

		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			bulletWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4(
			1.0f,
			0.85f,
			0.10f,
			bulletAlpha ),
			DirectX::XMFLOAT2(
			1.0f,
			1.0f ),
			BasicMeshRenderer::TextureType::Color );
	}

	m_GraphicsSystem.SetAlphaBlendEnabled(
		false );

	const float enemyHpRatio =
		std::clamp(
			m_EnemyHealth.GetCurrentHp() /
			m_EnemyHealth.GetMaxHp(),
			0.0f,
			1.0f );

	const bool isHpBarVisible =
		!m_EnemyHealth.IsDead() &&
		enemyHpRatio > 0.001f;

	const DirectX::XMFLOAT3 hpBarPosition =
	{
		ENEMY_BASE_X,
		enemyY +
		HP_BAR_Y_OFFSET,
		ENEMY_BASE_Z
	};

	DirectX::XMVECTOR toCameraVector =
		DirectX::XMVectorSubtract(
			DirectX::XMLoadFloat3(
		&cameraPosition ),
			DirectX::XMLoadFloat3(
		&hpBarPosition ) );

	toCameraVector =
		DirectX::XMVectorSetY(
			toCameraVector,
			0.0f );

	const float toCameraLengthSquared =
		DirectX::XMVectorGetX(
			DirectX::XMVector3LengthSq(
		toCameraVector ) );

	if ( isHpBarVisible &&
		toCameraLengthSquared > 0.0001f )
	{
		toCameraVector =
			DirectX::XMVector3Normalize(
				toCameraVector );

		const DirectX::XMVECTOR worldUpVector =
			DirectX::XMVectorSet(
				0.0f,
				1.0f,
				0.0f,
				0.0f );

		const DirectX::XMVECTOR billboardRightVector =
			DirectX::XMVector3Normalize(
				DirectX::XMVector3Cross(
			worldUpVector,
			toCameraVector ) );

		const DirectX::XMVECTOR billboardForwardVector =
			DirectX::XMVector3Normalize(
				DirectX::XMVector3Cross(
			billboardRightVector,
			worldUpVector ) );

		DirectX::XMMATRIX billboardRotationMatrix =
			DirectX::XMMatrixIdentity();

		billboardRotationMatrix.r[ 0 ] =
			DirectX::XMVectorSetW(
				billboardRightVector,
				0.0f );

		billboardRotationMatrix.r[ 1 ] =
			DirectX::XMVectorSetW(
				worldUpVector,
				0.0f );

		billboardRotationMatrix.r[ 2 ] =
			DirectX::XMVectorSetW(
				billboardForwardVector,
				0.0f );

		const float hpBarForegroundWidth =
			HP_BAR_WIDTH *
			enemyHpRatio;

		const float hpBarForegroundXOffset =
			-( HP_BAR_WIDTH -
			hpBarForegroundWidth ) *
			0.5f;

		const DirectX::XMMATRIX hpBarBackgroundWorldMatrix =
			DirectX::XMMatrixScaling(
				HP_BAR_WIDTH,
				HP_BAR_HEIGHT,
				HP_BAR_BACKGROUND_DEPTH ) *
			billboardRotationMatrix *
			DirectX::XMMatrixTranslation(
				hpBarPosition.x,
				hpBarPosition.y,
				hpBarPosition.z );

		const DirectX::XMVECTOR foregroundPositionVector =
			DirectX::XMVectorAdd(
				DirectX::XMVectorSet(
			hpBarPosition.x +
			hpBarForegroundXOffset,
			hpBarPosition.y,
			hpBarPosition.z,
			1.0f ),
				DirectX::XMVectorScale(
			toCameraVector,
			HP_BAR_FOREGROUND_CAMERA_OFFSET ) );

		DirectX::XMFLOAT3 foregroundPosition{};

		DirectX::XMStoreFloat3(
			&foregroundPosition,
			foregroundPositionVector );

		const DirectX::XMMATRIX hpBarForegroundWorldMatrix =
			DirectX::XMMatrixScaling(
				hpBarForegroundWidth,
				HP_BAR_HEIGHT,
				HP_BAR_FOREGROUND_DEPTH ) *
			billboardRotationMatrix *
			DirectX::XMMatrixTranslation(
				foregroundPosition.x,
				foregroundPosition.y,
				foregroundPosition.z );

		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			hpBarBackgroundWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4(
			0.0f,
			0.0f,
			0.0f,
			0.70f ),
			DirectX::XMFLOAT2(
			1.0f,
			1.0f ),
			BasicMeshRenderer::TextureType::Color );

		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			hpBarForegroundWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4(
			0.10f,
			1.0f,
			0.20f,
			0.95f ),
			DirectX::XMFLOAT2(
			1.0f,
			1.0f ),
			BasicMeshRenderer::TextureType::Color );
	}

	m_GraphicsSystem.SetDepthTestEnabled(
		false );

	m_HudRenderer.DrawCrosshair(
		m_GraphicsSystem );

	m_HudRenderer.DrawPlayerHealthBar(
		m_GraphicsSystem,
		m_PlayerHealth.GetCurrentHp(),
		m_PlayerHealth.GetMaxHp() );

	m_HudRenderer.DrawLowHealthWarning(
		m_GraphicsSystem,
		m_PlayerHealth.GetCurrentHp(),
		m_PlayerHealth.GetMaxHp() );

	m_HudTextRenderer.Begin();

	wchar_t stageText[ 64 ]{};

	swprintf_s(
		stageText,
		L"ステージ %d / %d",
		currentStage,
		StageConstants::MAX_STAGE_COUNT );

	m_HudTextRenderer.DrawText(
		stageText,
		DirectX::XMFLOAT2(
		520.0f,
		24.0f ),
		DirectX::Colors::White,
		0.90f );

	wchar_t currencyText[ 64 ]{};

	swprintf_s(
		currencyText,
		L"所持金: %d G",
		progress.GetCurrency() );

	m_HudTextRenderer.DrawText(
		currencyText,
		DirectX::XMFLOAT2(
		1040.0f,
		24.0f ),
		DirectX::Colors::Gold,
		0.75f );

	wchar_t healthText[ 64 ]{};

	swprintf_s(
		healthText,
		L"HP: %.0f / %.0f",
		m_PlayerHealth.GetCurrentHp(),
		m_PlayerHealth.GetMaxHp() );

	m_HudTextRenderer.DrawText(
		healthText,
		DirectX::XMFLOAT2(
		40.0f,
		612.0f ),
		DirectX::Colors::White,
		0.75f );

	const bool isSpecialAttackUnlocked =
		playerStats.isSpecialAttackUnlocked;

	const bool isSpecialAttackReady =
		m_SpecialAttackCooldownTimer <=
		0.0f;

	const wchar_t* specialAttackText =
		L"";

	if ( !isSpecialAttackUnlocked )
	{
		specialAttackText =
			L"Q: 未解放";
	}
	else if ( isSpecialAttackReady )
	{
		specialAttackText =
			L"Q: 使用可能";
	}
	else
	{
		specialAttackText =
			L"Q: 待機中";
	}

	const DirectX::XMVECTORF32 specialAttackColor =
		!isSpecialAttackUnlocked
		? DirectX::Colors::Yellow
		: isSpecialAttackReady
		? DirectX::Colors::Lime
		: DirectX::Colors::Orange;

	m_HudTextRenderer.DrawText(
		specialAttackText,
		DirectX::XMFLOAT2(
		1040.0f,
		640.0f ),
		specialAttackColor,
		0.75f );

	if ( isSpecialAttackUnlocked &&
		!isSpecialAttackReady )
	{
		wchar_t cooldownText[ 64 ]{};

		swprintf_s(
			cooldownText,
			L"残り %.1f 秒",
			m_SpecialAttackCooldownTimer );

		m_HudTextRenderer.DrawText(
			cooldownText,
			DirectX::XMFLOAT2(
			1040.0f,
			670.0f ),
			DirectX::Colors::White,
			0.65f );
	}

	if ( isNextGateAvailable &&
		IsPlayerNearGate(
		 NEXT_GATE_POSITION_X,
		 NEXT_GATE_POSITION_Z ) )
	{
		m_HudTextRenderer.DrawText(
			L"E: 次のステージへ",
			DirectX::XMFLOAT2(
			HUD_GATE_HINT_X,
			HUD_GATE_HINT_Y ),
			DirectX::Colors::Cyan,
			HUD_MESSAGE_SCALE );
	}
	else if ( isPreviousGateAvailable &&
		IsPlayerNearGate(
			  PREVIOUS_GATE_POSITION_X,
			  PREVIOUS_GATE_POSITION_Z ) )
	{
		m_HudTextRenderer.DrawText(
			L"E: 前のステージへ",
			DirectX::XMFLOAT2(
			HUD_GATE_HINT_X,
			HUD_GATE_HINT_Y ),
			DirectX::Colors::Violet,
			HUD_MESSAGE_SCALE );
	}
	else if ( IsPlayerNearGate(
		SHOP_GATE_POSITION_X,
		SHOP_GATE_POSITION_Z ) )
	{
		m_HudTextRenderer.DrawText(
			L"E: ショップへ",
			DirectX::XMFLOAT2(
			HUD_GATE_HINT_X,
			HUD_GATE_HINT_Y ),
			DirectX::Colors::Gold,
			HUD_MESSAGE_SCALE );
	}

	if ( currentStage ==
		TUTORIAL_STAGE_NUMBER &&
		!m_EnemyHealth.IsDead() )
	{
		m_HudTextRenderer.DrawText(
			L"WASD: 移動",
			DirectX::XMFLOAT2(
			HUD_TUTORIAL_X,
			HUD_TUTORIAL_Y ),
			DirectX::Colors::White,
			HUD_TUTORIAL_SCALE );

		m_HudTextRenderer.DrawText(
			L"マウス: 視点移動",
			DirectX::XMFLOAT2(
			HUD_TUTORIAL_X,
			HUD_TUTORIAL_Y +
			28.0f ),
			DirectX::Colors::White,
			HUD_TUTORIAL_SCALE );

		m_HudTextRenderer.DrawText(
			L"左クリック: 射撃",
			DirectX::XMFLOAT2(
			HUD_TUTORIAL_X,
			HUD_TUTORIAL_Y +
			56.0f ),
			DirectX::Colors::White,
			HUD_TUTORIAL_SCALE );

		m_HudTextRenderer.DrawText(
			L"Q: 範囲攻撃",
			DirectX::XMFLOAT2(
			HUD_TUTORIAL_X,
			HUD_TUTORIAL_Y +
			84.0f ),
			DirectX::Colors::White,
			HUD_TUTORIAL_SCALE );

		m_HudTextRenderer.DrawText(
			L"ゲートに近づいてEキー",
			DirectX::XMFLOAT2(
			HUD_TUTORIAL_X,
			HUD_TUTORIAL_Y +
			112.0f ),
			DirectX::Colors::White,
			HUD_TUTORIAL_SCALE );

		m_HudTextRenderer.DrawText(
			L"死亡時: 所持金の25%を失う",
			DirectX::XMFLOAT2(
			HUD_TUTORIAL_X,
			HUD_TUTORIAL_Y +
			140.0f ),
			DirectX::Colors::Orange,
			HUD_TUTORIAL_SCALE );

		if ( !isSpecialAttackUnlocked )
		{
			m_HudTextRenderer.DrawText(
				L"Qは未解放です - ショップで解放できます",
				DirectX::XMFLOAT2(
				HUD_TUTORIAL_X,
				HUD_TUTORIAL_Y +
				168.0f ),
				DirectX::Colors::Yellow,
				HUD_TUTORIAL_SCALE );
		}
	}

	if ( m_EnemyHealth.IsDead() &&
		!m_IsGameClear )
	{
		m_HudTextRenderer.DrawText(
			L"ステージクリア！ 青いゲートへ",
			DirectX::XMFLOAT2(
			420.0f,
			90.0f ),
			DirectX::Colors::Lime,
			0.90f );
	}

	m_HudTextRenderer.End();

	m_GraphicsSystem.SetDepthTestEnabled(
		true );

	m_GraphicsSystem.SetAlphaBlendEnabled(
		false );

	ImGui::Begin( "Game Debug" );

	ImGui::Text(
		"Stage %d / 5",
		stageData.stageNumber );

	ImGui::Text(
		"Enemy: %s",
		stageData.enemyName );

	ImGui::Text(
		"Clear Reward: %d",
		stageData.clearReward );

	if ( ImGui::Button(
		"Test: Take 10 Damage" ) )
	{
		m_PlayerHealth.TakeDamage(
			TEST_PLAYER_DAMAGE );
	}

	ImGui::Separator();

	ImGui::Text(
		"Enemy HP: %.0f / %.0f",
		m_EnemyHealth.GetCurrentHp(),
		m_EnemyHealth.GetMaxHp() );

	ImGui::Text(
		"Last Shot: %s",
		m_IsLastShotHit
		? "HIT"
		: "MISS" );

	ImGui::Text(
		"Currency: %d",
		progress.GetCurrency() );

	ImGui::Text(
		"Deaths: %d",
		progress.GetTotalDeaths() );

	ImGui::Text(
		"Play Time: %.1f sec",
		progress.GetTotalPlayTime() );

	ImGui::Separator();

	if ( isPreviousGateAvailable &&
		ImGui::Button(
		 PREVIOUS_STAGE_GATE_LABEL,
		 ImVec2(
		 GATE_BUTTON_WIDTH,
		 GATE_BUTTON_HEIGHT ) ) )
	{
		gateDestination =
			GateDestination::e_PREVIOUS_STAGE;
	}

	if ( currentStage <
		StageConstants::MAX_STAGE_COUNT )
	{
		if ( !isNextGateAvailable )
		{
			ImGui::BeginDisabled();
		}

		if ( ImGui::Button(
			NEXT_STAGE_GATE_LABEL,
			ImVec2(
			GATE_BUTTON_WIDTH,
			GATE_BUTTON_HEIGHT ) ) )
		{
			gateDestination =
				GateDestination::e_NEXT_STAGE;
		}

		if ( !isNextGateAvailable )
		{
			ImGui::EndDisabled();
		}
	}

	if ( ImGui::Button(
		SHOP_GATE_LABEL,
		ImVec2(
		GATE_BUTTON_WIDTH,
		GATE_BUTTON_HEIGHT ) ) )
	{
		gateDestination =
			GateDestination::e_SHOP;
	}

	ImGui::End();

	if ( m_IsResultSceneRequested )
	{
		m_AudioSystem.PlayWarpSe();

		m_InputSystem.SetMouseCaptureEnabled(
			false );

		m_SceneManager.
			RequestSceneChange<ResultScene>();

		return;
	}

	if ( gateDestination ==
		GateDestination::e_PREVIOUS_STAGE &&
		progress.TrySetCurrentStage(
		 currentStage -
		 PREVIOUS_STAGE_OFFSET ) )
	{
		m_AudioSystem.PlayWarpSe();

		m_InputSystem.SetMouseCaptureEnabled(
			false );

		m_SceneManager.
			RequestSceneChange<GameScene>();
	}
	else if ( gateDestination ==
		GateDestination::e_NEXT_STAGE &&
		progress.TrySetCurrentStage(
			  currentStage +
			  NEXT_STAGE_OFFSET ) )
	{
		m_AudioSystem.PlayWarpSe();

		m_InputSystem.SetMouseCaptureEnabled(
			false );

		m_SceneManager.
			RequestSceneChange<GameScene>();
	}
	else if ( gateDestination ==
		GateDestination::e_SHOP )
	{
		m_AudioSystem.PlayWarpSe();

		m_InputSystem.SetMouseCaptureEnabled(
			false );

		m_SceneManager.
			RequestSceneChange<ShopScene>();
	}
}

bool GameScene::IsPlayerNearGate(
	float gatePositionX,
	float gatePositionZ ) const
{
	const DirectX::XMFLOAT3 playerPosition =
		m_DebugPlayer.GetPosition();

	const float deltaX =
		playerPosition.x -
		gatePositionX;

	const float deltaZ =
		playerPosition.z -
		gatePositionZ;

	const float distanceSquared =
		deltaX * deltaX +
		deltaZ * deltaZ;

	return distanceSquared <=
		GATE_INTERACTION_RADIUS_SQUARED;
}

void GameScene::Uninit()
{
	m_HudTextRenderer.Uninit();
	m_HudRenderer.Uninit();

	m_EnemyModelRenderer.Uninit();
	m_SkyDomeRenderer.Uninit();

	m_BasicMeshRenderer.Uninit();
}