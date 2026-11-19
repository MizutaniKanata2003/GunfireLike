#include "../H/PlayerController.h"

// 動作の状態の切替、状態ごとの移動速度・視点の高さ、構え、近接攻撃の当たりの通知、怯み、地形のAABBからの押し戻しを実装する。
// 移動方向は視線を水平面へ投影して求める。移動を小さな距離に分けて押し戻すことで、薄い壁を1回の移動で通り抜けることを防ぐ。
// 数値はPlayerActionTable.hの表にまとめた。

//========= Windows インクルード=========
#include <windows.h> // VK_SHIFT・VK_SPACE・VK_LBUTTON・VK_RBUTTONで、ダッシュ・回避・構え・射撃の入力を判定する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで視点の高さを近づける割合とスライディングの進み具合を1以下に制限し、std::maxで怯みの残り時間を0以上にする。
#include <cmath> // std::sqrtで水平視線と合成移動方向の長さを求めて正規化し、std::ceilで移動の分割数を求める。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionResolver.h"
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Input/H/FpsCamera.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Player/H/PlayerActionTable.h"

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
	m_secondsSinceDodgeStart = PLAYER_ACTION_TABLE::ns_SECONDS_WITHOUT_DODGE;

	// 近接攻撃の当たりの通知と、怯みを解除する。
	m_isMeleeImpactPending = false;
	m_isMeleeImpactReady = false;
	m_flinchRemainingSeconds = 0.0f;
}

