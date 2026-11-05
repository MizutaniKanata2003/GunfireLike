#include "../H/FpsCamera.h"

//========= Windows インクルード=========
#include <windows.h>

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <cmath>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

// カメラの垂直回転に使用する制限定数を、この実装ファイル内に限定する。
namespace
{
	//========= Pitch制限定数=========
	// Pitchの最大角度。単位は度。
	constexpr float ns_MAX_PITCH_DEGREES = 89.0f;
	// Pitchとして許可する最大回転角。単位はラジアン。
	constexpr float ns_MAX_PITCH_RADIANS = ::XMConvertToRadians(ns_MAX_PITCH_DEGREES);

}

// Mouse Capture中のマウス移動量からYawとPitchを更新する。
// input: Mouse Capture状態と、このフレームのマウス移動量を取得する入力System。
// 戻り値: なし。
void FpsCamera::Update(InputSystem& input)
{
	// ImGui操作中はゲーム側のマウス入力を使用しない。
	if(!input.IsMouseCaptureEnabled())return;

	// このフレームの移動量を消費し、横移動をYaw、縦移動をPitchへ反映する。
	const POINT mouse_delta = input.ConsumeMouseDelta();
	m_yaw += static_cast<float>(mouse_delta.x) * m_mouseSensitivity;
	m_pitch -= static_cast<float>(mouse_delta.y) * m_mouseSensitivity;

	// 真上・真下を超えて視点が反転しないようPitchを制限する。
	m_pitch = std::clamp(m_pitch,-ns_MAX_PITCH_RADIANS,ns_MAX_PITCH_RADIANS);
}

// 現在の視線方向を計算する。
// 戻り値: PitchとYawを反映した正規化済みの前方向ベクトル。
XMFLOAT3 FpsCamera::GetForward() const
{
	// Pitchを考慮し、Yawが0のとき+Z方向を向く前方向を計算する。
	const float cos_pitch = std::cos(m_pitch);

	return
	{
		std::sin(m_yaw) * cos_pitch,
		std::sin(m_pitch),
		std::cos(m_yaw) * cos_pitch
	};
}

// 現在の右方向を計算する。
// 戻り値: ワールド上方向と前方向から求めた正規化済みの右方向ベクトル。
XMFLOAT3 FpsCamera::GetRight() const
{
	// 保存用ベクトルを演算用ベクトルへ読み込み、上方向との外積を正規化する。
	const XMFLOAT3 forward = GetForward();
	const XMVECTOR up_vector = ::XMVectorSet(0.0f,1.0f,0.0f,0.0f);
	const XMVECTOR forward_vector = ::XMLoadFloat3(&forward);
	const XMVECTOR right_vector = ::XMVector3Normalize(DirectX::XMVector3Cross(up_vector,forward_vector));

	// 演算結果を保存用ベクトルへ書き戻す。
	XMFLOAT3 right{};
	::XMStoreFloat3(&right,right_vector);
	return right;
}

// DirectX 11の左手座標系で使用するView行列を生成する。
// 戻り値: 現在位置と視線方向を反映したView行列。
XMMATRIX FpsCamera::GetViewMatrix() const
{
	// 現在位置、前方向、ワールド上方向からView行列を生成する。
	const XMVECTOR position_vector = ::XMLoadFloat3(&m_position);
	const XMFLOAT3 forward = GetForward();
	const XMVECTOR forward_vector = ::XMLoadFloat3(&forward);
	const XMVECTOR up_vector = ::XMVectorSet(0.0f,1.0f,0.0f,0.0f);

	return ::XMMatrixLookToLH(position_vector,forward_vector,up_vector);
}