#pragma once

// 接近型の水平追従を更新する。攻撃判定や敵の状態タイマーは更新しない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで停止距離を越えないよう移動距離を制限する。
#include <cmath> // std::isfiniteで経過時間を検証し、std::sqrtで水平距離を求める。

//========= Scene インクルード=========
#include "Scene/GameScene/H/Enemy.h"

// 接近型の試遊用設定をまとめる。
namespace ENEMY_APPROACH
{
	//========= 移動設定=========
	// 追従速度。単位はワールド距離単位/秒。試遊用の仮値。
	inline constexpr float ns_SPEED = 2.4f;
	// 追従を止める水平中心間距離。単位はワールド距離単位。攻撃範囲ではない。
	inline constexpr float ns_STOP_DISTANCE = 1.5f;
	// 地形の押し戻しを1回行うまでの最大移動距離。単位はワールド距離単位。
	inline constexpr float ns_MAX_STEP_DISTANCE = 0.15f;
	// フレーム停止後に一度で大きく移動しないよう、追従に使う経過時間を制限する。単位は秒。
	inline constexpr float ns_MAX_UPDATE_SECONDS = 0.1f;

	//========= 更新関数=========
	// 敵が行動可能なら、目標との水平距離を縮める。
	// enemy: 追従する敵。死亡・怯み・転倒中は移動しない。
	// targetPosition: 目標のワールド座標。Yは追従方向に使わない。
	// deltaTime: 敵側の経過時間。単位は秒。0以下または非有限値なら移動しない。
	// collisionWorld: この敵のColliderを登録したCollisionWorld。
	inline void Update(Enemy& enemy,const XMFLOAT3& targetPosition,float deltaTime,CollisionWorld& collisionWorld)
	{
		// 時間が進まないか、敵が行動不能なら、描画位置もColliderも変更しない。
		if(!std::isfinite(deltaTime) || deltaTime <= 0.0f || !enemy.IsAlive() || enemy.IsFlinching() || enemy.IsKnockedDown())return;

		// 速度と上限付きの経過時間から、この更新で使える移動距離を求める。
		float remaining_distance = ns_SPEED * std::min(deltaTime,ns_MAX_UPDATE_SECONDS);

		// 移動距離を小分けに使い、押し戻された後の実位置から毎回進行方向を求め直す。
		while(remaining_distance > 0.0f){
			// XZ平面での目標までの差を求める。敵の高さは移動候補にそのまま使う。
			const XMFLOAT3 position = enemy.GetBodySphere().center;
			const float offset_x = targetPosition.x - position.x;
			const float offset_z = targetPosition.z - position.z;
			const float distance = std::sqrt(offset_x * offset_x + offset_z * offset_z);
			// 停止距離内なら接近を終える。中心が一致する場合もここで戻るため、0で割らない。
			if(distance <= ns_STOP_DISTANCE)return;

			// 残り距離・1ステップの上限・停止位置までの距離のうち、最小の値だけ進める。
			const float step_distance = std::min({remaining_distance,ns_MAX_STEP_DISTANCE,distance - ns_STOP_DISTANCE});
			const float step_scale = step_distance / distance;
			const XMFLOAT3 next_position{position.x + offset_x * step_scale,position.y,position.z + offset_z * step_scale};
			// Colliderを更新できなければ、登録状態が変わるまでこのフレームの追従を打ち切る。
			if(!enemy.TryMoveTo(next_position,collisionWorld))return;
			remaining_distance -= step_distance;
		}
	}
}