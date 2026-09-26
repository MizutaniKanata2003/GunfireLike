#include "../H/GameScene.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <system_error>

//========= DirectX インクルード=========
#include <DirectXMath.h>
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

	// 敵への通常射撃Ray判定に使用するSphere設定。
	constexpr float ENEMY_HIT_CENTER_Y_OFFSET = 0.50f;
	constexpr float ENEMY_HIT_SPHERE_RADIUS = 0.55f;
	constexpr float SHOOT_MAX_DISTANCE = 30.0f;

	//========= 敵攻撃定数=========
	// 敵通常攻撃の射程と攻撃間隔。
	constexpr float ENEMY_NORMAL_ATTACK_RANGE = 3.0f;
	constexpr float ENEMY_NORMAL_ATTACK_RANGE_SQUARED = ENEMY_NORMAL_ATTACK_RANGE * ENEMY_NORMAL_ATTACK_RANGE;
	constexpr float ENEMY_NORMAL_ATTACK_INTERVAL = 1.2f;

	// 敵特殊攻撃のダメージ倍率。
	constexpr float ENEMY_SPECIAL_ATTACK_DAMAGE_MULTIPLIER = 2.0f;

	//========= Player特殊攻撃定数=========
	// Player特殊攻撃の射程とダメージ倍率。
	constexpr float PLAYER_SPECIAL_ATTACK_RANGE = 8.0f;
	constexpr float PLAYER_SPECIAL_ATTACK_RANGE_SQUARED = PLAYER_SPECIAL_ATTACK_RANGE * PLAYER_SPECIAL_ATTACK_RANGE;
	constexpr float PLAYER_SPECIAL_ATTACK_DAMAGE_MULTIPLIER = 3.0f;

	//========= Stage Gate定数=========
	// Gateのサイズ、回転、操作範囲。
	constexpr float GATE_WIDTH = 1.2f;
	constexpr float GATE_HEIGHT = 1.4f;
	constexpr float GATE_DEPTH = 0.35f;
	constexpr float GATE_INTERACTION_RADIUS = 2.0f;
	constexpr float GATE_INTERACTION_RADIUS_SQUARED = GATE_INTERACTION_RADIUS * GATE_INTERACTION_RADIUS;

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

	const float specialAttackRangeSquared =
		stageData.specialAttackHitboxRadius *
		stageData.specialAttackHitboxRadius;

	m_EnemyController.Initialize(
	stageData.enemyMaxHp,
	ENEMY_NORMAL_ATTACK_INTERVAL,
	ENEMY_NORMAL_ATTACK_RANGE_SQUARED,
	stageData.enemyDamage,
	stageData.specialAttackInterval,
	specialAttackRangeSquared,
	stageData.enemyDamage *
	ENEMY_SPECIAL_ATTACK_DAMAGE_MULTIPLIER );
	// EnemyのVisual、Transform、Animation状態を初期化する。
	m_EnemyVisual.Initialize(
	DirectX::XMFLOAT3{ ENEMY_BASE_X,ENEMY_BASE_Y,ENEMY_BASE_Z },
	DirectX::XMFLOAT3{ ENEMY_MODEL_SCALE,ENEMY_MODEL_SCALE,ENEMY_MODEL_SCALE } );
	// Projectileの使用状態を初期化する。
	m_ProjectileSystem.Initialize();
	// Stage FieldのTransformを初期化する。
	m_StageField.Initialize();
	// Gun View ModelのMuzzle Flash状態を初期化する。
	m_GunViewModel.Initialize();

	// 前後StageとShopのGateを初期化する。
	const DirectX::XMFLOAT3 gateScale
	{
	GATE_WIDTH,
	GATE_HEIGHT,
	GATE_DEPTH
	};

	m_PreviousStageGate.Initialize(
	StageGate::GateType::e_PREVIOUS_STAGE,
	DirectX::XMFLOAT3{
	PREVIOUS_GATE_POSITION_X,
	PREVIOUS_GATE_POSITION_Y,
	PREVIOUS_GATE_POSITION_Z },
	gateScale,
	DirectX::XMFLOAT4{ 0.85f, 0.30f, 1.0f, 1.0f } );

	m_NextStageGate.Initialize(
	StageGate::GateType::e_NEXT_STAGE,
	DirectX::XMFLOAT3{
	NEXT_GATE_POSITION_X,
	NEXT_GATE_POSITION_Y,
	NEXT_GATE_POSITION_Z },
	gateScale,
	DirectX::XMFLOAT4{ 0.10f, 0.85f, 1.0f, 1.0f } );

	m_ShopGate.Initialize(
	StageGate::GateType::e_SHOP,
	DirectX::XMFLOAT3{
	SHOP_GATE_POSITION_X,
	SHOP_GATE_POSITION_Y,
	SHOP_GATE_POSITION_Z },
	gateScale,
	DirectX::XMFLOAT4{ 1.0f, 0.75f, 0.10f, 1.0f } );

	// Sky DomeのTransformを初期化する。
	m_SkyDome.Initialize();

	// 戦闘、Scene進行、SE再生の状態を初期化する。
	m_IsLastShotHit = {};
	m_IsLastNormalAttackHit = {};
	m_IsLastSpecialAttackHit = {};
	m_IsLastPlayerSpecialAttackHit = {};
	m_HasPlayedLowHpSe = {};
	m_HasPlayedEnemyDefeatSe = {};
	m_GamePhase = GamePhase::e_PLAYING;
	m_SceneChangeRequest = {};
	m_DebugUiRequest = {};

	// 敵・Gate・特殊攻撃・銃口FlashのTimerを初期化する。
	m_EnemyAnimationTime = {};
	m_SpecialAttackCooldownTimer = {};

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
		Finalize();
		return false;
	}

	// Cube描画に使用するRendererを初期化する。
	const bool isBasicMeshRendererInitialized = m_BasicMeshRenderer.Initialize( m_GraphicsSystem );
	if ( !isBasicMeshRendererInitialized )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"BasicMeshRenderer初期化失敗" );
		Finalize();
		return false;
	}

	// Sky DomeのOBJ、Texture、Shaderを初期化する。
	const bool isSkyDomeRendererInitialized = m_SkyDomeRenderer.Initialize( m_GraphicsSystem, SKY_OBJ_PATH, SKY_TEXTURE_PATH );
	if ( !isSkyDomeRendererInitialized )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"SkyDomeRenderer初期化失敗" );
		Finalize();
		return false;
	}

	// 敵OBJと単色描画用Shaderを初期化する。
	const bool isEnemyModelRendererInitialized = m_EnemyModelRenderer.Initialize( m_GraphicsSystem, ENEMY_OBJ_PATH, L"" );
	if ( !isEnemyModelRendererInitialized )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"EnemyModelRenderer初期化失敗" );
		Finalize();
		return false;
	}

	// HUDのQuad描画Resourceを初期化する。
	const bool isHudRendererInitialized = m_HudRenderer.Initialize( m_GraphicsSystem );
	if ( !isHudRendererInitialized )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"HudRenderer初期化失敗" );
		Finalize();
		return false;
	}

	// HUDのText描画Resourceを初期化する。
	const bool isHudTextRendererInitialized = m_HudTextRenderer.Initialize( m_GraphicsSystem );
	if ( !isHudTextRendererInitialized )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_SCENE, L"HudTextRenderer初期化失敗" );
		Finalize();
		return false;
	}

	return true;
}

