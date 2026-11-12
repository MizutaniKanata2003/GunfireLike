#pragma once

// 移動・視点操作・床と壁の描画を確認する最小構成のGameSceneを宣言する。
// プレイヤーとカメラの更新を仲介し、描画Resourceを所有する。Scene切替はManagerへ予約する。

//========= Debug インクルード=========
#include "Debug/H/DebugPlayer.h"

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/Input/H/FpsCamera.h"

//========= Scene インクルード=========
#include "Scene/Common/H/IScene.h"
#include "Scene/Common/H/StageField.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// テスト用の操作・3D描画・デバッグ表示と、タイトルへの遷移予約を担当するScene。
class GameScene final: public IScene
{
public:
	//========= 生成関数=========
	// Sceneで使用する非所有参照を登録する。Resource生成はInitで行う。
	// sceneManager: タイトルへの遷移予約先。Sceneの使用期間中は有効であること。
	// inputSystem: キー入力の取得先とCapture設定先。Sceneより長く生存すること。
	// graphicsSystem: Resource生成・描画先サイズ取得・描画の基盤。Resource終了まで有効であること。
	// audioSystem: Game用BGMの再生先。Sceneの使用期間中は有効であること。
	GameScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

	//========= Sceneライフサイクル関数=========
	// プレイヤー・カメラ・Fieldの状態を初期化し、範囲補正後の位置へカメラを合わせる。
	void Initialize() override;
	// Mesh描画Resourceを生成し、成功後にFPS操作とGame用BGMを有効にする。
	// 戻り値: Mesh初期化成功ならtrue。falseを検出した場合はFinalize後にfalseを返す。
	// 下位処理の例外は捕捉しない。
	bool Init() override;
	// F2の遷移予約、F1のCapture切替、視点とプレイヤー位置の更新を既存順序で行う。
	// deltaTime: プレイヤー更新へ渡す経過時間。単位は秒。
	// Capture解除中は視点・移動更新を行わない。F2で予約した場合は後続処理を行わない。
	void Update(float deltaTime) override;
	// 描画先サイズに合わせて行列を作り、床・壁と読み取り専用のデバッグUIを描画する。
	// 幅または高さが0なら何もしない。ImGuiフレーム開始後、描画データ確定前に呼ぶ。
	void Draw() override;
	// 所有するMesh描画Resourceを解放する。繰り返し呼出可能。CaptureやBGMは変更しない。
	void Finalize() override;

private:
	//========= System・Scene参照=========
	// Scene遷移の予約先となる非所有参照。
	SceneManager& m_sceneManager;
	// 操作入力とCapture設定に使用する非所有参照。
	InputSystem& m_inputSystem;
	// Resource初期化と描画に使用する非所有参照。
	GraphicsSystem& m_graphicsSystem;
	// Init成功後のGame用BGM再生に使用する非所有参照。
	AudioSystem& m_audioSystem;

	//========= Scene所有状態=========
	// 仮の水平移動と床範囲内の位置補正を行うプレイヤー状態。
	DebugPlayer m_debugPlayer{};
	// マウスによる視線方向と、プレイヤー位置に追従するView行列の生成状態。
	FpsCamera m_FPSCamera{};
	// 床と4面の壁のTransformを保持するField。衝突判定は担当しない。
	StageField m_stageField{};
	// 床と壁のCube描画に使用するGPU Resourceを所有する。
	BasicMeshRenderer m_basicMeshRenderer{};
};