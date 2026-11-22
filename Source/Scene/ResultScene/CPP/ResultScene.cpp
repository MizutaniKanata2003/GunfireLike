#include "../H/ResultScene.h"

// リザルト用Resourceの初期化、背景・見出し・ボタンの画像と操作の案内の描画、ボタンのクリックまたはEnterによるTitleSceneへの遷移予約と終了を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_RETURNでタイトルへ戻る入力、VK_LBUTTONでボタンのクリックを判定する。

//========= 外部ライブラリ インクルード=========
#include <DirectXColors.h>

//========= Framework インクルード=========
#include "Framework/2D/H/HudCanvas.h"
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/InitCheck.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Core/H/SceneManager.h"
#include "Scene/Common/UI/H/UiLayout.h"
#include "Scene/GameScene/H/GameInputHelper.h"
#include "Scene/TitleScene/H/TitleScene.h"

// リザルトに描く操作の案内の設定をまとめる。位置は、基準Canvas（幅1280・高さ720）上の値。
namespace
{
  //========= 操作の案内=========
  // ボタンの下へ描く、操作の案内の文字列。静的な文字列定数を指し、所有しない。
  constexpr const wchar_t* ns_GUIDE_TEXT = L"CLICK OR ENTER: BACK TO TITLE";
  // 案内の文字の上端の位置。基準Canvas上の座標で、下向きY。ボタンの下端（480）の下へ置く。
  constexpr float ns_GUIDE_POSITION_Y = 520.0f;
  // 案内の文字の拡大率。1でFont元の大きさ。
  constexpr float ns_GUIDE_SCALE = 0.8f;

  //========= 色=========
  // 画像へ乗算しない色。白で、画像本来の色になる。
  const XMFLOAT4 ns_WHITE{1.0f,1.0f,1.0f,1.0f};
}

//========= 生成関数=========
// 共有SystemとManagerの所有権を受け取らず、各操作に使う参照だけを登録する。
ResultScene::ResultScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem)
  : m_sceneManager(sceneManager),m_inputSystem(inputSystem),m_graphicsSystem(graphicsSystem),m_audioSystem(audioSystem)
{}

//========= Sceneライフサイクル関数=========
// Resource生成前に初期化すべき状態がないため、現行実装では何もしない。
void ResultScene::Initialize()
{
  // 初期化処理の共通呼出順を維持し、Resource生成はInitで行う。
}

// 画像と文字の描画の準備ができた場合だけ、リザルト用の共有System設定へ切り替える。
bool ResultScene::Init()
{
  // 画像・文字の描画の初期化がfalseを返した場合は、部分Resourceを解放して管理側へ失敗を返す。最初に失敗した対象のメッセージだけを記録する。
  if(!InitCheck::Succeeded(m_spriteRenderer.Initialize(m_graphicsSystem),e_LogCategory::e_SCENE,L"ResultSceneの画像描画初期化に失敗しました。")
     || !InitCheck::Succeeded(m_hudTextRenderer.Initialize(m_graphicsSystem),e_LogCategory::e_SCENE,L"ResultSceneの文字描画初期化に失敗しました。")){
    Finalize();
    return false;
  }

  // 配置表の画像を読み込む。識別値が0（読込失敗）のものがあっても、全体の失敗にはせず、その画像だけを描かない。背景は、タイトルと同じ画像を使う。
  m_backgroundTexture = m_spriteRenderer.LoadTexture(m_graphicsSystem,TITLE_LAYOUT::ns_BACKGROUND.texturePath);
  m_logoTexture = m_spriteRenderer.LoadTexture(m_graphicsSystem,RESULT_LAYOUT::ns_LOGO.texturePath);
  m_buttonTexture = m_spriteRenderer.LoadTexture(m_graphicsSystem,RESULT_LAYOUT::ns_TITLE_BUTTON.texturePath);
  m_buttonHoverTexture = m_spriteRenderer.LoadTexture(m_graphicsSystem,RESULT_LAYOUT::ns_TITLE_BUTTON_HOVER_PATH);

  // ボタンの矩形を、画像の配置と同じ位置と大きさにする。
  m_titleButton.Initialize(RESULT_LAYOUT::ns_TITLE_BUTTON.position,RESULT_LAYOUT::ns_TITLE_BUTTON.size);

  // Resource生成の成功後にだけCaptureとBGMを変更し、false返却時にはこの副作用を発生させない。リザルト専用のBGMはないため、タイトルのBGMを使う。
  m_inputSystem.SetMouseCaptureEnabled(false);
  m_audioSystem.PlayTitleBgm();
  return true;
}

