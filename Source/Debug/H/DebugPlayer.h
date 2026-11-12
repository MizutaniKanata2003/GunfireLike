#pragma once

// WASDによる水平移動とカメラ追従の検証用に、プレイヤー位置を保持する。
// 描画・視線更新・Scene遷移・物理衝突判定は担当しない。範囲制限は固定のX・Z区間へのclampで行う。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class FpsCamera;
class InputSystem;

// カメラの向きを移動方向に使うデバッグ用プレイヤー。
class DebugPlayer final
{
public:
	//========= 更新関数=========
	// WASD入力をカメラ基準の水平方向へ変換し、位置を更新してX・Zを固定範囲へ制限する。
	// deltaTime: 移動量に掛ける経過時間。単位は秒。0秒でも最後の範囲補正は実行する。
	// input: 移動キーの押下状態を取得する非所有参照。Capture状態はこのクラスでは判定しない。
	// camera: 前・右方向の取得先。非所有参照で、位置や向きは変更しない。
	// Y座標は維持する。移動を止めるCapture判定などは呼出側で行う。
	void Update(float deltaTime,const InputSystem& input,const FpsCamera& camera);

	//========= Getter関数=========
	// 現在位置への読み取り専用参照を返す。参照先の寿命はこのプレイヤーの寿命まで。
	[[nodiscard]] const XMFLOAT3& GetPosition() const { return m_position; }

private:
	//========= プレイヤー状態=========
	// カメラ追従とデバッグ表示に使用するワールド座標。既定Zは初回Updateで範囲補正される。
	XMFLOAT3 m_position{0.0f,1.6f,-5.0f};
	// 入力方向の正規化後に掛ける水平移動速度。単位はワールド座標の距離/秒。
	float m_moveSpeed{6.0f};
};