#include "../H/StageGate.h"

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

namespace
{
	//========= Gate定数=========
	// GateのY軸回転速度。
	constexpr float GATE_ROTATION_SPEED = 1.4f;
}

// Gateの種類、Transform、表示色を初期化する。
void StageGate::Initialize( GateType gateType, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT4& color )
{
	m_GateType = gateType;
	m_Transform.SetPosition( position );
	m_Transform.SetRotation( DirectX::XMFLOAT3{} );
	m_Transform.SetScale( scale );
	m_Color = color;
	m_AnimationTime = {};
}

// Gateの回転アニメーションを更新する。
void StageGate::Update( float deltaTime )
{
	m_AnimationTime += deltaTime;

	m_Transform.SetRotation( DirectX::XMFLOAT3{ 0.0f,m_AnimationTime * GATE_ROTATION_SPEED,0.0f } );
}

// PlayerがGateの操作範囲内にいるかを返す。
bool StageGate::IsPlayerNear( const DirectX::XMFLOAT3& playerPosition, float interactionRadiusSquared ) const
{
	const DirectX::XMFLOAT3& gatePosition = m_Transform.GetPosition();
	const float deltaX = playerPosition.x - gatePosition.x;
	const float deltaZ = playerPosition.z - gatePosition.z;
	const float distanceSquared = deltaX * deltaX + deltaZ * deltaZ;

	return distanceSquared <= interactionRadiusSquared;
}

// GateをOpaque Passで描画する。
void StageGate::Draw( BasicMeshRenderer& basicMeshRenderer, GraphicsSystem& graphicsSystem,
					  const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix ) const
{
	basicMeshRenderer.DrawCube( graphicsSystem, m_Transform.GetWorldMatrix(), viewMatrix, projectionMatrix, m_Color,
								DirectX::XMFLOAT2{ 1.0f, 1.0f }, BasicMeshRenderer::TextureType::Color );
}
