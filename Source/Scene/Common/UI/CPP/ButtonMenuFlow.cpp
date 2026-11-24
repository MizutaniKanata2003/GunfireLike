#include "../H/ButtonMenuFlow.h"

// ボタン1つのメニュー画面の、初期化後の入力・BGMの設定と、決定の入力の判定、描画の呼出し、終了を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_RETURNで、決定の入力（Enter）を判定する。

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/Input/H/InputSystem.h"

//========= 初期化・終了関数=========
// メニュー画面の準備ができた場合だけ、メニュー画面用の共有System設定へ切り替える。
bool ButtonMenuFlow::Init(GraphicsSystem& graphicsSystem,InputSystem& inputSystem,AudioSystem& audioSystem,const BUTTON_MENU_CONFIG& config)
{
  // メニュー画面の初期化がfalseを返した場合は、部分Resourceの解放を含めて、メニュー画面が処理済み。管理側へ失敗を返す。
  if(!m_menu.Initialize(graphicsSystem,config))return false;

  // Resource生成の成功後にだけCaptureとBGMを変更し、false返却時にはこの副作用を発生させない。専用のBGMはないため、タイトルのBGMを使う。
  inputSystem.SetMouseCaptureEnabled(false);
  audioSystem.PlayTitleBgm();
  return true;
}

// メニュー画面の描画Resourceだけを終了する。
void ButtonMenuFlow::Uninit()
{
  // 遷移先が設定したCaptureやBGMを上書きしないよう、共有Systemの設定は変更しない。
  m_menu.Uninit();
}

//========= 更新・描画関数=========
// ボタンのクリック、またはEnterが押されたフレームで、決定とする。
bool ButtonMenuFlow::Update(const InputSystem& inputSystem,const GraphicsSystem& graphicsSystem)
{
  // ボタンのホバーとクリックを判定する。Enterの判定より先に行い、ホバーを毎フレーム更新する。
  const bool is_button_clicked = m_menu.UpdateButton(inputSystem,graphicsSystem);

  // ボタンのクリック、またはEnterが押されたフレームで、決定を返す。遷移の予約と、フェードの進行は、呼出側のSceneが行う。
  return is_button_clicked || inputSystem.IsKeyTriggered(VK_RETURN);
}

// メニュー画面の描画を呼び出す。
void ButtonMenuFlow::Draw(GraphicsSystem& graphicsSystem)
{
  m_menu.Draw(graphicsSystem);
}