// 戦闘、Player、敵、弾、ゲート、Scene遷移を更新する。
void GameScene::Update( float deltaTime )
{
	if ( UpdateSceneChangeRequest() ) return;
	if ( UpdateDebugUiRequest() ) return;
	if ( UpdateDebugSceneChange() ) return;
	if ( UpdatePlayerDeath() ) return;

	// SceneManagerが所有するゲーム進捗と現在Stage設定を取得する。
	GameProgress& progress = m_SceneManager.GetGameProgress();
	const StageData& stageData = progress.GetCurrentStageData();
	const PlayerStats& playerStats = progress.GetPlayerStats();

	UpdateGameProgressAndTimers( deltaTime, progress );
	UpdateLowHealthWarning();

	const float playerToEnemyDistanceSquared = GetPlayerToEnemyDistanceSquared();
	UpdateEnemyCombat( deltaTime, playerToEnemyDistanceSquared );
	UpdatePlayerAttack( progress, playerStats, playerToEnemyDistanceSquared );
	UpdateProjectiles( deltaTime );
	UpdateAnimations( deltaTime );
	UpdateEnemyDefeat( progress );
	UpdateMouseCapture();

	if ( UpdateGates( progress ) ) return;

	UpdatePlayerAndCamera( deltaTime );
}
// 3D Stage、敵、弾、ゲート、HUD、Debug UIを描画する。
void GameScene::Draw()
{
	GameProgress& progress = m_SceneManager.GetGameProgress();
	const PlayerStats& playerStats = progress.GetPlayerStats();
	const StageData& stageData = progress.GetCurrentStageData();

	// 現在Stageに応じたGateの表示可否を判定する。
	const int currentStage = progress.GetCurrentStage();
	const bool isPreviousGateAvailable = currentStage > StageConstants::FIRST_STAGE_NUMBER;
	const bool isNextGateAvailable =
		currentStage < StageConstants::MAX_STAGE_COUNT && progress.IsStageCleared( currentStage );

	// 3D描画に使用するProjection、View、Camera座標を取得する。
	const DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
	DirectX::XMConvertToRadians( PROJECTION_FOV_DEGREES ),
	PROJECTION_ASPECT_RATIO,
	PROJECTION_NEAR_Z,
	PROJECTION_FAR_Z );
	const DirectX::XMMATRIX viewMatrix = m_FpsCamera.GetViewMatrix();
	const DirectX::XMFLOAT3 cameraPosition = m_FpsCamera.GetPosition();

	DrawSky( viewMatrix, projectionMatrix, cameraPosition );
	DrawOpaqueWorld( viewMatrix, projectionMatrix, isPreviousGateAvailable, isNextGateAvailable );
	DrawTransparentWorld( viewMatrix, projectionMatrix );
	m_EnemyHealthBar.Draw(
	m_BasicMeshRenderer,
	m_GraphicsSystem,
	viewMatrix,
	projectionMatrix,
	cameraPosition,
	m_EnemyVisual.GetPosition(),
	m_EnemyController.GetCurrentHp(),
	m_EnemyController.GetMaxHp(),
	m_EnemyController.IsDead() );

	const DirectX::XMFLOAT3 playerPosition = m_DebugPlayer.GetPosition();
	const bool isNearPreviousGate = isPreviousGateAvailable && m_PreviousStageGate.IsPlayerNear( playerPosition, GATE_INTERACTION_RADIUS_SQUARED );
	const bool isNearNextGate = isNextGateAvailable && m_NextStageGate.IsPlayerNear( playerPosition, GATE_INTERACTION_RADIUS_SQUARED );
	const bool isNearShopGate = m_ShopGate.IsPlayerNear( playerPosition, GATE_INTERACTION_RADIUS_SQUARED );
	const bool isSpecialAttackUnlocked = playerStats.isSpecialAttackUnlocked;

	GameHudState hudState{};
	hudState.currentStage = currentStage;
	hudState.maxStageCount = StageConstants::MAX_STAGE_COUNT;
	hudState.currency = progress.GetCurrency();
	hudState.playerCurrentHp = m_PlayerHealth.GetCurrentHp();
	hudState.playerMaxHp = m_PlayerHealth.GetMaxHp();
	hudState.isSpecialAttackUnlocked = isSpecialAttackUnlocked;
	hudState.isSpecialAttackReady = m_SpecialAttackCooldownTimer <= 0.0f;
	hudState.specialAttackCooldownTimer = m_SpecialAttackCooldownTimer;
	hudState.isNearPreviousGate = isNearPreviousGate;
	hudState.isNearNextGate = isNearNextGate;
	hudState.isNearShopGate = isNearShopGate;
	if ( !m_EnemyController.IsDead() )
	{
		m_EnemyVisual.Draw(
		m_EnemyModelRenderer,
		m_GraphicsSystem,
		viewMatrix,
		projectionMatrix );
	}
	hudState.showSpecialAttackUnlockHint = hudState.showTutorial && !isSpecialAttackUnlocked;
	hudState.showStageClearMessage = m_GamePhase == GamePhase::e_STAGE_CLEAR;

	m_GameHud.Draw( m_HudRenderer, m_HudTextRenderer, m_GraphicsSystem, hudState );
	DrawDebugUi( progress, stageData, currentStage, isPreviousGateAvailable, isNextGateAvailable );
}

