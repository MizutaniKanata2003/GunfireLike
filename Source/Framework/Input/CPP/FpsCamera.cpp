#include "../H/FpsCamera.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <cmath>

//========= Framework インクルード=========
#include "Framework/Input/H/InputSystem.h"

namespace
{
	//========= Pitch制限定数=========
	// Pitchの最大角度を度で定義する。
	constexpr float MAX_PITCH_DEGREES = 89.0f;
	// Pitchとして許可する最大回転角をラジアンで定義する。
	constexpr float MAX_PITCH_RADIANS = DirectX::XMConvertToRadians( MAX_PITCH_DEGREES );
}

// マウス移動量を使ってYawとPitchを更新する。
void FpsCamera::Update( InputSystem& input )
{
	// ImGui操作中はゲーム側のマウス入力を使用しない。
	if ( !input.IsMouseCaptureEnabled() ) return;

	// InputSystemに蓄積されたこのフレームのマウス移動量を取得する。
	const POINT mouseDelta = input.ConsumeMouseDelta();

	// 横移動をYaw、縦移動をPitchへ反映する。
	m_Yaw += static_cast<float>( mouseDelta.x ) * m_MouseSensitivity;
	m_Pitch -= static_cast<float>( mouseDelta.y ) * m_MouseSensitivity;

	// 真上・真下を超えて視点が反転しないようPitchを制限する。
	m_Pitch = std::clamp( m_Pitch, -MAX_PITCH_RADIANS, MAX_PITCH_RADIANS );
}

// 現在の視線方向を正規化ベクトルで返す。
DirectX::XMFLOAT3 FpsCamera::GetForward() const
{
	// Pitchを考慮し、Yawが0のとき+Z方向を向く左手座標系の前方向を計算する。
	const float cosPitch = std::cos( m_Pitch );

	return
	{
		std::sin( m_Yaw ) * cosPitch,
		std::sin( m_Pitch ),
		std::cos( m_Yaw ) * cosPitch
	};
}

// 現在の右方向を正規化ベクトルで返す。
DirectX::XMFLOAT3 FpsCamera::GetRight() const
{
	// ワールド上方向と現在の前方向の外積から右方向を求める。
	const DirectX::XMFLOAT3 forward = GetForward();
	const DirectX::XMVECTOR upVector = DirectX::XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
	const DirectX::XMVECTOR forwardVector = DirectX::XMLoadFloat3( &forward );
	const DirectX::XMVECTOR rightVector = DirectX::XMVector3Normalize( DirectX::XMVector3Cross( upVector, forwardVector ) );

	DirectX::XMFLOAT3 right{};
	DirectX::XMStoreFloat3( &right, rightVector );

	return right;
}

// DirectX 11の左手座標系で使用するView行列を返す。
DirectX::XMMATRIX FpsCamera::GetViewMatrix() const
{
	// 現在位置、前方向、ワールド上方向から左手座標系のView行列を生成する。
	const DirectX::XMVECTOR positionVector = DirectX::XMLoadFloat3( &m_Position );
	const DirectX::XMFLOAT3 forward = GetForward();
	const DirectX::XMVECTOR forwardVector = DirectX::XMLoadFloat3( &forward );
	const DirectX::XMVECTOR upVector = DirectX::XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

	return DirectX::XMMatrixLookToLH( positionVector, forwardVector, upVector );
}