// 入力による遷移予約だけを行い、その場でSceneを切り替えない。
void ResultScene::Update(float deltaTime)
{
  // リザルトに時間依存の更新はないため、共通インターフェースの引数を意図的に未使用とする。
  static_cast<void>(deltaTime);

  // カーソルを基準Canvas上の座標へ変換し、ボタンのホバーとクリックを判定する。
  const XMFLOAT2 cursor_position = GAME_INPUT_HELPER::ConvertCursorToCanvas(m_inputSystem,m_graphicsSystem);
  const bool is_button_clicked = m_titleButton.Update(cursor_position,m_inputSystem.IsKeyTriggered(VK_LBUTTON));

  // ボタンのクリック、またはEnterが押されたフレームで遷移を予約し、フェードと実際の切替はSceneManagerへ任せる。
  if(is_button_clicked || m_inputSystem.IsKeyTriggered(VK_RETURN)){
    m_sceneManager.RequestSceneChange<TitleScene>();
  }
}

// 描画先がある場合だけ、背景・見出し・ボタンの画像を描き、その上に操作の案内を、横方向の中央へ揃えて描画する。
void ResultScene::Draw()
{
  // 最小化中など、描画先の大きさが0の状態ではPass設定と描画を行わない。
  if(m_graphicsSystem.GetRenderWidth() == 0 || m_graphicsSystem.GetRenderHeight() == 0)return;

  // 画面UI用のPassを設定する。画像は、背景・見出し・ボタンの順に重ねる。ボタンは、カーソルが乗っている間、ホバー用の画像にする。
  m_graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);
  m_spriteRenderer.DrawSprite(m_graphicsSystem,m_backgroundTexture,TITLE_LAYOUT::ns_BACKGROUND.position,TITLE_LAYOUT::ns_BACKGROUND.size,ns_WHITE,1.0f);
  m_spriteRenderer.DrawSprite(m_graphicsSystem,m_logoTexture,RESULT_LAYOUT::ns_LOGO.position,RESULT_LAYOUT::ns_LOGO.size,ns_WHITE,1.0f);
  const HudTextureHandle button_texture = m_titleButton.IsHovered() ? m_buttonHoverTexture : m_buttonTexture;
  m_spriteRenderer.DrawSprite(m_graphicsSystem,button_texture,RESULT_LAYOUT::ns_TITLE_BUTTON.position,RESULT_LAYOUT::ns_TITLE_BUTTON.size,ns_WHITE,1.0f);

  // 画像の後に、操作の案内を、一つの文字描画Batchとして描く。文字の幅を測って、基準Canvasの横方向の中央へ置く。左端 = (Canvasの幅 - 文字の幅) / 2。
  m_hudTextRenderer.Begin();
  const XMFLOAT2 guide_size = m_hudTextRenderer.MeasureText(ns_GUIDE_TEXT,ns_GUIDE_SCALE);
  const XMFLOAT2 guide_position{(HudCanvas::VIRTUAL_WIDTH - guide_size.x) * 0.5f,ns_GUIDE_POSITION_Y};
  m_hudTextRenderer.DrawText(ns_GUIDE_TEXT,guide_position,DirectX::Colors::White,ns_GUIDE_SCALE);
  m_hudTextRenderer.End();

  // 呼出前のPassへ戻す方式には変えず、既存どおりOPAQUEを明示的に設定する。
  m_graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);
}

// Sceneが所有する画像・文字の描画Resourceだけを終了する。
void ResultScene::Finalize()
{
  // 遷移先が設定したCaptureやBGMを上書きしないよう、共有Systemの設定は変更しない。
  // 画像の識別値は、画像の描画Resourceの解放で無効になるため、0へ戻す。ホバーも解除する。
  m_hudTextRenderer.Uninit();
  m_spriteRenderer.Uninit();
  m_backgroundTexture = 0;
  m_logoTexture = 0;
  m_buttonTexture = 0;
  m_buttonHoverTexture = 0;
  m_titleButton.ClearHover();
}