// GameSceneで使用した描画リソースを終了する。
void GameScene::Finalize()
{
	// HUD描画リソースを終了する。
	m_HudTextRenderer.Uninit();
	m_HudRenderer.Uninit();

	// OBJモデル描画リソースを終了する。
	m_EnemyModelRenderer.Uninit();
	m_SkyDomeRenderer.Uninit();

	// Cube描画リソースを終了する。
	m_BasicMeshRenderer.Uninit();
}

// DrawまたはGame進行で予約されたScene遷移をUpdate開始時に実行する。
bool GameScene::UpdateSceneChangeRequest()
{
	if ( m_SceneChangeRequest == SceneChangeRequest::e_NONE ) return false;

	m_AudioSystem.PlayWarpSe();
	m_InputSystem.SetMouseCaptureEnabled( false );

	switch ( m_SceneChangeRequest )
	{
		case SceneChangeRequest::e_RELOAD_GAME:
		m_SceneManager.RequestSceneChange<GameScene>();
		break;

		case SceneChangeRequest::e_SHOP:
		m_SceneManager.RequestSceneChange<ShopScene>();
		break;

		case SceneChangeRequest::e_RESULT:
		m_SceneManager.RequestSceneChange<ResultScene>();
		break;

		case SceneChangeRequest::e_NONE:
		break;

		default:
		break;
	}

	m_SceneChangeRequest = {};

	return true;
}

