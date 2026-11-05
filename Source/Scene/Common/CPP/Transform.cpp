#include "../H/Transform.h"

// Scale、Rotation、Translationの順にWorld行列を合成する。
// 戻り値: 現在のTransform状態を反映したWorld行列。
XMMATRIX Transform::GetWorldMatrix() const
{
	// 既存の合成順と、XをPitch、YをYaw、ZをRollへ渡す対応を維持する。
	return
		XMMatrixScaling(m_scale.x,m_scale.y,m_scale.z) *
		XMMatrixRotationRollPitchYaw(m_rotation.x,m_rotation.y,m_rotation.z) *
		XMMatrixTranslation(m_position.x,m_position.y,m_position.z);
}