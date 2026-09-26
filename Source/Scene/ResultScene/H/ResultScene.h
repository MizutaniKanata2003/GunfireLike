#pragma once

//========= Framework インクルード=========
#include "Framework/2D/H/HudRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/H/IScene.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// ゲームクリア後の最終結果、BGM、タイトル画面への遷移を管理する。
class ResultScene final : public IScene
{
public:
	//========= 生成関数=========
	// ResultSceneが使用するSceneManagerとFramework Systemを登録する。
	ResultScene( SceneManager& sceneManager, InputSystem& inputSystem, GraphicsSystem& graphicsSystem, AudioSystem& audioSystem );

	//========= Sceneライフサイクル関数=========
	// Result画面の入力設定、HUD、文字、BGMを初期化する。
	void Initialize() override;
	// Asset読込などの深い初期化を行う。
	bool Init() override
	{
		return true;
	}
	// Result画面の入力を更新する。
	void Update( float deltaTime ) override;
	// 最終結果、背景、タイトルへ戻るボタンを描画する。
	void Draw() override;
	// Result画面で使用したHUDと文字描画リソースを終了する。
	void Finalize() override;

private:
	//========= 補助関数=========
	// タイトル画面へのScene遷移を予約する。
	void ReturnToTitle();

	//========= Framework・Scene参照=========
	// Scene遷移とゲーム進捗取得に使用するSceneManager。
	SceneManager& m_SceneManager;
	// Result画面のキーボード入力に使用するInputSystem。
	InputSystem& m_InputSystem;
	// HUDの描画State設定に使用するGraphicsSystem。
	GraphicsSystem& m_GraphicsSystem;
	// Result BGMとワープSEの再生に使用するAudioSystem。
	AudioSystem& m_AudioSystem;

	//========= HUD描画=========
	// 単色Quadを描画するHUDレンダラー。
	HudRenderer m_HudRenderer{};
	// Result画面の文字列を描画するHUD文字レンダラー。
	HudTextRenderer m_HudTextRenderer{};
};