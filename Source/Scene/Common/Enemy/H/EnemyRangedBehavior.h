#pragma once

// 射撃型の距離維持の移動と、攻撃の数値を定義する。弾の生成・命中判定・予告の状態管理は担当しない。
// 数値は企画書で未確定の試遊用の仮値。距離の単位はワールド距離単位、時間の単位は秒。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで、1回の移動で使う距離を残りの距離と1ステップの上限の小さい方に制限する。
#include <cmath> // std::isfiniteで経過時間を検証し、std::sqrtで水平距離を求める。

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/Enemy.h"

// 射撃型の移動・攻撃の数値と、距離を保つ移動をまとめる。
namespace ENEMY_RANGED
{
  //========= 距離の設定=========
  // これより遠い間は、プレイヤーへ近づく。水平方向の中心間距離。攻撃を始められる最大の距離でもある。
  inline constexpr float ns_FAR_DISTANCE = 9.0f;
  // これより近い間は、プレイヤーから離れる。水平方向の中心間距離。この距離以上、遠い距離以下の間は、その場で攻撃の機会を待つ。
  inline constexpr float ns_NEAR_DISTANCE = 5.0f;

  //========= 移動の設定=========
  // 移動速度。単位はワールド距離単位/秒。接近型の追従（2.4）より遅い仮値。
  inline constexpr float ns_SPEED = 2.0f;
  // 地形の押し戻しを1回行うまでの最大移動距離。単位はワールド距離単位。接近型の追従と同じ値。
  inline constexpr float ns_MAX_STEP_DISTANCE = 0.15f;
  // フレーム停止後に一度で大きく移動しないよう、移動に使う経過時間を制限する。単位は秒。接近型の追従と同じ値。
  inline constexpr float ns_MAX_UPDATE_SECONDS = 0.1f;

  //========= 攻撃の設定=========
  // 攻撃の予告を始めてから、発射のタイミングに達するまでの時間。敵側の時間で0.9秒。仮値。
  inline constexpr float ns_WINDUP_SECONDS = 0.9f;
  // 発射のタイミング・予告の中断の後、次の予告を始められるまでの時間。敵側の時間で2.0秒。仮値。
  inline constexpr float ns_RECOVERY_SECONDS = 2.0f;
  // ジャスト回避の判定を使わない予告で、回避の開始からの経過時間として渡す値。単位は秒。判定の閾値より十分に大きい。
  inline constexpr float ns_UNUSED_DODGE_AGE_SECONDS = 1000.0f;

  //========= 移動の入力=========
  // 移動の指示。Moveの引数の数を減らすための値の束で、移動は変更しない。
  struct MOVE_REQUEST
  {
    // プレイヤーのワールド座標。Yは使わない。
    XMFLOAT3 targetPosition{};
    // 1なら目標へ近づき、-1なら目標から離れる。
    float directionSign{};
    // 敵側の経過時間。単位は秒。0以下または非有限値なら移動しない。
    float deltaTime{};
  };

  //========= 移動関数=========
  // プレイヤーへ近づく、または離れる向きに、水平に移動する。
  // enemy: 移動する敵。死亡中などの移動条件は、呼出側が確認すること。
  // request: 目標の座標・近づくか離れるかの向き・経過時間。
  // collisionWorld: この敵のColliderを登録したCollisionWorld。
  inline void Move(Enemy& enemy,const MOVE_REQUEST& request,CollisionWorld& collisionWorld)
  {
    // 時間が進まない、または時間が不正な場合は、描画位置もColliderも変更しない。
    if(!std::isfinite(request.deltaTime) || request.deltaTime <= 0.0f)return;

    // 速度と上限付きの経過時間から、この更新で使える移動距離を求める。
    float remaining_distance = ns_SPEED * std::min(request.deltaTime,ns_MAX_UPDATE_SECONDS);

    // 移動距離を小分けに使い、押し戻された後の実位置から毎回向きを求め直す。
    while(remaining_distance > 0.0f){
      // XZ平面での目標までの差と距離を求める。敵の高さは移動候補にそのまま使う。
      const XMFLOAT3 position = enemy.GetBodySphere().center;
      const float offset_x = request.targetPosition.x - position.x;
      const float offset_z = request.targetPosition.z - position.z;
      const float distance = std::sqrt(offset_x * offset_x + offset_z * offset_z);
      // 中心が一致している場合は向きを決められない（0で割れない）ため、移動を終える。
      if(distance <= 0.0f)return;

      // 残り距離と1ステップの上限のうち小さい方だけ進める。離れる場合は、向きの符号を反転して目標と逆へ進む。
      const float step_distance = std::min(remaining_distance,ns_MAX_STEP_DISTANCE);
      const float step_scale = request.directionSign * step_distance / distance;
      const XMFLOAT3 next_position{position.x + offset_x * step_scale,position.y,position.z + offset_z * step_scale};
      // Colliderを更新できなければ、登録状態が変わるまでこのフレームの移動を打ち切る。
      if(!enemy.TryMoveTo(next_position,collisionWorld))return;
      remaining_distance -= step_distance;
    }
  }
}