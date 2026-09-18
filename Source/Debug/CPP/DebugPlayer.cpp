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
	constexpr float STAGE_MIN_X = -9.5f;
	constexpr float STAGE_MAX_X = 9.5f;

	// 床の表示範囲に合わせたZ軸の移動可能領域。
	constexpr float STAGE_MIN_Z = -1.5f;
	constexpr float STAGE_MAX_Z = 17.5f;
}

// キーボード入力とカメラ方向を使い、地面に沿った位置を更新する。
void DebugPlayer::Update( float deltaTime, const InputSystem& input, const FpsCamera& camera )
{
	// Pitchを除外したカメラ前方向を作り、水平方向へ正規化する。
	DirectX::XMFLOAT3 forward = camera.GetForward();
	forward.y = 0.0f;

	const float forwardLength = std::sqrt( forward.x * forward.x + forward.z * forward.z );

	if ( forwardLength > 0.0f )
	{
		forward.x /= forwardLength;
		forward.z /= forwardLength;
	}

	// カメラの水平方向の右ベクトルを取得する。
	const DirectX::XMFLOAT3 right = camera.GetRight();

	// 前後左右の入力から作成する水平方向の移動ベクトル。
	float moveX{};
	float moveZ{};

	if ( input.IsKeyPressed( 'W' ) )
	{
		moveX += forward.x;
		moveZ += forward.z;
	}

	if ( input.IsKeyPressed( 'S' ) )
	{
		moveX -= forward.x;
		moveZ -= forward.z;
	}

	if ( input.IsKeyPressed( 'D' ) )
	{
		moveX += right.x;
		moveZ += right.z;
	}

	if ( input.IsKeyPressed( 'A' ) )
	{
		moveX -= right.x;
		moveZ -= right.z;
	}

	// 斜め移動時の速度を一定にするため、移動方向を正規化する。
	const float moveLength = std::sqrt( moveX * moveX + moveZ * moveZ );

	if ( moveLength > 0.0f )
	{
		moveX /= moveLength;
		moveZ /= moveLength;
	}

	// 移動速度とdeltaTimeを反映して、床の表示範囲内へ位置を更新する。
	m_Position.x += moveX * m_MoveSpeed * deltaTime;
	m_Position.z += moveZ * m_MoveSpeed * deltaTime;
	m_Position.x = std::clamp( m_Position.x, STAGE_MIN_X, STAGE_MAX_X );
	m_Position.z = std::clamp( m_Position.z, STAGE_MIN_Z, STAGE_MAX_Z );
}