#pragma once

// 移動・視点操作・床と壁の描画だけを確認する。

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

class GameScene final: public IScene
{
public:
	//========= 生成関数=========
	// Sceneが使用する非所有参照を登録する。
	// sceneManager: Scene遷移の管理先。Sceneより長く生存する。
	// inputSystem: 入力の取得先。Sceneより長く生存する。
	// graphicsSystem: 描画の管理先。Sceneより長く生存する。
	// audioSystem: 音声の管理先。Sceneより長く生存する。
	GameScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

	//========= Sceneライフサイクル関数=========
	// 初期状態を設定する。Resource生成はInitで行う。
	void Initialize() override;
	// 描画Resourceを生成する。失敗時はfalseを返す。
	bool Init() override;
	// 入力とScene状態を更新する。
	// deltaTime: この更新で進める時間。単位は秒。
	void Update(float deltaTime) override;
	// 現在のSceneを描画する。
	void Draw() override;
	// 所有する描画Resourceを解放する。繰り返し呼出可能。
	void Finalize() override;

private:
	//========= System・Scene参照=========
	// Scene遷移に使用する非所有参照。
	SceneManager& m_sceneManager;
	// 入力取得に使用する非所有参照。
	InputSystem& m_inputSystem;
	// 描画に使用する非所有参照。
	GraphicsSystem& m_graphicsSystem;
	// Init成功後のBGM再生に使用する非所有参照。
	AudioSystem& m_audioSystem;

	//========= Scene所有状態=========
	// 仮の水平移動状態を所有する。
	DebugPlayer m_debugPlayer{};
	// 視点とView行列を所有する。
	FpsCamera m_fpsCamera{};
	// 床と壁のTransformを所有する。
	StageField m_stageField{};
	// 床と壁の描画Resourceを所有する。
	BasicMeshRenderer m_basicMeshRenderer{};
};