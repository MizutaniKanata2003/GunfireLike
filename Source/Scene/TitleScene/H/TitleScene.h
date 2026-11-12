#pragma once

// タイトルから最小構成のGameSceneへ遷移する。

//========= Framework インクルード=========
#include "Framework/2D/H/HudTextRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/H/IScene.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

class TitleScene final: public IScene
{
public:
	//========= 生成関数=========
	// Sceneが使用する非所有参照を登録する。
	// sceneManager: Scene遷移の管理先。Sceneより長く生存する。
	// inputSystem: 入力の取得先。Sceneより長く生存する。
	// graphicsSystem: 描画の管理先。Sceneより長く生存する。
	// audioSystem: 音声の管理先。Sceneより長く生存する。
	TitleScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

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
	// タイトル文字を描画するResourceを所有する。
	HudTextRenderer m_hudTextRenderer{};
};