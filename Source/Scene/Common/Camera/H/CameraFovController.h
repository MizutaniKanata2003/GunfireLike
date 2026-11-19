#pragma once

// 動作の状態（通常・ダッシュ・しゃがみ・スライディング・構え）に応じた、垂直方向の視野角の目標値を決め、現在の視野角を目標値へ滑らかに近づける。
// 視野角の値を保持するだけで、投影行列の生成・Clip面の距離・描画は担当しない。数値は企画書で未確定の試遊用の仮値。単位は度。

//========= Scene インクルード=========
#include "Scene/Common/Player/H/PlayerController.h"

// 視野角の補間を行う。
class CameraFovController final
{
public:
	//========= 初期化関数=========
	// 現在の視野角を、通常時の値へ戻す。
	void Reset();

	//========= 更新関数=========
	// 動作の状態に応じた目標値へ、現在の視野角を滑らかに近づける。
	// deltaTime: この更新で進める経過時間。単位は秒。実時間。
	// actionState: プレイヤーの現在の動作の状態。
	// isAiming: プレイヤーが構えているか。スライディングとダッシュを除き、しゃがみ中でも構えを優先する。
	void Update(float deltaTime,PLAYER_ACTION_STATE actionState,bool isAiming);

	//========= Getter関数=========
	// 現在の垂直方向の視野角を返す。単位は度。投影行列の生成時にラジアンへ変換する。
	[[nodiscard]] float GetFovDegrees() const { return m_fovDegrees; }

private:
	//========= 視野角の状態=========
	// 現在の視野角。単位は度。動作の状態に応じた目標値へ滑らかに近づく。
	float m_fovDegrees{};
};
