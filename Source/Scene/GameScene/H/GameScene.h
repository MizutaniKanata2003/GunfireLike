#pragma once

//========= Debug インクルード=========
#include "Debug/H/DebugPlayer.h"

//========= Framework インクルード=========
#include "Framework/2D/H/HudRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/3D/H/ObjModelRenderer.h"
#include "Framework/Input/H/FpsCamera.h"

//========= Scene インクルード=========
#include "Scene/Common/H/CombatSystem.h"
#include "Scene/Common/H/EnemyController.h"
#include "Scene/Common/H/EnemyHealthBar.h"
#include "Scene/Common/H/EnemyVisual.h"
#include "Scene/Common/H/GameHud.h"
#include "Scene/Common/H/GunViewModel.h"
#include "Scene/Common/H/IScene.h"
#include "Scene/Common/H/PlayerHealth.h"
#include "Scene/Common/H/PlayerHealthStateController.h"
#include "Scene/Common/H/PlayerCombatController.h"
#include "Scene/Common/H/ProjectileSystem.h"
#include "Scene/Common/H/SkyDome.h"
#include "Scene/Common/H/StageField.h"
#include "Scene/Common/H/StageGate.h"
#include "Scene/Common/H/StageGateController.h"
#include "Scene/Common/H/StageProgressController.h"

//========= 前方宣言=========
class AudioSystem;
class GameProgress;
class GraphicsSystem;
class InputSystem;
class SceneManager;
struct PlayerStats;
struct StageData;

// 3D戦闘、敵、弾、ゲート、HUD、ゲームクリア判定を管理する。
class GameScene final : public IScene
{
public:
	//========= 生成関数=========
	// GameSceneが使用するSceneManagerとFramework Systemを登録する。
	GameScene( SceneManager& sceneManager, InputSystem& inputSystem, GraphicsSystem& graphicsSystem, AudioSystem& audioSystem );

	//========= Sceneライフサイクル関数=========
	// Stageの敵、Player、Renderer、HUD、BGMを初期化する。
	void Initialize() override;
	// Asset読込などの深い初期化を行う。
	bool Init() override;
	// 戦闘、Player、敵、弾、ゲート、Scene遷移を更新する。
	void Update( float deltaTime ) override;
	// 3D Stage、敵、弾、ゲート、HUD、Debug UIを描画する。
	void Draw() override;
	// GameSceneで使用した描画リソースを終了する。
	void Finalize() override;
private:
	//========= 構造体=========
	//========= Game進行状態=========
	enum class GamePhase
	{
		e_PLAYING,
		e_STAGE_CLEAR,
		e_RESULT_TRANSITION
	};
	//========= Scene遷移要求=========
	enum class SceneChangeRequest
	{
		e_NONE,
		e_RELOAD_GAME,
		e_SHOP,
		e_RESULT
	};
	//========= Debug UI要求=========
	enum class DebugUiRequest
	{
		e_NONE,
		e_TAKE_DAMAGE,
		e_PREVIOUS_STAGE,
		e_NEXT_STAGE
	};
	//========= Debug UI表示状態=========
	// ImGui Debug UIが読み取り専用で表示するGame状態。
	struct GameDebugState
	{
		int currentStage{};
		int maxStageCount{};
		const char* enemyName{};
		int clearReward{};

		float enemyCurrentHp{};
		float enemyMaxHp{};

		int currency{};
		int totalDeaths{};
		float totalPlayTime{};

		bool isLastShotHit{};
		bool isLastNormalAttackHit{};
		bool isLastSpecialAttackHit{};
		bool isLastPlayerSpecialAttackHit{};

		bool isPreviousGateAvailable{};
		bool isNextGateAvailable{};

		GamePhase gamePhase{ GamePhase::e_PLAYING };
	};

	//========= 初期化補助関数=========
	// Player HP、EnemyController、Projectile、特殊攻撃状態を初期化する。
	void InitializeCombatState( const StageData& stageData, const PlayerStats& playerStats );
	// Enemy、Stage Field、Gun、Sky DomeのWorld Objectを初期化する。
	void InitializeWorldObjects();
	// 前後StageとShopへ移動するGateを初期化する。
	void InitializeGates();
	// GamePhase、Scene遷移要求、Debug要求、戦闘結果、SE再生状態を初期化する。
	void InitializeSceneState();
	// FPS操作、Mouse Capture、Game BGMを初期化する。
	void InitializePlayerAndCamera();