// DrawDebugUiで予約されたDebug操作をUpdate開始時に実行する。
bool GameScene::UpdateDebugUiRequest()
{
	switch ( m_DebugUiRequest )
	{
		case DebugUiRequest::e_TAKE_DAMAGE:
		m_PlayerHealth.TakeDamage( TEST_PLAYER_DAMAGE );
		m_DebugUiRequest = {};
		return false;

		case DebugUiRequest::e_PREVIOUS_STAGE:
		{
			GameProgress& progress = m_SceneManager.GetGameProgress();
			const int currentStage = progress.GetCurrentStage();

			if ( progress.TrySetCurrentStage( currentStage - PREVIOUS_STAGE_OFFSET ) )
			{
				m_SceneChangeRequest = SceneChangeRequest::e_RELOAD_GAME;
			}

			m_DebugUiRequest = {};
			return false;
		}

		case DebugUiRequest::e_NEXT_STAGE:
		{
			GameProgress& progress = m_SceneManager.GetGameProgress();
			const int currentStage = progress.GetCurrentStage();

			if ( progress.TrySetCurrentStage( currentStage + NEXT_STAGE_OFFSET ) )
			{
				m_SceneChangeRequest = SceneChangeRequest::e_RELOAD_GAME;
			}

			m_DebugUiRequest = {};
			return false;
		}

		case DebugUiRequest::e_NONE:
		default:
		return false;
	}
}

// F2キーによるShopSceneへの遷移を処理する。
bool GameScene::UpdateDebugSceneChange()
{
	if ( !m_InputSystem.IsKeyTriggered( RETURN_TO_SHOP_KEY ) ) return false;

	m_AudioSystem.PlayWarpSe();
	m_InputSystem.SetMouseCaptureEnabled( false );
	m_SceneManager.RequestSceneChange<ShopScene>();

	return true;
}

// Player死亡時のペナルティとShopScene遷移を処理する。
bool GameScene::UpdatePlayerDeath()
{
	if ( !m_PlayerHealth.IsDead() ) return false;

	GameProgress& progress = m_SceneManager.GetGameProgress();

	progress.AddDeath();
	static_cast<void>( progress.ApplyDeathCurrencyPenalty() );

	m_AudioSystem.PlayWarpSe();
	m_InputSystem.SetMouseCaptureEnabled( false );
	m_SceneManager.RequestSceneChange<ShopScene>();

	return true;
}

// ゲーム進行時間、特殊攻撃、銃口FlashのTimerを更新する。
void GameScene::UpdateGameProgressAndTimers( float deltaTime, GameProgress& progress )
{
	// ゲーム進行時間と攻撃・Flash用Timerを更新する。
	progress.Update( deltaTime );
	m_SpecialAttackCooldownTimer =
		std::max( 0.0f, m_SpecialAttackCooldownTimer - deltaTime );
	m_GunViewModel.Update( deltaTime );
}

