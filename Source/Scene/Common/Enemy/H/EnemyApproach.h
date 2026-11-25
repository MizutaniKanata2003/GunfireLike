#pragma once

// 敵の水平移動（目標へ近づく）を更新する。攻撃判定や敵の状態タイマーは更新しない。
// 接近型の追従（Update）と、任意の速さ・停止距離で近づく共通の移動（MoveToward。ボスが使う）を持つ。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで停止距離を越えないよう移動距離を制限する。
#include <cmath> // std::isfiniteで経過時間を検証し、std::sqrtで水平距離を求める。

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/Enemy.h"

// 敵の水平移動の設定と、移動の関数をまとめる。
namespace ENEMY_APPROACH
{
  //========= 接近型の移動設定=========
  // 追従速度の既定値。単位はワールド距離単位/秒。モデルの歩きの速度の表がある敵は、足の動きに合わせた速度を使うため、この値は使わない。モデルを使えない時（Cubeの表示）と、速度の表を作れなかった時にだけ使う。
  inline constexpr float ns_SPEED = 0.6f;
  // 追従を止める水平中心間距離。単位はワールド距離単位。攻撃範囲ではない。
  inline constexpr float ns_STOP_DISTANCE = 1.5f;

  //========= 全ての移動に共通の設定=========
  // 地形の押し戻しを1回行うまでの最大移動距離。単位はワールド距離単位。
  inline constexpr float ns_MAX_STEP_DISTANCE = 0.15f;
  // フレーム停止後に一度で大きく移動しないよう、追従に使う経過時間を制限する。単位は秒。
  inline constexpr float ns_MAX_UPDATE_SECONDS = 0.1f;

  //========= 移動の指示=========
  // 目標へ近づく移動1回分の指示。
  struct ENEMY_MOVE_ORDER
  {
    // 目標のワールド座標。Yは移動方向に使わない。
    XMFLOAT3 targetPosition;
    // 移動速度。単位はワールド距離単位/秒。
    float speed;
    // 近づくのを止める水平中心間距離。単位はワールド距離単位。この距離の内では動かない。
    float stopDistance;
  };

  //========= 更新関数=========
  // 敵が生きていて、転倒していなければ、指示に従って、目標との水平距離を縮める。怯み中でも動く。怯みで止めるかは、呼出側が決める。
  // enemy: 移動する敵。死亡・転倒中は移動しない。
  // order: 目標・速度・停止距離。
  // deltaTime: 敵側の経過時間。単位は秒。0以下または非有限値なら移動しない。
  // collisionWorld: この敵のColliderを登録したCollisionWorld。
  inline void MoveToward(Enemy& enemy,const ENEMY_MOVE_ORDER& order,float deltaTime,CollisionWorld& collisionWorld)
  {
    // 時間が進まないか、敵が死亡・転倒していれば、描画位置もColliderも変更しない。
    if(!std::isfinite(deltaTime) || deltaTime <= 0.0f || !enemy.IsAlive() || enemy.IsKnockedDown())return;

    // 速度と上限付きの経過時間から、この更新で使える移動距離を求める。
    float remaining_distance = order.speed * std::min(deltaTime,ns_MAX_UPDATE_SECONDS);

    // 移動距離を小分けに使い、押し戻された後の実位置から毎回進行方向を求め直す。
    while(remaining_distance > 0.0f){
      // XZ平面での目標までの差を求める。敵の高さは移動候補にそのまま使う。
      const XMFLOAT3 position = enemy.GetBodySphere().center;
      const float offset_x = order.targetPosition.x - position.x;
      const float offset_z = order.targetPosition.z - position.z;
      const float distance = std::sqrt(offset_x * offset_x + offset_z * offset_z);
      // 停止距離内なら接近を終える。中心が一致する場合もここで戻るため、0で割らない。
      if(distance <= order.stopDistance)return;

      // 残り距離・1ステップの上限・停止位置までの距離のうち、最小の値だけ進める。
      const float step_distance = std::min({remaining_distance,ns_MAX_STEP_DISTANCE,distance - order.stopDistance});
      const float step_scale = step_distance / distance;
      const XMFLOAT3 next_position{position.x + offset_x * step_scale,position.y,position.z + offset_z * step_scale};
      // Colliderを更新できなければ、登録状態が変わるまでこのフレームの追従を打ち切る。
      if(!enemy.TryMoveTo(next_position,collisionWorld))return;
      remaining_distance -= step_distance;
    }
  }

  // 接近型の追従。敵が行動可能（生きていて、怯み・転倒していない）なら、足の動きに合わせた速度（モデルがない時は既定の速度）と、接近型の停止距離で、目標との水平距離を縮める。
  // enemy: 追従する敵。死亡・怯み・転倒中は移動しない。
  // targetPosition: 目標のワールド座標。Yは追従方向に使わない。
  // deltaTime: 敵側の経過時間。単位は秒。0以下または非有限値なら移動しない。
  // collisionWorld: この敵のColliderを登録したCollisionWorld。
  inline void Update(Enemy& enemy,const XMFLOAT3& targetPosition,float deltaTime,CollisionWorld& collisionWorld)
  {
    // 怯み中は追従しない。死亡・転倒中と時間の検証は、MoveTowardが行う。
    if(enemy.IsFlinching())return;

    // 速度は、歩きの動きの位相から求めた、足が地面を滑らない速さ。足の動きが速い時に速く、遅い時に遅く進む。
    MoveToward(enemy,ENEMY_MOVE_ORDER{targetPosition,enemy.GetModelMoveSpeed(ns_SPEED),ns_STOP_DISTANCE},deltaTime,collisionWorld);
  }
}