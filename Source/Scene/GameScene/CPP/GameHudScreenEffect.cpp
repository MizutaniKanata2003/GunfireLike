#include "../H/GameHudScreenEffect.h"

// 画面全体に重ねる画像（暗幕・低HP警告・速度線）の読込と描画を実装する。画像の配置は表から読み、見た目の色だけをこのファイルで決める。

//========= Scene インクルード=========
#include "Scene/Common/UI/H/UiLayout.h"
#include "Scene/GameScene/H/GameHudStyle.h"

// 画面全体の画像の見た目に使う固定設定をまとめる。企画書で未確定の見た目の数値は、仮値。
namespace
{
  //========= 速度線の設定=========
  // 速度線の濃さ。画像の透明度へ掛ける。画像は、縁で濃くなるため、画面を覆いすぎないよう、下げて描く。仮値。
  constexpr float ns_SLIDE_LINES_ALPHA = 0.6f;
}

//========= 初期化・終了関数=========
// 画像用Rendererを借用し、画面全体の画像を全て読み込む。
void GameHudScreenEffect::Initialize(GraphicsSystem& graphicsSystem,HudSpriteRenderer& spriteRenderer)
{
  // 画像用Rendererを借用し、配置表の画像を順に読み込む。識別値が0（読込失敗）のものがあっても、全体の失敗にはせず、その画像だけを描かない。
  m_spriteRenderer = &spriteRenderer;
  m_backdropTexture = spriteRenderer.LoadTexture(graphicsSystem,REWARD_LAYOUT::ns_BACKDROP.texturePath);
  m_lowHpWarningTexture = spriteRenderer.LoadTexture(graphicsSystem,HUD_LAYOUT::ns_LOW_HP_WARNING.texturePath);
  m_slideLinesTexture = spriteRenderer.LoadTexture(graphicsSystem,HUD_LAYOUT::ns_SLIDE_LINES.texturePath);
}

// 借用した参照と、画像の識別値を手放す。
void GameHudScreenEffect::Uninit()
{
  // 画像の識別値は、画像用Rendererの解放で無効になるため、0へ戻す。画像用Rendererは、所有しないため、解放しない。
  m_spriteRenderer = nullptr;
  m_backdropTexture = 0;
  m_lowHpWarningTexture = 0;
  m_slideLinesTexture = 0;
}

//========= 描画関数=========
// 画面全体を暗くする背景を描く。
void GameHudScreenEffect::DrawBackdrop(GraphicsSystem& graphicsSystem)
{
  // 画像用Rendererを借用していない場合は、描画しない。
  if(m_spriteRenderer == nullptr)return;

  // 画像本来の色（白）で、画面全体へ描く。
  const HUD_SPRITE_APPEARANCE white_appearance{GAME_HUD_STYLE::ns_WHITE,1.0f};
  m_spriteRenderer->DrawSprite(graphicsSystem,m_backdropTexture,
                               HUD_RECT::FromPositionAndSize(REWARD_LAYOUT::ns_BACKDROP.position,REWARD_LAYOUT::ns_BACKDROP.size),white_appearance);
}

// 速度線と低HP警告を、この順に描く。
void GameHudScreenEffect::DrawOverlays(GraphicsSystem& graphicsSystem,bool isSliding,bool isLowHealth)
{
  // 画像用Rendererを借用していない場合は、描画しない。
  if(m_spriteRenderer == nullptr)return;

  // スライディング中だけ、速度線を、表の濃さで描く。色のAlphaだけを下げ、色味は画像のまま。
  if(isSliding){
    const HUD_SPRITE_APPEARANCE slide_appearance{XMFLOAT4{GAME_HUD_STYLE::ns_WHITE.x,GAME_HUD_STYLE::ns_WHITE.y,GAME_HUD_STYLE::ns_WHITE.z,ns_SLIDE_LINES_ALPHA},1.0f};
    m_spriteRenderer->DrawSprite(graphicsSystem,m_slideLinesTexture,
                                 HUD_RECT::FromPositionAndSize(HUD_LAYOUT::ns_SLIDE_LINES.position,HUD_LAYOUT::ns_SLIDE_LINES.size),slide_appearance);
  }

  // 低HPの場合だけ、低HP警告を描く。速度線の後に描き、警告が速度線の上に重なるようにする。
  if(isLowHealth){
    const HUD_SPRITE_APPEARANCE white_appearance{GAME_HUD_STYLE::ns_WHITE,1.0f};
    m_spriteRenderer->DrawSprite(graphicsSystem,m_lowHpWarningTexture,
                                 HUD_RECT::FromPositionAndSize(HUD_LAYOUT::ns_LOW_HP_WARNING.position,HUD_LAYOUT::ns_LOW_HP_WARNING.size),white_appearance);
  }
}
