#include "../H/PlayerController.h"

// 動作の状態の切替、状態ごとの移動速度・視点の高さ、構え、殴打の当たりの通知、怯み、地形のAABBからの押し戻しを実装する。
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

// 視線の水平化に使う補助関数をまとめる。
namespace
{
  // カメラの前方向を水平面へ投影し、単位化して返す。上下を向いても移動を水平に保つため、Y成分を除き、水平成分の長さで割る。
  // camera: 視線の取得元。参照は保持しない。
  // 戻り値: 水平な前方向。Yは0。真上・真下を向いて水平成分が0の場合は、割らずに0のまま返す。
  XMFLOAT3 GetHorizontalForward(const FpsCamera& camera)
  {
    XMFLOAT3 forward = camera.GetForward();
    forward.y = 0.0f;
    const float forward_length = std::sqrt(forward.x * forward.x + forward.z * forward.z);

    // 長さが正の場合だけ単位化する。真上・真下を向いて水平成分が0の場合は、割らずに0のまま使う。
    if(forward_length > 0.0f){
      forward.x /= forward_length;
      forward.z /= forward_length;
    }
    return forward;
  }
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
  m_secondsSinceDodgeStart = PLAYER_ACTION_TABLE::ns_SECONDS_WITHOUT_DODGE;

  // 殴打の当たりの通知と、怯みを解除する。
  m_isMeleeImpactPending = false;
  m_isMeleeImpactReady = false;
  m_flinchRemainingSeconds = 0.0f;
}

// 動作の状態の切替・移動・視点の高さ・構えを更新し、分割した移動ごとに押し戻す。
void PlayerController::Update(float deltaTime,const InputSystem& input,const FpsCamera& camera,const CollisionWorld& collisionWorld)
{
  // 入力を読み取り、経過時間を進める。回避からの経過時間は、回避中も、回避後も増え続ける。怯みの残り時間は0未満にしない。
  const PLAYER_MOVE_INPUT move_input = ReadMoveInput(input,camera);
  m_stateElapsedSeconds += deltaTime;
  m_secondsSinceDodgeStart += deltaTime;
  m_flinchRemainingSeconds = std::max(0.0f,m_flinchRemainingSeconds - deltaTime);

  // 入力から動作の状態を切り替え、殴打の当たる時刻を迎えていれば、当たりを通知できる状態にする。
  UpdateActionState(input,move_input);
  UpdateMeleeImpact();

  // 右クリックを押している間は構える。ただし、ダッシュ・回避・殴打の間は構えられず、構え入力があっても解除される。
  m_isAiming = input.IsKeyPressed(VK_RBUTTON) && m_actionState != PLAYER_ACTION_STATE::e_DASH && m_actionState != PLAYER_ACTION_STATE::e_DODGE &&
    m_actionState != PLAYER_ACTION_STATE::e_PUNCH;

  // 動作の状態ごとの移動の内容を決める。
  PLAYER_MOTION motion = DecideMotion(move_input);

  // 怯み中は、入力で動かせる状態（通常・ダッシュ・しゃがみ）の移動速度を下げる。固定した動作（スライディング以降）の速度は変えない。
  if(m_flinchRemainingSeconds > 0.0f && (m_actionState == PLAYER_ACTION_STATE::e_NORMAL || m_actionState == PLAYER_ACTION_STATE::e_DASH ||
     m_actionState == PLAYER_ACTION_STATE::e_CROUCH)){
    motion.speed *= PLAYER_ACTION_TABLE::ns_FLINCH_SPEED_MULTIPLIER;
  }

  // 視点の高さを近づけ、分割した移動ごとに、地形のAABBから押し戻す。
  MoveAndResolve(motion,deltaTime,collisionWorld);
}

// 怯みの残り時間を、長い方へ更新する。
void PlayerController::ApplyFlinch(float seconds)
{
  m_flinchRemainingSeconds = std::max(m_flinchRemainingSeconds,seconds);
}

// 当たりが発生していれば、通知を消費する。
bool PlayerController::TryConsumeMeleeImpact()
{
  // 当たりが発生していなければ、falseを返す。
  if(!m_isMeleeImpactReady)return false;

  // 通知を消費する。
  m_isMeleeImpactReady = false;
  return true;
}