// 低HP状態の警告SEを更新する。
void GameScene::UpdateLowHealthWarning()
{
	// 低HP状態に入ったときだけ警告SEを再生する。
	const float playerMaxHp = m_PlayerHealth.GetMaxHp();
	const float playerCurrentHp = m_PlayerHealth.GetCurrentHp();
	const bool isLowHealth =
		playerMaxHp > 0.0f &&
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
}

// PlayerとEnemyの水平距離の二乗を返す。
float GameScene::GetPlayerToEnemyDistanceSquared() const
{
	return m_CombatSystem.GetHorizontalDistanceSquared( m_DebugPlayer.GetPosition(), m_EnemyVisual.GetPosition() );
}
// 敵通常攻撃と特殊攻撃を更新する。
void GameScene::UpdateEnemyCombat(
float deltaTime,
float playerToEnemyDistanceSquared )
{
	const bool isCombatActive =
		m_GamePhase == GamePhase::e_PLAYING &&
		!m_EnemyController.IsDead();

	const EnemyAttackResult attackResult =
		m_EnemyController.UpdateCombat(
		deltaTime,
		isCombatActive,
		playerToEnemyDistanceSquared );

	m_IsLastNormalAttackHit = false;
	m_IsLastSpecialAttackHit = false;

	if ( !attackResult.didAttack )
	{
		return;
	}

	if ( attackResult.isSpecialAttack )
	{
		m_IsLastSpecialAttackHit =
			attackResult.didHitPlayer;
	}
	else
	{
		m_IsLastNormalAttackHit =
			attackResult.didHitPlayer;
	}

	if ( !attackResult.didHitPlayer )
	{
		return;
	}

	m_PlayerHealth.TakeDamage(
	attackResult.playerDamage );

	m_AudioSystem.PlayDamageSe();
}

// Playerの通常射撃と特殊攻撃を更新する。
void GameScene::UpdatePlayerAttack( GameProgress& progress, const PlayerStats& playerStats, float playerToEnemyDistanceSquared )
{
	TryFireNormalShot( progress, playerStats );
	TryUseSpecialAttack( progress, playerStats, playerToEnemyDistanceSquared );
}

// 左クリックによる通常射撃、Projectile生成、Ray命中判定を処理する。
void GameScene::TryFireNormalShot( GameProgress& progress, const PlayerStats& playerStats )
{
	if ( !m_InputSystem.IsKeyTriggered( SHOOT_ENEMY_KEY ) ||
	m_EnemyController.IsDead() ||
	m_GamePhase != GamePhase::e_PLAYING )
	{
		return;
	}

	m_AudioSystem.PlayGunSe();
	m_GunViewModel.TriggerMuzzleFlash();

	// Camera空間の銃口座標をWorld座標へ変換してProjectileの生成位置にする。
	const DirectX::XMMATRIX inverseViewMatrix =
		DirectX::XMMatrixInverse(
		nullptr,
		m_FpsCamera.GetViewMatrix() );

	const DirectX::XMVECTOR projectileSpawnCameraSpace =
		DirectX::XMVectorSet(
		BULLET_SPAWN_OFFSET_X,
		BULLET_SPAWN_OFFSET_Y,
		BULLET_SPAWN_OFFSET_Z,
		1.0f );

	const DirectX::XMVECTOR projectileSpawnWorldSpace =
		DirectX::XMVector3TransformCoord(
		projectileSpawnCameraSpace,
		inverseViewMatrix );

	DirectX::XMFLOAT3 projectileSpawnPosition{};

	DirectX::XMStoreFloat3(
	&projectileSpawnPosition,
	projectileSpawnWorldSpace );

	const DirectX::XMFLOAT3 projectileDirection =
		m_FpsCamera.GetForward();

	m_ProjectileSystem.Spawn(
	projectileSpawnPosition,
	projectileDirection,
	BULLET_LIFETIME );

	const EnemyHitTest enemyHitTest
	{
	m_EnemyVisual.GetHitSphereCenter(
	ENEMY_HIT_CENTER_Y_OFFSET ),
	ENEMY_HIT_SPHERE_RADIUS,
	SHOOT_MAX_DISTANCE
	};

	m_IsLastShotHit =
		m_CombatSystem.IsHitScanHit(
		m_FpsCamera.GetPosition(),
		projectileDirection,
		enemyHitTest );

	if ( !m_IsLastShotHit )return;


	const EnemyDamageResult damageResult =
		m_EnemyController.TakeDamage(
		playerStats.gunDamage );

	progress.AddDamageReward(
	damageResult.actualDamage );
}

