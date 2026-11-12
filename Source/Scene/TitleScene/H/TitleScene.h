#pragma once

// タイトル文字の描画、Enter入力によるGameSceneへの遷移予約、タイトル開始時の入力・BGM設定を担当する。
// Scene切替とフェード進行はSceneManagerへ任せ、共有Systemは所有しない。

//========= Framework インクルード=========
#include "Framework/2D/H/HudTextRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/H/IScene.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// タイトル用の文字描画Resourceを所有するScene。
class TitleScene final: public IScene
{
public:
	//========= 生成関数=========
	// 使用するManagerとSystemへの非所有参照を登録する。Resource生成は行わない。
	// sceneManager: GameSceneへの遷移予約先。Sceneの使用期間中は有効であること。
	// inputSystem: Enter入力の取得先とマウスキャプチャ設定先。Sceneより長く生存すること。
	// graphicsSystem: 文字描画の初期化・描画先サイズ取得・Render Pass設定先。Resource終了まで有効であること。
	// audioSystem: タイトルBGMの再生先。Sceneの使用期間中は有効であること。
	TitleScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

	//========= Sceneライフサイクル関数=========
	// Resource生成前の初期状態を準備する。このSceneの現行実装では処理を行わない。
	void Initialize() override;
	// 文字描画を初期化し、成功後にマウスキャプチャ解除とタイトルBGM再生を行う。
	// 戻り値: 文字描画の初期化成功ならtrue、false返却を検出した場合はFinalize後にfalse。
	// 下位処理の例外は捕捉しない。
	bool Init() override;
	// Enterのトリガー入力に応じてGameSceneへの遷移を予約する。
	// deltaTime: 経過時間。単位は秒。タイトルには時間経過による更新がないため使用しない。
	void Update(float deltaTime) override;
	// UI用Render Passでタイトル文字を描画する。描画先の幅または高さが0なら何もしない。
	// 文字描画後はOPAQUE Passへ設定する。呼出前のPassを保存・復元する処理ではない。
	void Draw() override;
	// 所有する文字描画Resourceを解放する。繰り返し呼出可能。CaptureやBGMの設定は変更しない。
	void Finalize() override;

private:
	//========= System・Scene参照=========
	// Scene遷移の予約先となる非所有参照。
	SceneManager& m_sceneManager;
	// 入力取得とCapture設定に使用する非所有参照。
	InputSystem& m_inputSystem;
	// 文字描画とRender Pass設定に使用する非所有参照。
	GraphicsSystem& m_graphicsSystem;
	// Init成功後のBGM再生に使用する非所有参照。
	AudioSystem& m_audioSystem;

	//========= Scene所有状態=========
	// タイトル文字の描画Resourceを所有する。Finalizeで明示的に解放する。
	HudTextRenderer m_hudTextRenderer{};
};