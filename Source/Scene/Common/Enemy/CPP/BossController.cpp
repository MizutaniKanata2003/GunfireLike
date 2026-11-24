#include "../H/BossController.h"

// ボスの接近・攻撃の選択・予告・判定を実装する。ボスの体力と状態はEnemyが持ち、このファイルでは変更しない（位置の移動だけ、EnemyApproachを通じて行う）。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで、攻撃の待ち時間を0未満にしない。
#include <cmath> // std::sqrtでボスとプレイヤーの水平距離を求め、std::fabsで範囲の中心からのずれの大きさを求める。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/AreaTelegraph.h"
#include "Scene/Common/Enemy/H/Enemy.h"
#include "Scene/Common/Enemy/H/EnemyApproach.h"
#include "Scene/Common/Player/H/PlayerController.h"
#include "Scene/Common/Player/H/PlayerHealth.h"

// ボスの距離の計算に使う補助関数をまとめる。
namespace
{
  // 2点のXZ平面での距離を返す。高さは使わない。
  // from: 始点のワールド座標。
  // to: 終点のワールド座標。
  // 戻り値: 水平距離。
  float GetHorizontalDistance(const XMFLOAT3& from,const XMFLOAT3& to)
  {
    const float offset_x = to.x - from.x;
    const float offset_z = to.z - from.z;
    return std::sqrt(offset_x * offset_x + offset_z * offset_z);
  }
}

//========= 初期化・終了関数=========
// 参照先を登録する。
bool BossController::Initialize(const BOSS_CONTROLLER_LINKS& links)
{
  // 参照先が1つでも欠けている場合は、以後の更新で空の参照を使ってしまうため、何も登録せず失敗を返す。
  if(links.playerController == nullptr || links.playerHealth == nullptr || links.collisionWorld == nullptr || links.areaTelegraph == nullptr){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"BossControllerの参照先の登録に失敗しました。");
    return false;
  }

  m_playerController = links.playerController;
  m_playerHealth = links.playerHealth;
  m_collisionWorld = links.collisionWorld;
  m_areaTelegraph = links.areaTelegraph;
  return true;
}

// 参照先を手放す。
void BossController::Uninit()
{
  m_playerController = nullptr;
  m_playerHealth = nullptr;
  m_collisionWorld = nullptr;
  m_areaTelegraph = nullptr;
}

// 攻撃の予告と待ち時間を初期状態へ戻す。
void BossController::Reset()
{
  // 予告を取り消し、攻撃の履歴を消す。最初の攻撃は、出現してから一定時間が過ぎるまで予告しない。
  m_judge.Initialize();
  m_currentAttack = BOSS_ATTACK::e_NONE;
  m_lastAttack = BOSS_ATTACK::e_NONE;
  m_recoverySeconds = BOSS_TABLE::ns_FIRST_ATTACK_DELAY_SECONDS;
}

//========= 更新関数=========
// ボスを1回分更新する。
BOSS_UPDATE_RESULT BossController::Update(Enemy& boss,std::size_t slotIndex,float worldDeltaTime)
{
  // 戻す結果。攻撃の判定がなければ、初期値のまま。
  BOSS_UPDATE_RESULT result{};

  // 参照先が未登録の場合は、何も更新しない。
  if(m_playerController == nullptr || m_playerHealth == nullptr || m_collisionWorld == nullptr || m_areaTelegraph == nullptr)return result;

  // ボスが死亡・転倒している、またはプレイヤーが死亡している場合は、予告を取り消し、範囲の表示を消して戻る。怯みでは、取り消さない。
  if(!boss.IsAlive() || boss.IsKnockedDown() || m_playerHealth->IsDead()){
    CancelAttack(slotIndex);
    return result;
  }

  // ヒットストップで敵側の時間が進まない場合は、移動も新しい予告も始めず、予告も進めない。
  if(worldDeltaTime <= 0.0f)return result;

  // ボスとプレイヤーの位置と、水平距離を求める。
  const XMFLOAT3 boss_position = boss.GetBodySphere().center;
  const XMFLOAT3& player_position = m_playerController->GetPosition();

  // 予告中は、その場に立ち止まり、予告を進める。判定の瞬間だけ、結果が返る。
  if(m_judge.HasIncomingAttack())return UpdateAttack(slotIndex,boss_position,player_position,worldDeltaTime);

  // 待ち時間を、敵側の時間で減らす。残り時間が負にならないようにする。待ち時間が終わっていて、攻撃を選べる距離なら、予告を始めて戻る。
  m_recoverySeconds = std::max(0.0f,m_recoverySeconds - worldDeltaTime);
  const float distance = GetHorizontalDistance(boss_position,player_position);
  const BOSS_ATTACK next_attack = m_recoverySeconds <= 0.0f ? ChooseAttack(distance) : BOSS_ATTACK::e_NONE;
  if(next_attack != BOSS_ATTACK::e_NONE){
    StartAttack(next_attack,slotIndex,player_position);
    return result;
  }

  // 攻撃を始めない間は、プレイヤーへ近づく。停止距離の内では動かない。怯み中も動く。
  ENEMY_APPROACH::MoveToward(boss,ENEMY_APPROACH::ENEMY_MOVE_ORDER{player_position,BOSS_TABLE::ns_SPEED,BOSS_TABLE::ns_STOP_DISTANCE},
                             worldDeltaTime,*m_collisionWorld);
  return result;
}

