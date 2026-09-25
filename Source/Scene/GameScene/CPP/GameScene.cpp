#include "../H/GameScene.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <filesystem>
#include <system_error>

//========= DirectX インクルード=========
#include <DirectXCollision.h>
#include <DirectXColors.h>

//========= 外部ライブラリ インクルード=========
#include "imgui.h"

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/H/SceneManager.h"
#include "Scene/ResultScene/H/ResultScene.h"
#include "Scene/ShopScene/H/ShopScene.h"

namespace
{
	//========= 入力定数=========
	// ゲート操作、射撃、特殊攻撃、Shopへのデバッグ遷移に使用するキー。
	constexpr unsigned char USE_GATE_KEY = 'E';
	constexpr unsigned char SHOOT_ENEMY_KEY = VK_LBUTTON;
	constexpr unsigned char SPECIAL_ATTACK_KEY = 'Q';
	constexpr unsigned char TOGGLE_MOUSE_CAPTURE_KEY = VK_F1;
	constexpr unsigned char RETURN_TO_SHOP_KEY = VK_F2;

	//========= Stage移動定数=========
	// 前後のStageへ移動するための番号差分。
	constexpr int PREVIOUS_STAGE_OFFSET = 1;
	constexpr int NEXT_STAGE_OFFSET = 1;

	// チュートリアルを表示するStage番号。
	constexpr int TUTORIAL_STAGE_NUMBER = StageConstants::FIRST_STAGE_NUMBER;

	//========= Camera設定定数=========
	// 3D描画に使用するProjection設定。
	constexpr float PROJECTION_ASPECT_RATIO = 1280.0f / 720.0f;
	constexpr float PROJECTION_FOV_DEGREES = 60.0f;
	constexpr float PROJECTION_NEAR_Z = 0.1f;
	constexpr float PROJECTION_FAR_Z = 1000.0f;

	//========= 敵モデル定数=========
	// 敵モデルの初期位置と描画Scale。
	constexpr float ENEMY_BASE_X = 0.0f;
	constexpr float ENEMY_BASE_Y = -0.45f;
	constexpr float ENEMY_BASE_Z = 8.0f;
	constexpr float ENEMY_MODEL_SCALE = 0.50f;

	// 敵モデルの浮遊・回転演出設定。
	constexpr float ENEMY_FLOAT_HEIGHT = 0.10f;
	constexpr float ENEMY_FLOAT_SPEED = 2.0f;
	constexpr float ENEMY_ROTATION_SPEED = 1.5f;

	// 敵への通常射撃Ray判定に使用するSphere設定。
	constexpr float ENEMY_HIT_CENTER_Y_OFFSET = 0.50f;
	constexpr float ENEMY_HIT_SPHERE_RADIUS = 0.55f;
	constexpr float SHOOT_MAX_DISTANCE = 30.0f;

	//========= Sky Dome定数=========
	// Sky DomeのScaleとCamera追従時のY座標補正。
	constexpr float SKY_DOME_SCALE = 150.0f;
	constexpr float SKY_DOME_Y_OFFSET = 0.0f;

	//========= 敵攻撃定数=========
	// 敵通常攻撃の射程と攻撃間隔。
	constexpr float ENEMY_NORMAL_ATTACK_RANGE = 3.0f;
	constexpr float ENEMY_NORMAL_ATTACK_RANGE_SQUARED =
		ENEMY_NORMAL_ATTACK_RANGE * ENEMY_NORMAL_ATTACK_RANGE;
	constexpr float ENEMY_NORMAL_ATTACK_INTERVAL = 1.2f;

	// 敵特殊攻撃のダメージ倍率。
	constexpr float ENEMY_SPECIAL_ATTACK_DAMAGE_MULTIPLIER = 2.0f;

	//========= Player特殊攻撃定数=========
	// Player特殊攻撃の射程とダメージ倍率。
	constexpr float PLAYER_SPECIAL_ATTACK_RANGE = 8.0f;
	constexpr float PLAYER_SPECIAL_ATTACK_RANGE_SQUARED =
		PLAYER_SPECIAL_ATTACK_RANGE * PLAYER_SPECIAL_ATTACK_RANGE;
	constexpr float PLAYER_SPECIAL_ATTACK_DAMAGE_MULTIPLIER = 3.0f;

	//========= 敵HPバー定数=========
	// 敵頭上に描画するHPバーのサイズと配置。
	constexpr float HP_BAR_WIDTH = 2.0f;
	constexpr float HP_BAR_HEIGHT = 0.18f;
	constexpr float HP_BAR_Y_OFFSET = 1.10f;

	// HPバーの厚さ、Camera方向への補正、表示判定に使用する値。
	constexpr float HP_BAR_BACKGROUND_DEPTH = 0.02f;
	constexpr float HP_BAR_FOREGROUND_DEPTH = 0.02f;
	constexpr float HP_BAR_FOREGROUND_CAMERA_OFFSET = 0.08f;
	constexpr float HP_BAR_VISIBLE_RATIO_THRESHOLD = 0.001f;
	constexpr float BILLBOARD_MIN_CAMERA_DISTANCE_SQUARED = 0.0001f;

	//========= Stage Gate定数=========
	// Gateのサイズ、回転、操作範囲。
	constexpr float GATE_WIDTH = 1.2f;
	constexpr float GATE_HEIGHT = 1.4f;
	constexpr float GATE_DEPTH = 0.35f;
	constexpr float GATE_ROTATION_SPEED = 1.4f;
	constexpr float GATE_INTERACTION_RADIUS = 2.0f;
	constexpr float GATE_INTERACTION_RADIUS_SQUARED =
		GATE_INTERACTION_RADIUS * GATE_INTERACTION_RADIUS;

	// 前Stage、次Stage、Shopへ移動するGateの位置。
	constexpr float PREVIOUS_GATE_POSITION_X = -6.5f;
	constexpr float PREVIOUS_GATE_POSITION_Y = 1.2f;
	constexpr float PREVIOUS_GATE_POSITION_Z = 10.0f;
	constexpr float NEXT_GATE_POSITION_X = 0.0f;
	constexpr float NEXT_GATE_POSITION_Y = 1.2f;
	constexpr float NEXT_GATE_POSITION_Z = 13.0f;
	constexpr float SHOP_GATE_POSITION_X = 6.5f;
	constexpr float SHOP_GATE_POSITION_Y = 1.2f;
	constexpr float SHOP_GATE_POSITION_Z = 10.0f;

	//========= 銃・弾定数=========
	// Camera空間における銃の表示位置。
	constexpr float GUN_POSITION_X = 0.55f;
	constexpr float GUN_POSITION_Y = -0.38f;
	constexpr float GUN_POSITION_Z = 1.20f;

	// 銃本体と銃身のサイズ・位置補正。
	constexpr float GUN_BODY_SCALE_X = 0.24f;
	constexpr float GUN_BODY_SCALE_Y = 0.16f;
	constexpr float GUN_BODY_SCALE_Z = 0.65f;
	constexpr float GUN_BARREL_SCALE_X = 0.09f;
	constexpr float GUN_BARREL_SCALE_Y = 0.09f;
	constexpr float GUN_BARREL_SCALE_Z = 0.55f;
	constexpr float GUN_BARREL_OFFSET_Y = 0.04f;
	constexpr float GUN_BARREL_OFFSET_Z = 0.55f;

