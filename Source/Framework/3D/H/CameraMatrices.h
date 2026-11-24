#pragma once

// 3D描画に渡す、視点の変換行列（View行列とProjection行列）を、1つにまとめた型を宣言する。
// 描画関数の引数の数を減らすための値の束で、行列の計算・保持・転置は担当しない。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// View行列とProjection行列の組。描画のたびに、呼出側が、現在の視点から作って渡す。
struct CAMERA_MATRICES
{
  // World空間からView空間への変換行列。描画側は、view*projectionの順で合成する。
  XMMATRIX view{};
  // View空間からClip空間への変換行列。
  XMMATRIX projection{};
};
