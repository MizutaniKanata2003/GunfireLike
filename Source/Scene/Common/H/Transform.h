#pragma once

// 3D Objectの位置、Euler角、Scaleを保持し、World行列を生成する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

class Transform final
{
public:
	//========= Setter関数=========
	// Objectのワールド座標を設定する。
	// position: 設定するワールド座標。
	void SetPosition(const XMFLOAT3& position) { m_position = position; }
	// ObjectのEuler角を設定する。
	// rotation: X、Y、Z軸周りの回転角。単位はラジアン。
	void SetRotation(const XMFLOAT3& rotation) { m_rotation = rotation; }
	// Objectの各軸の拡大縮小率を設定する。
	// scale: X、Y、Z軸方向の拡大縮小率。
	void SetScale(const XMFLOAT3& scale) { m_scale = scale; }

	//========= Getter関数=========
	// 保持しているワールド座標への読み取り専用参照を返す。
	[[nodiscard]] const XMFLOAT3& GetPosition() const { return m_position; }
	// 保持しているEuler角への読み取り専用参照を返す。単位はラジアン。
	[[nodiscard]] const XMFLOAT3& GetRotation() const { return m_rotation; }
	// 保持している各軸の拡大縮小率への読み取り専用参照を返す。
	[[nodiscard]] const XMFLOAT3& GetScale() const { return m_scale; }
	// Scale、Rotation、Translationの順に合成したWorld行列を返す。
	[[nodiscard]] XMMATRIX GetWorldMatrix() const;

private:
	//========= Transform状態=========
	// Objectのワールド座標。
	XMFLOAT3 m_position{};
	// X、Y、Z軸周りのEuler角。単位はラジアン。
	XMFLOAT3 m_rotation{};
	// X、Y、Z軸方向の拡大縮小率。
	XMFLOAT3 m_scale{1.0f,1.0f,1.0f};
};