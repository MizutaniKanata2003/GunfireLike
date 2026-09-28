#include "../H/Transform.h"

// Scale、Rotation、Translationを合成したWorld行列を返す。
DirectX::XMMATRIX Transform::GetWorldMatrix() const
{
	return
		DirectX::XMMatrixScaling( m_Scale.x, m_Scale.y, m_Scale.z ) *
		DirectX::XMMatrixRotationRollPitchYaw( m_Rotation.x, m_Rotation.y, m_Rotation.z ) *
		DirectX::XMMatrixTranslation( m_Position.x, m_Position.y, m_Position.z );
}