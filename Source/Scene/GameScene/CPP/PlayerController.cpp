#include "../H/PlayerController.h"

// 構え・移動入力の反映と、地形のAABBからの押し戻しを実装する。移動方向は視線を水平面へ投影して求める。
// 移動を小さな距離に分けて押し戻すことで、薄い壁を1回の移動で通り抜けることを防ぐ。

//========= Windows インクルード=========
#include <windows.h> // VK_RBUTTONで構え入力を判定する。

//========= C++標準ライブラリ インクルード=========
#include <cmath> // std::sqrtで水平視線と合成移動方向の長さを求めて正規化し、std::ceilで移動の分割数を求める。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionResolver.h"
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Input/H/FpsCamera.h"
#include "Framework/Input/H/InputSystem.h"

// 移動の速度と、押し戻しに使う体の大きさをまとめる。
namespace
{
  //========= 体の大きさ=========
  // 壁との押し戻しに使う球の半径。単位はワールド距離単位。
  // 内側の面がX=±9.9・Z=-1.9・Z=17.9の壁に対し、中心の移動範囲がX=±9.5・Z=-1.5〜17.5（旧DebugPlayerの範囲）と一致する値。
  constexpr float ns_BODY_RADIUS = 0.4f;

  //========= 移動速度=========
  // 通常時の水平移動速度。単位はワールド距離単位/秒。旧DebugPlayerの設定と同じ。
  constexpr float ns_MOVE_SPEED = 6.0f;
  // 構え中の移動速度の倍率。企画書では「構え射撃は移動速度が低下する」とだけ定められており、具体値は未確定の仮値。
  constexpr float ns_AIM_SPEED_MULTIPLIER = 0.5f;

  //========= 移動の分割=========
  // 1回の押し戻しの前に進める最大距離。単位はワールド距離単位。
  // 壁の厚み（0.2）と体の半径（0.4）に対し、中心が壁の反対側へ抜けないよう、厚み以下にする。
  constexpr float ns_MAX_STEP_DISTANCE = 0.2f;
}

//========= 初期化・更新関数=========
// 開始位置を設定し、構えを解除する。
void PlayerController::Initialize(const XMFLOAT3& startPosition)
{
  m_position = startPosition;
  m_isAiming = false;
}

// 構えと移動の入力を反映し、分割した移動ごとに押し戻す。
void PlayerController::Update(float deltaTime,const InputSystem& input,const FpsCamera& camera,const CollisionWorld& collisionWorld)
{
  // 右クリックを押している間だけ構える。
  m_isAiming = input.IsKeyPressed(VK_RBUTTON);

  // 上下を向いても移動を水平に保つため、前方向のY成分を除き、水平成分の長さで割って単位化する。
  XMFLOAT3 forward = camera.GetForward();
  forward.y = 0.0f;
  const float forward_length = std::sqrt(forward.x * forward.x + forward.z * forward.z);
  // 長さが正の場合だけ単位化する。真上・真下を向いて水平成分が0の場合は、割らずに0のまま使う。
  if(forward_length > 0.0f){
    forward.x /= forward_length;
    forward.z /= forward_length;
  }

  // 左右移動に使うカメラの右方向を取得する。X・Z成分だけを使う。
  const XMFLOAT3 right = camera.GetRight();

  // 4つのキーは同時押しを合成するため、else で結ばず独立して判定する。初期値0は移動入力なしを表す。
  float move_x{};
  float move_z{};
  // Wを押している間は、水平化した前方向を加える。
  if(input.IsKeyPressed('W')){
    move_x += forward.x;
    move_z += forward.z;
  }
  // Sを押している間は、前方向を引いて後退する。Wと同時なら相殺する。
  if(input.IsKeyPressed('S')){
    move_x -= forward.x;
    move_z -= forward.z;
  }
  // Dを押している間は、右方向を加える。
  if(input.IsKeyPressed('D')){
    move_x += right.x;
    move_z += right.z;
  }
  // Aを押している間は、右方向を引いて左へ進む。Dと同時なら相殺する。
  if(input.IsKeyPressed('A')){
    move_x -= right.x;
    move_z -= right.z;
  }

  // 斜め入力で速くならないよう、合成した方向を単位化する。長さが0（入力なし・相殺）の場合は割らずに0のままにする。
  const float move_length = std::sqrt(move_x * move_x + move_z * move_z);
  // 合成方向の長さが正の場合だけ、単位長に揃える。
  if(move_length > 0.0f){
    move_x /= move_length;
    move_z /= move_length;
  }

  // 構え中は速度を落とし、方向と速度と時間の積で、この更新の移動距離を求める。
  const float speed = m_isAiming ? ns_MOVE_SPEED * ns_AIM_SPEED_MULTIPLIER : ns_MOVE_SPEED;
  const float move_distance = speed * deltaTime;

  // 最大距離ごとに分割した回数を求める。移動距離が0でも最低1回は押し戻しを行い、壁にめり込んだ位置を補正する。
  const int step_count = move_distance > 0.0f ? static_cast<int>(std::ceil(move_distance / ns_MAX_STEP_DISTANCE)) : 1;
  const float step_scale = move_distance / static_cast<float>(step_count);

  // 分割した移動を1回ずつ進め、毎回、地形のAABBから押し戻す。
  for(int step_index = 0; step_index < step_count; ++step_index){
    // 1回分の移動を加える。高さ（Y）は変更しない。
    m_position.x += move_x * step_scale;
    m_position.z += move_z * step_scale;

    // 位置を中心にした球をSTATICレイヤーのAABBから押し戻し、水平成分（X・Z）だけを反映する。Yは固定のため反映しない。
    const COLLISION_SPHERE body_sphere{m_position,ns_BODY_RADIUS};
    const XMFLOAT3 resolved_position = CollisionResolver::ResolveSphere(collisionWorld,body_sphere,COLLISION_LAYER::ns_STATIC);
    m_position.x = resolved_position.x;
    m_position.z = resolved_position.z;
  }
}
