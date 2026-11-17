#include "../H/FpsCamera.h"

// マウス差分を視線角度へ変換し、方向ベクトルと左手系View行列を計算する。
// 感度・回転符号・Pitch制限・外積順・行列生成を維持し、条件・値・計算の意味を補完する。反動の回転量の加算も行う。

//========= Windows インクルード=========
#include <windows.h> // ConsumeMouseDeltaが返すPOINTのX・Yを受け取り、視線更新の入力に使用する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampでPitchを上下の制限角内に収める。
#include <cmath> // std::sin・std::cosでYawとPitchから前方向の各成分を計算する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

// 視線の上下回転を真上・真下の手前に収める、既存のPitch制限値をまとめる。
namespace
{
	//========= Pitch制限定数=========
	// 上下に許可する角度の絶対値。89度を維持し、Updateでは負値と正値を下限・上限に使う。
	// ±90度を超える視線反転を避ける範囲。89という具体値の採用経緯は未確認。
	constexpr float ns_MAX_PITCH_DEGREES = 89.0f;
	// 三角関数と保持角度がラジアンを使うため、同じ上限を度から変換する。感度や時間を掛ける値ではない。
	constexpr float ns_MAX_PITCH_RADIANS = XMConvertToRadians(ns_MAX_PITCH_DEGREES);
}

//========= 更新関数=========
// FPS操作中の累積マウス差分を一度消費し、角度へ反映してからPitchを制限する。
void FpsCamera::Update(InputSystem& input)
{
	// Capture解除中は視線を変えず、累積差分も消費しない。差分破棄やCapture切替はInputSystem側が担当する。
	if(!input.IsMouseCaptureEnabled())return;

	// 取得時に入力側の差分がクリアされるので、一度取得したPOINTを両軸へ使用する。
	// 入力はpixel差分、感度はラジアン/pixelで、積が今回の角度変化になる。deltaTimeは掛けない。
	const POINT mouse_delta = input.ConsumeMouseDelta();
	m_yaw += static_cast<float>(mouse_delta.x) * m_mouseSensitivity; // 右向きの正のX差分でYawを増やす。
	m_pitch -= static_cast<float>(mouse_delta.y) * m_mouseSensitivity; // 下向きの正のY差分でPitchを減らす。

	// 更新したPitchだけを上下限へ収める。Yawの累積値や周期は変更せず、位置もここでは更新しない。
	m_pitch = std::clamp(m_pitch,-ns_MAX_PITCH_RADIANS,ns_MAX_PITCH_RADIANS);
}

// 反動の回転量を、現在の向きへ直接加える。
void FpsCamera::ApplyRecoil(float yawRadians,float pitchRadians)
{
	// YawとPitchへ、引数の回転量をそのまま加える。マウス感度は掛けない。
	m_yaw += yawRadians;
	m_pitch += pitchRadians;

	// 加算後のPitchを、Updateと同じ上下限へ収める。反動が上限を超える場合も視線が反転しないようにする。
	m_pitch = std::clamp(m_pitch,-ns_MAX_PITCH_RADIANS,ns_MAX_PITCH_RADIANS);
}

//========= Getter関数=========
// 保持角度から前方向を生成する。位置を加えないため、注視点ではなく方向ベクトルを返す。
XMFLOAT3 FpsCamera::GetForward() const
{
	// Pitchが上下へ傾くほど水平成分を縮め、Yへsin(Pitch)を使う。角度はラジアンで解釈する。
	const float cos_pitch = std::cos(m_pitch);
	return
	{
		std::sin(m_yaw) * cos_pitch, // X: Yawの左右成分へ水平面への投影倍率を掛ける。
		std::sin(m_pitch), // Y: Pitchの正値は上向き、負値は下向きの成分になる。
		std::cos(m_yaw) * cos_pitch // Z: Yaw=0かつPitch=0で+Zを向く。追加の正規化は行わない。
	};
}

// 前方向とワールド上方向から、水平移動に使う単位右方向を求める。
XMFLOAT3 FpsCamera::GetRight() const
{
	// up×forwardの順で右方向を求める。順序を逆にすると符号が反転し、左右移動も反転する。
	const XMFLOAT3 forward = GetForward();
	const XMVECTOR up_vector = XMVectorSet(0.0f,1.0f,0.0f,0.0f); // ワールドの+Yを上とする。Wは0で方向として用意する。
	const XMVECTOR forward_vector = XMLoadFloat3(&forward); // 保存用float3をSIMD演算用ベクトルへ読み込む。
	const XMVECTOR right_vector = XMVector3Normalize(XMVector3Cross(up_vector,forward_vector)); // 外積の長さを1へ揃える。

	// 外積は視線が上方向と平行な場合に長さ0になる。既存Pitch制限を維持し、特別な退避方向は追加しない。
	// SIMDのXYZを保存用型へ書き戻す。ローカル変数への参照ではなく、値を返す。
	XMFLOAT3 right{};
	XMStoreFloat3(&right,right_vector);
	return right;
}

// 現在の位置と向きから、World座標をCameraのView座標へ変換する行列を返す。
XMMATRIX FpsCamera::GetViewMatrix() const
{
	// LookToは注視点ではなく方向を受け取るため、位置を足さずGetForwardの結果をそのまま読み込む。
	const XMVECTOR position_vector = XMLoadFloat3(&m_position); // 視点のワールド座標を演算用型へ読み込む。
	const XMFLOAT3 forward = GetForward(); // YawとPitchを反映した前方向を求める。
	const XMVECTOR forward_vector = XMLoadFloat3(&forward); // 同じ前方向を行列生成へ渡せる型へ変換する。
	const XMVECTOR up_vector = XMVectorSet(0.0f,1.0f,0.0f,0.0f); // ワールド上方向を使い、Roll回転は加えない。
	return XMMatrixLookToLH(position_vector,forward_vector,up_vector);
}