#include "../H/Transform.h"

// 既存のS*R*Tと、X=Pitch、Y=Yaw、Z=Rollの対応を維持する。
XMMATRIX Transform::GetWorldMatrix() const
{
	return
		XMMatrixScaling(m_scale.x,m_scale.y,m_scale.z) *
		XMMatrixRotationRollPitchYaw(m_rotation.x,m_rotation.y,m_rotation.z) *
		XMMatrixTranslation(m_position.x,m_position.y,m_position.z);
}