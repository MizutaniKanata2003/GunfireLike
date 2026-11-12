#pragma once

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class FpsCamera;
class InputSystem;

// FPS操作とカメラ追従を検証するためのデバッグプレイヤー。
class DebugPlayer final
{
public:
	//========= 更新関数=========
	// キーボード入力とカメラ方向を使い、地面に沿った位置を更新する。
	// deltaTime: この更新で進める時間。単位は秒。
	// input: 移動キーの状態を取得する非所有参照。
	// camera: 移動方向の計算に使用する非所有参照。
	void Update(float deltaTime,const InputSystem& input,const FpsCamera& camera);

	//========= Getter関数=========
	// 現在のデバッグプレイヤー位置を返す。
	[[nodiscard]] const XMFLOAT3& GetPosition() const { return m_position; }
private:
	//========= メンバー変数=========
	// カメラ追従とデバッグ表示に使用するプレイヤー位置。
	XMFLOAT3 m_position{0.0f,1.6f,-5.0f};
	// 1秒あたりの水平移動距離。
	float m_moveSpeed{6.0f};
};