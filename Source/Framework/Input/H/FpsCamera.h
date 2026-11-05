#pragma once

// 一人称視点の位置、Yaw、Pitch、視線方向、View行列を管理する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class InputSystem;

class FpsCamera final
{
public:
	//========= 更新関数=========
	// Mouse Capture中のマウス移動量からYawとPitchを更新する。
	// input: Mouse Capture状態と、このフレームのマウス移動量を取得する入力System。
	// 戻り値: なし。
	void Update(InputSystem& input);

	//========= Getter関数=========
	// 現在のカメラ位置を取得する。
	// 戻り値: 保持しているワールド座標への読み取り専用参照。
	[[nodiscard]] const XMFLOAT3& GetPosition() const { return m_position; }
	// 現在の水平方向の回転角を取得する。
	// 戻り値: Y軸周りの回転角。単位はラジアン。
	[[nodiscard]] float GetYaw() const { return m_yaw; }
	// 現在の垂直方向の回転角を取得する。
	// 戻り値: X軸周りの回転角。単位はラジアン。
	[[nodiscard]] float GetPitch() const { return m_pitch; }
	// 現在の視線方向を計算する。
	// 戻り値: PitchとYawを反映した正規化済みの前方向ベクトル。
	[[nodiscard]] XMFLOAT3 GetForward() const;
	// 現在の右方向を計算する。
	// 戻り値: ワールド上方向と前方向から求めた正規化済みの右方向ベクトル。
	[[nodiscard]] XMFLOAT3 GetRight() const;
	// DirectX 11の左手座標系で使用するView行列を生成する。
	// 戻り値: 現在位置と視線方向を反映したView行列。
	[[nodiscard]] XMMATRIX GetViewMatrix() const;

	//========= Setter関数=========
	// カメラを指定したワールド座標へ移動する。
	// position: 設定するカメラのワールド座標。
	// 戻り値: なし。
	void SetPosition(const XMFLOAT3& position) { m_position = position; }

private:
	//========= カメラ操作設定=========
	// マウス1ピクセルあたりのカメラ回転量。単位はラジアン。
	float m_mouseSensitivity{0.0025f};

	//========= カメラTransform情報=========
	// カメラのワールド座標。
	XMFLOAT3 m_position{0.0f,1.6f,-5.0f};
	// Y軸を中心とした水平方向の回転角。単位はラジアン。
	float m_yaw{};
	// X軸を中心とした垂直方向の回転角。単位はラジアン。
	float m_pitch{};
};