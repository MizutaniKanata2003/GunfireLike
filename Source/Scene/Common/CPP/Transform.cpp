#include "../H/Transform.h"

// ObjectのWorld座標を設定する。
void Transform::SetPosition( const DirectX::XMFLOAT3& position )
{
	m_Position = position;
}

// ObjectのEuler角RotationをRadianで設定する。
void Transform::SetRotation( const DirectX::XMFLOAT3& rotation )
{
	m_Rotation = rotation;
}

// Objectの各軸Scaleを設定する。
void Transform::SetScale( const DirectX::XMFLOAT3& scale )
{
	m_Scale = scale;
}

// ObjectのWorld座標を返す。
const DirectX::XMFLOAT3& Transform::GetPosition() const
{
	return m_Position;
}

// ObjectのEuler角RotationをRadianで返す。
const DirectX::XMFLOAT3& Transform::GetRotation() const
{
	return m_Rotation;
}

// Objectの各軸Scaleを返す。
const DirectX::XMFLOAT3& Transform::GetScale() const
{
	return m_Scale;
}

// Scale、Rotation、Translationを合成したWorld行列を返す。
DirectX::XMMATRIX Transform::GetWorldMatrix() const
{
	return
		DirectX::XMMatrixScaling( m_Scale.x, m_Scale.y, m_Scale.z ) *
		DirectX::XMMatrixRotationRollPitchYaw(
		m_Rotation.x,
		m_Rotation.y,
		m_Rotation.z ) *
		DirectX::XMMatrixTranslation(
		m_Position.x,
		m_Position.y,
		m_Position.z );
}