// 動作の状態の切替・移動・視点の高さ・構えを更新し、分割した移動ごとに押し戻す。
void PlayerController::Update(float deltaTime,const InputSystem& input,const FpsCamera& camera,const CollisionWorld& collisionWorld,bool canKick)
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

	// 経過時間を進め、入力から動作の状態を切り替える。回避からの経過時間は、回避中も、回避後も増え続ける。怯みの残り時間は0未満にしない。
	m_stateElapsedSeconds += deltaTime;
	m_secondsSinceDodgeStart += deltaTime;
	m_flinchRemainingSeconds = std::max(0.0f,m_flinchRemainingSeconds - deltaTime);
	UpdateActionState(input,has_move_input,move_x,move_z,forward.x,forward.z,canKick);

	// 蹴り・殴打の当たる時刻を迎えていれば、当たりを通知できる状態にする。当たる時刻は、動作の種類で異なる。
	if(m_isMeleeImpactPending){
		const float impact_seconds = m_actionState == PLAYER_ACTION_STATE::e_KICK ? PLAYER_ACTION_TABLE::ns_KICK_IMPACT_SECONDS :
			PLAYER_ACTION_TABLE::ns_PUNCH_IMPACT_SECONDS;
		// 動作の開始から当たる時刻に達していれば、待ち状態を終えて、当たりを通知できる状態にする。
		if(m_stateElapsedSeconds >= impact_seconds){
			m_isMeleeImpactPending = false;
			m_isMeleeImpactReady = true;
		}
	}

	// 右クリックを押している間は構える。ただし、ダッシュ・回避・蹴り・殴打の間は構えられず、構え入力があっても解除される。
	m_isAiming = input.IsKeyPressed(VK_RBUTTON) && m_actionState != PLAYER_ACTION_STATE::e_DASH && m_actionState != PLAYER_ACTION_STATE::e_DODGE &&
		m_actionState != PLAYER_ACTION_STATE::e_KICK && m_actionState != PLAYER_ACTION_STATE::e_PUNCH;

	// 動作の状態ごとに、移動方向・移動速度・目標の視点の高さを決める。初期値は通常の移動。
	float direction_x = move_x;
	float direction_z = move_z;
	float speed = m_isAiming ? PLAYER_ACTION_TABLE::ns_WALK_SPEED * PLAYER_ACTION_TABLE::ns_AIM_SPEED_MULTIPLIER : PLAYER_ACTION_TABLE::ns_WALK_SPEED;
	float target_eye_height = PLAYER_ACTION_TABLE::ns_STAND_EYE_HEIGHT;
	// 動作の状態に応じて、移動方向・速度・目標の視点の高さを上書きする。
	switch(m_actionState){
		// 通常は、入力の方向へ、構えの有無に応じた速度で進む。初期値のまま使う。
	case PLAYER_ACTION_STATE::e_NORMAL:
	break;

	// ダッシュは、入力の方向へ、ダッシュの速度で進む。
	case PLAYER_ACTION_STATE::e_DASH:
	speed = PLAYER_ACTION_TABLE::ns_DASH_SPEED;
	break;

	// しゃがみは、入力の方向へ、遅い速度で進み、視点を低くする。
	case PLAYER_ACTION_STATE::e_CROUCH:
	speed = PLAYER_ACTION_TABLE::ns_WALK_SPEED * PLAYER_ACTION_TABLE::ns_CROUCH_SPEED_MULTIPLIER;
	target_eye_height = PLAYER_ACTION_TABLE::ns_CROUCH_EYE_HEIGHT;
	break;

	// スライディングは、固定した方向へ、時間に比例して減速しながら進み、しゃがみより視点を低くする。入力の方向は使わない。
	case PLAYER_ACTION_STATE::e_SLIDE:
	{
		// 開始から終了までの進み具合を0〜1で求め、開始速度から終了速度へ直線的に補間する。
		const float slide_progress = std::min(1.0f,m_stateElapsedSeconds / PLAYER_ACTION_TABLE::ns_SLIDE_SECONDS);
		direction_x = m_fixedDirectionX;
		direction_z = m_fixedDirectionZ;
		speed = PLAYER_ACTION_TABLE::ns_SLIDE_START_SPEED + (PLAYER_ACTION_TABLE::ns_SLIDE_END_SPEED - PLAYER_ACTION_TABLE::ns_SLIDE_START_SPEED) * slide_progress;
		target_eye_height = PLAYER_ACTION_TABLE::ns_SLIDE_EYE_HEIGHT;
		break;
	}

	// 回避は、固定した方向へ、一定の速度で進み、視点を少し低くする。入力の方向は使わない。
	case PLAYER_ACTION_STATE::e_DODGE:
	direction_x = m_fixedDirectionX;
	direction_z = m_fixedDirectionZ;
	speed = PLAYER_ACTION_TABLE::ns_DODGE_SPEED;
	target_eye_height = PLAYER_ACTION_TABLE::ns_DODGE_EYE_HEIGHT;
	break;

	// 蹴りは、その場で行う。移動せず、視点を少し低くする。
	case PLAYER_ACTION_STATE::e_KICK:
	speed = 0.0f;
	target_eye_height = PLAYER_ACTION_TABLE::ns_KICK_EYE_HEIGHT;
	break;

	// 殴打は、開始からの踏み込みの時間だけ、固定した前方向へ進み、その後は止まる。入力の方向は使わない。
	case PLAYER_ACTION_STATE::e_PUNCH:
	direction_x = m_fixedDirectionX;
	direction_z = m_fixedDirectionZ;
	speed = m_stateElapsedSeconds < PLAYER_ACTION_TABLE::ns_PUNCH_LUNGE_SECONDS ? PLAYER_ACTION_TABLE::ns_PUNCH_LUNGE_SPEED : 0.0f;
	target_eye_height = PLAYER_ACTION_TABLE::ns_PUNCH_EYE_HEIGHT;
	break;
	}

	// 怯み中は、入力で動かせる状態（通常・ダッシュ・しゃがみ）の移動速度を下げる。固定した動作（スライディング以降）の速度は変えない。
	if(m_flinchRemainingSeconds > 0.0f && (m_actionState == PLAYER_ACTION_STATE::e_NORMAL || m_actionState == PLAYER_ACTION_STATE::e_DASH ||
		 m_actionState == PLAYER_ACTION_STATE::e_CROUCH)){
		speed *= PLAYER_ACTION_TABLE::ns_FLINCH_SPEED_MULTIPLIER;
	}

	// 視点の高さを、目標の高さへ滑らかに近づける。1回の更新で差を超えないよう、近づく割合は1以下に制限する。
	m_position.y += (target_eye_height - m_position.y) * std::min(1.0f,PLAYER_ACTION_TABLE::ns_EYE_HEIGHT_SMOOTHING_RATE * deltaTime);

	// 速度と時間の積で、この更新の移動距離を求める。
	const float move_distance = speed * deltaTime;

	// 最大距離ごとに分割した回数を求める。移動距離が0でも最低1回は押し戻しを行い、壁にめり込んだ位置を補正する。
	const int step_count = move_distance > 0.0f ? static_cast<int>(std::ceil(move_distance / PLAYER_ACTION_TABLE::ns_MAX_STEP_DISTANCE)) : 1;
	const float step_scale = move_distance / static_cast<float>(step_count);

	// 分割した移動を1回ずつ進め、毎回、地形のAABBから押し戻す。
	for(int step_index = 0; step_index < step_count; ++step_index){
		// 1回分の移動を加える。高さ（Y）は、上で滑らかに変えた値を使う。
		m_position.x += direction_x * step_scale;
		m_position.z += direction_z * step_scale;

		// 位置を中心にした球をSTATICレイヤーのAABBから押し戻し、水平成分（X・Z）だけを反映する。Yは視点の高さのため反映しない。
		const COLLISION_SPHERE body_sphere{m_position,PLAYER_ACTION_TABLE::ns_BODY_RADIUS};
		const XMFLOAT3 resolved_position = CollisionResolver::ResolveSphere(collisionWorld,body_sphere,COLLISION_LAYER::ns_STATIC);
		m_position.x = resolved_position.x;
		m_position.z = resolved_position.z;
	}
}