	//========= 更新補助関数=========
	// DrawDebugUiで予約されたDebug操作をUpdate開始時に実行する。
	[[nodiscard]] bool UpdateDebugUiRequest();
	// F2キーによるShopSceneへの遷移要求を設定する。
	[[nodiscard]] bool UpdateDebugSceneChange();
	// Player死亡時のペナルティとShopScene遷移を処理する。
	[[nodiscard]] bool UpdatePlayerDeath();
	// ゲーム進行時間、特殊攻撃、銃口FlashのTimerを更新する。
	void UpdateGameProgressAndTimers( float deltaTime, GameProgress& progress );
	// 現フレームのPlayer HP状態を更新する。
	void UpdatePlayerHealthState();
	// 低HP状態の警告SEを更新する。
	void UpdateLowHealthWarning();
	// 敵通常攻撃と特殊攻撃を更新する。
	void UpdateEnemyCombat( float deltaTime, float playerToEnemyDistanceSquared );
	// Playerの通常射撃と特殊攻撃を更新する。
	void UpdatePlayerAttack( GameProgress& progress, const PlayerStats& playerStats, float playerToEnemyDistanceSquared );
	// Projectileの移動と寿命を更新する。
	void UpdateProjectiles( float deltaTime );
	// EnemyとGateのアニメーション時間を更新する。
	void UpdateAnimations( float deltaTime );
	// Enemy撃破報酬、Stage Clear、Result遷移予約を更新する。
	void UpdateEnemyDefeat( GameProgress& progress );
	// F1キーによるMouse Capture切替を処理する。
	void UpdateMouseCapture();
	// Gate操作によるStageまたはShopへのScene遷移を処理する。
	[[nodiscard]] bool UpdateGates( GameProgress& progress );
	// Camera回転、Player移動、Camera追従を更新する。
	void UpdatePlayerAndCamera( float deltaTime );
	// DrawまたはGame進行で予約されたScene遷移をUpdate開始時に実行する。
	[[nodiscard]] bool UpdateSceneChangeRequest();

	//========= 描画補助関数=========
	// Sky DomeをSky Passで描画する。
	void DrawSky( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix, const DirectX::XMFLOAT3& cameraPosition );
	// Enemy、Floor、Wall、Gate、GunをOpaque Passで描画する。
	void DrawOpaqueWorld( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix, bool isPreviousGateAvailable, bool isNextGateAvailable );
	// Muzzle FlashとBulletをTransparent Passで描画する。
	void DrawTransparentWorld( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix );
	// ImGui Debug UIを描画し、Debug操作要求を設定する。
	void DrawDebugUi( const GameDebugState& gameDebugState );
	// 通常射撃のSE、Muzzle Flash、Projectile生成、Ray命中判定、Enemy Damageを実行する。
	void ExecuteNormalShot( GameProgress& progress, const PlayerStats& playerStats );
	// 特殊攻撃のSE、範囲判定、Enemy Damageを実行する。
	void ExecuteSpecialAttack( GameProgress& progress, const PlayerStats& playerStats, float playerToEnemyDistanceSquared );
	// PlayerとEnemyの水平距離の二乗を返す。
	[[nodiscard]] float GetPlayerToEnemyDistanceSquared() const;

	//========= Framework・Scene参照=========
	// Scene遷移とゲーム進捗操作に使用するSceneManager。
	SceneManager& m_SceneManager;
	// Player操作と攻撃入力に使用するInputSystem。
	InputSystem& m_InputSystem;
	// 描画State設定に使用するGraphicsSystem。
	GraphicsSystem& m_GraphicsSystem;
	// BGMとSEの再生に使用するAudioSystem。
	AudioSystem& m_AudioSystem;

	//========= Player・Camera=========
	// FPS視点に追従するデバッグ用Player。
	DebugPlayer m_DebugPlayer{};
	// Playerの視点、前方向、View行列を管理するCamera。
	FpsCamera m_FpsCamera{};