//========= Getter関数=========
// 現在の位置を中心にした、壁との押し戻しと同じ半径の球を返す。
COLLISION_SPHERE PlayerController::GetBodySphere() const
{
  return COLLISION_SPHERE{m_position,PLAYER_ACTION_TABLE::ns_BODY_RADIUS};
}

// 回避・殴打の、それぞれの無敵の時間の内なら、無敵とする。
bool PlayerController::IsInvincible() const
{
  // 動作の状態ごとに、開始からの経過時間と無敵の時間を比べる。無敵がない動作では、falseを返す。
  switch(m_actionState){
    // 回避は、開始から無敵の時間の内なら無敵。
  case PLAYER_ACTION_STATE::e_DODGE:
  return m_stateElapsedSeconds <= PLAYER_ACTION_TABLE::ns_DODGE_INVINCIBLE_SECONDS;

  // 殴打は、前半だけ無敵。
  case PLAYER_ACTION_STATE::e_PUNCH:
  return m_stateElapsedSeconds <= PLAYER_ACTION_TABLE::ns_PUNCH_INVINCIBLE_SECONDS;

  // それ以外の動作は、無敵ではない。
  default:
  return false;
  }
}

// 回避・殴打の間は射撃できない。それ以外は射撃できる。
bool PlayerController::IsShootingAllowed() const
{
  return m_actionState != PLAYER_ACTION_STATE::e_DODGE && m_actionState != PLAYER_ACTION_STATE::e_PUNCH;
}

//========= 入力の読取関数=========
// W・A・S・Dの入力を、カメラの水平な向きを基準に合成し、単位化する。
PLAYER_MOVE_INPUT PlayerController::ReadMoveInput(const InputSystem& input,const FpsCamera& camera)
{
  // 水平な前方向と、左右移動に使うカメラの右方向を取得する。右方向は、X・Z成分だけを使う。
  const XMFLOAT3 forward = GetHorizontalForward(camera);
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

  PLAYER_MOVE_INPUT move_input{};
  move_input.hasMoveInput = has_move_input;
  move_input.moveX = move_x;
  move_input.moveZ = move_z;
  move_input.forwardX = forward.x;
  move_input.forwardZ = forward.z;
  return move_input;
}

//========= 動作の状態の関数=========
// 状態を切り替え、経過時間を0へ戻す。殴打に入る場合は、当たる時刻を待つ状態にする。
void PlayerController::EnterState(PLAYER_ACTION_STATE newState)
{
  m_actionState = newState;
  m_stateElapsedSeconds = 0.0f;
  m_isMeleeImpactPending = newState == PLAYER_ACTION_STATE::e_PUNCH;
  m_isMeleeImpactReady = false;
}

// 時間で終わる動作の確認、新しい動作の開始、ダッシュ・しゃがみの切替を、この順に判断する。
void PlayerController::UpdateActionState(const InputSystem& input,const PLAYER_MOVE_INPUT& moveInput)
{
  // 左ShiftとCキーの押下状態を取得し、左Shiftを離していれば、ダッシュの禁止を解く。
  const bool is_shift_pressed = input.IsKeyPressed(VK_SHIFT);
  const bool is_crouch_pressed = input.IsKeyPressed(PLAYER_ACTION_TABLE::ns_CROUCH_KEY);
  // 左Shiftを離している間は、構えや射撃で終えたダッシュを、次の押下から再開できるようにする。
  if(!is_shift_pressed){
    m_isDashLocked = false;
  }

  // 回避中・スライディング中・殴打中は、時間が経つまで、他の入力で状態を変えない（途中でやめられない）。
  if(UpdateTimedAction(is_crouch_pressed))return;

  // 回避・殴打・スライディングを始める入力があれば、その動作へ入り、他の切替は行わない。
  if(TryStartAction(input,moveInput))return;

  // 始める動作がなければ、ダッシュとしゃがみの切替を判断する。
  UpdateDashAndCrouch(input,moveInput.hasMoveInput,is_shift_pressed,is_crouch_pressed);
}