// Qキーによる範囲特殊攻撃、Cooldown、EnemyへのDamageを処理する。
void GameScene::TryUseSpecialAttack(
GameProgress& progress,
const PlayerStats& playerStats,
float playerToEnemyDistanceSquared )
{
	if ( !m_InputSystem.IsKeyTriggered(
		SPECIAL_ATTACK_KEY ) ||
	m_EnemyController.IsDead() ||
	m_GamePhase != GamePhase::e_PLAYING ||
	!playerStats.isSpecialAttackUnlocked ||
	m_SpecialAttackCooldownTimer > 0.0f )
	{
		return;
	}

	m_AudioSystem.PlaySpecialSe();

	m_IsLastPlayerSpecialAttackHit = m_CombatSystem.IsWithinRangeSquared( playerToEnemyDistanceSquared, PLAYER_SPECIAL_ATTACK_RANGE_SQUARED );

	m_SpecialAttackCooldownTimer = playerStats.specialAttackCooldown;

	if ( !m_IsLastPlayerSpecialAttackHit )return;

	const EnemyDamageResult damageResult =
		m_EnemyController.TakeDamage(
		playerStats.gunDamage *
		PLAYER_SPECIAL_ATTACK_DAMAGE_MULTIPLIER );

	progress.AddDamageReward(
	damageResult.actualDamage );
}
// Projectileの移動と寿命を更新する。
void GameScene::UpdateProjectiles( float deltaTime )
{
	m_ProjectileSystem.Update( deltaTime, BULLET_SPEED );
}

// EnemyとGateのアニメーションを更新する。
void GameScene::UpdateAnimations(
float deltaTime )
{
	if ( !m_EnemyController.IsDead() )
	{
		m_EnemyVisual.Update( deltaTime );
	}

	m_PreviousStageGate.Update( deltaTime );
	m_NextStageGate.Update( deltaTime );
	m_ShopGate.Update( deltaTime );
}

// Enemy撃破報酬、Stage Clear、Result遷移予約を更新する。
void GameScene::UpdateEnemyDefeat(
GameProgress& progress )
{
	if ( !m_EnemyController.IsDead() ||
	m_HasPlayedEnemyDefeatSe ||
	m_GamePhase != GamePhase::e_PLAYING )
	{
		return;
	}

	m_AudioSystem.PlayEnemyDefeatSe();
	m_HasPlayedEnemyDefeatSe = true;

	progress.AddEnemyDefeat();

	const bool isFirstClear =
		progress.MarkCurrentStageCleared();

	if ( !isFirstClear )
	{
		return;
	}

	progress.AddStageClearReward();

	if ( progress.GetCurrentStage() ==
	StageConstants::MAX_STAGE_COUNT )
	{
		m_GamePhase =
			GamePhase::e_RESULT_TRANSITION;

		m_SceneChangeRequest =
			SceneChangeRequest::e_RESULT;

		return;
	}

	m_GamePhase =
		GamePhase::e_STAGE_CLEAR;
}

// F1キーによるMouse Capture切替を処理する。
void GameScene::UpdateMouseCapture()
{
	// F1キーでFPSマウスキャプチャの有効・無効を切り替える。
	if ( m_InputSystem.IsKeyTriggered( TOGGLE_MOUSE_CAPTURE_KEY ) )
	{
		m_InputSystem.SetMouseCaptureEnabled(
		!m_InputSystem.IsMouseCaptureEnabled() );
	}
}

