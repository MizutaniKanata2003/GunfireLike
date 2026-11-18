#include "../H/PlayerController.h"

// 動作の状態の切替、状態ごとの移動速度・視点の高さ、構え、地形のAABBからの押し戻しを実装する。移動方向は視線を水平面へ投影して求める。
// 移動を小さな距離に分けて押し戻すことで、薄い壁を1回の移動で通り抜けることを防ぐ。数値は企画書で未確定の仮値。

//========= Windows インクルード=========
#include <windows.h> // VK_SHIFT・VK_CONTROL・VK_SPACE・VK_LBUTTON・VK_RBUTTONで、ダッシュ・しゃがみ・回避・構え・射撃の入力を判定する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで、視点の高さを近づける割合とスライディングの進み具合を1以下に制限する。
#include <cmath> // std::sqrtで水平視線と合成移動方向の長さを求めて正規化し、std::ceilで移動の分割数を求める。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionResolver.h"
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Input/H/FpsCamera.h"
#include "Framework/Input/H/InputSystem.h"

// 移動の速度・時間・視点の高さ・体の大きさをまとめる。企画書で未確定の数値は、プレイ確認後に調整する仮値。
namespace
{
	//========= 体の大きさ=========
	// 壁との押し戻しに使う球の半径。単位はワールド距離単位。
	// 内側の面がX=±9.9・Z=-1.9・Z=17.9の壁に対し、中心の移動範囲がX=±9.5・Z=-1.5〜17.5（旧DebugPlayerの範囲）と一致する値。
	constexpr float ns_BODY_RADIUS = 0.4f;
	// 1回の押し戻しの前に進める最大距離。単位はワールド距離単位。壁の厚み（0.2）以下にして、中心が壁の反対側へ抜けないようにする。
	constexpr float ns_MAX_STEP_DISTANCE = 0.2f;

	//========= 移動速度=========
	// 通常時の水平移動速度。単位はワールド距離単位/秒。旧DebugPlayerの設定と同じ。
	constexpr float ns_WALK_SPEED = 6.0f;
	// 構え中の移動速度の倍率。企画書では「構え射撃は移動速度が低下する」とだけ定められており、具体値は未確定の仮値。
	constexpr float ns_AIM_SPEED_MULTIPLIER = 0.5f;
	// ダッシュの移動速度。単位はワールド距離単位/秒。通常の1.5倍の仮値。
	constexpr float ns_DASH_SPEED = 9.0f;
	// しゃがみ中の移動速度の倍率。仮値。
	constexpr float ns_CROUCH_SPEED_MULTIPLIER = 0.5f;

	//========= スライディング=========
	// スライディングの長さ。単位は秒。仮値。
	constexpr float ns_SLIDE_SECONDS = 0.8f;
	// スライディング開始時の速度。単位はワールド距離単位/秒。ダッシュより速い仮値。
	constexpr float ns_SLIDE_START_SPEED = 10.0f;
	// スライディング終了時の速度。単位はワールド距離単位/秒。開始から終了へ、時間に比例して減速する。
	constexpr float ns_SLIDE_END_SPEED = 4.0f;

	//========= 回避=========
	// 回避の長さ。単位は秒。途中でやめられない。仮値。
	constexpr float ns_DODGE_SECONDS = 0.3f;
	// 回避の移動速度。単位はワールド距離単位/秒。0.3秒で約3.6進む仮値。
	constexpr float ns_DODGE_SPEED = 12.0f;
	// 回避の開始から無敵が続く時間。単位は秒。回避の長さ以下の仮値。
	constexpr float ns_DODGE_INVINCIBLE_SECONDS = 0.25f;
	// 一度も回避していない状態を表す、十分に大きい経過時間。単位は秒。
	constexpr float ns_SECONDS_WITHOUT_DODGE = 1000.0f;

	//========= 視点の高さ=========
	// 立っている時の視点の高さ。単位はワールド距離単位。旧DebugPlayerの初期の高さ。
	constexpr float ns_STAND_EYE_HEIGHT = 1.6f;
	// しゃがみ中の視点の高さ。仮値。
	constexpr float ns_CROUCH_EYE_HEIGHT = 1.0f;
	// スライディング中の視点の高さ。しゃがみより低い仮値。
	constexpr float ns_SLIDE_EYE_HEIGHT = 0.8f;
	// 回避中の視点の高さ。少し低くして、身をかわす動きを出す仮値。
	constexpr float ns_DODGE_EYE_HEIGHT = 1.3f;
	// 視点の高さが目標へ近づく速さ。1秒あたりに差の何倍を縮めるかを表し、大きいほど速い。仮値。
	constexpr float ns_EYE_HEIGHT_SMOOTHING_RATE = 14.0f;
}

