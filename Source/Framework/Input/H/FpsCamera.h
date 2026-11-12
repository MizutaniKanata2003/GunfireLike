#pragma once

// 一人称視点の位置・Yaw・Pitchを保持し、方向ベクトルと左手系View行列を生成する。
// 移動位置の決定や描画は担当しない。位置の追従は呼出側がSetPositionで反映する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class InputSystem;

// マウス入力で視線方向を更新するカメラ。公開クラス名は既存参照との互換性のため維持する。
class FpsCamera final
{
public:
	//========= 更新関数=========
	// FPS操作中のマウス差分を消費し、YawとPitchを更新する。位置は変更しない。
	// input: FPS操作状態と累積差分の取得先。ConsumeMouseDeltaで取得先の差分をクリアする。
	// 解除中は何もしない。回転量にdeltaTimeは掛けず、Pitchだけを±89度へ制限する。
	void Update(InputSystem& input);

	//========= Getter関数=========
	// 保持するワールド座標への読み取り専用参照を返す。参照先の寿命はこのCameraの寿命まで。
	[[nodiscard]] const XMFLOAT3& GetPosition() const { return m_position; }
	// Y軸周りの水平方向の回転角をラジアンで返す。周期内へ折り返す処理は行わない。
	[[nodiscard]] float GetYaw() const { return m_yaw; }
	// 垂直方向の回転角をラジアンで返す。正値は上向きの視線に対応する。
	[[nodiscard]] float GetPitch() const { return m_pitch; }
	// YawとPitchを反映した単位前方向ベクトルを返す。両角度が0なら+Z方向。
	[[nodiscard]] XMFLOAT3 GetForward() const;
	// ワールド上方向と前方向の外積を正規化した右方向ベクトルを返す。
	[[nodiscard]] XMFLOAT3 GetRight() const;
	// 現在位置・前方向・ワールド上方向から左手系View行列を返す。
	[[nodiscard]] XMMATRIX GetViewMatrix() const;

	//========= Setter関数=========
	// 位置をコピーする。向きの変更や、移動範囲の制限は行わない。
	// position: 設定するワールド座標。呼出後に引数への参照は保持しない。
	void SetPosition(const XMFLOAT3& position) { m_position = position; }

private:
	//========= カメラ操作設定=========
	// マウス差分1pixelに対する回転倍率。単位はラジアン/pixel。
	float m_mouseSensitivity{0.0025f};

	//========= カメラTransform情報=========
	// カメラのワールド座標。既定値はプレイヤー追従反映前の初期位置。
	XMFLOAT3 m_position{0.0f,1.6f,-5.0f};
	// 水平方向の回転角。単位はラジアン。
	float m_yaw{};
	// 垂直方向の回転角。単位はラジアン。Updateで制限する。
	float m_pitch{};
};