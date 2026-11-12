#include "../H/DebugPlayer.h"

// カメラ基準の移動入力を水平ベクトルへ変換し、速度・経過時間と固定範囲制限を反映する。
// 入力順・正規化・移動計算・最後のclampを既存のまま維持する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <cmath>

//========= Framework インクルード=========
#include "Framework/Input/H/FpsCamera.h"
#include "Framework/Input/H/InputSystem.h"

// デバッグ移動で使用するX・Zの固定範囲をまとめる。壁との形状ベースの衝突判定ではない。
namespace
{
	//========= 移動範囲定数=========
	// 床の表示範囲に合わせたX軸位置の下限。
	constexpr float ns_STAGE_MIN_X = -9.5f;
	// 床の表示範囲に合わせたX軸位置の上限。
	constexpr float ns_STAGE_MAX_X = 9.5f;
	// 床の表示範囲に合わせたZ軸位置の下限。
	constexpr float ns_STAGE_MIN_Z = -1.5f;
	// 床の表示範囲に合わせたZ軸位置の上限。
	constexpr float ns_STAGE_MAX_Z = 17.5f;
}

//========= 更新関数=========
// 入力方向を正規化して移動し、入力の有無にかかわらず最後に位置範囲を補正する。
void DebugPlayer::Update(float deltaTime,const InputSystem& input,const FpsCamera& camera)
{
	// 視線のY成分を除外し、上下を向いても前後移動が地面に沿うよう水平成分を正規化する。
	XMFLOAT3 forward = camera.GetForward();
	forward.y = 0.0f;
	const float forward_length = std::sqrt(forward.x * forward.x + forward.z * forward.z);
	if(forward_length > 0.0f){
		forward.x /= forward_length;
		forward.z /= forward_length;
	}

	// カメラが算出した右方向を取得する。移動計算にはX・Zだけを使用する。
	const XMFLOAT3 right = camera.GetRight();

	// 同時押しを加算するため、各キーを独立に判定する。反対方向の入力は加算結果で相殺される。
	float move_x{};
	float move_z{};
	if(input.IsKeyPressed('W')){
		move_x += forward.x;
		move_z += forward.z;
	}
	if(input.IsKeyPressed('S')){
		move_x -= forward.x;
		move_z -= forward.z;
	}
	if(input.IsKeyPressed('D')){
		move_x += right.x;
		move_z += right.z;
	}
	if(input.IsKeyPressed('A')){
		move_x -= right.x;
		move_z -= right.z;
	}

	// 合成した方向を単位長にし、斜め入力で移動速度が増えないようにする。長さ0では除算しない。
	const float move_length = std::sqrt(move_x * move_x + move_z * move_z);
	if(move_length > 0.0f){
		move_x /= move_length;
		move_z /= move_length;
	}

	// 秒単位の経過時間と速度を反映し、Yを変えずX・Zだけを移動する。
	m_position.x += move_x * m_moveSpeed * deltaTime;
	m_position.z += move_z * m_moveSpeed * deltaTime;

	// 入力なし・0秒更新でも必ず制限するため、既定位置が範囲外でも初回Updateで補正される。
	m_position.x = std::clamp(m_position.x,ns_STAGE_MIN_X,ns_STAGE_MAX_X);
	m_position.z = std::clamp(m_position.z,ns_STAGE_MIN_Z,ns_STAGE_MAX_Z);
}