// Gate操作によるStageまたはShopへのScene遷移を処理する。
bool GameScene::UpdateGates( GameProgress& progress )
{
	const DirectX::XMFLOAT3 playerPosition = m_DebugPlayer.GetPosition();
	// 現在Stageに応じた前後Gateの使用可否を判定する。
	const int currentStage = progress.GetCurrentStage();
	const bool isPreviousGateAvailable = currentStage > StageConstants::FIRST_STAGE_NUMBER;
	const bool isNextGateAvailable = currentStage < StageConstants::MAX_STAGE_COUNT && progress.IsStageCleared( currentStage );

	// Eキーで近くにあるGateを使用し、必要ならStage番号を更新してScene遷移する。
	if ( !m_InputSystem.IsKeyTriggered( USE_GATE_KEY ) ) return false;

	if ( isPreviousGateAvailable &&
		 m_PreviousStageGate.IsPlayerNear( playerPosition, GATE_INTERACTION_RADIUS_SQUARED ) &&
		 progress.TrySetCurrentStage( currentStage - PREVIOUS_STAGE_OFFSET ) )
	{
		m_AudioSystem.PlayWarpSe();
		m_InputSystem.SetMouseCaptureEnabled( false );
		m_SceneManager.RequestSceneChange<GameScene>();

		return true;
	}

	if ( isNextGateAvailable &&
	m_NextStageGate.IsPlayerNear( playerPosition, GATE_INTERACTION_RADIUS_SQUARED ) &&
	progress.TrySetCurrentStage( currentStage + NEXT_STAGE_OFFSET ) )
	{
		m_AudioSystem.PlayWarpSe();
		m_InputSystem.SetMouseCaptureEnabled( false );
		m_SceneManager.RequestSceneChange<GameScene>();

		return true;
	}

	if ( m_ShopGate.IsPlayerNear( playerPosition, GATE_INTERACTION_RADIUS_SQUARED ) )
	{
		m_AudioSystem.PlayWarpSe();
		m_InputSystem.SetMouseCaptureEnabled( false );
		m_SceneManager.RequestSceneChange<ShopScene>();

		return true;
	}

	return false;
}

// Camera回転、Player移動、Camera追従を更新する。
void GameScene::UpdatePlayerAndCamera( float deltaTime )
{
	// 最後にCamera回転、Player移動、Camera追従位置を更新する。
	m_FpsCamera.Update( m_InputSystem );
	m_DebugPlayer.Update( deltaTime, m_InputSystem, m_FpsCamera );
	m_FpsCamera.SetPosition( m_DebugPlayer.GetPosition() );
}

// Sky DomeをSky Passで描画する。
void GameScene::DrawSky( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix, const DirectX::XMFLOAT3& cameraPosition )
{
	m_SkyDome.Draw( m_SkyDomeRenderer, m_GraphicsSystem, viewMatrix, projectionMatrix, cameraPosition );
}

// Enemy、Floor、Wall、Gate、GunをOpaque Passで描画する。
void GameScene::DrawOpaqueWorld( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix, bool isPreviousGateAvailable, bool isNextGateAvailable )
{
	if ( !m_EnemyController.IsDead() )
	{
		m_EnemyVisual.Draw(
		m_EnemyModelRenderer,
		m_GraphicsSystem,
		viewMatrix,
		projectionMatrix );
	}

	// Stage FieldのFloorとWallをOpaque Passで描画する。
	m_StageField.Draw(
	m_BasicMeshRenderer,
	m_GraphicsSystem,
	viewMatrix,
	projectionMatrix );

	// 使用可能な前後Gateと常時使用可能なShop Gateを描画する。
	if ( isPreviousGateAvailable )
	{
		m_PreviousStageGate.Draw(
		m_BasicMeshRenderer,
		m_GraphicsSystem,
		viewMatrix,
		projectionMatrix );
	}

	if ( isNextGateAvailable )
	{
		m_NextStageGate.Draw(
		m_BasicMeshRenderer,
		m_GraphicsSystem,
		viewMatrix,
		projectionMatrix );
	}

	m_ShopGate.Draw(
	m_BasicMeshRenderer,
	m_GraphicsSystem,
	viewMatrix,
	projectionMatrix );

	// FPS Cameraに追従するGun本体とGun Barrelを描画する。
	m_GunViewModel.DrawOpaque(
	m_BasicMeshRenderer,
	m_GraphicsSystem,
	viewMatrix,
	projectionMatrix );
}

