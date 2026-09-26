#pragma once

//========= DirectX インクルード=========
#include <DirectXMath.h>

// 射撃RayのEnemy Sphere命中判定に必要な設定。
struct EnemyHitTest
{
	DirectX::XMFLOAT3 center{};
	float radius{};
	float maxDistance{};
};

// Combatに関するHitScan、距離判定を提供する。
// HP、Damage、Reward、SE、Scene遷移は保持しない。
class CombatSystem final
{
public:
	//========= 命中判定関数=========
	// Camera位置・前方向からのRayがEnemy Sphereに命中するか返す。
	[[nodiscard]] bool IsHitScanHit(
	const DirectX::XMFLOAT3& rayOrigin,
	const DirectX::XMFLOAT3& rayDirection,
	const EnemyHitTest& enemyHitTest ) const;

	// PlayerとEnemyの水平距離の二乗を返す。
	[[nodiscard]] float GetHorizontalDistanceSquared(
	const DirectX::XMFLOAT3& firstPosition,
	const DirectX::XMFLOAT3& secondPosition ) const;

	// Playerが指定した攻撃範囲内にいるか返す。
	[[nodiscard]] bool IsWithinRangeSquared(
	float distanceSquared,
	float rangeSquared ) const;
};