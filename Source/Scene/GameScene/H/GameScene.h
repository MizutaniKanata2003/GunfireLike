#pragma once

//========= C++標準ライブラリ インクルード=========
#include <array>

//========= Debug インクルード=========
#include "Debug/H/DebugPlayer.h"

//========= Framework インクルード=========
#include "Framework/2D/H/HudRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/3D/H/ObjModelRenderer.h"
#include "Framework/Input/H/FpsCamera.h"

//========= Scene インクルード=========
#include "Scene/Common/H/EnemyHealth.h"
#include "Scene/Common/H/IScene.h"
#include "Scene/Common/H/PlayerHealth.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// 3D戦闘、敵、弾、ゲート、HUD、ゲームクリア判定を管理する。
class GameScene final : public IScene
{
public:
	//========= 生成関数=========
	// GameSceneが使用するSceneManagerとFramework Systemを登録する。
	GameScene(
		SceneManager& sceneManager,
		InputSystem& inputSystem,
		GraphicsSystem& graphicsSystem,
		AudioSystem& audioSystem );

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
	// 発射後に一定時間だけ移動・描画する弾情報。
	struct Bullet
	{
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 direction{};
		float remainingLifetime{};
		bool isActive{};
	};

	//========= 補助関数=========
	// Playerが指定したゲートの操作範囲内にいるかを返す。
	[[nodiscard]] bool IsPlayerNearGate( float gatePositionX, float gatePositionZ ) const;

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
	// Playerの現在HPと最大HPを管理する。
	PlayerHealth m_PlayerHealth{};
	// 現在Stageの敵の現在HPと最大HPを管理する。
	EnemyHealth m_EnemyHealth{};

	//========= 3D描画=========
	// 床、壁、ゲート、銃、弾、HPバーを描画する基本Mesh Renderer。
	BasicMeshRenderer m_BasicMeshRenderer{};
	// Sky DomeのOBJモデルを描画するRenderer。
	ObjModelRenderer m_SkyDomeRenderer{};
	// 敵のOBJモデルを描画するRenderer。
	ObjModelRenderer m_EnemyModelRenderer{};

	//========= HUD描画=========
	// Crosshair、HPバー、低HP警告を描画するHUD Renderer。
	HudRenderer m_HudRenderer{};
	// Stage、通貨、操作説明を描画するHUD文字Renderer。
	HudTextRenderer m_HudTextRenderer{};

	//========= アニメーション・Timer状態=========
	// 敵の浮遊・回転に使用する累計時間。
	float m_EnemyAnimationTime{};
	// Stage Gateの回転に使用する累計時間。
	float m_GateAnimationTime{};
	// 敵通常攻撃までの経過時間。
	float m_EnemyNormalAttackTimer{};
	// 敵特殊攻撃までの経過時間。
	float m_EnemySpecialAttackTimer{};
	// Player特殊攻撃が再使用可能になるまでの残り時間。
	float m_SpecialAttackCooldownTimer{};
	// 銃口Flashを表示する残り時間。
	float m_MuzzleFlashTimer{};

	//========= Scene進行状態=========
	// 最終Stageを初回クリアしたかを示すフラグ。
	bool m_IsGameClear{};
	// ResultSceneへの遷移を予約したかを示すフラグ。
	bool m_IsResultSceneRequested{};

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
	// 現在の低HP状態で警告SEを再生済みかを示すフラグ。
	bool m_HasPlayedLowHpSe{};
	// 現在の敵撃破で撃破SEを再生済みかを示すフラグ。
	bool m_HasPlayedEnemyDefeatSe{};

	//========= 弾管理=========
	// 発射中の弾を最大数まで保持する固定配列。
	std::array<Bullet, 16> m_Bullets{};
};