	// 銃口Flashのサイズ、位置、表示時間。
	constexpr float MUZZLE_FLASH_SCALE = 0.20f;
	constexpr float MUZZLE_FLASH_OFFSET_Z = 1.10f;
	constexpr float MUZZLE_FLASH_DURATION = 0.08f;

	// 弾の速度、寿命、サイズ、発射位置補正。
	constexpr float BULLET_SPEED = 28.0f;
	constexpr float BULLET_LIFETIME = 1.20f;
	constexpr float BULLET_SCALE = 0.08f;
	constexpr float BULLET_SPAWN_OFFSET_X = 0.55f;
	constexpr float BULLET_SPAWN_OFFSET_Y = -0.34f;
	constexpr float BULLET_SPAWN_OFFSET_Z = 1.85f;

	//========= HP・HUD定数=========
	// 低HP警告を開始するHP割合。
	constexpr float LOW_HP_RATIO_THRESHOLD = 0.25f;

	// Gate操作案内の表示位置と文字Scale。
	constexpr float HUD_GATE_HINT_X = 440.0f;
	constexpr float HUD_GATE_HINT_Y = 590.0f;
	constexpr float HUD_MESSAGE_SCALE = 0.85f;

	// Tutorial操作説明の表示位置と文字Scale。
	constexpr float HUD_TUTORIAL_X = 40.0f;
	constexpr float HUD_TUTORIAL_Y = 80.0f;
	constexpr float HUD_TUTORIAL_LINE_HEIGHT = 28.0f;
	constexpr float HUD_TUTORIAL_SCALE = 0.65f;

	// HUD文字の各表示位置とScale。
	constexpr float HUD_STAGE_TEXT_X = 520.0f;
	constexpr float HUD_STAGE_TEXT_Y = 24.0f;
	constexpr float HUD_CURRENCY_TEXT_X = 1040.0f;
	constexpr float HUD_CURRENCY_TEXT_Y = 24.0f;
	constexpr float HUD_HP_TEXT_X = 40.0f;
	constexpr float HUD_HP_TEXT_Y = 612.0f;
	constexpr float HUD_SPECIAL_ATTACK_TEXT_X = 1040.0f;
	constexpr float HUD_SPECIAL_ATTACK_TEXT_Y = 640.0f;
	constexpr float HUD_COOLDOWN_TEXT_X = 1040.0f;
	constexpr float HUD_COOLDOWN_TEXT_Y = 670.0f;
	constexpr float HUD_TEXT_SCALE = 0.75f;
	constexpr float HUD_COOLDOWN_TEXT_SCALE = 0.65f;

	//========= ImGui Debug UI定数=========
	// Debug UIからPlayerへ与えるテストダメージ。
	constexpr float TEST_PLAYER_DAMAGE = 10.0f;

	// Gate操作ボタンのサイズと表示名。
	constexpr float GATE_BUTTON_WIDTH = 240.0f;
	constexpr float GATE_BUTTON_HEIGHT = 32.0f;
	constexpr const char* PREVIOUS_STAGE_GATE_LABEL = "Previous Stage Gate";
	constexpr const char* NEXT_STAGE_GATE_LABEL = "Next Stage Gate";
	constexpr const char* SHOP_GATE_LABEL = "Shop Gate";

	//========= Assetパス定数=========
	// Sky Domeと敵モデルの読み込みに使用するパス。
	constexpr const wchar_t* SKY_OBJ_PATH = L"Assets\\Models\\Sky\\sky.obj";
	constexpr const wchar_t* SKY_TEXTURE_PATH = L"Assets\\Models\\Sky\\sky.jpg";
	constexpr const wchar_t* ENEMY_OBJ_PATH = L"Assets\\Models\\Enemy\\player.obj";

	// 指定した必須Assetが存在するかを確認し、失敗時は対象パスを出力する。
	bool IsRequiredAssetAvailable( const wchar_t* assetPath )
	{
		std::error_code errorCode{};
		const bool isAssetAvailable = std::filesystem::exists( std::filesystem::path{ assetPath }, errorCode );

		if ( errorCode )
		{
			std::wstring message{ L"必須Assetの存在確認に失敗しました: " };
			message += assetPath;

			Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_ASSET, message );

			return false;
		}

		if ( !isAssetAvailable )
		{
			std::wstring message{ L"必須Assetが見つかりません: " };
			message += assetPath;

			Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_ASSET, message );

			return false;
		}

		return true;
	}
}
// GameSceneが使用するSceneManagerとFramework Systemを登録する。
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

// Stageの敵、Player、HUD、BGMに使用するゲーム状態を初期化する。
void GameScene::Initialize()
{
	// SceneManagerが所有する現在Stageの設定とPlayer強化後能力を取得する。
	const GameProgress& progress = m_SceneManager.GetGameProgress();
	const StageData& stageData = progress.GetCurrentStageData();
	const PlayerStats& playerStats = progress.GetPlayerStats();

	// Playerと敵のHPを現在Stageの設定で初期化する。
	m_PlayerHealth.Initialize( playerStats.maxHp );
	m_EnemyHealth.Initialize( stageData.enemyMaxHp );

	// 戦闘、Scene進行、SE再生の状態を初期化する。
	m_IsGameClear = {};
	m_IsResultSceneRequested = {};
	m_IsLastShotHit = {};
	m_IsLastNormalAttackHit = {};
	m_IsLastSpecialAttackHit = {};
	m_IsLastPlayerSpecialAttackHit = {};
	m_HasPlayedLowHpSe = {};
	m_HasPlayedEnemyDefeatSe = {};

	// 敵・Gate・特殊攻撃・銃口FlashのTimerを初期化する。
	m_EnemyAnimationTime = {};
	m_GateAnimationTime = {};
	m_EnemyNormalAttackTimer = {};
	m_EnemySpecialAttackTimer = {};
	m_SpecialAttackCooldownTimer = {};
	m_MuzzleFlashTimer = {};

	// 弾の使用状態を初期化する。
	m_Bullets.fill( {} );

	// GameSceneではFPS操作を有効にし、Game用BGMを再生する。
	m_InputSystem.SetMouseCaptureEnabled( true );
	m_AudioSystem.PlayGameBgm();
}

