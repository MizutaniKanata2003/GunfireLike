#pragma once

//========= Scene インクルード=========
#include "Scene/Common/H/IScene.h"

//========= Framework インクルード=========
#include "Framework/2D/H/HudRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// タイトル画面の入力、UI描画、BGM、Scene遷移を管理する。
class TitleScene final : public IScene
{
public:
	//========= 生成関数=========
	// TitleSceneが使用するSceneManagerとFramework Systemを登録する。
	TitleScene( SceneManager& sceneManager, InputSystem& inputSystem, GraphicsSystem& graphicsSystem, AudioSystem& audioSystem );

	//========= Sceneライフサイクル関数=========
	// タイトル画面の入力設定、HUD、文字、BGMを初期化する。
	void Initialize() override;
	// Asset読込などの深い初期化を行う。
	bool Init() override
	{
		return true;
	}
	// タイトル画面の入力を更新する。
	void Update( float deltaTime ) override;
	// タイトル画面の背景、操作説明、ボタンを描画する。
	void Draw() override;
	// タイトル画面で使用したHUDと文字描画リソースを終了する。
	void Finalize() override;

private:
	//========= 補助関数=========
	// 新しいゲームを開始してShopSceneへの遷移を予約する。
	void StartGame();

	//========= Framework・Scene参照=========
	// Scene遷移とゲーム進捗操作に使用するSceneManager。
	SceneManager& m_SceneManager;
	// タイトル画面のキーボード入力に使用するInputSystem。
	InputSystem& m_InputSystem;
	// HUDの描画State設定に使用するGraphicsSystem。
	GraphicsSystem& m_GraphicsSystem;
	// タイトルBGMとワープSEの再生に使用するAudioSystem。
	AudioSystem& m_AudioSystem;

	//========= HUD描画=========
	// 単色Quadを描画するHUDレンダラー。
	HudRenderer m_HudRenderer{};
	// タイトル画面の文字列を描画するHUD文字レンダラー。
	HudTextRenderer m_HudTextRenderer{};
};