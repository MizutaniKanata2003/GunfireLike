#pragma once

// GameHudの複数のcppで共有する、文字を置く領域の型と、複数の画面で共通の色をまとめる。
// 描画の処理は持たない。色は、企画書で未確定の見た目の仮値。

//========= Framework インクルード=========
#include "Framework/2D/H/HudTextRenderer.h" // XMFLOAT2・XMFLOAT4・XMVECTORF32を使用する。

// 文字を置く領域。DrawFittedTextへ、領域の位置・大きさ・文字の高さの目標を、まとめて渡すために使う。
struct HUD_TEXT_AREA
{
  //========= 領域=========
  // 領域の左上の位置。基準Canvas上の座標。ShiftTextAreaで、カードや行の左上を基準とした距離として使う場合もある。
  XMFLOAT2 position{};
  // 領域の幅と高さ。基準Canvas上の大きさ。文字は、この領域の中央へ置く。
  XMFLOAT2 size{};

  //========= 文字=========
  // 文字の高さの目標。基準Canvas上の大きさ。領域の幅に収まらない場合は、幅に合わせて小さくする。
  float targetHeight{};
};

// 複数の画面（HUD・報酬選択・ショップ）で共通に使う色と、領域の補助関数をまとめる。
namespace GAME_HUD_STYLE
{
  //========= 色=========
  // 画像に乗算しない色。白で、画像本来の色になる。
  inline const XMFLOAT4 ns_WHITE{1.0f,1.0f,1.0f,1.0f};
  // 通貨の数字と、ショップの予備弾薬・強化の対象の武器名の文字の色。金色。通貨のアイコンの色に合わせる。
  inline const XMVECTORF32 ns_CURRENCY_TEXT_COLOR{{{1.0f,0.85f,0.3f,1.0f}}};
  // 報酬選択・ショップの見出しと、カードの見出しの文字の色。金色。
  inline const XMVECTORF32 ns_TITLE_TEXT_COLOR{{{1.0f,0.85f,0.3f,1.0f}}};
  // 報酬選択・ショップの説明・取得数・操作の案内の文字の色。白に近い色。
  inline const XMVECTORF32 ns_BODY_TEXT_COLOR{{{0.92f,0.92f,0.92f,1.0f}}};

  //========= 補助関数=========
  // 領域の位置へ、基準の位置を加えた領域を返す。カードや行の左上を基準に、領域を置くために使う。大きさと文字の高さは変えない。
  // area: 元の領域。位置は、基準の位置からの距離として扱う。
  // origin: 加える基準の位置。基準Canvas上の座標。
  // 戻り値: 位置だけを移した領域。
  inline HUD_TEXT_AREA ShiftTextArea(const HUD_TEXT_AREA& area,const XMFLOAT2& origin)
  {
    return HUD_TEXT_AREA{XMFLOAT2{area.position.x + origin.x,area.position.y + origin.y},area.size,area.targetHeight};
  }
}