// 怯みの残り時間を、長い方へ更新する。
void PlayerController::ApplyFlinch(float seconds)
{
	m_flinchRemainingSeconds = std::max(m_flinchRemainingSeconds,seconds);
}

// 当たりが発生していれば、動作の種類から近接攻撃の種類を決めて通知を消費する。
bool PlayerController::TryConsumeMeleeImpact(PLAYER_MELEE_TYPE& meleeType)
{
	// 当たりが発生していなければ、出力先を変更せずfalseを返す。
	if(!m_isMeleeImpactReady)return false;

	// 通知を消費し、蹴りの動作中なら蹴り、そうでなければ殴打として返す。
	m_isMeleeImpactReady = false;
	meleeType = m_actionState == PLAYER_ACTION_STATE::e_KICK ? PLAYER_MELEE_TYPE::e_KICK : PLAYER_MELEE_TYPE::e_PUNCH;
	return true;
}

//========= Getter関数=========
// 現在の位置を中心にした、壁との押し戻しと同じ半径の球を返す。
COLLISION_SPHERE PlayerController::GetBodySphere() const
{
	return COLLISION_SPHERE{m_position,PLAYER_ACTION_TABLE::ns_BODY_RADIUS};
}

// 回避・蹴り・殴打の、それぞれの無敵の時間の内なら、無敵とする。
bool PlayerController::IsInvincible() const
{
	// 動作の状態ごとに、開始からの経過時間と無敵の時間を比べる。無敵がない動作では、falseを返す。
	switch(m_actionState){
		// 回避は、開始から無敵の時間の内なら無敵。
	case PLAYER_ACTION_STATE::e_DODGE:
	return m_stateElapsedSeconds <= PLAYER_ACTION_TABLE::ns_DODGE_INVINCIBLE_SECONDS;

	// 蹴りは、動作中ずっと無敵。
	case PLAYER_ACTION_STATE::e_KICK:
	return m_stateElapsedSeconds <= PLAYER_ACTION_TABLE::ns_KICK_INVINCIBLE_SECONDS;

	// 殴打は、前半だけ無敵。
	case PLAYER_ACTION_STATE::e_PUNCH:
	return m_stateElapsedSeconds <= PLAYER_ACTION_TABLE::ns_PUNCH_INVINCIBLE_SECONDS;

	// それ以外の動作は、無敵ではない。
	default:
	return false;
	}
}

// 回避・蹴り・殴打の間は射撃できない。それ以外は射撃できる。
bool PlayerController::IsShootingAllowed() const
{
	return m_actionState != PLAYER_ACTION_STATE::e_DODGE && m_actionState != PLAYER_ACTION_STATE::e_KICK && m_actionState != PLAYER_ACTION_STATE::e_PUNCH;
}

//========= 補助関数=========
// 状態を切り替え、経過時間を0へ戻す。蹴り・殴打に入る場合は、当たる時刻を待つ状態にする。
void PlayerController::EnterState(PLAYER_ACTION_STATE newState)
{
	m_actionState = newState;
	m_stateElapsedSeconds = 0.0f;
	m_isMeleeImpactPending = newState == PLAYER_ACTION_STATE::e_KICK || newState == PLAYER_ACTION_STATE::e_PUNCH;
	m_isMeleeImpactReady = false;
}

