#include "../H/EnemyVisual.h"

//========= C++標準ライブラリ インクルード=========
#include <cmath>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

namespace
{
	//========= Enemy Animation定数=========
	// Enemyの浮遊演出設定。
	constexpr float ENEMY_FLOAT_HEIGHT = 0.10f;
	constexpr float ENEMY_FLOAT_SPEED = 2.0f;

	// EnemyのY軸回転速度。
	constexpr float ENEMY_ROTATION_SPEED = 1.5f;

	// Enemy OBJ Modelの単色Tint。
	const DirectX::XMFLOAT4 ENEMY_COLOR{ 0.18f,0.95f,0.28f,1.0f };
}

// EnemyのTransform、浮遊・回転Animation状態を初期化する。
void EnemyVisual::Initialize( const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& scale )
{
	m_BasePosition = position;
	m_Transform.SetPosition( position );
	m_Transform.SetRotation( DirectX::XMFLOAT3{} );
	m_Transform.SetScale( scale );
	m_AnimationTime = {};
}

// Enemyの浮遊・回転Animationを更新する。
void EnemyVisual::Update( float deltaTime )
{
	m_AnimationTime += deltaTime;

	const float floatOffset = std::sinf( m_AnimationTime * ENEMY_FLOAT_SPEED ) * ENEMY_FLOAT_HEIGHT;

	m_Transform.SetPosition( DirectX::XMFLOAT3{ m_BasePosition.x,m_BasePosition.y + floatOffset,m_BasePosition.z } );

	m_Transform.SetRotation( DirectX::XMFLOAT3{ 0.0f,m_AnimationTime * ENEMY_ROTATION_SPEED,0.0f } );
}

// Enemy OBJ ModelをOpaque Passで描画する。
void EnemyVisual::Draw( ObjModelRenderer& enemyModelRenderer, GraphicsSystem& graphicsSystem,
						const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix ) const
{
	enemyModelRenderer.Draw( graphicsSystem, m_Transform.GetWorldMatrix(), viewMatrix, projectionMatrix, ENEMY_COLOR );
}

// EnemyのRay命中判定に使用するSphere中心座標を返す。
DirectX::XMFLOAT3 EnemyVisual::GetHitSphereCenter( float hitCenterYOffset ) const
{
	const DirectX::XMFLOAT3& enemyPosition = m_Transform.GetPosition();

	return
		DirectX::XMFLOAT3{ enemyPosition.x,enemyPosition.y + hitCenterYOffset,enemyPosition.z };
}

// Enemy頭上HPバーのWorld座標を返す。
DirectX::XMFLOAT3 EnemyVisual::GetHealthBarPosition(
float healthBarYOffset ) const
{
	const DirectX::XMFLOAT3& enemyPosition = m_Transform.GetPosition();

	return
		DirectX::XMFLOAT3{ enemyPosition.x,enemyPosition.y + healthBarYOffset,enemyPosition.z };
}