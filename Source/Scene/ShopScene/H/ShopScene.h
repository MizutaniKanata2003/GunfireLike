#pragma once

//========= C++標準ライブラリ インクルード=========
#include <array>

//========= Windows インクルード=========
#include <windows.h>

//========= Debug インクルード=========
#include "Debug/H/DebugPlayer.h"

//========= Framework インクルード=========
#include "Framework/2D/H/HudRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/Input/H/FpsCamera.h"

//========= Scene インクルード=========
#include "Scene/Common/H/CombatSystem.h"
#include "Scene/Common/H/GameProgress.h"
#include "Scene/Common/H/IScene.h"
#include "Scene/ShopScene/H/ShopGate.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;
struct PlayerStats;

// Shopの3D空間、強化、ゲート、HUD、Scene遷移を管理する。
class ShopScene final : public IScene
{
public:
	//========= 生成関数=========
	// ShopSceneが使用するSceneManagerとFramework Systemを登録する。
	ShopScene( SceneManager& sceneManager, InputSystem& inputSystem, GraphicsSystem& graphicsSystem, AudioSystem& audioSystem );

	//========= Sceneライフサイクル関数=========
	// ShopのPlayer、Camera、Renderer、HUD、BGMを初期化する。
	void Initialize() override;
	// 3D、HUD、文字描画に使用する深い描画Resourceを初期化する。
	bool Init() override;
	// Shop内の操作、カメラ、Player、選択対象、メッセージ表示時間を更新する。
	void Update( float deltaTime ) override;
	// Shopの3D空間、強化Object、ゲート、HUDを描画する。
	void Draw() override;
	// Shopで使用した描画リソースを終了する。
	void Finalize() override;
private:
	//========= 列挙型=========
	// プレイヤーが照準を合わせられるShop内の操作対象。
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
	// 強化購入後にHUDへ表示する結果状態。
	enum class PurchaseResult
	{
		e_NONE,
		e_SUCCESS,
		e_FAILURE
	};

	//========= 構造体=========
	// Shopの強化Objectに対応する購入対象、配置、表示色を保持する。
	struct ShopUpgradeData
	{
		InteractionTarget interactionTarget { InteractionTarget::e_NONE };
		UpgradeType upgradeType { UpgradeType::e_MAX_HP };
		DirectX::XMFLOAT3 position {};
		DirectX::XMFLOAT4 color {};
	};

	//========= 初期化補助関数=========
	// Challenge GateとTitle GateのTransform、色、Raycast設定を初期化する。
	void InitializeGates();

	//========= 更新補助関数=========
	// F1キーによるFPSマウスキャプチャ切替を処理する。
	void UpdateMouseCapture();
	// 強化ObjectとGateの回転・浮遊に使用する時間を更新する。
	void UpdateAnimation( float deltaTime );
	// 購入結果Messageの表示時間と表示状態を更新する。
	void UpdateInteractionMessage( float deltaTime );
	// FPS Camera、DebugPlayer、Camera追従位置を更新する。
	void UpdatePlayerAndCamera( float deltaTime );
	// Camera中央Rayが照準しているShop内の操作対象を更新する。
	void UpdateAimedInteractionTarget() { m_AimedTarget = GetAimedInteractionTarget(); }
	// Eキーによる現在照準中の操作対象との相互作用を処理する。
	void UpdateInteraction();

	//========= 相互作用補助関数=========
	// カメラ中央のRayが当たる最も近い操作対象を返す。
	[[nodiscard]] InteractionTarget GetAimedInteractionTarget() const;
	// 指定した操作対象に応じて強化購入またはScene遷移を実行する。
	void TryInteractWithTarget( InteractionTarget target );
	// Shopの全強化Object設定を読み取り専用で返す。
	[[nodiscard]] static const std::array<ShopUpgradeData, 4>& GetUpgradeData();
	// 指定した操作対象に対応する強化Object設定を返す。対応しない場合はnullptrを返す。
	[[nodiscard]] const ShopUpgradeData* FindUpgradeData( InteractionTarget target ) const;

	//========= 描画補助関数=========
	// Camera位置に追従する単色SkyboxをSky Passで描画する。
	void DrawSky( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix, const DirectX::XMFLOAT3& cameraPosition );
	// Floorと4面のWallをOpaque Passで描画する。
	void DrawOpaqueField( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix );
	// 強化Objectを浮遊・回転・照準状態に応じてOpaque Passで描画する。
	void DrawUpgradeObjects( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix );
	// Challenge GateとTitle GateをOpaque Passで描画する。
	void DrawGates( const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix );
	// Shop HUD、照準、所持金、操作案内、購入結果MessageをScreen UI Passで描画する。
	void DrawHud( GameProgress& progress, const PlayerStats& playerStats );

	//========= Framework・Scene参照=========
	// Scene遷移とゲーム進捗操作に使用するSceneManager。
	SceneManager& m_SceneManager;
	// Shop内のキー・マウス入力に使用するInputSystem。
	InputSystem& m_InputSystem;
	// 描画State設定に使用するGraphicsSystem。
	GraphicsSystem& m_GraphicsSystem;
	// BGMとSEの再生に使用するAudioSystem。
	AudioSystem& m_AudioSystem;

	//========= Player・Camera=========
	// Shop内を移動するデバッグ用Player。
	DebugPlayer m_DebugPlayer {};
	// FPS視点を管理するCamera。
	FpsCamera m_FpsCamera {};

	//========= Raycast判定=========
	// Shop内の強化ObjectとGateへのRay判定を管理する。
	CombatSystem m_CombatSystem {};

	//========= Shop Gate=========
	// GameSceneへ遷移するChallenge Gate。
	ShopGate m_ChallengeGate {};
	// TitleSceneへ遷移するTitle Gate。
	ShopGate m_TitleGate {};

	//========= Renderer=========
	// Shopの床、壁、強化Object、ゲートを描画する3D Renderer。
	BasicMeshRenderer m_BasicMeshRenderer {};
	// 画面固定のQuadを描画するHUD Renderer。
	HudRenderer m_HudRenderer {};
	// HUD文字列を描画するText Renderer。
	HudTextRenderer m_HudTextRenderer {};

	//========= 操作・演出状態=========
	// 強化Objectとゲートの回転・浮遊に使用する累計時間。
	float m_AnimationTime {};
	// 現在カメラ中央のRayが照準している操作対象。
	InteractionTarget m_AimedTarget { InteractionTarget::e_NONE };
	// 購入結果メッセージの表示状態。
	PurchaseResult m_PurchaseResult { PurchaseResult::e_NONE };
	// 購入結果メッセージを表示する残り時間。
	float m_InteractionMessageTimer {};
};