// 回避・スライディング・殴打は時間で終わる。長さを超えていれば、次の状態へ移す。
bool PlayerController::UpdateTimedAction(bool isCrouchPressed)
{
  // 動作の状態ごとに、終わる長さを比べる。時間で終わらない状態では、falseを返し、入力による切替へ進ませる。
  switch(m_actionState){
    // 回避は、長さを超えたら通常へ戻る。
  case PLAYER_ACTION_STATE::e_DODGE:
  // 回避の長さを超えていれば、通常へ戻る。
  if(m_stateElapsedSeconds >= PLAYER_ACTION_TABLE::ns_DODGE_SECONDS){
    EnterState(PLAYER_ACTION_STATE::e_NORMAL);
  }
  return true;

  // スライディングは、長さを超えたら、Cキーを押し続けていればしゃがみ、そうでなければ通常へ戻る。
  case PLAYER_ACTION_STATE::e_SLIDE:
  // スライディングの長さを超えていれば、Cキーの押下状態に応じた状態へ移る。
  if(m_stateElapsedSeconds >= PLAYER_ACTION_TABLE::ns_SLIDE_SECONDS){
    EnterState(isCrouchPressed ? PLAYER_ACTION_STATE::e_CROUCH : PLAYER_ACTION_STATE::e_NORMAL);
  }
  return true;

  // 殴打は、長さを超えたら通常へ戻る。
  case PLAYER_ACTION_STATE::e_PUNCH:
  // 殴打の長さを超えていれば、通常へ戻る。
  if(m_stateElapsedSeconds >= PLAYER_ACTION_TABLE::ns_PUNCH_SECONDS){
    EnterState(PLAYER_ACTION_STATE::e_NORMAL);
  }
  return true;

  // 通常・ダッシュ・しゃがみは、時間で終わらない。入力による切替へ進む。
  default:
  return false;
  }
}

// 回避・殴打・スライディングの開始の入力を、この順に調べる。
bool PlayerController::TryStartAction(const InputSystem& input,const PLAYER_MOVE_INPUT& moveInput)
{
  // Spaceを押した瞬間に移動入力があれば、その方向へ回避を始める。方向なしのSpaceは、何もしない。
  if(input.IsKeyTriggered(VK_SPACE) && moveInput.hasMoveInput){
    m_fixedDirectionX = moveInput.moveX;
    m_fixedDirectionZ = moveInput.moveZ;
    m_secondsSinceDodgeStart = 0.0f;
    EnterState(PLAYER_ACTION_STATE::e_DODGE);
    return true;
  }

  // Eを押した瞬間は、視線の水平方向へ踏み込む殴打を始める。リロード中でも始められ、リロードのキャンセルは呼出側が行う。
  if(input.IsKeyTriggered('E')){
    m_fixedDirectionX = moveInput.forwardX;
    m_fixedDirectionZ = moveInput.forwardZ;
    EnterState(PLAYER_ACTION_STATE::e_PUNCH);
    return true;
  }

  // ダッシュ中にCキーを押した瞬間は、直近の移動方向を固定して、スライディングを始める。
  if(m_actionState == PLAYER_ACTION_STATE::e_DASH && input.IsKeyTriggered(PLAYER_ACTION_TABLE::ns_CROUCH_KEY)){
    m_fixedDirectionX = m_lastMoveDirectionX;
    m_fixedDirectionZ = m_lastMoveDirectionZ;
    EnterState(PLAYER_ACTION_STATE::e_SLIDE);
    return true;
  }

  // どの動作の開始の入力もなかった。
  return false;
}