// Muzzle FlashとBulletをTransparent Passで描画する。
void GameScene::DrawTransparentWorld(
const DirectX::XMMATRIX& viewMatrix,
const DirectX::XMMATRIX& projectionMatrix )
{
	// FPS Cameraに追従するMuzzle FlashをTransparent Passで描画する。
	m_GunViewModel.DrawTransparent(
	m_BasicMeshRenderer,
	m_GraphicsSystem,
	viewMatrix,
	projectionMatrix );

	// 発射中の弾を半透明で描画する。
	m_GraphicsSystem.SetRenderPass( e_RenderPass::e_TRANSPARENT );

	const std::array<ProjectileSystem::Projectile, 16>& projectiles = m_ProjectileSystem.GetProjectiles();

	for ( const ProjectileSystem::Projectile& projectile : projectiles )
	{
		if ( !projectile.isActive ) continue;

		const float projectileAlpha =
			std::clamp( projectile.remainingLifetime / BULLET_LIFETIME, 0.0f, 1.0f );
		const DirectX::XMMATRIX projectileWorldMatrix =
			DirectX::XMMatrixScaling( BULLET_SCALE, BULLET_SCALE, BULLET_SCALE ) *
			DirectX::XMMatrixTranslation(
			projectile.position.x,
			projectile.position.y,
			projectile.position.z );

		m_BasicMeshRenderer.DrawCube(
		m_GraphicsSystem,
		projectileWorldMatrix,
		viewMatrix,
		projectionMatrix,
		DirectX::XMFLOAT4{ 1.0f, 0.85f, 0.10f, projectileAlpha },
		DirectX::XMFLOAT2{ 1.0f, 1.0f },
		BasicMeshRenderer::TextureType::Color );
	}
}

// ImGui Debug UIを描画し、既存のScene遷移処理を行う。
void GameScene::DrawDebugUi(
GameProgress& progress,
const StageData& stageData,
int currentStage,
bool isPreviousGateAvailable,
bool isNextGateAvailable )
{
	// ImGuiでStage、敵、通貨、Gate操作を確認できるDebug UIを描画する。
	ImGui::Begin( "Game Debug" );
	ImGui::Text( "Stage %d / %d", stageData.stageNumber, StageConstants::MAX_STAGE_COUNT );
	ImGui::Text( "Enemy: %s", stageData.enemyName );
	ImGui::Text( "Clear Reward: %d", stageData.clearReward );

	if ( ImGui::Button( "Test: Take 10 Damage" ) )m_DebugUiRequest = DebugUiRequest::e_TAKE_DAMAGE;

	ImGui::Separator();

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
	ImGui::Button( PREVIOUS_STAGE_GATE_LABEL, ImVec2( GATE_BUTTON_WIDTH, GATE_BUTTON_HEIGHT ) ) )
	{
		gateDestination = GateDestination::e_PREVIOUS_STAGE;
	}

	if ( currentStage < StageConstants::MAX_STAGE_COUNT )
	{
		if ( !isNextGateAvailable ) ImGui::BeginDisabled();

		if ( ImGui::Button( NEXT_STAGE_GATE_LABEL, ImVec2( GATE_BUTTON_WIDTH, GATE_BUTTON_HEIGHT ) ) )
		{
			gateDestination = GateDestination::e_NEXT_STAGE;
		}

		if ( !isNextGateAvailable ) ImGui::EndDisabled();
	}

	if ( ImGui::Button( SHOP_GATE_LABEL, ImVec2( GATE_BUTTON_WIDTH, GATE_BUTTON_HEIGHT ) ) )
	{
		gateDestination = GateDestination::e_SHOP;
	}

	ImGui::End();

	// ImGui Debug UIで選択されたGateに応じて次フレームのScene遷移を予約する。
	if ( gateDestination == GateDestination::e_PREVIOUS_STAGE )
	{
		m_DebugUiRequest = DebugUiRequest::e_PREVIOUS_STAGE;
	}
	else if ( gateDestination == GateDestination::e_NEXT_STAGE )
	{
		m_DebugUiRequest = DebugUiRequest::e_NEXT_STAGE;
	}
	else if ( gateDestination == GateDestination::e_SHOP )
	{
		m_SceneChangeRequest = SceneChangeRequest::e_SHOP;
	}
}