#pragma once

//========= DirectX インクルード=========
#include <DirectXMath.h>

//========= 前方宣言=========
class InputSystem;

// 一人称視点の位置、回転、視線方向、View行列を管理する。
class FpsCamera final
{
public:
	//========= 更新関数=========
	// マウス移動量を使ってYawとPitchを更新する。
	void Update( InputSystem& input );

	//========= Setter関数=========
	// カメラを指定したワールド座標へ移動する。
	void SetPosition( const DirectX::XMFLOAT3& position )
	{
		m_Position = position;
	}

	//========= Getter関数=========
	// 現在のカメラ位置を返す。
	[[nodiscard]] const DirectX::XMFLOAT3& GetPosition() const
	{
		return m_Position;
	}
	// 現在の水平方向の回転角をラジアンで返す。
	[[nodiscard]] float GetYaw() const
	{
		return m_Yaw;
	}
	// 現在の垂直方向の回転角をラジアンで返す。
	[[nodiscard]] float GetPitch() const
	{
		return m_Pitch;
	}
	// 現在の視線方向を正規化ベクトルで返す。
	[[nodiscard]] DirectX::XMFLOAT3 GetForward() const;
	// 現在の右方向を正規化ベクトルで返す。
	[[nodiscard]] DirectX::XMFLOAT3 GetRight() const;
	// DirectX 11の左手座標系で使用するView行列を返す。
	[[nodiscard]] DirectX::XMMATRIX GetViewMatrix() const;

private:
	//========= カメラ操作設定=========
	// マウス1ピクセルあたりのカメラ回転量をラジアンで保持する。
	float m_MouseSensitivity{ 0.0025f };

	//========= カメラTransform情報=========
	// カメラのワールド座標を保持する。
	DirectX::XMFLOAT3 m_Position{ 0.0f, 1.6f, -5.0f };
	// Y軸を中心とした水平方向の回転角をラジアンで保持する。
	float m_Yaw{};
	// X軸を中心とした垂直方向の回転角をラジアンで保持する。
	float m_Pitch{};
};