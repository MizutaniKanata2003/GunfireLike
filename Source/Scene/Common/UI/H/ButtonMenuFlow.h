#pragma once

// ボタン1つのメニュー画面（ButtonMenuView）の、初期化・決定の入力の判定・描画・終了を、1つの流れとしてまとめる。
// 初期化の成功後に行うマウスキャプチャの解除とタイトルBGMの再生と、ボタンのクリックまたはEnterによる決定の判定を担当する。
// Sceneの遷移先・遷移の予約・フェードは担当しない。TitleSceneとResultSceneが、メンバーとして所有して使う。継承は使わない。共有Systemは所有しない。

//========= Scene インクルード=========
#include "Scene/Common/UI/H/ButtonMenuView.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;

// ボタン1つのメニュー画面の、初期化から終了までの流れを担当するクラス。
class ButtonMenuFlow final
{
public:
  //========= 初期化・終了関数=========
  // メニュー画面を初期化し、成功後にだけ、マウスキャプチャの解除とタイトルBGMの再生を行う。
  // graphicsSystem: 画像・文字の描画の初期化に使う描画基盤。Uninitまで有効であること。
  // inputSystem: マウスキャプチャの設定先。参照は保持しない。
  // audioSystem: BGMの再生先。参照は保持しない。タイトルとリザルトは、同じタイトルBGMを使う。
  // config: 画面の構成。値はメニュー画面がコピーして保持する。文字列は、静的な文字列定数であること。
  // 戻り値: メニュー画面の初期化に成功すればtrue。失敗した場合は、キャプチャとBGMを変更せずfalse。失敗した対象のメッセージは、メニュー画面が記録する。
  // 画像の読込失敗は、その画像だけを描かず、全体の失敗にはしない。下位処理の例外は捕捉しない。
  [[nodiscard]] bool Init(GraphicsSystem& graphicsSystem,InputSystem& inputSystem,AudioSystem& audioSystem,const BUTTON_MENU_CONFIG& config);
  // メニュー画面の描画Resourceを解放する。未初期化・終了済みでも、繰り返し呼出可能。CaptureやBGMの設定は変更しない。
  void Uninit();

  //========= 更新・描画関数=========
  // ボタンのホバーを更新し、決定されたかを返す。ボタンのクリック、またはEnterのトリガー入力で、決定とする。
  // inputSystem: カーソルとキー入力の取得元。
  // graphicsSystem: カーソルの座標変換に使う、描画先の大きさの取得元。
  // 戻り値: このフレームで決定されたらtrue。遷移の予約は、呼出側が行う。
  [[nodiscard]] bool Update(const InputSystem& inputSystem,const GraphicsSystem& graphicsSystem);
  // メニュー画面（背景・見出し・ボタンの画像と、操作の案内の文字）を描画する。描画先の幅または高さが0なら何もしない。
  // graphicsSystem: 描画基盤。
  void Draw(GraphicsSystem& graphicsSystem);

private:
  //========= メニュー画面=========
  // 背景・見出し・ボタンの画像と、操作の案内の文字を描く、メニュー画面。Uninitで明示的に解放する。
  ButtonMenuView m_menu{};
};
