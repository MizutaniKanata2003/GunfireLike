#include "../H/CombatSystem.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

//========= DirectX インクルード=========
#include <DirectXCollision.h>
#include <DirectXMath.h>

// Camera位置・前方向からのRayがEnemy Sphereに命中するか返す。
bool CombatSystem::IsHitScanHit( const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection, const EnemyHitTest& enemyHitTest ) const
{
	const float safeRadius = std::max( 0.0f, enemyHitTest.radius );

	const float safeMaxDistance = std::max( 0.0f, enemyHitTest.maxDistance );

	if ( safeRadius <= 0.0f || safeMaxDistance <= 0.0f )return false;

	const DirectX::XMVECTOR originVector = DirectX::XMLoadFloat3( &rayOrigin );

	const DirectX::XMVECTOR directionVector = DirectX::XMLoadFloat3( &rayDirection );

	const float directionLengthSquared = DirectX::XMVectorGetX( DirectX::XMVector3LengthSq( directionVector ) );

	if ( directionLengthSquared <= 0.0f )return false;

	const DirectX::XMVECTOR normalizedDirection = DirectX::XMVector3Normalize( directionVector );

	const DirectX::BoundingSphere enemyHitSphere( enemyHitTest.center, safeRadius );

	float hitDistance{};

	const bool isHit = enemyHitSphere.Intersects( originVector, normalizedDirection, hitDistance );

	return isHit && hitDistance <= safeMaxDistance;
}

// PlayerとEnemyの水平距離の二乗を返す。
float CombatSystem::GetHorizontalDistanceSquared( const DirectX::XMFLOAT3& firstPosition, const DirectX::XMFLOAT3& secondPosition ) const
{
	const float deltaX = firstPosition.x - secondPosition.x;
	const float deltaZ = firstPosition.z - secondPosition.z;

	return deltaX * deltaX + deltaZ * deltaZ;
}