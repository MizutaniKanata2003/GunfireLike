#include "../H/GameHudScreenEffect.h"

// 画面全体に重ねる画像（暗幕・低HP警告・速度線・被弾のフラッシュ）の読込と描画を実装する。画像の配置は表から読み、見た目の色と、濃さの変化だけをこのファイルで決める。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampで濃さを0〜1へ制限し、std::minで1回に進める時間を上限までに抑える。

//========= Scene インクルード=========
#include "Scene/Common/UI/H/UiLayout.h"
#include "Scene/GameScene/H/GameHudStyle.h"

// 画面全体の画像の見た目に使う固定設定をまとめる。企画書で未確定の見た目の数値は、仮値。
namespace
{
  //========= 時間の設定=========
  // 1回の描画で進める実時間の上限。単位は秒。処理が長く止まった後に、濃さが一気に変わるのを防ぐ。
  constexpr float ns_MAX_DELTA_SECONDS = 0.1f;

  //========= 速度線の設定=========
  // 速度線の最大の濃さ。画像の透明度へ掛ける。画像は、縁で濃くなるため、画面を覆いすぎないよう、下げて描く。仮値。
  constexpr float ns_SLIDE_LINES_ALPHA = 0.6f;
  // 速度線が、見えない状態から最も濃くなるまでの時間。単位は秒。短すぎると、一瞬で出たように見える。仮値。
  constexpr float ns_SLIDE_LINES_FADE_IN_SECONDS = 0.2f;
  // 速度線が、最も濃い状態から見えなくなるまでの時間。単位は秒。出る時より、ゆっくり消す。仮値。
  constexpr float ns_SLIDE_LINES_FADE_OUT_SECONDS = 0.4f;

  //========= 被弾のフラッシュの設定=========
  // 被弾のフラッシュの最大の濃さ。画像の透明度へ掛ける。仮値。
  constexpr float ns_HIT_FLASH_ALPHA = 0.8f;
  // 被弾のフラッシュが、最も濃い状態から見えなくなるまでの時間。単位は秒。仮値。
  constexpr float ns_HIT_FLASH_SECONDS = 0.35f;
  // HPが減ったと見なす、最小の差。浮動小数の誤差による、誤った開始を防ぐ。
  constexpr float ns_HP_DECREASE_EPSILON = 0.001f;
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
  m_hitFlashTexture = spriteRenderer.LoadTexture(graphicsSystem,HUD_LAYOUT::ns_HIT_FLASH.texturePath);
}

