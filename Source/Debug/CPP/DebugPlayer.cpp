#include "../H/DebugPlayer.h"

// 視線の水平化、独立したキー判定、合成方向の正規化、速度・秒数の反映、範囲補正をこの順に行う。
// 条件・計算・初期値の説明だけを補完し、移動方向・速度・範囲・呼出契約は変更しない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampで更新後のX・Z座標を固定範囲へ制限する。
#include <cmath> // std::sqrtで水平視線と合成移動方向の長さを求め、正規化に使用する。

//========= Framework インクルード=========
#include "Framework/Input/H/FpsCamera.h"
#include "Framework/Input/H/InputSystem.h"

// デバッグ移動に許可する固定のX・Z区間をまとめる。壁の形状から境界を求める処理ではない。
namespace
{
	//========= 移動範囲定数=========
	// X軸の下限。既存のワールド座標-9.5より小さい位置は、Updateの最後にこの値へ戻す。
	constexpr float ns_STAGE_MIN_X = -9.5f;
	// X軸の上限。既存のワールド座標9.5より大きい位置は、Updateの最後にこの値へ戻す。
	constexpr float ns_STAGE_MAX_X = 9.5f;
	// Z軸の下限。初期位置Z=-5もこの制限を受ける。数値は既存のワールド座標設定を維持する。
	constexpr float ns_STAGE_MIN_Z = -1.5f;
	// Z軸の上限。既存のワールド座標17.5より大きい位置は、Updateの最後にこの値へ戻す。
	constexpr float ns_STAGE_MAX_Z = 17.5f;
}

//========= 更新関数=========
// カメラ基準の移動入力を反映し、入力なし・0秒更新でも最後の範囲補正を実行する。
void DebugPlayer::Update(float deltaTime,const InputSystem& input,const FpsCamera& camera)
{
	// 上下を向いても移動を水平に保ち、Pitchで縮んだ水平方向の長さを正規化前に求める。
	XMFLOAT3 forward = camera.GetForward(); // カメラの現在の前方向をコピーし、以後はローカル値だけを加工する。
	forward.y = 0.0f; // 上下成分を除いてX・Z平面へ投影する。カメラ本体の視線は変えない。
	const float forward_length = std::sqrt(forward.x * forward.x + forward.z * forward.z); // 水平成分の長さを求める。
	// 長さが正の場合だけ単位化する。水平成分が0の場合は除算せず、そのまま移動合成へ渡す。
	if(forward_length > 0.0f){
		forward.x /= forward_length; // X成分を水平距離で割る。
		forward.z /= forward_length; // Z成分も同じ距離で割り、水平ベクトル全体を単位長にする。
	}

	// 左右移動に使うカメラの右方向を取得する。ここでは再正規化せず、後続の計算ではX・Z成分だけ使う。
	const XMFLOAT3 right = camera.GetRight();

	// 複数キーの同時押しを合成するため、4条件はelseで結ばず独立して判定する。初期値0は移動入力なしを表す。
	float move_x{};
	float move_z{};
	// W押下中は水平化した前方向を加算し、カメラの向いている水平方向へ進む。
	if(input.IsKeyPressed('W')){
		move_x += forward.x;
		move_z += forward.z;
	}
	// S押下中は前方向を減算して後退する。Wと同時押しなら同じ前方向成分を相殺する。
	if(input.IsKeyPressed('S')){
		move_x -= forward.x;
		move_z -= forward.z;
	}
	// D押下中は右方向を加算する。前後キーと併用した斜め方向は後段で単位化する。
	if(input.IsKeyPressed('D')){
		move_x += right.x;
		move_z += right.z;
	}
	// A押下中は右方向を減算して左へ進む。Dとの同時押しでは左右成分を相殺する。
	if(input.IsKeyPressed('A')){
		move_x -= right.x;
		move_z -= right.z;
	}

	// 合成後に一度単位化し、斜め入力による速度増加を抑える。前方向の水平化とは別の正規化である。
	const float move_length = std::sqrt(move_x * move_x + move_z * move_z);
	// 合成方向が0でなければ単位長へ揃える。入力なし・相殺時は0除算せず、位置更新へ0成分を渡す。
	if(move_length > 0.0f){
		move_x /= move_length; // 合成方向のX成分をその長さで割る。
		move_z /= move_length; // Z成分も同じ長さで割り、方向比を保って単位化する。
	}

	// 方向×ワールド距離/秒×秒で今回の変位を求める。Yを更新せず、既存順序でX・Zへ加算する。
	m_position.x += move_x * m_moveSpeed * deltaTime;
	m_position.z += move_z * m_moveSpeed * deltaTime;

	// 移動後の座標を固定区間へ収める。入力なし・0秒でも通るため、初期Z=-5は-1.5へ補正される。
	// 境界からはみ出した成分だけを切り詰める処理であり、壁形状との衝突検出や経路の検査ではない。
	m_position.x = std::clamp(m_position.x,ns_STAGE_MIN_X,ns_STAGE_MAX_X);
	m_position.z = std::clamp(m_position.z,ns_STAGE_MIN_Z,ns_STAGE_MAX_Z);
}