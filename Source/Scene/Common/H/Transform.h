#pragma once

// 3D Objectの位置・Euler角・Scaleを保持し、LocalからWorldへの変換行列を生成する。
// 親子階層の合成、移動範囲の制限、GPU転送用のtransposeは担当しない。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// Scale・Rotation・Translationを個別に保持するTransform。
class Transform final
{
public:
	//========= Setter関数=========
	// 位置をコピーする。移動量の加算や範囲制限は行わない。
	// position: 設定するワールド座標。参照自体は保持しない。
	void SetPosition(const XMFLOAT3& position) { m_position = position; }
	// Euler角をコピーする。角度の制限や周期内への折り返しは行わない。
	// rotation: X=Pitch、Y=Yaw、Z=Rollの回転角。単位はラジアン。
	void SetRotation(const XMFLOAT3& rotation) { m_rotation = rotation; }
	// 各軸の拡大縮小率をコピーする。0や負値もそのまま保持する。
	// scale: X・Y・Zの倍率。位置や回転は変更しない。
	void SetScale(const XMFLOAT3& scale) { m_scale = scale; }

	//========= Getter関数=========
	// 保持するワールド座標への読み取り専用参照を返す。参照先はこの実体の寿命まで有効。
	[[nodiscard]] const XMFLOAT3& GetPosition() const { return m_position; }
	// 保持するEuler角への読み取り専用参照を返す。単位はラジアン。参照先はこの実体の寿命まで有効。
	[[nodiscard]] const XMFLOAT3& GetRotation() const { return m_rotation; }
	// 保持する各軸倍率への読み取り専用参照を返す。参照先はこの実体の寿命まで有効。
	[[nodiscard]] const XMFLOAT3& GetScale() const { return m_scale; }
	// S*R*Tの順に合成したWorld行列を返す。呼出時に計算し、キャッシュやtransposeは行わない。
	[[nodiscard]] XMMATRIX GetWorldMatrix() const;

private:
	//========= Transform状態=========
	// World行列の平行移動へ使用する座標。既定値は原点。
	XMFLOAT3 m_position{};
	// 回転行列へ渡すPitch・Yaw・Roll。単位はラジアン。既定値は全軸0。
	XMFLOAT3 m_rotation{};
	// 拡大縮小行列へ使用する各軸倍率。既定値は全軸1。
	XMFLOAT3 m_scale{1.0f,1.0f,1.0f};
};