//========= 初期化・更新関数=========
// 開始位置を設定し、動作の状態を通常へ戻す。
void PlayerController::Initialize(const XMFLOAT3& startPosition)
{
	// 位置と動作の状態を既定へ戻す。
	m_position = startPosition;
	m_actionState = PLAYER_ACTION_STATE::e_NORMAL;
	m_stateElapsedSeconds = 0.0f;
	m_isAiming = false;

	// 固定方向・直近の移動方向・ダッシュの禁止・回避からの経過時間を初期値へ戻す。直近の移動方向の初期値は+Z（前方）。
	m_fixedDirectionX = 0.0f;
	m_fixedDirectionZ = 0.0f;
	m_lastMoveDirectionX = 0.0f;
	m_lastMoveDirectionZ = 1.0f;
	m_isDashLocked = false;
	m_secondsSinceDodgeStart = ns_SECONDS_WITHOUT_DODGE;
}

// 動作の状態の切替・移動・視点の高さ・構えを更新し、分割した移動ごとに押し戻す。
void PlayerController::Update(float deltaTime,const InputSystem& input,const FpsCamera& camera,const CollisionWorld& collisionWorld)
{
	// 上下を向いても移動を水平に保つため、前方向のY成分を除き、水平成分の長さで割って単位化する。
	XMFLOAT3 forward = camera.GetForward();
	forward.y = 0.0f;
	const float forward_length = std::sqrt(forward.x * forward.x + forward.z * forward.z);
	// 長さが正の場合だけ単位化する。真上・真下を向いて水平成分が0の場合は、割らずに0のまま使う。
	if(forward_length > 0.0f){
		forward.x /= forward_length;
		forward.z /= forward_length;
	}

	// 左右移動に使うカメラの右方向を取得する。X・Z成分だけを使う。
	const XMFLOAT3 right = camera.GetRight();

	// 4つのキーは同時押しを合成するため、else で結ばず独立して判定する。初期値0は移動入力なしを表す。
	float move_x{};
	float move_z{};
	// Wを押している間は、水平化した前方向を加える。
	if(input.IsKeyPressed('W')){
		move_x += forward.x;
		move_z += forward.z;
	}
	// Sを押している間は、前方向を引いて後退する。Wと同時なら相殺する。
	if(input.IsKeyPressed('S')){
		move_x -= forward.x;
		move_z -= forward.z;
	}
	// Dを押している間は、右方向を加える。
	if(input.IsKeyPressed('D')){
		move_x += right.x;
		move_z += right.z;
	}
	// Aを押している間は、右方向を引いて左へ進む。Dと同時なら相殺する。
	if(input.IsKeyPressed('A')){
		move_x -= right.x;
		move_z -= right.z;
	}

	// 斜め入力で速くならないよう、合成した方向を単位化する。長さが0（入力なし・相殺）の場合は割らずに0のままにする。
	const float move_length = std::sqrt(move_x * move_x + move_z * move_z);
	const bool has_move_input = move_length > 0.0f;
	// 合成方向の長さが正の場合だけ、単位長に揃え、直近の移動方向として記録する。
	if(has_move_input){
		move_x /= move_length;
		move_z /= move_length;
		m_lastMoveDirectionX = move_x;
		m_lastMoveDirectionZ = move_z;
	}

	// 経過時間を進め、入力から動作の状態を切り替える。回避からの経過時間は、回避中も、回避後も増え続ける。
	m_stateElapsedSeconds += deltaTime;
	m_secondsSinceDodgeStart += deltaTime;
	UpdateActionState(input,has_move_input,move_x,move_z);

	// 右クリックを押している間は構える。ただし、ダッシュ中と回避中は構えられず、構え入力があっても解除される。
	m_isAiming = input.IsKeyPressed(VK_RBUTTON) && m_actionState != PLAYER_ACTION_STATE::e_DASH && m_actionState != PLAYER_ACTION_STATE::e_DODGE;

	// 動作の状態ごとに、移動方向・移動速度・目標の視点の高さを決める。初期値は通常の移動。
	float direction_x = move_x;
	float direction_z = move_z;
	float speed = m_isAiming ? ns_WALK_SPEED * ns_AIM_SPEED_MULTIPLIER : ns_WALK_SPEED;
	float target_eye_height = ns_STAND_EYE_HEIGHT;
	// 動作の状態に応じて、移動方向・速度・目標の視点の高さを上書きする。
	switch(m_actionState){
		// 通常は、入力の方向へ、構えの有無に応じた速度で進む。初期値のまま使う。
	case PLAYER_ACTION_STATE::e_NORMAL:
	break;

	// ダッシュは、入力の方向へ、ダッシュの速度で進む。
	case PLAYER_ACTION_STATE::e_DASH:
	speed = ns_DASH_SPEED;
	break;

	// しゃがみは、入力の方向へ、遅い速度で進み、視点を低くする。
	case PLAYER_ACTION_STATE::e_CROUCH:
	speed = ns_WALK_SPEED * ns_CROUCH_SPEED_MULTIPLIER;
	target_eye_height = ns_CROUCH_EYE_HEIGHT;
	break;

	// スライディングは、固定した方向へ、時間に比例して減速しながら進み、視点を低くする。入力の方向は使わない。
	case PLAYER_ACTION_STATE::e_SLIDE:
	{
		// 開始から終了までの進み具合を0〜1で求め、開始速度から終了速度へ直線的に補間する。
		const float slide_progress = std::min(1.0f,m_stateElapsedSeconds / ns_SLIDE_SECONDS);
		direction_x = m_fixedDirectionX;
		direction_z = m_fixedDirectionZ;
		speed = ns_SLIDE_START_SPEED + (ns_SLIDE_END_SPEED - ns_SLIDE_START_SPEED) * slide_progress;
		target_eye_height = ns_SLIDE_EYE_HEIGHT;
		break;
	}

	// 回避は、固定した方向へ、一定の速度で進み、視点を少し低くする。入力の方向は使わない。
	case PLAYER_ACTION_STATE::e_DODGE:
	direction_x = m_fixedDirectionX;
	direction_z = m_fixedDirectionZ;
	speed = ns_DODGE_SPEED;
	target_eye_height = ns_DODGE_EYE_HEIGHT;
	break;
	}

	// 視点の高さを、目標の高さへ滑らかに近づける。1回の更新で差を超えないよう、近づく割合は1以下に制限する。
	m_position.y += (target_eye_height - m_position.y) * std::min(1.0f,ns_EYE_HEIGHT_SMOOTHING_RATE * deltaTime);

	// 速度と時間の積で、この更新の移動距離を求める。
	const float move_distance = speed * deltaTime;

	// 最大距離ごとに分割した回数を求める。移動距離が0でも最低1回は押し戻しを行い、壁にめり込んだ位置を補正する。
	const int step_count = move_distance > 0.0f ? static_cast<int>(std::ceil(move_distance / ns_MAX_STEP_DISTANCE)) : 1;
	const float step_scale = move_distance / static_cast<float>(step_count);

	// 分割した移動を1回ずつ進め、毎回、地形のAABBから押し戻す。
	for(int step_index = 0; step_index < step_count; ++step_index){
		// 1回分の移動を加える。高さ（Y）は、上で滑らかに変えた値を使う。
		m_position.x += direction_x * step_scale;
		m_position.z += direction_z * step_scale;

		// 位置を中心にした球をSTATICレイヤーのAABBから押し戻し、水平成分（X・Z）だけを反映する。Yは視点の高さのため反映しない。
		const COLLISION_SPHERE body_sphere{m_position,ns_BODY_RADIUS};
		const XMFLOAT3 resolved_position = CollisionResolver::ResolveSphere(collisionWorld,body_sphere,COLLISION_LAYER::ns_STATIC);
		m_position.x = resolved_position.x;
		m_position.z = resolved_position.z;
	}
}