// GameSceneで使用する必須の3D・2D描画Resourceを初期化する。
bool GameScene::Init()
{
	// GameSceneで必須となるモデルとTextureの存在を確認する。
	const bool isSkyObjAvailable = IsRequiredAssetAvailable( SKY_OBJ_PATH );
	const bool isSkyTextureAvailable = IsRequiredAssetAvailable( SKY_TEXTURE_PATH );
	const bool isEnemyObjAvailable = IsRequiredAssetAvailable( ENEMY_OBJ_PATH );

	if ( !isSkyObjAvailable || !isSkyTextureAvailable || !isEnemyObjAvailable )
	{
		Uninit();
		return false;
	}

	// Cube描画に使用するRendererを初期化する。
	const bool isBasicMeshRendererInitialized = m_BasicMeshRenderer.Initialize( m_GraphicsSystem );
	if ( !isBasicMeshRendererInitialized )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"BasicMeshRenderer初期化失敗" );
		Uninit();
		return false;
	}

	// Sky DomeのOBJ、Texture、Shaderを初期化する。
	const bool isSkyDomeRendererInitialized = m_SkyDomeRenderer.Initialize( m_GraphicsSystem, SKY_OBJ_PATH, SKY_TEXTURE_PATH );
	if ( !isSkyDomeRendererInitialized )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"SkyDomeRenderer初期化失敗" );
		Uninit();
		return false;
	}

	// 敵OBJと単色描画用Shaderを初期化する。
	const bool isEnemyModelRendererInitialized = m_EnemyModelRenderer.Initialize( m_GraphicsSystem, ENEMY_OBJ_PATH, L"" );
	if ( !isEnemyModelRendererInitialized )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"EnemyModelRenderer初期化失敗" );
		Uninit();
		return false;
	}

	// HUDのQuad描画Resourceを初期化する。
	const bool isHudRendererInitialized = m_HudRenderer.Initialize( m_GraphicsSystem );
	if ( !isHudRendererInitialized )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"HudRenderer初期化失敗" );
		Uninit();
		return false;
	}

	// HUDのText描画Resourceを初期化する。
	const bool isHudTextRendererInitialized = m_HudTextRenderer.Initialize( m_GraphicsSystem );
	if ( !isHudTextRendererInitialized )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"HudTextRenderer初期化失敗" );
		Uninit();
		return false;
	}

	return true;
}

