#include "../H/FpsCamera.h"

// マウス差分を視線角度へ変換し、反動によるずれの補間と復帰、方向ベクトルと左手系View行列の計算を行う。
// 感度・回転符号・Pitch制限・外積順・行列生成を維持し、条件・値・計算の意味を補完する。

//========= Windows インクルード=========
#include <windows.h> // ConsumeMouseDeltaが返すPOINTのX・Yを受け取り、視線更新の入力に使用する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampでPitchと反動のずれを上下限内に収め、std::minで補間の割合を1以下に制限する。
#include <cmath> // std::sin・std::cosでYawとPitchから前方向の各成分を計算し、std::fabsで反動のずれの大きさを調べる。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

// 視線の上下回転の制限と、反動の補間・復帰の数値をまとめる。反動の数値は企画書で未確定の仮値。
namespace
{
	//========= Pitch制限定数=========
	// 上下に許可する角度の絶対値。89度を維持し、Updateでは負値と正値を下限・上限に使う。
	// ±90度を超える視線反転を避ける範囲。89という具体値の採用経緯は未確認。
	constexpr float ns_MAX_PITCH_DEGREES = 89.0f;
	// 三角関数と保持角度がラジアンを使うため、同じ上限を度から変換する。感度や時間を掛ける値ではない。
	constexpr float ns_MAX_PITCH_RADIANS = XMConvertToRadians(ns_MAX_PITCH_DEGREES);

