#pragma once

// タイトルの画像（背景・ロゴ・開始ボタン）と操作の案内の描画、開始ボタンのクリックまたはEnterによるGameSceneへの遷移予約、タイトル開始時の入力・BGM設定を担当する。
// 画像・文字の描画Resource・ボタンの判定・開始時の入力とBGMの設定・決定の判定は、ButtonMenuFlowが担当する。Scene切替とフェード進行はSceneManagerへ任せ、共有Systemは所有しない。

//========= Scene インクルード=========
#include "Scene/Common/Core/H/IScene.h"
#include "Scene/Common/UI/H/ButtonMenuFlow.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// IScene実装の骨組み（4つの前方宣言・同じ4つの参照を受ける生成関数・5つのoverride宣言）は、TitleScene・ResultScene・GameSceneで同じ形になり、重複として検出されるが、例外とする（規約02の13.8）。
// 理由は、overrideは各クラスが宣言する必要があること（C++の規則）と、生成関数が受ける4つの参照は、各Sceneの依存の契約であること。
// 共通の基底や部品へまとめる案は、所有を優先する13.4に反し、Sceneの生成・遷移の予約を行う呼出し側の変更も要るため、採用しない。
// タイトルのSceneとして、ボタン1つのメニュー画面の流れ（ButtonMenuFlow）の呼出しと、開始の入力に応じた遷移の予約を行うクラス。
class TitleScene final: public IScene
{
public:
	//========= 生成関数=========
	// 使用するManagerとSystemへの非所有参照を登録する。Resource生成は行わない。
	// sceneManager: GameSceneへの遷移予約先。Sceneの使用期間中は有効であること。
	// inputSystem: カーソルとキー入力の取得先とマウスキャプチャ設定先。Sceneより長く生存すること。
	// graphicsSystem: 画像・文字描画の初期化・描画先サイズ取得・Render Pass設定先。Resource終了まで有効であること。
	// audioSystem: タイトルBGMの再生先。Sceneの使用期間中は有効であること。
	TitleScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

	//========= Sceneライフサイクル関数=========
	// Resource生成前の初期状態を準備する。このSceneの現行実装では処理を行わない。
	void Initialize() override;
	// メニュー画面を初期化し、成功後にマウスキャプチャ解除とタイトルBGM再生を行う。
	// 画像の読込失敗は、その画像だけを描かず、全体の失敗にはしない。
	// 戻り値: メニュー画面の初期化成功ならtrue、失敗した場合はFinalize後にfalse。
	// 下位処理の例外は捕捉しない。
	bool Init() override;
	// 開始ボタンのクリックまたはEnterのトリガー入力に応じて、GameSceneへの遷移を予約する。
	// deltaTime: 経過時間。単位は秒。タイトルには時間経過による更新がないため使用しない。
	void Update(float deltaTime) override;
	// メニュー画面（背景・ロゴ・開始ボタンの画像と、操作の案内の文字）を描画する。描画先の幅または高さが0なら何もしない。
	void Draw() override;
	// メニュー画面の描画Resourceを解放する。繰り返し呼出可能。CaptureやBGMの設定は変更しない。
	void Finalize() override;

private:
	//========= System・Scene参照=========
	// Scene遷移の予約先となる非所有参照。
	SceneManager& m_sceneManager;
	// 入力取得とCapture設定に使用する非所有参照。
	InputSystem& m_inputSystem;
	// 画像・文字の描画とRender Pass設定に使用する非所有参照。
	GraphicsSystem& m_graphicsSystem;
	// Init成功後のBGM再生に使用する非所有参照。
	AudioSystem& m_audioSystem;

	//========= Scene所有状態=========
	// 背景・ロゴ・開始ボタンの画像と、操作の案内の文字を描く、メニュー画面の流れ。Finalizeで明示的に解放する。
	ButtonMenuFlow m_menuFlow{};
};