// ダッシュの終了・開始と、しゃがみの開始・終了を、入力から切り替える。
void PlayerController::UpdateDashAndCrouch(const InputSystem& input,bool hasMoveInput,bool isShiftPressed,bool isCrouchPressed)
{
  // ダッシュ中に右クリックを押した瞬間、または左クリックを押している場合は、ダッシュを終えて、左Shiftを離すまで再開しない。
  if(m_actionState == PLAYER_ACTION_STATE::e_DASH && (input.IsKeyTriggered(VK_RBUTTON) || input.IsKeyPressed(VK_LBUTTON))){
    m_isDashLocked = true;
    EnterState(PLAYER_ACTION_STATE::e_NORMAL);
  }
  // ダッシュ中に左Shiftを離した、または移動入力がなくなった場合は、ダッシュを終える。
  else if(m_actionState == PLAYER_ACTION_STATE::e_DASH && (!isShiftPressed || !hasMoveInput)){
    EnterState(PLAYER_ACTION_STATE::e_NORMAL);
  }

  // 通常・しゃがみから、左Shiftを押して移動入力があり、ダッシュが禁止されていなければ、ダッシュを始める。構えは、この状態で解除される。
  if((m_actionState == PLAYER_ACTION_STATE::e_NORMAL || m_actionState == PLAYER_ACTION_STATE::e_CROUCH) &&
     isShiftPressed && hasMoveInput && !m_isDashLocked){
    EnterState(PLAYER_ACTION_STATE::e_DASH);
  }

  // 通常でCキーを押している間はしゃがみへ、しゃがみでCキーを離したら通常へ移る。ダッシュ中は、しゃがみへ移らない。
  if(m_actionState == PLAYER_ACTION_STATE::e_NORMAL && isCrouchPressed){
    EnterState(PLAYER_ACTION_STATE::e_CROUCH);
  }
  // しゃがみ中にCキーを離したら、通常へ戻る。
  else if(m_actionState == PLAYER_ACTION_STATE::e_CROUCH && !isCrouchPressed){
    EnterState(PLAYER_ACTION_STATE::e_NORMAL);
  }
}

// 殴打の当たる時刻を迎えていれば、当たりを通知できる状態にする。
void PlayerController::UpdateMeleeImpact()
{
  // 当たる時刻をまだ迎えていない殴打がなければ、何もしない。
  if(!m_isMeleeImpactPending)return;

  // 動作の開始から当たる時刻に達していれば、待ち状態を終えて、当たりを通知できる状態にする。
  if(m_stateElapsedSeconds >= PLAYER_ACTION_TABLE::ns_PUNCH_IMPACT_SECONDS){
    m_isMeleeImpactPending = false;
    m_isMeleeImpactReady = true;
  }
}

//========= 移動の関数=========
// 動作の状態ごとに、移動方向・移動速度・目標の視点の高さを決める。
PLAYER_MOTION PlayerController::DecideMotion(const PLAYER_MOVE_INPUT& moveInput) const
{
  // 初期値は通常の移動。入力の方向へ、構えの有無に応じた速度で進み、立っている時の視点の高さを目標にする。
  PLAYER_MOTION motion{};
  motion.directionX = moveInput.moveX;
  motion.directionZ = moveInput.moveZ;
  motion.speed = m_isAiming ? PLAYER_ACTION_TABLE::ns_WALK_SPEED * PLAYER_ACTION_TABLE::ns_AIM_SPEED_MULTIPLIER : PLAYER_ACTION_TABLE::ns_WALK_SPEED;
  motion.targetEyeHeight = PLAYER_ACTION_TABLE::ns_STAND_EYE_HEIGHT;

  // 動作の状態に応じて、移動方向・速度・目標の視点の高さを上書きする。入力の方向を使わない動作は、ApplyFixedMotionで上書きする。
  switch(m_actionState){
    // 通常は、初期値のまま使う。
  case PLAYER_ACTION_STATE::e_NORMAL:
  break;

  // ダッシュは、入力の方向へ、ダッシュの速度で進む。
  case PLAYER_ACTION_STATE::e_DASH:
  motion.speed = PLAYER_ACTION_TABLE::ns_DASH_SPEED;
  break;

  // しゃがみは、入力の方向へ、遅い速度で進み、視点を低くする。
  case PLAYER_ACTION_STATE::e_CROUCH:
  motion.speed = PLAYER_ACTION_TABLE::ns_WALK_SPEED * PLAYER_ACTION_TABLE::ns_CROUCH_SPEED_MULTIPLIER;
  motion.targetEyeHeight = PLAYER_ACTION_TABLE::ns_CROUCH_EYE_HEIGHT;
  break;

  // スライディング・回避・殴打は、入力の方向を使わない動作のため、別の関数で上書きする。
  default:
  ApplyFixedMotion(motion);
  break;
  }
  return motion;
}

