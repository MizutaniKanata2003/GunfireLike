#include "../H/ProjectileSystem.h"

// 保持している全Projectileを未使用状態へ初期化する。
void ProjectileSystem::Initialize()
{
	m_Projectiles.fill( {} );
}

// 未使用スロットへProjectileを生成する。
void ProjectileSystem::Spawn( const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, float lifetime )
{
	for ( Projectile& projectile : m_Projectiles )
	{
		if ( projectile.isActive ) continue;

		projectile.position = position;
		projectile.direction = direction;
		projectile.remainingLifetime = lifetime;
		projectile.isActive = true;

		return;
	}
}

// 有効なProjectileを移動し、寿命切れのProjectileを無効化する。
void ProjectileSystem::Update( float deltaTime, float speed )
{
	for ( Projectile& projectile : m_Projectiles )
	{
		if ( !projectile.isActive ) continue;

		projectile.position.x += projectile.direction.x * speed * deltaTime;
		projectile.position.y += projectile.direction.y * speed * deltaTime;
		projectile.position.z += projectile.direction.z * speed * deltaTime;
		projectile.remainingLifetime -= deltaTime;

		if ( projectile.remainingLifetime <= 0.0f ) projectile.isActive = false;
	}
}