// 回避・スライディング・蹴り・殴打は時間で終わる。それ以外は、入力から、回避・殴打・蹴り・スライディング・ダッシュ・しゃがみの順に切替を判断する。
void PlayerController::UpdateActionState(const InputSystem& input,bool hasMoveInput,float moveX,float moveZ,float forwardX,float forwardZ,bool canKick)
{
	// 左ShiftとCキーの押下状態を取得し、左Shiftを離していれば、ダッシュの禁止を解く。
	const bool is_shift_pressed = input.IsKeyPressed(VK_SHIFT);
	const bool is_crouch_pressed = input.IsKeyPressed(PLAYER_ACTION_TABLE::ns_CROUCH_KEY);
	// 左Shiftを離している間は、構えや射撃で終えたダッシュを、次の押下から再開できるようにする。
	if(!is_shift_pressed){
		m_isDashLocked = false;
	}

	// 回避中・スライディング中・蹴り中・殴打中は、時間が経つまで、他の入力で状態を変えない（途中でやめられない）。
	switch(m_actionState){
		// 回避は、長さを超えたら通常へ戻る。
	case PLAYER_ACTION_STATE::e_DODGE:
	// 回避の長さを超えていれば、通常へ戻る。
	if(m_stateElapsedSeconds >= PLAYER_ACTION_TABLE::ns_DODGE_SECONDS){
		EnterState(PLAYER_ACTION_STATE::e_NORMAL);
	}
	return;

	// スライディングは、長さを超えたら、Cキーを押し続けていればしゃがみ、そうでなければ通常へ戻る。
	case PLAYER_ACTION_STATE::e_SLIDE:
	// スライディングの長さを超えていれば、Cキーの押下状態に応じた状態へ移る。
	if(m_stateElapsedSeconds >= PLAYER_ACTION_TABLE::ns_SLIDE_SECONDS){
		EnterState(is_crouch_pressed ? PLAYER_ACTION_STATE::e_CROUCH : PLAYER_ACTION_STATE::e_NORMAL);
	}
	return;

	// 蹴りは、長さを超えたら通常へ戻る。
	case PLAYER_ACTION_STATE::e_KICK:
	// 蹴りの長さを超えていれば、通常へ戻る。
	if(m_stateElapsedSeconds >= PLAYER_ACTION_TABLE::ns_KICK_SECONDS){
		EnterState(PLAYER_ACTION_STATE::e_NORMAL);
	}
	return;

	// 殴打は、長さを超えたら通常へ戻る。
	case PLAYER_ACTION_STATE::e_PUNCH:
	// 殴打の長さを超えていれば、通常へ戻る。
	if(m_stateElapsedSeconds >= PLAYER_ACTION_TABLE::ns_PUNCH_SECONDS){
		EnterState(PLAYER_ACTION_STATE::e_NORMAL);
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

	// Eを押した瞬間は、視線の水平方向へ踏み込む殴打を始める。リロード中でも始められ、リロードのキャンセルは呼出側が行う。
	if(input.IsKeyTriggered('E')){
		m_fixedDirectionX = forwardX;
		m_fixedDirectionZ = forwardZ;
		EnterState(PLAYER_ACTION_STATE::e_PUNCH);
		return;
	}

	// Fを押した瞬間に、蹴れる相手（射程内で怯んでいて転倒していない敵）がいれば、蹴りを始める。いなければ何もしない。
	if(input.IsKeyTriggered('F') && canKick){
		EnterState(PLAYER_ACTION_STATE::e_KICK);
		return;
	}

	// ダッシュ中にCキーを押した瞬間は、直近の移動方向を固定して、スライディングを始める。
	if(m_actionState == PLAYER_ACTION_STATE::e_DASH && input.IsKeyTriggered(PLAYER_ACTION_TABLE::ns_CROUCH_KEY)){
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

	// 通常でCキーを押している間はしゃがみへ、しゃがみでCキーを離したら通常へ移る。ダッシュ中は、しゃがみへ移らない。
	if(m_actionState == PLAYER_ACTION_STATE::e_NORMAL && is_crouch_pressed){
		EnterState(PLAYER_ACTION_STATE::e_CROUCH);
	}
	// しゃがみ中にCキーを離したら、通常へ戻る。
	else if(m_actionState == PLAYER_ACTION_STATE::e_CROUCH && !is_crouch_pressed){
		EnterState(PLAYER_ACTION_STATE::e_NORMAL);
	}
}