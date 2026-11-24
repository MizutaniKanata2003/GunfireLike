#include "../H/ButtonMenuView.h"

// ボタン1つのメニュー画面の、画像・文字の描画の初期化、ボタンの判定、描画、解放を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_LBUTTONで、ボタンのクリックを判定する。

//========= C++標準ライブラリ インクルード=========
#include <string> // std::wstringで、Sceneの名前と失敗の内容を連結して、ログのメッセージを組み立てる。

//========= 外部ライブラリ インクルード=========
#include <DirectXColors.h> // DirectX::Colors::Whiteで、操作の案内の文字の色を指定する。

//========= Framework インクルード=========
#include "Framework/2D/H/HudCanvas.h"
#include "Framework/2D/H/HudRect.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/InitCheck.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/GameScene/H/GameInputHelper.h"

// 操作の案内と色の設定をまとめる。位置は、基準Canvas（幅1280・高さ720）上の値。
namespace
{
  //========= 操作の案内=========
  // 案内の文字の上端の位置。基準Canvas上の座標で、下向きY。ボタンの下端（480）の下へ置く。
  constexpr float ns_GUIDE_POSITION_Y = 520.0f;
  // 案内の文字の拡大率。1でFont元の大きさ。
  constexpr float ns_GUIDE_SCALE = 0.8f;

  //========= 色=========
  // 画像へ乗算しない色。白で、画像本来の色になる。
  const XMFLOAT4 ns_WHITE{1.0f,1.0f,1.0f,1.0f};
}

//========= 初期化・終了関数=========
// 画像・文字の描画を初期化し、配置表の画像を読み込み、ボタンの矩形を設定する。
bool ButtonMenuView::Initialize(GraphicsSystem& graphicsSystem,const BUTTON_MENU_CONFIG& config)
{
  m_config = config;

  // 画像・文字の描画の初期化がfalseを返した場合は、部分Resourceを解放して失敗を返す。最初に失敗した対象のメッセージだけを記録する。
  // メッセージは、Sceneの名前と、失敗した対象を連結して作る。&&の短絡評価により、画像の初期化が失敗した場合は、文字の初期化を実行しない。
  const std::wstring scene_name{config.sceneName};
  if(!InitCheck::Succeeded(m_spriteRenderer.Initialize(graphicsSystem),e_LogCategory::e_SCENE,scene_name + L"の画像描画初期化に失敗しました。")
     || !InitCheck::Succeeded(m_textRenderer.Initialize(graphicsSystem),e_LogCategory::e_SCENE,scene_name + L"の文字描画初期化に失敗しました。")){
    Uninit();
    return false;
  }

  // 配置表の画像を読み込む。識別値が0（読込失敗）のものがあっても、全体の失敗にはせず、その画像だけを描かない。
  m_backgroundTexture = m_spriteRenderer.LoadTexture(graphicsSystem,config.background.texturePath);
  m_logoTexture = m_spriteRenderer.LoadTexture(graphicsSystem,config.logo.texturePath);
  m_buttonTexture = m_spriteRenderer.LoadTexture(graphicsSystem,config.button.texturePath);
  m_buttonHoverTexture = m_spriteRenderer.LoadTexture(graphicsSystem,config.buttonHoverPath);

  // ボタンの矩形を、画像の配置と同じ位置と大きさにする。
  m_button.Initialize(config.button.position,config.button.size);
  return true;
}

// 画像・文字の描画Resourceを解放し、ボタンのホバーを解除する。
void ButtonMenuView::Uninit()
{
  // 画像の識別値は、画像の描画Resourceの解放で無効になるため、0へ戻す。ホバーも解除する。
  m_textRenderer.Uninit();
  m_spriteRenderer.Uninit();
  m_backgroundTexture = 0;
  m_logoTexture = 0;
  m_buttonTexture = 0;
  m_buttonHoverTexture = 0;
  m_button.ClearHover();
}

//========= 更新・描画関数=========
// カーソルの位置から、ボタンのホバーを更新し、クリックされたかを返す。
bool ButtonMenuView::UpdateButton(const InputSystem& inputSystem,const GraphicsSystem& graphicsSystem)
{
  // カーソルを基準Canvas上の座標へ変換し、ボタンのホバーとクリックを判定する。
  const XMFLOAT2 cursor_position = GAME_INPUT_HELPER::ConvertCursorToCanvas(inputSystem,graphicsSystem);
  return m_button.Update(cursor_position,inputSystem.IsKeyTriggered(VK_LBUTTON));
}

// 背景・見出し・ボタンの画像を描き、その上に操作の案内を、横方向の中央へ揃えて描く。
void ButtonMenuView::Draw(GraphicsSystem& graphicsSystem)
{
  // 最小化中など、描画先の大きさが0の状態ではPass設定と描画を行わない。
  if(graphicsSystem.GetRenderWidth() == 0 || graphicsSystem.GetRenderHeight() == 0)return;

  // 画面UI用のPassを設定する。画像は、背景・見出し・ボタンの順に重ねる。ボタンは、カーソルが乗っている間、ホバー用の画像にする。
  // 3つの画像は全て、画像本来の色（白）で、全体を描く。見た目の指定を1つにまとめて使い回す。
  graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);
  const HUD_SPRITE_APPEARANCE white_appearance{ns_WHITE,1.0f};
  m_spriteRenderer.DrawSprite(graphicsSystem,m_backgroundTexture,HUD_RECT::FromPositionAndSize(m_config.background.position,m_config.background.size),
                              white_appearance);
  m_spriteRenderer.DrawSprite(graphicsSystem,m_logoTexture,HUD_RECT::FromPositionAndSize(m_config.logo.position,m_config.logo.size),white_appearance);
  const HudTextureHandle button_texture = m_button.IsHovered() ? m_buttonHoverTexture : m_buttonTexture;
  m_spriteRenderer.DrawSprite(graphicsSystem,button_texture,HUD_RECT::FromPositionAndSize(m_config.button.position,m_config.button.size),white_appearance);

  // 画像の後に、操作の案内を、一つの文字描画Batchとして描く。文字の幅を測って、基準Canvasの横方向の中央へ置く。左端 = (Canvasの幅 - 文字の幅) / 2。
  // MeasureTextはCanvas倍率を含まない基準Canvas上の寸法を返すため、基準Canvasの幅と同じ単位で計算できる。
  m_textRenderer.Begin();
  const XMFLOAT2 guide_size = m_textRenderer.MeasureText(m_config.guideText,ns_GUIDE_SCALE);
  const XMFLOAT2 guide_position{(HudCanvas::VIRTUAL_WIDTH - guide_size.x) * 0.5f,ns_GUIDE_POSITION_Y};
  m_textRenderer.DrawText(m_config.guideText,guide_position,DirectX::Colors::White,ns_GUIDE_SCALE);
  m_textRenderer.End();

  // 呼出前のPassへ戻す方式には変えず、既存どおりOPAQUEを明示的に設定する。
  graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);
}