	//========= HP管理=========
	// HitScan、距離二乗、攻撃範囲内判定を管理する。
	CombatSystem m_CombatSystem{};
	// Playerの現在HPと最大HPを管理する。
	PlayerHealth m_PlayerHealth{};
	// Playerの低HP状態変化と死亡状態を管理する。
	PlayerHealthStateController m_PlayerHealthStateController{};
	// 現フレームのPlayer HP状態。Update開始時に一度だけ更新する。
	PlayerHealthStateResult m_PlayerHealthStateResult{};
	// Player攻撃の入力条件、特殊攻撃解放条件、Cooldownを管理する。
	PlayerCombatController m_PlayerCombatController{};
	// EnemyのHP、生死、通常攻撃・特殊攻撃Timerを管理する。
	EnemyController m_EnemyController{};

	//========= Projectile管理=========
	// Playerが発射したProjectileの生成、更新、寿命を管理する。
	ProjectileSystem m_ProjectileSystem{};

	//========= 3D描画=========
	// EnemyのTransform、Animation、Model描画、Hit位置を管理する。
	EnemyVisual m_EnemyVisual{};
	// FloorとWallで構成される静的な3Dフィールドを管理する。
	StageField m_StageField{};
	// 前Stageへ移動するGate。
	StageGate m_PreviousStageGate{};
	// 次Stageへ移動するGate。
	StageGate m_NextStageGate{};
	// Shopへ移動するGate。
	StageGate m_ShopGate{};
	// Stageに応じたGate利用可否と使用対象を判定する。
	StageGateController m_StageGateController{};
	// Enemy撃破後のStage Clear、Reward、Result判定を担当する。
	StageProgressController m_StageProgressController{};
	// FPS Cameraに追従するGunとMuzzle Flashの描画を管理する。
	GunViewModel m_GunViewModel{};
	// Cameraに追従するSky DomeのTransformとSky Pass描画を管理する。
	SkyDome m_SkyDome{};
	// 床、壁、ゲート、銃、弾、HPバーを描画する基本Mesh Renderer。
	BasicMeshRenderer m_BasicMeshRenderer{};
	// Sky DomeのOBJモデルを描画するRenderer。
	ObjModelRenderer m_SkyDomeRenderer{};
	// 敵のOBJモデルを描画するRenderer。
	ObjModelRenderer m_EnemyModelRenderer{};

	//========= HUD描画=========
	// GameSceneのCrosshair、HP、Text、Tutorial、Gate案内を描画する。
	GameHud m_GameHud{};
	// Crosshair、HPバー、低HP警告を描画するHUD Renderer。
	HudRenderer m_HudRenderer{};
	// Stage、通貨、操作説明を描画するHUD文字Renderer。
	HudTextRenderer m_HudTextRenderer{};

	//========= HUD3D描画=========
	// Enemy頭上のWorld Space HPバーを描画する。
	EnemyHealthBar m_EnemyHealthBar{};

	//========= Scene進行状態=========
	// 現在の戦闘・Stage Clear・Result遷移状態を管理する。
	GamePhase m_GamePhase{ GamePhase::e_PLAYING };
	// DrawまたはGame進行から受け取ったScene遷移要求を保持する。
	SceneChangeRequest m_SceneChangeRequest{};
	// DrawDebugUiから受け取ったDebug操作要求を保持する。
	DebugUiRequest m_DebugUiRequest{};

	//========= 戦闘結果状態=========
	// 直前の通常射撃が敵へ命中したかを示すフラグ。
	bool m_IsLastShotHit{};
	// 直前の敵通常攻撃がPlayerへ命中したかを示すフラグ。
	bool m_IsLastNormalAttackHit{};
	// 直前の敵特殊攻撃がPlayerへ命中したかを示すフラグ。
	bool m_IsLastSpecialAttackHit{};
	// 直前のPlayer特殊攻撃が敵へ命中したかを示すフラグ。
	bool m_IsLastPlayerSpecialAttackHit{};

	//========= SE再生状態=========
	// 現在の敵撃破で撃破SEを再生済みかを示すフラグ。
	bool m_HasPlayedEnemyDefeatSe{};
};