	//========= 反動の補間・復帰=========
	// 実際のずれが目標へ近づく速さ。1秒あたりに差の何倍を縮めるかを表し、大きいほど速く、小さいほど滑らかに見える。仮値。
	constexpr float ns_RECOIL_FOLLOW_RATE = 16.0f;
	// 最後の反動から、目標が0へ戻り始めるまでの時間。単位は秒。連射の間は戻り始めないよう、連射の間隔より短い仮値。
	constexpr float ns_RECOIL_RECOVERY_DELAY_SECONDS = 0.15f;
	// 目標が0へ戻る速さ。1秒あたりに残りの何倍を縮めるかを表す。仮値。
	constexpr float ns_RECOIL_RECOVERY_RATE = 6.0f;
	// 反動のずれの目標のPitchの上限。単位はラジアン。連射で視線が上へ上がり続けないための仮値で、12度。
	constexpr float ns_MAX_RECOIL_PITCH_RADIANS = XMConvertToRadians(12.0f);
	// 反動のずれの目標のYawの上限。単位はラジアン。左右へ振れ続けないための仮値で、6度。
	constexpr float ns_MAX_RECOIL_YAW_RADIANS = XMConvertToRadians(6.0f);
	// この大きさ（ラジアン）より小さいずれは、0とみなして0へ揃える。指数的な復帰が、いつまでも0にならないことを防ぐ。
	constexpr float ns_RECOIL_SNAP_EPSILON = 0.0002f;
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

// 反動の目標を、時間が経つと0へ戻し、実際のずれを目標へ滑らかに近づける。
void FpsCamera::UpdateRecoil(float deltaTime)
{
	// 最後の反動からの経過時間を進める。
	m_secondsSinceRecoil += deltaTime;

	// 最後の反動から一定時間が過ぎたら、目標を0へ向けて指数的に縮める。連射中は経過時間が0へ戻るため、縮まない。
	if(m_secondsSinceRecoil > ns_RECOIL_RECOVERY_DELAY_SECONDS){
		const float recovery_blend = std::min(1.0f,ns_RECOIL_RECOVERY_RATE * deltaTime);
		m_recoilTargetYaw -= m_recoilTargetYaw * recovery_blend;
		m_recoilTargetPitch -= m_recoilTargetPitch * recovery_blend;

		// 十分に小さくなった目標は0へ揃え、完全に元の視線へ戻す。
		if(std::fabs(m_recoilTargetYaw) < ns_RECOIL_SNAP_EPSILON){
			m_recoilTargetYaw = 0.0f;
		}
		// Pitchの目標も同様に、十分に小さければ0へ揃える。
		if(std::fabs(m_recoilTargetPitch) < ns_RECOIL_SNAP_EPSILON){
			m_recoilTargetPitch = 0.0f;
		}
	}

	// 実際のずれを、目標へ滑らかに近づける。1回の更新で差を超えないよう、近づく割合は1以下に制限する。
	const float follow_blend = std::min(1.0f,ns_RECOIL_FOLLOW_RATE * deltaTime);
	m_recoilYaw += (m_recoilTargetYaw - m_recoilYaw) * follow_blend;
	m_recoilPitch += (m_recoilTargetPitch - m_recoilPitch) * follow_blend;

	// 目標が0で、実際のずれも十分に小さければ、0へ揃える。目標が残っている間は揃えない。
	if(m_recoilTargetYaw == 0.0f && std::fabs(m_recoilYaw) < ns_RECOIL_SNAP_EPSILON){
		m_recoilYaw = 0.0f;
	}
	// Pitchも同様に、目標が0で十分に小さければ0へ揃える。
	if(m_recoilTargetPitch == 0.0f && std::fabs(m_recoilPitch) < ns_RECOIL_SNAP_EPSILON){
		m_recoilPitch = 0.0f;
	}
}

// 反動の目標へ、引数の回転量を加え、上限を超えないよう制限して、経過時間を0へ戻す。
void FpsCamera::ApplyRecoil(float yawRadians,float pitchRadians)
{
	// 目標へ加え、上限の内に収める。実際の視線は、UpdateRecoilで目標へ近づく。
	m_recoilTargetYaw = std::clamp(m_recoilTargetYaw + yawRadians,-ns_MAX_RECOIL_YAW_RADIANS,ns_MAX_RECOIL_YAW_RADIANS);
	m_recoilTargetPitch = std::clamp(m_recoilTargetPitch + pitchRadians,-ns_MAX_RECOIL_PITCH_RADIANS,ns_MAX_RECOIL_PITCH_RADIANS);

	// 反動を加えた直後は、目標が0へ戻り始めないよう、経過時間を0へ戻す。
	m_secondsSinceRecoil = 0.0f;
}

//========= Getter関数=========
// 保持角度と反動のずれから前方向を生成する。位置を加えないため、注視点ではなく方向ベクトルを返す。
XMFLOAT3 FpsCamera::GetForward() const
{
	// マウスで決めた角度に、反動のずれを加える。Pitchは、反動を加えた後も、上下限の内に収める。
	const float yaw = m_yaw + m_recoilYaw;
	const float pitch = std::clamp(m_pitch + m_recoilPitch,-ns_MAX_PITCH_RADIANS,ns_MAX_PITCH_RADIANS);

	// Pitchが上下へ傾くほど水平成分を縮め、Yへsin(Pitch)を使う。角度はラジアンで解釈する。
	const float cos_pitch = std::cos(pitch);
	return
	{
		std::sin(yaw) * cos_pitch, // X: Yawの左右成分へ水平面への投影倍率を掛ける。
		std::sin(pitch), // Y: Pitchの正値は上向き、負値は下向きの成分になる。
		std::cos(yaw) * cos_pitch // Z: Yaw=0かつPitch=0で+Zを向く。追加の正規化は行わない。
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
	const XMFLOAT3 forward = GetForward(); // YawとPitchと反動のずれを反映した前方向を求める。
	const XMVECTOR forward_vector = XMLoadFloat3(&forward); // 同じ前方向を行列生成へ渡せる型へ変換する。
	const XMVECTOR up_vector = XMVectorSet(0.0f,1.0f,0.0f,0.0f); // ワールド上方向を使い、Roll回転は加えない。
	return XMMatrixLookToLH(position_vector,forward_vector,up_vector);
}