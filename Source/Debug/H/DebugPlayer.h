#pragma once

//========= DirectX インクルード=========
#include <DirectXMath.h>

//========= 前方宣言=========
class FpsCamera;
class InputSystem;

// FPS操作とカメラ追従を検証するためのデバッグプレイヤー。
class DebugPlayer final
{
public:
	//========= 更新関数=========
	// キーボード入力とカメラ方向を使い、地面に沿った位置を更新する。
	void Update( float deltaTime, const InputSystem& input, const FpsCamera& camera );

	//========= Getter関数=========
	// 現在のデバッグプレイヤー位置を返す。
	[[nodiscard]] const DirectX::XMFLOAT3& GetPosition() const
	{
		return m_Position;
	}

private:
	//========= メンバー変数=========
	// カメラ追従とデバッグ表示に使用するプレイヤー位置。
	DirectX::XMFLOAT3 m_Position{ 0.0f, 1.6f, -5.0f };

	// 1秒あたりの水平移動距離。
	float m_MoveSpeed{ 6.0f };
};