// 入力の方向を使わない動作（スライディング・回避・殴打）の移動の内容を上書きする。
void PlayerController::ApplyFixedMotion(PLAYER_MOTION& motion) const
{
  // 動作の状態ごとに、固定した方向・速度・目標の視点の高さへ上書きする。他の状態では、何も変えない。
  switch(m_actionState){
    // スライディングは、固定した方向へ、時間に比例して減速しながら進み、しゃがみより視点を低くする。
  case PLAYER_ACTION_STATE::e_SLIDE:
  {
    // 開始から終了までの進み具合を0〜1で求め、開始速度から終了速度へ直線的に補間する。
    const float slide_progress = std::min(1.0f,m_stateElapsedSeconds / PLAYER_ACTION_TABLE::ns_SLIDE_SECONDS);
    motion.directionX = m_fixedDirectionX;
    motion.directionZ = m_fixedDirectionZ;
    motion.speed = PLAYER_ACTION_TABLE::ns_SLIDE_START_SPEED + (PLAYER_ACTION_TABLE::ns_SLIDE_END_SPEED - PLAYER_ACTION_TABLE::ns_SLIDE_START_SPEED) * slide_progress;
    motion.targetEyeHeight = PLAYER_ACTION_TABLE::ns_SLIDE_EYE_HEIGHT;
    break;
  }

  // 回避は、固定した方向へ、一定の速度で進み、視点を少し低くする。
  case PLAYER_ACTION_STATE::e_DODGE:
  motion.directionX = m_fixedDirectionX;
  motion.directionZ = m_fixedDirectionZ;
  motion.speed = PLAYER_ACTION_TABLE::ns_DODGE_SPEED;
  motion.targetEyeHeight = PLAYER_ACTION_TABLE::ns_DODGE_EYE_HEIGHT;
  break;

  // 殴打は、開始からの踏み込みの時間だけ、固定した前方向へ進み、その後は止まる。
  case PLAYER_ACTION_STATE::e_PUNCH:
  motion.directionX = m_fixedDirectionX;
  motion.directionZ = m_fixedDirectionZ;
  motion.speed = m_stateElapsedSeconds < PLAYER_ACTION_TABLE::ns_PUNCH_LUNGE_SECONDS ? PLAYER_ACTION_TABLE::ns_PUNCH_LUNGE_SPEED : 0.0f;
  motion.targetEyeHeight = PLAYER_ACTION_TABLE::ns_PUNCH_EYE_HEIGHT;
  break;

  // 入力の方向で動く状態は、何も変えない。
  default:
  break;
  }
}

// 視点の高さを目標へ近づけ、移動を分割して進め、毎回、地形のAABBから押し戻す。
void PlayerController::MoveAndResolve(const PLAYER_MOTION& motion,float deltaTime,const CollisionWorld& collisionWorld)
{
  // 視点の高さを、目標の高さへ滑らかに近づける。1回の更新で差を超えないよう、近づく割合は1以下に制限する。
  m_position.y += (motion.targetEyeHeight - m_position.y) * std::min(1.0f,PLAYER_ACTION_TABLE::ns_EYE_HEIGHT_SMOOTHING_RATE * deltaTime);

  // 速度と時間の積で、この更新の移動距離を求める。
  const float move_distance = motion.speed * deltaTime;

  // 最大距離ごとに分割した回数を求める。移動距離が0でも最低1回は押し戻しを行い、壁にめり込んだ位置を補正する。
  const int step_count = move_distance > 0.0f ? static_cast<int>(std::ceil(move_distance / PLAYER_ACTION_TABLE::ns_MAX_STEP_DISTANCE)) : 1;
  const float step_scale = move_distance / static_cast<float>(step_count);

  // 分割した移動を1回ずつ進め、毎回、地形のAABBから押し戻す。
  for(int step_index = 0; step_index < step_count; ++step_index){
    // 1回分の移動を加える。高さ（Y）は、上で滑らかに変えた値を使う。
    m_position.x += motion.directionX * step_scale;
    m_position.z += motion.directionZ * step_scale;

    // 位置を中心にした球をSTATICレイヤーのAABBから押し戻し、水平成分（X・Z）だけを反映する。Yは視点の高さのため反映しない。
    const COLLISION_SPHERE body_sphere{m_position,PLAYER_ACTION_TABLE::ns_BODY_RADIUS};
    const XMFLOAT3 resolved_position = CollisionResolver::ResolveSphere(collisionWorld,body_sphere,COLLISION_LAYER::ns_STATIC);
    m_position.x = resolved_position.x;
    m_position.z = resolved_position.z;
  }
}