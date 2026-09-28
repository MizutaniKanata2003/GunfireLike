#pragma once

//========= DirectX インクルード=========
#include <DirectXMath.h>

// 3D ObjectのPosition、Rotation、Scale、World行列を管理する。
class Transform final
{
public:
	//========= Setter関数=========
	// ObjectのWorld座標を設定する。
	void SetPosition( const DirectX::XMFLOAT3& position ) { m_Position = position; }
	// ObjectのEuler角RotationをRadianで設定する。
	void SetRotation( const DirectX::XMFLOAT3& rotation ) { m_Rotation = rotation; }
	// Objectの各軸Scaleを設定する。
	void SetScale( const DirectX::XMFLOAT3& scale ) { m_Scale = scale; }

	//========= Getter関数=========
	// ObjectのWorld座標を返す。
	[[nodiscard]] const DirectX::XMFLOAT3& GetPosition() const { return m_Position; }
	// ObjectのEuler角RotationをRadianで返す。
	[[nodiscard]] const DirectX::XMFLOAT3& GetRotation() const { return m_Rotation; }
	// Objectの各軸Scaleを返す。
	[[nodiscard]] const DirectX::XMFLOAT3& GetScale() const { return m_Scale; }
	// Scale、Rotation、Translationを合成したWorld行列を返す。
	[[nodiscard]] DirectX::XMMATRIX GetWorldMatrix() const;
private:
	//========= Transform状態=========
	// ObjectのWorld座標。
	DirectX::XMFLOAT3 m_Position{};
	// ObjectのEuler角Rotation。単位はRadian。
	DirectX::XMFLOAT3 m_Rotation{};
	// Objectの各軸Scale。
	DirectX::XMFLOAT3 m_Scale{ 1.0f, 1.0f, 1.0f };
};