#pragma once

//========= C++標準ライブラリ インクルード=========
#include <array>

//========= DirectX インクルード=========
#include <DirectXMath.h>

// 弾の生成、移動、寿命、Pool管理を行う。
class ProjectileSystem final
{
public:
	//========= 構造体=========
	// 発射後に一定時間だけ移動・描画する弾情報。
	struct Projectile
	{
		DirectX::XMFLOAT3 position {};
		DirectX::XMFLOAT3 direction {};
		float remainingLifetime {};
		bool isActive {};
	};

	//========= ライフサイクル関数=========
	// 保持している全Projectileを未使用状態へ初期化する。
	void Initialize();

	//========= Projectile操作関数=========
	// 未使用スロットへProjectileを生成する。
	void Spawn( const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, float lifetime );
	// 有効なProjectileを移動し、寿命切れのProjectileを無効化する。
	void Update( float deltaTime, float speed );

	//========= Getter関数=========
	// 描画用にProjectile Poolを読み取り専用で返す。
	[[nodiscard]] const std::array<Projectile, 16>& GetProjectiles() const { return m_Projectiles; }
private:
	//========= Projectile管理=========
	// 発射中のProjectileを最大数まで保持する固定配列。
	std::array<Projectile, 16> m_Projectiles {};
};