//========= 補助関数=========
// 距離と直前の攻撃から、次の攻撃を選ぶ。
BOSS_ATTACK BossController::ChooseAttack(float horizontalDistance) const
{
  // 近い距離で、直前の攻撃が振り下ろしでなければ、振り下ろしを選ぶ。振り下ろしが続かないよう、直前が振り下ろしなら、次の判断へ進む。
  if(horizontalDistance <= BOSS_TABLE::ns_SWING_ENGAGE_DISTANCE && m_lastAttack != BOSS_ATTACK::e_SWING){
    return BOSS_ATTACK::e_SWING;
  }

  // 叩きつけを始められる距離の内なら、叩きつけを選ぶ。近い距離で、直前が振り下ろしだった場合も、ここで叩きつけになる。
  if(horizontalDistance <= BOSS_TABLE::ns_SLAM_ENGAGE_DISTANCE)return BOSS_ATTACK::e_SLAM;

  // どちらも始められない距離なら、選ばない。
  return BOSS_ATTACK::e_NONE;
}

// 攻撃の予告を始める。
void BossController::StartAttack(BOSS_ATTACK attack,std::size_t slotIndex,const XMFLOAT3& playerPosition)
{
  m_currentAttack = attack;

  // 叩きつけは、プレイヤーのいる位置へ範囲を固定して表示し、予告の時間を設定する。範囲は、プレイヤーが動いても追わない。
  if(attack == BOSS_ATTACK::e_SLAM){
    m_areaTelegraph->Begin(slotIndex,playerPosition);
    m_judge.StartIncomingAttack(BOSS_TABLE::ns_SLAM_WINDUP_SECONDS);
    return;
  }

  // 振り下ろしは、範囲を表示せず、予告の時間だけを設定する。ボスの色が、予告の色になる。
  m_judge.StartIncomingAttack(BOSS_TABLE::ns_SWING_WINDUP_SECONDS);
}

// 予告中の攻撃を進め、判定の瞬間に結果を返す。
BOSS_UPDATE_RESULT BossController::UpdateAttack(std::size_t slotIndex,const XMFLOAT3& bossPosition,const XMFLOAT3& playerPosition,float worldDeltaTime)
{
  BOSS_UPDATE_RESULT result{};

  // 回避以外の無敵も被弾を防ぐが、回避開始時刻としては使わない。回避の動作中だけ、回避の開始からの経過時間を渡す。
  const bool is_dodging = m_playerController->GetActionState() == PLAYER_ACTION_STATE::e_DODGE;
  const float dodge_age_seconds = is_dodging ? m_playerController->GetSecondsSinceDodgeStart() : BOSS_TABLE::ns_NON_DODGE_AGE_SECONDS;
  const DODGE_JUDGE_RESULT judge_result = m_judge.Update(worldDeltaTime,m_playerController->IsInvincible(),dodge_age_seconds);

  // 予告が続いている間は、範囲を表示したまま、何も起こさない。
  if(judge_result == DODGE_JUDGE_RESULT::e_NONE)return result;

  // 判定の瞬間になったため、攻撃を終えて、攻撃ごとの待ち時間に入る。
  const BOSS_ATTACK finished_attack = m_currentAttack;
  m_currentAttack = BOSS_ATTACK::e_NONE;
  m_lastAttack = finished_attack;
  m_recoverySeconds = finished_attack == BOSS_ATTACK::e_SWING ? BOSS_TABLE::ns_SWING_RECOVERY_SECONDS : BOSS_TABLE::ns_SLAM_RECOVERY_SECONDS;

  // プレイヤーが、攻撃の範囲の内にいるかを調べる。振り下ろしは、ボスの中心からの水平距離。叩きつけは、固定した範囲の中心から、X・Z方向とも半分の大きさ以内。
  // 叩きつけの範囲の表示は、判定の瞬間に消す。しゃがんでも、範囲の内にいれば避けられない。
  bool is_in_range = false;
  float damage = BOSS_TABLE::ns_SWING_DAMAGE;
  if(finished_attack == BOSS_ATTACK::e_SWING){
    is_in_range = GetHorizontalDistance(bossPosition,playerPosition) <= BOSS_TABLE::ns_SWING_REACH;
  }
  // 叩きつけの場合は、固定した範囲の中心を取り出して、表示を終えてから、範囲の内にいるかを調べる。
  else{
    const XMFLOAT3 area_center = m_areaTelegraph->GetCenter(slotIndex);
    m_areaTelegraph->End(slotIndex);
    is_in_range = std::fabs(playerPosition.x - area_center.x) <= AREA_TELEGRAPH::ns_HALF_SIZE &&
      std::fabs(playerPosition.z - area_center.z) <= AREA_TELEGRAPH::ns_HALF_SIZE;
    damage = BOSS_TABLE::ns_SLAM_DAMAGE;
  }

  // 範囲の外にいる場合は、被弾もジャスト回避のスローも発生しない。
  if(!is_in_range)return result;

  // 範囲の内にいた場合の結果を返す。実際に回避中のジャスト回避だけが、スローを開始する。
  result.hasAttackEvent = true;
  result.result = judge_result;
  result.damage = damage;
  result.shouldStartSlowMotion = is_dodging && judge_result == DODGE_JUDGE_RESULT::e_JUST_DODGED;
  return result;
}

// 予告中の攻撃を取り消す。
void BossController::CancelAttack(std::size_t slotIndex)
{
  // 予告中だった場合だけ、予告を消して、待ち時間を設定する。行動不能が続いても、毎回は設定しない。
  if(m_judge.HasIncomingAttack()){
    m_judge.Initialize();
    m_recoverySeconds = BOSS_TABLE::ns_INTERRUPT_RECOVERY_SECONDS;
  }

  // 攻撃の状態と範囲の表示を消す。範囲を表示していない場合も、呼出可能。
  m_currentAttack = BOSS_ATTACK::e_NONE;
  if(m_areaTelegraph != nullptr){
    m_areaTelegraph->End(slotIndex);
  }
}