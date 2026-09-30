#include "../H/ShopGate.h"

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

namespace
{
	//========= Gate定数=========
	// GateのY軸回転速度。
	constexpr float SHOP_GATE_ROTATION_SPEED = 1.4f;
	// 照準中のGateに適用するScale倍率。
	constexpr float AIMED_GATE_SCALE_MULTIPLIER = 1.10f;
}

// Gateの種類、Transform、表示色、Raycast Sphere半径を初期化する。
void ShopGate::Initialize( GateType gateType, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT4& color, float hitSphereRadius )
{
	m_GateType = gateType;

	m_Transform.SetPosition( position );
	m_Transform.SetRotation( DirectX::XMFLOAT3{} );
	m_Transform.SetScale( scale );

	m_Color = color;
	m_HitSphereRadius = hitSphereRadius;
	m_AnimationTime = {};
}

// GateのY軸回転Animationを更新する。
void ShopGate::Update( float deltaTime )
{
	m_AnimationTime += deltaTime;
	m_Transform.SetRotation( DirectX::XMFLOAT3{ 0.0f,m_AnimationTime * SHOP_GATE_ROTATION_SPEED,0.0f } );
}

// 指定したRayがGateのSphereへ命中した場合、命中距離を返す。
RaycastResult ShopGate::Raycast( const CombatSystem& combatSystem, const DirectX::XMFLOAT3& rayOrigin,
								 const DirectX::XMFLOAT3& rayDirection, float maxDistance ) const
{
	return combatSystem.RaycastSphere( rayOrigin, rayDirection, m_Transform.GetPosition(), m_HitSphereRadius, maxDistance );
}

// 照準状態に応じたScaleでGateをOpaque Passに描画する。
void ShopGate::Draw( BasicMeshRenderer& basicMeshRenderer, GraphicsSystem& graphicsSystem, const DirectX::XMMATRIX& viewMatrix,
					 const DirectX::XMMATRIX& projectionMatrix, bool isAimed ) const
{
	const float scaleMultiplier = isAimed ? AIMED_GATE_SCALE_MULTIPLIER : 1.0f;

	const DirectX::XMFLOAT3& scale = m_Transform.GetScale();
	const DirectX::XMFLOAT3& position = m_Transform.GetPosition();
	const DirectX::XMFLOAT3& rotation = m_Transform.GetRotation();

	const DirectX::XMMATRIX worldMatrix =
		DirectX::XMMatrixScaling( scale.x * scaleMultiplier, scale.y * scaleMultiplier, scale.z * scaleMultiplier ) *
		DirectX::XMMatrixRotationRollPitchYaw( rotation.x, rotation.y, rotation.z ) *
		DirectX::XMMatrixTranslation( position.x, position.y, position.z );

	basicMeshRenderer.DrawCube( graphicsSystem, worldMatrix, viewMatrix, projectionMatrix, m_Color,
								DirectX::XMFLOAT2{ 1.0f,1.0f }, BasicMeshRenderer::TextureType::Color );
}