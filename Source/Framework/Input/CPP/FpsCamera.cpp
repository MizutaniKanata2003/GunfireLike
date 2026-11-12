#include "../H/FpsCamera.h"

// マウス差分から視線角度を更新し、前・右方向と左手系View行列を計算する。
// 回転符号・感度・Pitch制限・外積と行列生成の順序は既存のまま維持する。

//========= Windows インクルード=========
#include <windows.h>

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <cmath>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

// 垂直回転で視点が反転しないようにするための制限値をまとめる。
namespace
{
	//========= Pitch制限定数=========
	// 垂直方向の最大回転角。単位は度。
	constexpr float ns_MAX_PITCH_DEGREES = 89.0f;
	// clampで使用する同じ制限値のラジアン表現。
	constexpr float ns_MAX_PITCH_RADIANS = XMConvertToRadians(ns_MAX_PITCH_DEGREES);
}

//========= 更新関数=========
// FPS操作中だけ入力を消費し、視線角度を更新する。
void FpsCamera::Update(InputSystem& input)
{
	// ImGui操作などでFPS操作を解除している間は、差分の消費も角度の変更も行わない。
	if(!input.IsMouseCaptureEnabled())return;

	// 横移動をYawへ加算し、下向きに増えるマウスYは減算して視線の上下へ反映する。
	const POINT mouse_delta = input.ConsumeMouseDelta();
	m_yaw += static_cast<float>(mouse_delta.x) * m_mouseSensitivity;
	m_pitch -= static_cast<float>(mouse_delta.y) * m_mouseSensitivity;

	// 真上・真下を超える反転を防ぐ。Yawの範囲や周期は変更しない。
	m_pitch = std::clamp(m_pitch,-ns_MAX_PITCH_RADIANS,ns_MAX_PITCH_RADIANS);
}

//========= Getter関数=========
// YawとPitchから、位置に依存しない前方向を計算する。
XMFLOAT3 FpsCamera::GetForward() const
{
	// 水平成分へcos(Pitch)、高さへsin(Pitch)を使い、Yawが0のとき+Zを向く方向を作る。
	const float cos_pitch = std::cos(m_pitch);
	return
	{
		std::sin(m_yaw) * cos_pitch,
		std::sin(m_pitch),
		std::cos(m_yaw) * cos_pitch
	};
}

// ワールド上方向と視線から、左手系で使用する右方向を求める。
XMFLOAT3 FpsCamera::GetRight() const
{
	// up×forwardの順を維持する。逆順にすると移動に使用する左右方向も反転する。
	const XMFLOAT3 forward = GetForward();
	const XMVECTOR up_vector = XMVectorSet(0.0f,1.0f,0.0f,0.0f);
	const XMVECTOR forward_vector = XMLoadFloat3(&forward);
	const XMVECTOR right_vector = XMVector3Normalize(XMVector3Cross(up_vector,forward_vector));

	// SIMD演算の結果を、呼出側で参照できる保存用の3成分型へ書き戻す。
	XMFLOAT3 right{};
	XMStoreFloat3(&right,right_vector);
	return right;
}

// 現在位置と視線方向から、描画に使用するView行列を生成する。
XMMATRIX FpsCamera::GetViewMatrix() const
{
	// LookToには注視点ではなく方向を渡すため、位置とは別に前方向を読み込む。
	const XMVECTOR position_vector = XMLoadFloat3(&m_position);
	const XMFLOAT3 forward = GetForward();
	const XMVECTOR forward_vector = XMLoadFloat3(&forward);
	const XMVECTOR up_vector = XMVectorSet(0.0f,1.0f,0.0f,0.0f);
	return XMMatrixLookToLH(position_vector,forward_vector,up_vector);
}