// 戦闘、Player、敵、弾、ゲート、Scene遷移を更新する。
void GameScene::Update( float deltaTime )
{
	// F2キーでShopSceneへの遷移を予約する。
	if ( m_InputSystem.IsKeyTriggered( RETURN_TO_SHOP_KEY ) )
	{
		m_AudioSystem.PlayWarpSe();
		m_InputSystem.SetMouseCaptureEnabled( false );
		m_SceneManager.RequestSceneChange<ShopScene>();
		return;
	}

	// Player死亡時は死亡回数と通貨ペナルティを適用してShopSceneへ戻る。
	if ( m_PlayerHealth.IsDead() )
	{
		GameProgress& progress = m_SceneManager.GetGameProgress();

		progress.AddDeath();
		progress.ApplyDeathCurrencyPenalty();

		m_AudioSystem.PlayWarpSe();
		m_InputSystem.SetMouseCaptureEnabled( false );
		m_SceneManager.RequestSceneChange<ShopScene>();
		return;
	}

	// SceneManagerが所有するゲーム進捗と現在Stage設定を取得する。
	GameProgress& progress = m_SceneManager.GetGameProgress();
	const StageData& stageData = progress.GetCurrentStageData();
	const PlayerStats& playerStats = progress.GetPlayerStats();

	// ゲーム進行時間と攻撃・Flash用Timerを更新する。
	progress.Update( deltaTime );
	m_SpecialAttackCooldownTimer = std::max(
		0.0f,
		m_SpecialAttackCooldownTimer - deltaTime );
	m_MuzzleFlashTimer = std::max(
		0.0f,
		m_MuzzleFlashTimer - deltaTime );

	// 低HP状態に入ったときだけ警告SEを再生する。
	const float playerMaxHp = m_PlayerHealth.GetMaxHp();
	const float playerCurrentHp = m_PlayerHealth.GetCurrentHp();
	const bool isLowHealth = playerMaxHp > 0.0f &&
		playerCurrentHp <= playerMaxHp * LOW_HP_RATIO_THRESHOLD;

	if ( isLowHealth && !m_HasPlayedLowHpSe )
	{
		m_AudioSystem.PlayLowHpSe();
		m_HasPlayedLowHpSe = true;
	}
	else if ( !isLowHealth )
	{
		m_HasPlayedLowHpSe = false;
	}

	// Playerと敵の水平距離を計算して各攻撃判定に使用する。
	const DirectX::XMFLOAT3 playerPosition = m_DebugPlayer.GetPosition();
	const float deltaX = playerPosition.x - ENEMY_BASE_X;
	const float deltaZ = playerPosition.z - ENEMY_BASE_Z;
	const float playerToEnemyDistanceSquared = deltaX * deltaX + deltaZ * deltaZ;

	// 敵が生存中なら通常攻撃と特殊攻撃のTimer・命中判定を更新する。
	if ( !m_EnemyHealth.IsDead() && !m_IsResultSceneRequested )
	{
		m_EnemyNormalAttackTimer += deltaTime;

		if ( m_EnemyNormalAttackTimer >= ENEMY_NORMAL_ATTACK_INTERVAL )
		{
			m_EnemyNormalAttackTimer = {};

			m_IsLastNormalAttackHit =
				playerToEnemyDistanceSquared <= ENEMY_NORMAL_ATTACK_RANGE_SQUARED;

			if ( m_IsLastNormalAttackHit )
			{
				m_PlayerHealth.TakeDamage( stageData.enemyDamage );
				m_AudioSystem.PlayDamageSe();
			}
		}

		m_EnemySpecialAttackTimer += deltaTime;

		if ( m_EnemySpecialAttackTimer >= stageData.specialAttackInterval )
		{
			m_EnemySpecialAttackTimer = {};

			const float specialAttackRangeSquared =
				stageData.specialAttackHitboxRadius * stageData.specialAttackHitboxRadius;

			m_IsLastSpecialAttackHit =
				playerToEnemyDistanceSquared <= specialAttackRangeSquared;

			if ( m_IsLastSpecialAttackHit )
			{
				m_PlayerHealth.TakeDamage(
					stageData.enemyDamage * ENEMY_SPECIAL_ATTACK_DAMAGE_MULTIPLIER );
				m_AudioSystem.PlayDamageSe();
			}
		}
	}

	// 通常射撃時は弾を生成し、Camera中央のRayで敵への命中を判定する。
	if ( m_InputSystem.IsKeyTriggered( SHOOT_ENEMY_KEY ) &&
		!m_EnemyHealth.IsDead() &&
		!m_IsResultSceneRequested )
	{
		m_AudioSystem.PlayGunSe();
		m_MuzzleFlashTimer = MUZZLE_FLASH_DURATION;

		// Camera空間の銃口座標をWorld座標へ変換して弾の生成位置にする。
		const DirectX::XMMATRIX inverseViewMatrix = DirectX::XMMatrixInverse(
			nullptr,
			m_FpsCamera.GetViewMatrix() );
		const DirectX::XMVECTOR bulletSpawnCameraSpace = DirectX::XMVectorSet(
			BULLET_SPAWN_OFFSET_X,
			BULLET_SPAWN_OFFSET_Y,
			BULLET_SPAWN_OFFSET_Z,
			1.0f );
		const DirectX::XMVECTOR bulletSpawnWorldSpace = DirectX::XMVector3TransformCoord(
			bulletSpawnCameraSpace,
			inverseViewMatrix );

		DirectX::XMFLOAT3 bulletSpawnPosition{};
		DirectX::XMStoreFloat3( &bulletSpawnPosition, bulletSpawnWorldSpace );

		const DirectX::XMFLOAT3 bulletDirection = m_FpsCamera.GetForward();

		// 未使用の弾スロットを探して弾情報を設定する。
		for ( Bullet& bullet : m_Bullets )
		{
			if ( bullet.isActive ) continue;

			bullet.position = bulletSpawnPosition;
			bullet.direction = bulletDirection;
			bullet.remainingLifetime = BULLET_LIFETIME;
			bullet.isActive = true;
			break;
		}

		// Camera中央Rayと敵のSphereによる命中判定を行う。
		const DirectX::XMFLOAT3 rayOrigin = m_FpsCamera.GetPosition();
		const DirectX::XMVECTOR rayOriginVector = DirectX::XMLoadFloat3( &rayOrigin );
		const DirectX::XMVECTOR rayDirection = DirectX::XMVector3Normalize(
			DirectX::XMLoadFloat3( &bulletDirection ) );
		const float enemyFloatOffset = std::sinf(
			m_EnemyAnimationTime * ENEMY_FLOAT_SPEED ) * ENEMY_FLOAT_HEIGHT;
		const DirectX::XMFLOAT3 enemyHitPosition
		{
			ENEMY_BASE_X,
			ENEMY_BASE_Y + enemyFloatOffset + ENEMY_HIT_CENTER_Y_OFFSET,
			ENEMY_BASE_Z
		};

		const DirectX::BoundingSphere enemyHitSphere(
			enemyHitPosition,
			ENEMY_HIT_SPHERE_RADIUS );
		float hitDistance{};

		m_IsLastShotHit = enemyHitSphere.Intersects(
			rayOriginVector,
			rayDirection,
			hitDistance ) && hitDistance <= SHOOT_MAX_DISTANCE;

		if ( m_IsLastShotHit )
		{
			const float actualDamage = m_EnemyHealth.TakeDamage( playerStats.gunDamage );
			progress.AddDamageReward( actualDamage );
		}
	}

	// 特殊攻撃が解放済みかつ再使用可能なら、範囲内の敵へ大きなダメージを与える。
	if ( m_InputSystem.IsKeyTriggered( SPECIAL_ATTACK_KEY ) &&
		!m_EnemyHealth.IsDead() &&
		!m_IsResultSceneRequested &&
		playerStats.isSpecialAttackUnlocked &&
		m_SpecialAttackCooldownTimer <= 0.0f )
	{
		m_AudioSystem.PlaySpecialSe();

		m_IsLastPlayerSpecialAttackHit =
			playerToEnemyDistanceSquared <= PLAYER_SPECIAL_ATTACK_RANGE_SQUARED;
		m_SpecialAttackCooldownTimer = playerStats.specialAttackCooldown;

		if ( m_IsLastPlayerSpecialAttackHit )
		{
			const float actualDamage = m_EnemyHealth.TakeDamage(
				playerStats.gunDamage * PLAYER_SPECIAL_ATTACK_DAMAGE_MULTIPLIER );
			progress.AddDamageReward( actualDamage );
		}
	}

	// 発射中の弾を移動し、寿命が切れた弾を非アクティブにする。
	for ( Bullet& bullet : m_Bullets )
	{
		if ( !bullet.isActive ) continue;

		bullet.position.x += bullet.direction.x * BULLET_SPEED * deltaTime;
		bullet.position.y += bullet.direction.y * BULLET_SPEED * deltaTime;
		bullet.position.z += bullet.direction.z * BULLET_SPEED * deltaTime;
		bullet.remainingLifetime -= deltaTime;

		if ( bullet.remainingLifetime <= 0.0f ) bullet.isActive = false;
	}

	// 敵が生存中だけ敵アニメーションを進め、Gateアニメーションは常に進める。
	if ( !m_EnemyHealth.IsDead() ) m_EnemyAnimationTime += deltaTime;

	m_GateAnimationTime += deltaTime;

	// 敵撃破時に初回撃破報酬・撃破数・最終Stageクリアを1度だけ処理する。
	if ( m_EnemyHealth.IsDead() && !m_IsResultSceneRequested )
	{
		if ( !m_HasPlayedEnemyDefeatSe )
		{
			m_AudioSystem.PlayEnemyDefeatSe();
			m_HasPlayedEnemyDefeatSe = true;
		}

		progress.AddEnemyDefeat();

		const bool isFirstClear = progress.MarkCurrentStageCleared();

		if ( isFirstClear )
		{
			progress.AddStageClearReward();

			if ( progress.GetCurrentStage() == StageConstants::MAX_STAGE_COUNT )
			{
				m_IsGameClear = true;
				m_IsResultSceneRequested = true;
			}
		}
	}

	// F1キーでFPSマウスキャプチャの有効・無効を切り替える。
	if ( m_InputSystem.IsKeyTriggered( TOGGLE_MOUSE_CAPTURE_KEY ) )
	{
		m_InputSystem.SetMouseCaptureEnabled( !m_InputSystem.IsMouseCaptureEnabled() );
	}

	// 現在Stageに応じた前後Gateの使用可否を判定する。
	const int currentStage = progress.GetCurrentStage();
	const bool isPreviousGateAvailable =
		currentStage > StageConstants::FIRST_STAGE_NUMBER;
	const bool isNextGateAvailable =
		currentStage < StageConstants::MAX_STAGE_COUNT &&
		progress.IsStageCleared( currentStage );

	// Eキーで近くにあるGateを使用し、必要ならStage番号を更新してScene遷移する。
	if ( m_InputSystem.IsKeyTriggered( USE_GATE_KEY ) )
	{
		if ( isPreviousGateAvailable &&
			IsPlayerNearGate( PREVIOUS_GATE_POSITION_X, PREVIOUS_GATE_POSITION_Z ) &&
			progress.TrySetCurrentStage( currentStage - PREVIOUS_STAGE_OFFSET ) )
		{
			m_AudioSystem.PlayWarpSe();
			m_InputSystem.SetMouseCaptureEnabled( false );
			m_SceneManager.RequestSceneChange<GameScene>();
			return;
		}

		if ( isNextGateAvailable &&
			IsPlayerNearGate( NEXT_GATE_POSITION_X, NEXT_GATE_POSITION_Z ) &&
			progress.TrySetCurrentStage( currentStage + NEXT_STAGE_OFFSET ) )
		{
			m_AudioSystem.PlayWarpSe();
			m_InputSystem.SetMouseCaptureEnabled( false );
			m_SceneManager.RequestSceneChange<GameScene>();
			return;
		}

		if ( IsPlayerNearGate( SHOP_GATE_POSITION_X, SHOP_GATE_POSITION_Z ) )
		{
			m_AudioSystem.PlayWarpSe();
			m_InputSystem.SetMouseCaptureEnabled( false );
			m_SceneManager.RequestSceneChange<ShopScene>();
			return;
		}
	}

	// 最後にCamera回転、Player移動、Camera追従位置を更新する。
	m_FpsCamera.Update( m_InputSystem );
	m_DebugPlayer.Update( deltaTime, m_InputSystem, m_FpsCamera );
	m_FpsCamera.SetPosition( m_DebugPlayer.GetPosition() );
}

