#pragma once

// クリア後のリザルトの画像（背景・見出し・タイトルへ戻るボタン）と操作の案内の描画、ボタンのクリックまたはEnterによるタイトルへの遷移予約、開始時の入力・BGM設定を担当する。
// Scene切替とフェード進行はSceneManagerへ任せ、共有Systemは所有しない。プレイの結果の数値は、表示しない。

//========= Framework インクルード=========
#include "Framework/2D/H/HudSpriteRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/Core/H/IScene.h"
#include "Scene/Common/UI/H/UiButton.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// リザルト用の画像・文字の描画Resourceと、タイトルへ戻るボタンを所有するScene。
class ResultScene final: public IScene
{
public:
  //========= 生成関数=========
  // 使用するManagerとSystemへの非所有参照を登録する。Resource生成は行わない。
  // sceneManager: TitleSceneへの遷移予約先。Sceneの使用期間中は有効であること。
  // inputSystem: カーソルとキー入力の取得先とマウスキャプチャ設定先。Sceneより長く生存すること。
  // graphicsSystem: 画像・文字描画の初期化・描画先サイズ取得・Render Pass設定先。Resource終了まで有効であること。
  // audioSystem: BGMの再生先。Sceneの使用期間中は有効であること。
  ResultScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

  //========= Sceneライフサイクル関数=========
  // Resource生成前の初期状態を準備する。このSceneの現行実装では処理を行わない。
  void Initialize() override;
  // 画像・文字の描画を初期化し、画像を読み込み、ボタンの矩形を設定して、成功後にマウスキャプチャ解除とタイトルBGM再生を行う。
  // 画像の読込失敗は、その画像だけを描かず、全体の失敗にはしない。
  // 戻り値: 画像・文字の描画の初期化成功ならtrue、false返却を検出した場合はFinalize後にfalse。
  // 下位処理の例外は捕捉しない。
  bool Init() override;
  // ボタンのクリックまたはEnterのトリガー入力に応じて、TitleSceneへの遷移を予約する。
  // deltaTime: 経過時間。単位は秒。リザルトには時間経過による更新がないため使用しない。
  void Update(float deltaTime) override;
  // UI用Render Passで、背景・見出し・ボタンの画像と、操作の案内の文字を描画する。描画先の幅または高さが0なら何もしない。
  // 描画後はOPAQUE Passへ設定する。呼出前のPassを保存・復元する処理ではない。
  void Draw() override;
  // 所有する画像・文字の描画Resourceを解放する。繰り返し呼出可能。CaptureやBGMの設定は変更しない。
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
  // リザルトの画像の描画Resourceを所有する。Finalizeで明示的に解放する。
  HudSpriteRenderer m_spriteRenderer{};
  // 操作の案内の文字の描画Resourceを所有する。Finalizeで明示的に解放する。
  HudTextRenderer m_hudTextRenderer{};
  // 背景の画像の識別値。読込失敗や未初期化は0で、0の画像は描かない。
  HudTextureHandle m_backgroundTexture{};
  // 見出しの画像の識別値。0なら描かない。
  HudTextureHandle m_logoTexture{};
  // ボタンの画像（通常）の識別値。0なら描かない。
  HudTextureHandle m_buttonTexture{};
  // ボタンの画像（カーソルが乗っている時）の識別値。0なら描かない。
  HudTextureHandle m_buttonHoverTexture{};
  // タイトルへ戻るボタンの当たりとホバーの判定。
  UiButton m_titleButton{};
};
