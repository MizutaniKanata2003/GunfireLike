#include "../H/Transform.h"

// 保持するScale・Euler角・位置から、既存の乗算順でWorld行列を生成する。

//========= Getter関数=========
// X=Pitch、Y=Yaw、Z=Rollの対応を維持してS*R*Tを返す。
XMMATRIX Transform::GetWorldMatrix() const
{
	// 行ベクトルで使用する既存の合成順を保ち、拡縮・回転の後に位置を反映する。
	return
		XMMatrixScaling(m_scale.x,m_scale.y,m_scale.z) *
		XMMatrixRotationRollPitchYaw(m_rotation.x,m_rotation.y,m_rotation.z) *
		XMMatrixTranslation(m_position.x,m_position.y,m_position.z);
}