// 借用した参照と、画像の識別値と、速度線・フラッシュの状態を手放す。
void GameHudScreenEffect::Uninit()
{
  // 画像の識別値は、画像用Rendererの解放で無効になるため、0へ戻す。画像用Rendererは、所有しないため、解放しない。
  m_spriteRenderer = nullptr;
  m_backdropTexture = 0;
  m_lowHpWarningTexture = 0;
  m_slideLinesTexture = 0;
  m_hitFlashTexture = 0;

  // 時間・濃さ・前回のHPを、初期状態へ戻す。
  m_hasLastMeasureTime = false;
  m_slideLinesRatio = 0.0f;
  m_hitFlashRatio = 0.0f;
  m_previousHP = 0.0f;
  m_hasPreviousHP = false;
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

// 速度線・被弾のフラッシュ・低HP警告を、この順に描く。
void GameHudScreenEffect::DrawOverlays(GraphicsSystem& graphicsSystem,bool isSliding,bool isLowHealth,float currentHP)
{
  // 画像用Rendererを借用していない場合は、描画しない。濃さも進めない。
  if(m_spriteRenderer == nullptr)return;

  // 前回からの実時間で、速度線とフラッシュの濃さを進めて、描く。
  const float delta_seconds = MeasureDeltaSeconds();
  DrawSlideLines(graphicsSystem,isSliding,delta_seconds);
  DrawHitFlash(graphicsSystem,currentHP,delta_seconds);

  // 低HPの場合だけ、低HP警告を描く。最後に描き、警告が他の演出の上に重なるようにする。
  if(isLowHealth){
    const HUD_SPRITE_APPEARANCE white_appearance{GAME_HUD_STYLE::ns_WHITE,1.0f};
    m_spriteRenderer->DrawSprite(graphicsSystem,m_lowHpWarningTexture,
                                 HUD_RECT::FromPositionAndSize(HUD_LAYOUT::ns_LOW_HP_WARNING.position,HUD_LAYOUT::ns_LOW_HP_WARNING.size),white_appearance);
  }
}

//========= 描画の補助関数=========
// 前回の呼出しからの実時間を測る。
float GameHudScreenEffect::MeasureDeltaSeconds()
{
  // 初回は、前回の時刻がないため、0とする。長く止まった後は、上限までに抑える。
  const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  float delta_seconds = 0.0f;
  if(m_hasLastMeasureTime){
    delta_seconds = std::min(std::chrono::duration<float>(now - m_lastMeasureTime).count(),ns_MAX_DELTA_SECONDS);
  }
  m_lastMeasureTime = now;
  m_hasLastMeasureTime = true;
  return delta_seconds;
}

// 速度線の濃さを進めて、見える間だけ描く。
void GameHudScreenEffect::DrawSlideLines(GraphicsSystem& graphicsSystem,bool isSliding,float deltaSeconds)
{
  // スライディング中は、出る時間で上げ、そうでなければ、消える時間で下げる。割合は、0〜1へ制限する。
  const float change_per_second = isSliding ? 1.0f / ns_SLIDE_LINES_FADE_IN_SECONDS : -1.0f / ns_SLIDE_LINES_FADE_OUT_SECONDS;
  m_slideLinesRatio = std::clamp(m_slideLinesRatio + change_per_second * deltaSeconds,0.0f,1.0f);
  if(m_slideLinesRatio <= 0.0f)return;

  // 濃さの割合を、滑らかな曲線（0付近と1付近でゆっくり変わる）へ直し、最大の濃さへ掛ける。色のAlphaだけを下げ、色味は画像のまま。
  const float smooth_ratio = m_slideLinesRatio * m_slideLinesRatio * (3.0f - 2.0f * m_slideLinesRatio);
  const HUD_SPRITE_APPEARANCE appearance{XMFLOAT4{GAME_HUD_STYLE::ns_WHITE.x,GAME_HUD_STYLE::ns_WHITE.y,GAME_HUD_STYLE::ns_WHITE.z,
    ns_SLIDE_LINES_ALPHA * smooth_ratio},1.0f};
  m_spriteRenderer->DrawSprite(graphicsSystem,m_slideLinesTexture,
                               HUD_RECT::FromPositionAndSize(HUD_LAYOUT::ns_SLIDE_LINES.position,HUD_LAYOUT::ns_SLIDE_LINES.size),appearance);
}

// HPが減ったらフラッシュを始め、濃さを下げて、見える間だけ描く。
void GameHudScreenEffect::DrawHitFlash(GraphicsSystem& graphicsSystem,float currentHP,float deltaSeconds)
{
  // 前回のHPがある場合だけ、減ったかを調べる。減った瞬間に、濃さを最大へ戻す。最初の呼出しと、回復・やり直し（HPが増える場合）では、始めない。
  if(m_hasPreviousHP && currentHP < m_previousHP - ns_HP_DECREASE_EPSILON){
    m_hitFlashRatio = 1.0f;
  }
  m_previousHP = currentHP;
  m_hasPreviousHP = true;

  // 時間とともに、濃さを0へ下げる。0以下になったら、描かない。
  m_hitFlashRatio = std::max(m_hitFlashRatio - deltaSeconds / ns_HIT_FLASH_SECONDS,0.0f);
  if(m_hitFlashRatio <= 0.0f)return;

  // 濃さの割合を2乗して、始めに強く、後半はゆっくり消える形にし、最大の濃さへ掛ける。色のAlphaだけを下げ、色味は画像のまま。
  const HUD_SPRITE_APPEARANCE appearance{XMFLOAT4{GAME_HUD_STYLE::ns_WHITE.x,GAME_HUD_STYLE::ns_WHITE.y,GAME_HUD_STYLE::ns_WHITE.z,
    ns_HIT_FLASH_ALPHA * m_hitFlashRatio * m_hitFlashRatio},1.0f};
  m_spriteRenderer->DrawSprite(graphicsSystem,m_hitFlashTexture,
                               HUD_RECT::FromPositionAndSize(HUD_LAYOUT::ns_HIT_FLASH.position,HUD_LAYOUT::ns_HIT_FLASH.size),appearance);
}