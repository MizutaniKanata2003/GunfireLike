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
#include "Scene/Common/H/IScene.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// Shopの3D空間、強化、ゲート、HUD、Scene遷移を管理する。
class ShopScene final : public IScene
{
public:
	//========= 生成関数=========
	// ShopSceneが使用するSceneManagerとFramework Systemを登録する。
	ShopScene(
		SceneManager& sceneManager,
		InputSystem& inputSystem,
		GraphicsSystem& graphicsSystem,
		AudioSystem& audioSystem );

	//========= Sceneライフサイクル関数=========
	// ShopのPlayer、Camera、Renderer、HUD、BGMを初期化する。
	void Initialize() override;
	// Asset読込などの深い初期化を行う。
	bool Init() override
	{
		return true;
	}
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

	//========= 補助関数=========
	// カメラ中央のRayが当たる最も近い操作対象を返す。
	[[nodiscard]] InteractionTarget GetAimedInteractionTarget() const;
	// 指定した操作対象に応じて強化購入またはScene遷移を実行する。
	void TryInteractWithTarget( InteractionTarget target );
	// 指定した強化Objectのワールド座標を返す。
	[[nodiscard]] DirectX::XMFLOAT3 GetUpgradeObjectPosition( InteractionTarget target ) const;
	// 指定した強化Objectの表示色を返す。
	[[nodiscard]] DirectX::XMFLOAT4 GetUpgradeObjectColor( InteractionTarget target ) const;

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
	DebugPlayer m_DebugPlayer{};
	// FPS視点を管理するCamera。
	FpsCamera m_FpsCamera{};

	//========= Renderer=========
	// Shopの床、壁、強化Object、ゲートを描画する3D Renderer。
	BasicMeshRenderer m_BasicMeshRenderer{};
	// 画面固定のQuadを描画するHUD Renderer。
	HudRenderer m_HudRenderer{};
	// HUD文字列を描画するText Renderer。
	HudTextRenderer m_HudTextRenderer{};

	//========= 操作・演出状態=========
	// 強化Objectとゲートの回転・浮遊に使用する累計時間。
	float m_AnimationTime{};
	// 現在カメラ中央のRayが照準している操作対象。
	InteractionTarget m_AimedTarget{ InteractionTarget::e_NONE };

	// 購入結果メッセージの表示状態。
	bool m_ShowPurchaseSuccess{};
	bool m_ShowPurchaseFailure{};
	// 購入結果メッセージを表示する残り時間。
	float m_InteractionMessageTimer{};
};