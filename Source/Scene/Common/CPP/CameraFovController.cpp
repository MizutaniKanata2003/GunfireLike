#include "../H/CameraFovController.h"

// 動作の状態に応じた視野角の目標値の選択と、現在の視野角の補間を実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで、視野角の補間割合を1以下に制限する。

// 視野角の目標値と補間の速さをまとめる。
namespace
{
  //========= 視野角の設定=========
  // 通常時の垂直方向の視野角。単位は度。行列生成時にラジアンへ変換する。
  constexpr float ns_FOV_DEGREES = 60.0f;
  // 構えている時の垂直方向の視野角。単位は度。通常より狭くして、拡大して見える。企画書で未確定の仮値。
  constexpr float ns_AIM_FOV_DEGREES = 45.0f;
  // ダッシュ中の垂直方向の視野角。単位は度。通常より広くして、速さを出す。仮値。
  constexpr float ns_DASH_FOV_DEGREES = 66.0f;
  // スライディング中の垂直方向の視野角。単位は度。ダッシュよりさらに広くして、滑る速さと低さを出し、しゃがみとの違いを分かりやすくする。仮値。
  constexpr float ns_SLIDE_FOV_DEGREES = 78.0f;
  // しゃがみ中の垂直方向の視野角。単位は度。通常より少し狭くして、慎重な状態を示し、スライディングとの違いを分かりやすくする。仮値。
  constexpr float ns_CROUCH_FOV_DEGREES = 56.0f;
  // 視野角が目標値へ近づく速さ。1秒あたりに差の何倍を縮めるかを表し、大きいほど速い。仮値。
  constexpr float ns_FOV_SMOOTHING_RATE = 12.0f;
}

//========= 初期化関数=========
// 現在の視野角を、通常時の値へ戻す。
void CameraFovController::Reset()
{
  m_fovDegrees = ns_FOV_DEGREES;
}

//========= 更新関数=========
// 動作の状態に応じた視野角の目標値へ、滑らかに近づける。
void CameraFovController::Update(float deltaTime,PLAYER_ACTION_STATE actionState,bool isAiming)
{
  // 動作の状態に応じた視野角の目標値へ、滑らかに近づける。スライディングが最も広く、次にダッシュ。構えは狭く、しゃがみは少し狭い。それ以外は通常。
  // 1回の更新で差を超えないよう、近づく割合は1以下に制限する。
  float target_fov_degrees = ns_FOV_DEGREES;
  // スライディング中は、視野角を最も広げる。
  if(actionState == PLAYER_ACTION_STATE::e_SLIDE){
    target_fov_degrees = ns_SLIDE_FOV_DEGREES;
  }
  // ダッシュ中は、視野角を広げる。
  else if(actionState == PLAYER_ACTION_STATE::e_DASH){
    target_fov_degrees = ns_DASH_FOV_DEGREES;
  }
  // 構えている間は、視野角を狭める。しゃがみ中でも、構えを優先する。
  else if(isAiming){
    target_fov_degrees = ns_AIM_FOV_DEGREES;
  }
  // しゃがみ中は、視野角を少し狭める。
  else if(actionState == PLAYER_ACTION_STATE::e_CROUCH){
    target_fov_degrees = ns_CROUCH_FOV_DEGREES;
  }
  const float fov_blend = std::min(1.0f,ns_FOV_SMOOTHING_RATE * deltaTime);
  m_fovDegrees += (target_fov_degrees - m_fovDegrees) * fov_blend;
}
