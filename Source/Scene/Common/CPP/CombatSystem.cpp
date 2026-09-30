#include "../H/CombatSystem.h"

//========= DirectX インクルード=========
#include <DirectXCollision.h>
#include <DirectXMath.h>

// 指定したRayがSphereへ命中した場合、命中距離を含む判定結果を返す。
RaycastResult CombatSystem::RaycastSphere( const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection,
										   const DirectX::XMFLOAT3& sphereCenter, float sphereRadius, float maxDistance ) const
{
	RaycastResult result{};

	const float safeRadius = std::max( 0.0f, sphereRadius );
	const float safeMaxDistance = std::max( 0.0f, maxDistance );

	if ( safeRadius <= 0.0f || safeMaxDistance <= 0.0f ) return result;

	const DirectX::XMVECTOR originVector = DirectX::XMLoadFloat3( &rayOrigin );
	const DirectX::XMVECTOR directionVector = DirectX::XMLoadFloat3( &rayDirection );
	const float directionLengthSquared = DirectX::XMVectorGetX( DirectX::XMVector3LengthSq( directionVector ) );

	if ( directionLengthSquared <= 0.0f ) return result;

	const DirectX::XMVECTOR normalizedDirection = DirectX::XMVector3Normalize( directionVector );
	const DirectX::BoundingSphere sphere( sphereCenter, safeRadius );

	float hitDistance{};

	if ( !sphere.Intersects( originVector, normalizedDirection, hitDistance ) ) return result;

	if ( hitDistance > safeMaxDistance ) return result;

	result.isHit = true;
	result.hitDistance = hitDistance;

	return result;
}

// Camera位置・前方向からのRayがEnemy Sphereに命中するか返す。
bool CombatSystem::IsHitScanHit( const DirectX::XMFLOAT3& rayOrigin, const DirectX::XMFLOAT3& rayDirection, const EnemyHitTest& enemyHitTest ) const
{
	const RaycastResult raycastResult = RaycastSphere( rayOrigin, rayDirection, enemyHitTest.center, enemyHitTest.radius, enemyHitTest.maxDistance );

	return raycastResult.isHit;
}

// PlayerとEnemyの水平距離の二乗を返す。
float CombatSystem::GetHorizontalDistanceSquared( const DirectX::XMFLOAT3& firstPosition, const DirectX::XMFLOAT3& secondPosition ) const
{
	const float deltaX = firstPosition.x - secondPosition.x;
	const float deltaZ = firstPosition.z - secondPosition.z;

	return deltaX * deltaX + deltaZ * deltaZ;
}