//========= Getter関数=========
// 回避中で、開始から無敵の時間の内なら、無敵とする。
bool PlayerController::IsInvincible() const
{
	return m_actionState == PLAYER_ACTION_STATE::e_DODGE && m_stateElapsedSeconds <= ns_DODGE_INVINCIBLE_SECONDS;
}

//========= 補助関数=========
// 状態を切り替え、経過時間を0へ戻す。
void PlayerController::EnterState(PLAYER_ACTION_STATE newState)
{
	m_actionState = newState;
	m_stateElapsedSeconds = 0.0f;
}

// 回避・スライディングは時間で終わる。それ以外は、入力から、回避・スライディング・ダッシュ・しゃがみの順に切替を判断する。
void PlayerController::UpdateActionState(const InputSystem& input,bool hasMoveInput,float moveX,float moveZ)
{
	// 左ShiftとCtrlの押下状態を取得し、左Shiftを離していれば、ダッシュの禁止を解く。
	const bool is_shift_pressed = input.IsKeyPressed(VK_SHIFT);
	const bool is_control_pressed = input.IsKeyPressed(VK_CONTROL);
	// 左Shiftを離している間は、構えや射撃で終えたダッシュを、次の押下から再開できるようにする。
	if(!is_shift_pressed){
		m_isDashLocked = false;
	}

	// 回避中とスライディング中は、時間が経つまで、他の入力で状態を変えない（途中でやめられない）。
	switch(m_actionState){
		// 回避は、長さを超えたら通常へ戻る。
	case PLAYER_ACTION_STATE::e_DODGE:
	// 回避の長さを超えていれば、通常へ戻る。
	if(m_stateElapsedSeconds >= ns_DODGE_SECONDS){
		EnterState(PLAYER_ACTION_STATE::e_NORMAL);
	}
	return;

	// スライディングは、長さを超えたら、Ctrlを押し続けていればしゃがみ、そうでなければ通常へ戻る。
	case PLAYER_ACTION_STATE::e_SLIDE:
	// スライディングの長さを超えていれば、Ctrlの押下状態に応じた状態へ移る。
	if(m_stateElapsedSeconds >= ns_SLIDE_SECONDS){
		EnterState(is_control_pressed ? PLAYER_ACTION_STATE::e_CROUCH : PLAYER_ACTION_STATE::e_NORMAL);
	}
	return;

	// 通常・ダッシュ・しゃがみは、下の入力による切替へ進む。
	default:
	break;
	}

	// Spaceを押した瞬間に移動入力があれば、その方向へ回避を始める。方向なしのSpaceは未確定の仕様のため、何もしない。
	if(input.IsKeyTriggered(VK_SPACE) && hasMoveInput){
		m_fixedDirectionX = moveX;
		m_fixedDirectionZ = moveZ;
		m_secondsSinceDodgeStart = 0.0f;
		EnterState(PLAYER_ACTION_STATE::e_DODGE);
		return;
	}

	// ダッシュ中にCtrlを押した瞬間は、直近の移動方向を固定して、スライディングを始める。
	if(m_actionState == PLAYER_ACTION_STATE::e_DASH && input.IsKeyTriggered(VK_CONTROL)){
		m_fixedDirectionX = m_lastMoveDirectionX;
		m_fixedDirectionZ = m_lastMoveDirectionZ;
		EnterState(PLAYER_ACTION_STATE::e_SLIDE);
		return;
	}

	// ダッシュ中に右クリックを押した瞬間、または左クリックを押している場合は、ダッシュを終えて、左Shiftを離すまで再開しない。
	if(m_actionState == PLAYER_ACTION_STATE::e_DASH && (input.IsKeyTriggered(VK_RBUTTON) || input.IsKeyPressed(VK_LBUTTON))){
		m_isDashLocked = true;
		EnterState(PLAYER_ACTION_STATE::e_NORMAL);
	}
	// ダッシュ中に左Shiftを離した、または移動入力がなくなった場合は、ダッシュを終える。
	else if(m_actionState == PLAYER_ACTION_STATE::e_DASH && (!is_shift_pressed || !hasMoveInput)){
		EnterState(PLAYER_ACTION_STATE::e_NORMAL);
	}

	// 通常・しゃがみから、左Shiftを押して移動入力があり、ダッシュが禁止されていなければ、ダッシュを始める。構えは、この状態で解除される。
	if((m_actionState == PLAYER_ACTION_STATE::e_NORMAL || m_actionState == PLAYER_ACTION_STATE::e_CROUCH) &&
		 is_shift_pressed && hasMoveInput && !m_isDashLocked){
		EnterState(PLAYER_ACTION_STATE::e_DASH);
	}

	// 通常でCtrlを押している間はしゃがみへ、しゃがみでCtrlを離したら通常へ移る。ダッシュ中は、しゃがみへ移らない。
	if(m_actionState == PLAYER_ACTION_STATE::e_NORMAL && is_control_pressed){
		EnterState(PLAYER_ACTION_STATE::e_CROUCH);
	}
	// しゃがみ中にCtrlを離したら、通常へ戻る。
	else if(m_actionState == PLAYER_ACTION_STATE::e_CROUCH && !is_control_pressed){
		EnterState(PLAYER_ACTION_STATE::e_NORMAL);
	}
}