#pragma once

// 範囲攻撃型の接近の移動と、攻撃の数値を定義する。範囲の予告の表示・範囲の判定・予告の状態管理は担当しない。
// 数値は企画書で未確定の試遊用の仮値。距離の単位はワールド距離単位、時間の単位は秒。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで、1回の移動で使う距離を残りの距離・1ステップの上限・停止位置までの距離の最小値に制限する。
#include <cmath> // std::isfiniteで経過時間を検証し、std::sqrtで水平距離を求める。

//========= Scene インクルード=========
#include "Scene/GameScene/H/Enemy.h"

// 範囲攻撃型の移動・攻撃の数値と、接近の移動をまとめる。
namespace ENEMY_AREA
{
	//========= 距離の設定=========
	// 攻撃を始める水平距離の上限。プレイヤーがこの距離以内にいる間は、立ち止まって攻撃の機会を待つ。水平方向の中心間距離。
	inline constexpr float ns_ENGAGE_DISTANCE = 6.0f;

	//========= 移動の設定=========
	// 移動速度。単位はワールド距離単位/秒。接近型の追従（2.4）や射撃型（2.0）より遅い仮値。
	inline constexpr float ns_SPEED = 1.8f;
	// 地形の押し戻しを1回行うまでの最大移動距離。単位はワールド距離単位。接近型の追従と同じ値。
	inline constexpr float ns_MAX_STEP_DISTANCE = 0.15f;
	// フレーム停止後に一度で大きく移動しないよう、移動に使う経過時間を制限する。単位は秒。接近型の追従と同じ値。
	inline constexpr float ns_MAX_UPDATE_SECONDS = 0.1f;

	//========= 攻撃の設定=========
	// 攻撃の予告を始めてから、範囲の判定までの時間。敵側の時間で1.2秒。仮値。
	inline constexpr float ns_WINDUP_SECONDS = 1.2f;
	// 予告の終わり・予告の中断の後、次の予告を始められるまでの時間。敵側の時間で2.5秒。仮値。
	inline constexpr float ns_RECOVERY_SECONDS = 2.5f;
	// 範囲内のプレイヤーへ与えるダメージ。しゃがみでは避けられず、回避の無敵でだけ避けられる。仮値。
	inline constexpr float ns_DAMAGE = 20.0f;
	// 回避の動作中ではない場合に、回避の開始からの経過時間として渡す値。単位は秒。ジャスト回避の判定の閾値より十分に大きく、蹴り・殴打の無敵をジャスト回避にしない。
	inline constexpr float ns_NON_DODGE_AGE_SECONDS = 1000.0f;

	//========= 移動関数=========
	// 攻撃を始める距離に入るまで、プレイヤーへ向かって水平に近づく。すでに距離の内なら、動かない。
	// enemy: 移動する敵。死亡中などの移動条件は、呼出側が確認すること。
	// targetPosition: プレイヤーのワールド座標。Yは使わない。
	// deltaTime: 敵側の経過時間。単位は秒。0以下または非有限値なら移動しない。
	// collisionWorld: この敵のColliderを登録したCollisionWorld。
	inline void Approach(Enemy& enemy,const XMFLOAT3& targetPosition,float deltaTime,CollisionWorld& collisionWorld)
	{
		// 時間が進まない、または時間が不正な場合は、描画位置もColliderも変更しない。
		if(!std::isfinite(deltaTime) || deltaTime <= 0.0f)return;

		// 速度と上限付きの経過時間から、この更新で使える移動距離を求める。
		float remaining_distance = ns_SPEED * std::min(deltaTime,ns_MAX_UPDATE_SECONDS);

		// 移動距離を小分けに使い、押し戻された後の実位置から毎回向きを求め直す。
		while(remaining_distance > 0.0f){
			// XZ平面での目標までの差と距離を求める。敵の高さは移動候補にそのまま使う。
			const XMFLOAT3 position = enemy.GetBodySphere().center;
			const float offset_x = targetPosition.x - position.x;
			const float offset_z = targetPosition.z - position.z;
			const float distance = std::sqrt(offset_x * offset_x + offset_z * offset_z);
			// 攻撃を始める距離の内なら、近づくのを終える。中心が一致する場合もここで戻るため、0で割らない。
			if(distance <= ns_ENGAGE_DISTANCE)return;

			// 残り距離・1ステップの上限・攻撃を始める距離までの距離のうち、最小の値だけ進める。
			const float step_distance = std::min({remaining_distance,ns_MAX_STEP_DISTANCE,distance - ns_ENGAGE_DISTANCE});
			const float step_scale = step_distance / distance;
			const XMFLOAT3 next_position{position.x + offset_x * step_scale,position.y,position.z + offset_z * step_scale};
			// Colliderを更新できなければ、登録状態が変わるまでこのフレームの移動を打ち切る。
			if(!enemy.TryMoveTo(next_position,collisionWorld))return;
			remaining_distance -= step_distance;
		}
	}
}