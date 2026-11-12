#include "../H/DebugPlayer.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <cmath>

//========= Framework インクルード=========
#include "Framework/Input/H/FpsCamera.h"
#include "Framework/Input/H/InputSystem.h"

namespace
{
	//========= 移動範囲定数=========
	// 床の表示範囲に合わせたX軸の移動可能領域。
	constexpr float ns_STAGE_MIN_X = -9.5f;
	constexpr float ns_STAGE_MAX_X = 9.5f;

	// 床の表示範囲に合わせたZ軸の移動可能領域。
	constexpr float ns_STAGE_MIN_Z = -1.5f;
	constexpr float ns_STAGE_MAX_Z = 17.5f;
}

// キーボード入力とカメラ方向を使い、地面に沿った位置を更新する。
void DebugPlayer::Update(float deltaTime,const InputSystem& input,const FpsCamera& camera)
{
	// Pitchを除外したカメラ前方向を作り、水平方向へ正規化する。
	XMFLOAT3 forward = camera.GetForward();
	forward.y = 0.0f;

	const float forward_length = std::sqrt(forward.x * forward.x + forward.z * forward.z);

	if(forward_length > 0.0f){
		forward.x /= forward_length;
		forward.z /= forward_length;
	}

	// カメラの水平方向の右ベクトルを取得する。
	const XMFLOAT3 right = camera.GetRight();

	// 前後左右の入力から作成する水平方向の移動ベクトル。
	float move_x{};
	float move_z{};

	if(input.IsKeyPressed('W')){
		move_x += forward.x;
		move_z += forward.z;
	}

	if(input.IsKeyPressed('S')){
		move_x -= forward.x;
		move_z -= forward.z;
	}

	if(input.IsKeyPressed('D')){
		move_x += right.x;
		move_z += right.z;
	}

	if(input.IsKeyPressed('A')){
		move_x -= right.x;
		move_z -= right.z;
	}

	// 斜め移動時の速度を一定にするため、移動方向を正規化する。
	const float move_length = std::sqrt(move_x * move_x + move_z * move_z);

	if(move_length > 0.0f){
		move_x /= move_length;
		move_z /= move_length;
	}

	// 移動速度とdeltaTimeを反映して、床の表示範囲内へ位置を更新する。
	m_position.x += move_x * m_moveSpeed * deltaTime;
	m_position.z += move_z * m_moveSpeed * deltaTime;
	m_position.x = std::clamp(m_position.x,ns_STAGE_MIN_X,ns_STAGE_MAX_X);
	m_position.z = std::clamp(m_position.z,ns_STAGE_MIN_Z,ns_STAGE_MAX_Z);
}