// 3D Stage、敵、弾、ゲート、HUD、Debug UIを描画する。
void GameScene::Draw()
{
	// SceneManagerが所有するゲーム進捗、Player能力、現在Stage設定を取得する。
	GameProgress& progress = m_SceneManager.GetGameProgress();
	const PlayerStats& playerStats = progress.GetPlayerStats();
	const StageData& stageData = progress.GetCurrentStageData();

	// 現在Stageに応じたGateの表示可否を判定する。
	const int currentStage = progress.GetCurrentStage();
	const bool isPreviousGateAvailable =
		currentStage > StageConstants::FIRST_STAGE_NUMBER;
	const bool isNextGateAvailable =
		currentStage < StageConstants::MAX_STAGE_COUNT &&
		progress.IsStageCleared( currentStage );

	// 3D描画に使用するProjection、View、Camera座標を取得する。
	const DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
		DirectX::XMConvertToRadians( PROJECTION_FOV_DEGREES ),
		PROJECTION_ASPECT_RATIO,
		PROJECTION_NEAR_Z,
		PROJECTION_FAR_Z );
	const DirectX::XMMATRIX viewMatrix = m_FpsCamera.GetViewMatrix();
	const DirectX::XMFLOAT3 cameraPosition = m_FpsCamera.GetPosition();

	// 敵の浮遊量と描画時のY座標を計算する。
	const float enemyFloatOffset = std::sinf(
		m_EnemyAnimationTime * ENEMY_FLOAT_SPEED ) * ENEMY_FLOAT_HEIGHT;
	const float enemyY = ENEMY_BASE_Y + enemyFloatOffset;

	// Camera位置に追従するSky DomeのWorld行列を作成する。
	const DirectX::XMMATRIX skyDomeWorldMatrix =
		DirectX::XMMatrixScaling( SKY_DOME_SCALE, SKY_DOME_SCALE, SKY_DOME_SCALE ) *
		DirectX::XMMatrixTranslation(
			cameraPosition.x,
			cameraPosition.y + SKY_DOME_Y_OFFSET,
			cameraPosition.z );

	// Sky DomeはDepth Testを無効にして最初に描画する。
	m_GraphicsSystem.SetRenderPass( e_RenderPass::e_SKY );
	m_SkyDomeRenderer.Draw(
		m_GraphicsSystem,
		skyDomeWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f } );
	m_GraphicsSystem.SetRenderPass( e_RenderPass::e_OPAQUE );

	// 敵が生存中なら単色の敵OBJを描画する。
	if ( !m_EnemyHealth.IsDead() )
	{
		const DirectX::XMMATRIX enemyWorldMatrix =
			DirectX::XMMatrixScaling(
				ENEMY_MODEL_SCALE,
				ENEMY_MODEL_SCALE,
				ENEMY_MODEL_SCALE ) *
			DirectX::XMMatrixRotationY( m_EnemyAnimationTime * ENEMY_ROTATION_SPEED ) *
			DirectX::XMMatrixTranslation( ENEMY_BASE_X, enemyY, ENEMY_BASE_Z );

		m_EnemyModelRenderer.Draw(
			m_GraphicsSystem,
			enemyWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4{ 0.18f, 0.95f, 0.28f, 1.0f } );
	}

	// 床のWorld行列を作成してFloor Textureで描画する。
	const DirectX::XMMATRIX floorWorldMatrix =
		DirectX::XMMatrixScaling( 20.0f, 0.2f, 20.0f ) *
		DirectX::XMMatrixTranslation( 0.0f, -0.6f, 8.0f );

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		floorWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f },
		DirectX::XMFLOAT2{ 10.0f, 10.0f },
		BasicMeshRenderer::TextureType::Floor );

	// 4面の壁の色とWorld行列を作成する。
	const DirectX::XMFLOAT4 wallColor{ 1.0f, 1.0f, 1.0f, 1.0f };

	const DirectX::XMMATRIX leftWallWorldMatrix =
		DirectX::XMMatrixScaling( 0.2f, 3.2f, 20.0f ) *
		DirectX::XMMatrixTranslation( -10.0f, 1.1f, 8.0f );
	const DirectX::XMMATRIX rightWallWorldMatrix =
		DirectX::XMMatrixScaling( 0.2f, 3.2f, 20.0f ) *
		DirectX::XMMatrixTranslation( 10.0f, 1.1f, 8.0f );
	const DirectX::XMMATRIX nearWallWorldMatrix =
		DirectX::XMMatrixScaling( 20.0f, 3.2f, 0.2f ) *
		DirectX::XMMatrixTranslation( 0.0f, 1.1f, -2.0f );
	const DirectX::XMMATRIX farWallWorldMatrix =
		DirectX::XMMatrixScaling( 20.0f, 3.2f, 0.2f ) *
		DirectX::XMMatrixTranslation( 0.0f, 1.1f, 18.0f );

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

	// Gateの回転角と各GateのWorld行列を作成する。
	const float gateRotationAngle = m_GateAnimationTime * GATE_ROTATION_SPEED;

	const DirectX::XMMATRIX previousGateWorldMatrix =
		DirectX::XMMatrixScaling( GATE_WIDTH, GATE_HEIGHT, GATE_DEPTH ) *
		DirectX::XMMatrixRotationY( gateRotationAngle ) *
		DirectX::XMMatrixTranslation(
			PREVIOUS_GATE_POSITION_X,
			PREVIOUS_GATE_POSITION_Y,
			PREVIOUS_GATE_POSITION_Z );
	const DirectX::XMMATRIX nextGateWorldMatrix =
		DirectX::XMMatrixScaling( GATE_WIDTH, GATE_HEIGHT, GATE_DEPTH ) *
		DirectX::XMMatrixRotationY( gateRotationAngle ) *
		DirectX::XMMatrixTranslation(
			NEXT_GATE_POSITION_X,
			NEXT_GATE_POSITION_Y,
			NEXT_GATE_POSITION_Z );
	const DirectX::XMMATRIX shopGateWorldMatrix =
		DirectX::XMMatrixScaling( GATE_WIDTH, GATE_HEIGHT, GATE_DEPTH ) *
		DirectX::XMMatrixRotationY( gateRotationAngle ) *
		DirectX::XMMatrixTranslation(
			SHOP_GATE_POSITION_X,
			SHOP_GATE_POSITION_Y,
			SHOP_GATE_POSITION_Z );

	// 使用可能な前後Gateと常時使用可能なShop Gateを描画する。
	if ( isPreviousGateAvailable )
	{
		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			previousGateWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4{ 0.85f, 0.30f, 1.0f, 1.0f },
			DirectX::XMFLOAT2{ 1.0f, 1.0f },
			BasicMeshRenderer::TextureType::Color );
	}

	if ( isNextGateAvailable )
	{
		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			nextGateWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4{ 0.10f, 0.85f, 1.0f, 1.0f },
			DirectX::XMFLOAT2{ 1.0f, 1.0f },
			BasicMeshRenderer::TextureType::Color );
	}

	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		shopGateWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4{ 1.0f, 0.75f, 0.10f, 1.0f },
		DirectX::XMFLOAT2{ 1.0f, 1.0f },
		BasicMeshRenderer::TextureType::Color );

	// View行列の逆行列を使い、銃をCamera空間へ固定する。
	const DirectX::XMMATRIX inverseViewMatrix = DirectX::XMMatrixInverse( nullptr, viewMatrix );

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
	const DirectX::XMMATRIX gunBarrelWorldMatrix =
		DirectX::XMMatrixScaling(
			GUN_BARREL_SCALE_X,
			GUN_BARREL_SCALE_Y,
			GUN_BARREL_SCALE_Z ) *
		DirectX::XMMatrixTranslation(
			GUN_POSITION_X,
			GUN_POSITION_Y + GUN_BARREL_OFFSET_Y,
			GUN_POSITION_Z + GUN_BARREL_OFFSET_Z ) *
		inverseViewMatrix;

	// 銃本体と銃身を単色Cubeで描画する。
	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		gunBodyWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4{ 0.12f, 0.12f, 0.14f, 1.0f },
		DirectX::XMFLOAT2{ 1.0f, 1.0f },
		BasicMeshRenderer::TextureType::Color );
	m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		gunBarrelWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4{ 0.30f, 0.32f, 0.36f, 1.0f },
		DirectX::XMFLOAT2{ 1.0f, 1.0f },
		BasicMeshRenderer::TextureType::Color );

	// 銃口Flashの表示時間中だけ、半透明の単色Cubeを描画する。
	if ( m_MuzzleFlashTimer > 0.0f )
	{
		const float muzzleFlashAlpha = m_MuzzleFlashTimer / MUZZLE_FLASH_DURATION;

		const DirectX::XMMATRIX muzzleFlashWorldMatrix =
			DirectX::XMMatrixScaling(
				MUZZLE_FLASH_SCALE,
				MUZZLE_FLASH_SCALE,
				MUZZLE_FLASH_SCALE ) *
			DirectX::XMMatrixTranslation(
				GUN_POSITION_X,
				GUN_POSITION_Y + GUN_BARREL_OFFSET_Y,
				GUN_POSITION_Z + MUZZLE_FLASH_OFFSET_Z ) *
			inverseViewMatrix;

		m_GraphicsSystem.SetRenderPass( e_RenderPass::e_TRANSPARENT );

		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			muzzleFlashWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4{ 1.0f, 0.65f, 0.05f, muzzleFlashAlpha },
			DirectX::XMFLOAT2{ 1.0f, 1.0f },
			BasicMeshRenderer::TextureType::Color );
	}

	// 発射中の弾を半透明で描画する。
	m_GraphicsSystem.SetRenderPass( e_RenderPass::e_TRANSPARENT );

	for ( const Bullet& bullet : m_Bullets )
	{
		if ( !bullet.isActive ) continue;

		const float bulletAlpha = std::clamp(
			bullet.remainingLifetime / BULLET_LIFETIME,
			0.0f,
			1.0f );
		const DirectX::XMMATRIX bulletWorldMatrix =
			DirectX::XMMatrixScaling( BULLET_SCALE, BULLET_SCALE, BULLET_SCALE ) *
			DirectX::XMMatrixTranslation(
				bullet.position.x,
				bullet.position.y,
				bullet.position.z );

		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			bulletWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4{ 1.0f, 0.85f, 0.10f, bulletAlpha },
			DirectX::XMFLOAT2{ 1.0f, 1.0f },
			BasicMeshRenderer::TextureType::Color );
	}


	// 敵が生存中なら、Camera方向を向くHPバーを敵の頭上へ描画する。
	const float enemyHpRatio = std::clamp(
		m_EnemyHealth.GetCurrentHp() / m_EnemyHealth.GetMaxHp(),
		0.0f,
		1.0f );
	const bool isHpBarVisible =
		!m_EnemyHealth.IsDead() && enemyHpRatio > HP_BAR_VISIBLE_RATIO_THRESHOLD;
	const DirectX::XMFLOAT3 hpBarPosition
	{
		ENEMY_BASE_X,
		enemyY + HP_BAR_Y_OFFSET,
		ENEMY_BASE_Z
	};

	DirectX::XMVECTOR toCameraVector = DirectX::XMVectorSubtract(
		DirectX::XMLoadFloat3( &cameraPosition ),
		DirectX::XMLoadFloat3( &hpBarPosition ) );
	toCameraVector = DirectX::XMVectorSetY( toCameraVector, 0.0f );

	const float toCameraLengthSquared = DirectX::XMVectorGetX( DirectX::XMVector3LengthSq( toCameraVector ) );

	m_GraphicsSystem.SetRenderPass( e_RenderPass::e_TRANSPARENT );

	if ( isHpBarVisible && toCameraLengthSquared > BILLBOARD_MIN_CAMERA_DISTANCE_SQUARED )
	{
		// Billboardの右・前方向を作り、HPバー用の回転行列を構成する。
		toCameraVector = DirectX::XMVector3Normalize( toCameraVector );

		const DirectX::XMVECTOR worldUpVector = DirectX::XMVectorSet(
			0.0f,
			1.0f,
			0.0f,
			0.0f );
		const DirectX::XMVECTOR billboardRightVector = DirectX::XMVector3Normalize(
			DirectX::XMVector3Cross( worldUpVector, toCameraVector ) );
		const DirectX::XMVECTOR billboardForwardVector = DirectX::XMVector3Normalize(
			DirectX::XMVector3Cross( billboardRightVector, worldUpVector ) );

		DirectX::XMMATRIX billboardRotationMatrix = DirectX::XMMatrixIdentity();
		billboardRotationMatrix.r[ 0 ] = DirectX::XMVectorSetW( billboardRightVector, 0.0f );
		billboardRotationMatrix.r[ 1 ] = DirectX::XMVectorSetW( worldUpVector, 0.0f );
		billboardRotationMatrix.r[ 2 ] = DirectX::XMVectorSetW( billboardForwardVector, 0.0f );

		// 現在HPの割合に合わせてHPバー前景の幅と左寄せ補正を計算する。
		const float hpBarForegroundWidth = HP_BAR_WIDTH * enemyHpRatio;
		const float hpBarForegroundXOffset =
			-( HP_BAR_WIDTH - hpBarForegroundWidth ) * 0.5f;

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

		const DirectX::XMVECTOR foregroundPositionVector = DirectX::XMVectorAdd(
			DirectX::XMVectorSet(
			hpBarPosition.x + hpBarForegroundXOffset,
			hpBarPosition.y,
			hpBarPosition.z,
			1.0f ),
			DirectX::XMVectorScale(
			toCameraVector,
			HP_BAR_FOREGROUND_CAMERA_OFFSET ) );

		DirectX::XMFLOAT3 foregroundPosition{};
		DirectX::XMStoreFloat3( &foregroundPosition, foregroundPositionVector );

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

		// HPバーの背景と現在HPを単色Cubeとして描画する。
		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			hpBarBackgroundWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.70f },
			DirectX::XMFLOAT2{ 1.0f, 1.0f },
			BasicMeshRenderer::TextureType::Color );
		m_BasicMeshRenderer.DrawCube(
			m_GraphicsSystem,
			hpBarForegroundWorldMatrix,
			viewMatrix,
			projectionMatrix,
			DirectX::XMFLOAT4{ 0.10f, 1.0f, 0.20f, 0.95f },
			DirectX::XMFLOAT2{ 1.0f, 1.0f },
			BasicMeshRenderer::TextureType::Color );
	}

	// HUDを画面固定で描画するためDepth Testを無効化する。
	m_GraphicsSystem.SetRenderPass( e_RenderPass::e_SCREEN_UI );
	m_HudRenderer.DrawCrosshair( m_GraphicsSystem );
	m_HudRenderer.DrawPlayerHealthBar(
		m_GraphicsSystem,
		m_PlayerHealth.GetCurrentHp(),
		m_PlayerHealth.GetMaxHp() );
	m_HudRenderer.DrawLowHealthWarning(
		m_GraphicsSystem,
		m_PlayerHealth.GetCurrentHp(),
		m_PlayerHealth.GetMaxHp() );

	// Stage、通貨、HP、特殊攻撃、Gate案内、Tutorialを文字で描画する。
	m_HudTextRenderer.Begin();

	wchar_t stageText[ 64 ]{};
	wchar_t currencyText[ 64 ]{};
	wchar_t healthText[ 64 ]{};

	swprintf_s(
		stageText,
		L"ステージ %d / %d",
		currentStage,
		StageConstants::MAX_STAGE_COUNT );
	swprintf_s( currencyText, L"所持金: %d G", progress.GetCurrency() );
	swprintf_s(
		healthText,
		L"HP: %.0f / %.0f",
		m_PlayerHealth.GetCurrentHp(),
		m_PlayerHealth.GetMaxHp() );

	m_HudTextRenderer.DrawText(
		stageText,
		DirectX::XMFLOAT2{ HUD_STAGE_TEXT_X, HUD_STAGE_TEXT_Y },
		DirectX::Colors::White,
		0.90f );
	m_HudTextRenderer.DrawText(
		currencyText,
		DirectX::XMFLOAT2{ HUD_CURRENCY_TEXT_X, HUD_CURRENCY_TEXT_Y },
		DirectX::Colors::Gold,
		HUD_TEXT_SCALE );
	m_HudTextRenderer.DrawText(
		healthText,
		DirectX::XMFLOAT2{ HUD_HP_TEXT_X, HUD_HP_TEXT_Y },
		DirectX::Colors::White,
		HUD_TEXT_SCALE );

	// 特殊攻撃の解放状態とCooldown状態を文字と色で表示する。
	const bool isSpecialAttackUnlocked = playerStats.isSpecialAttackUnlocked;
	const bool isSpecialAttackReady = m_SpecialAttackCooldownTimer <= 0.0f;

	const wchar_t* specialAttackText = L"";
	DirectX::XMVECTORF32 specialAttackColor = DirectX::Colors::Yellow;

	if ( !isSpecialAttackUnlocked )
	{
		specialAttackText = L"Q: 未解放";
		specialAttackColor = DirectX::Colors::Yellow;
	}
	else if ( isSpecialAttackReady )
	{
		specialAttackText = L"Q: 使用可能";
		specialAttackColor = DirectX::Colors::Lime;
	}
	else
	{
		specialAttackText = L"Q: 待機中";
		specialAttackColor = DirectX::Colors::Orange;
	}

	m_HudTextRenderer.DrawText(
		specialAttackText,
		DirectX::XMFLOAT2{
			HUD_SPECIAL_ATTACK_TEXT_X,
			HUD_SPECIAL_ATTACK_TEXT_Y },
			specialAttackColor,
			HUD_TEXT_SCALE );

	if ( isSpecialAttackUnlocked && !isSpecialAttackReady )
	{
		wchar_t cooldownText[ 64 ]{};

		swprintf_s(
			cooldownText,
			L"残り %.1f 秒",
			m_SpecialAttackCooldownTimer );

		m_HudTextRenderer.DrawText(
			cooldownText,
			DirectX::XMFLOAT2{
				HUD_COOLDOWN_TEXT_X,
				HUD_COOLDOWN_TEXT_Y },
				DirectX::Colors::White,
				HUD_COOLDOWN_TEXT_SCALE );
	}

	// PlayerがGateの操作範囲内にいる場合、対象に応じた操作案内を表示する。
	if ( isNextGateAvailable &&
		IsPlayerNearGate( NEXT_GATE_POSITION_X, NEXT_GATE_POSITION_Z ) )
	{
		m_HudTextRenderer.DrawText(
			L"E: 次のステージへ",
			DirectX::XMFLOAT2{ HUD_GATE_HINT_X, HUD_GATE_HINT_Y },
			DirectX::Colors::Cyan,
			HUD_MESSAGE_SCALE );
	}
	else if ( isPreviousGateAvailable &&
		IsPlayerNearGate( PREVIOUS_GATE_POSITION_X, PREVIOUS_GATE_POSITION_Z ) )
	{
		m_HudTextRenderer.DrawText(
			L"E: 前のステージへ",
			DirectX::XMFLOAT2{ HUD_GATE_HINT_X, HUD_GATE_HINT_Y },
			DirectX::Colors::Violet,
			HUD_MESSAGE_SCALE );
	}
	else if ( IsPlayerNearGate( SHOP_GATE_POSITION_X, SHOP_GATE_POSITION_Z ) )
	{
		m_HudTextRenderer.DrawText(
			L"E: ショップへ",
			DirectX::XMFLOAT2{ HUD_GATE_HINT_X, HUD_GATE_HINT_Y },
			DirectX::Colors::Gold,
			HUD_MESSAGE_SCALE );
	}

	// Stage 1の戦闘中だけ基本操作のTutorialを表示する。
	if ( currentStage == TUTORIAL_STAGE_NUMBER && !m_EnemyHealth.IsDead() )
	{
		m_HudTextRenderer.DrawText(
			L"WASD: 移動",
			DirectX::XMFLOAT2{ HUD_TUTORIAL_X, HUD_TUTORIAL_Y },
			DirectX::Colors::White,
			HUD_TUTORIAL_SCALE );
		m_HudTextRenderer.DrawText(
			L"マウス: 視点移動",
			DirectX::XMFLOAT2{
				HUD_TUTORIAL_X,
				HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT },
				DirectX::Colors::White,
				HUD_TUTORIAL_SCALE );
		m_HudTextRenderer.DrawText(
			L"左クリック: 射撃",
			DirectX::XMFLOAT2{
				HUD_TUTORIAL_X,
				HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT * 2.0f },
				DirectX::Colors::White,
				HUD_TUTORIAL_SCALE );
		m_HudTextRenderer.DrawText(
			L"Q: 範囲攻撃",
			DirectX::XMFLOAT2{
				HUD_TUTORIAL_X,
				HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT * 3.0f },
				DirectX::Colors::White,
				HUD_TUTORIAL_SCALE );
		m_HudTextRenderer.DrawText(
			L"ゲートに近づいてEキー",
			DirectX::XMFLOAT2{
				HUD_TUTORIAL_X,
				HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT * 4.0f },
				DirectX::Colors::White,
				HUD_TUTORIAL_SCALE );
		m_HudTextRenderer.DrawText(
			L"死亡時: 所持金の25%を失う",
			DirectX::XMFLOAT2{
				HUD_TUTORIAL_X,
				HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT * 5.0f },
				DirectX::Colors::Orange,
				HUD_TUTORIAL_SCALE );

		if ( !isSpecialAttackUnlocked )
		{
			m_HudTextRenderer.DrawText(
				L"Qは未解放です - ショップで解放できます",
				DirectX::XMFLOAT2{
					HUD_TUTORIAL_X,
					HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT * 6.0f },
					DirectX::Colors::Yellow,
					HUD_TUTORIAL_SCALE );
		}
	}

	// 最終Stage以外で敵を倒した場合、次Stage Gateへ向かう案内を表示する。
	if ( m_EnemyHealth.IsDead() && !m_IsGameClear )
	{
		m_HudTextRenderer.DrawText(
			L"ステージクリア！ 青いゲートへ",
			DirectX::XMFLOAT2{ 420.0f, 90.0f },
			DirectX::Colors::Lime,
			0.90f );
	}

	m_HudTextRenderer.End();

	// 次の3D描画へ影響を残さないようDepth TestとAlpha Blendを戻す。
	m_GraphicsSystem.SetRenderPass( e_RenderPass::e_OPAQUE );

	// ImGuiでStage、敵、通貨、Gate操作を確認できるDebug UIを描画する。
	ImGui::Begin( "Game Debug" );
	ImGui::Text( "Stage %d / %d", stageData.stageNumber, StageConstants::MAX_STAGE_COUNT );
	ImGui::Text( "Enemy: %s", stageData.enemyName );
	ImGui::Text( "Clear Reward: %d", stageData.clearReward );

	if ( ImGui::Button( "Test: Take 10 Damage" ) )
	{
		m_PlayerHealth.TakeDamage( TEST_PLAYER_DAMAGE );
	}

	ImGui::Separator();
	ImGui::Text(
		"Enemy HP: %.0f / %.0f",
		m_EnemyHealth.GetCurrentHp(),
		m_EnemyHealth.GetMaxHp() );
	ImGui::Text( "Last Shot: %s", m_IsLastShotHit ? "HIT" : "MISS" );
	ImGui::Text( "Currency: %d", progress.GetCurrency() );
	ImGui::Text( "Deaths: %d", progress.GetTotalDeaths() );
	ImGui::Text( "Play Time: %.1f sec", progress.GetTotalPlayTime() );

	ImGui::Separator();

	// ImGuiボタンで選択したGate遷移先を一時的に保持する。
	enum class GateDestination
	{
		e_NONE,
		e_PREVIOUS_STAGE,
		e_NEXT_STAGE,
		e_SHOP
	};

	GateDestination gateDestination{};

	if ( isPreviousGateAvailable &&
		ImGui::Button(
		 PREVIOUS_STAGE_GATE_LABEL,
		 ImVec2( GATE_BUTTON_WIDTH, GATE_BUTTON_HEIGHT ) ) )
	{
		gateDestination = GateDestination::e_PREVIOUS_STAGE;
	}

	if ( currentStage < StageConstants::MAX_STAGE_COUNT )
	{
		if ( !isNextGateAvailable ) ImGui::BeginDisabled();

		if ( ImGui::Button(
			NEXT_STAGE_GATE_LABEL,
			ImVec2( GATE_BUTTON_WIDTH, GATE_BUTTON_HEIGHT ) ) )
		{
			gateDestination = GateDestination::e_NEXT_STAGE;
		}

		if ( !isNextGateAvailable ) ImGui::EndDisabled();
	}

	if ( ImGui::Button(
		SHOP_GATE_LABEL,
		ImVec2( GATE_BUTTON_WIDTH, GATE_BUTTON_HEIGHT ) ) )
	{
		gateDestination = GateDestination::e_SHOP;
	}

	ImGui::End();

	// 最終Stageクリア後はResultSceneへの遷移を予約する。
	if ( m_IsResultSceneRequested )
	{
		m_AudioSystem.PlayWarpSe();
		m_InputSystem.SetMouseCaptureEnabled( false );
		m_SceneManager.RequestSceneChange<ResultScene>();
		return;
	}

	// ImGui Debug UIで選択されたGateに応じてScene遷移する。
	if ( gateDestination == GateDestination::e_PREVIOUS_STAGE &&
		progress.TrySetCurrentStage( currentStage - PREVIOUS_STAGE_OFFSET ) )
	{
		m_AudioSystem.PlayWarpSe();
		m_InputSystem.SetMouseCaptureEnabled( false );
		m_SceneManager.RequestSceneChange<GameScene>();
	}
	else if ( gateDestination == GateDestination::e_NEXT_STAGE &&
		progress.TrySetCurrentStage( currentStage + NEXT_STAGE_OFFSET ) )
	{
		m_AudioSystem.PlayWarpSe();
		m_InputSystem.SetMouseCaptureEnabled( false );
		m_SceneManager.RequestSceneChange<GameScene>();
	}
	else if ( gateDestination == GateDestination::e_SHOP )
	{
		m_AudioSystem.PlayWarpSe();
		m_InputSystem.SetMouseCaptureEnabled( false );
		m_SceneManager.RequestSceneChange<ShopScene>();
	}
}

// Playerが指定したGateの操作範囲内にいるかを返す。
bool GameScene::IsPlayerNearGate( float gatePositionX, float gatePositionZ ) const
{
	// Player位置とGate位置の水平距離の二乗を計算する。
	const DirectX::XMFLOAT3 playerPosition = m_DebugPlayer.GetPosition();
	const float deltaX = playerPosition.x - gatePositionX;
	const float deltaZ = playerPosition.z - gatePositionZ;
	const float distanceSquared = deltaX * deltaX + deltaZ * deltaZ;

	return distanceSquared <= GATE_INTERACTION_RADIUS_SQUARED;
}

// GameSceneで使用した描画リソースを解放する。
void GameScene::Uninit()
{
	// HUD描画リソースを解放する。
	m_HudTextRenderer.Uninit();
	m_HudRenderer.Uninit();

	// OBJモデル描画リソースを解放する。
	m_EnemyModelRenderer.Uninit();
	m_SkyDomeRenderer.Uninit();

	// Cube描画リソースを解放する。
	m_BasicMeshRenderer.Uninit();
}