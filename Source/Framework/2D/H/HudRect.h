#pragma once

// 画面UIの矩形（位置と大きさ）を、Virtual Canvas基準で1つにまとめた型を宣言する。
// 描画関数の引数の数を減らすための値の束で、実pixelへの変換・範囲の検証は担当しない。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// Canvas基準の矩形。左上の位置と、幅・高さを持つ。単位はCanvas座標で、実Render Targetのpixelではない。
struct HUD_RECT
{
  // 矩形の左上のX座標。右向きを正とする。範囲制限は行わない。
  float x{};
  // 矩形の左上のY座標。下向きを正とする。範囲制限は行わない。
  float y{};
  // 矩形の幅。描画側は、0以下なら何も描かない。
  float width{};
  // 矩形の高さ。描画側は、0以下なら何も描かない。
  float height{};

  // 位置と大きさの2つのXMFLOAT2から、矩形を作る。
  // position: 矩形の左上のCanvas座標。xがX、yがY。
  // size: 矩形の大きさ。xが幅、yが高さ。
  // 戻り値: 同じ値を持つ矩形。値の検証は行わない。
  static HUD_RECT FromPositionAndSize(const XMFLOAT2& position,const XMFLOAT2& size)
  {
    return HUD_RECT{position.x,position.y,size.x,size.y};
  }
};