#include "../H/EnemyController.h"

// 敵の種類別の更新（接近型の追従と近接、射撃型の距離維持と発射、範囲攻撃型の接近と範囲の予告・判定、ボスの攻撃結果の通知）、敵の弾の更新、ウェーブの生成、敵側の描画を実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで、再攻撃待ちの残り時間を0未満にしない。
#include <cmath> // std::sqrtで射撃型・範囲攻撃型とプレイヤーの水平距離を求め、std::fabsで範囲の中心からのずれの大きさを求める。
#include <vector> // 敵の弾の判定結果を、std::vectorで受け取る。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/CombatTable.h"
#include "Scene/Common/Enemy/H/EnemyApproach.h"
#include "Scene/Common/Enemy/H/EnemyAreaBehavior.h"
#include "Scene/Common/Enemy/H/EnemyRangedBehavior.h"
#include "Scene/Common/Object/H/ModelRegistry.h"
#include "Scene/Common/Player/H/PlayerController.h"
#include "Scene/Common/Player/H/PlayerHealth.h"

// 敵の更新に使う、位置の補助関数をまとめる。
namespace
{
  // 2点の水平距離（X・Z平面の距離）を返す。高さは使わない。
  // fromPosition: 距離の起点。
  // toPosition: 距離の終点。
  // 戻り値: 水平距離。単位はワールド距離単位。
  float CalculateHorizontalDistance(const XMFLOAT3& fromPosition,const XMFLOAT3& toPosition)
  {
    const float offset_x = toPosition.x - fromPosition.x;
    const float offset_z = toPosition.z - fromPosition.z;
    return std::sqrt(offset_x * offset_x + offset_z * offset_z);
  }
}

//========= 初期化・終了関数=========
// 参照先と通知先を登録し、敵の弾と範囲の予告のCube Rendererを借用し、ボスの更新へ参照先を渡す。
bool EnemyController::Initialize(const ENEMY_CONTROLLER_LINKS& links)
{
  // 参照先または通知先が1つでも欠けている場合は、以後の更新で空の参照を使ってしまうため、何も登録せず失敗を返す。
  if(links.modelRegistry == nullptr || links.collisionWorld == nullptr || links.playerController == nullptr ||
     links.playerHealth == nullptr || !links.onAttackEvent){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"EnemyControllerの参照先の登録に失敗しました。");
    return false;
  }

  // 参照先と通知先を登録する。以後の更新と生成は、この参照先を使う。
  m_modelRegistry = links.modelRegistry;
  m_collisionWorld = links.collisionWorld;
  m_playerController = links.playerController;
  m_playerHealth = links.playerHealth;
  m_onAttackEvent = links.onAttackEvent;

  // 敵の弾の描画に使うCube Rendererを借用する。借用できなければ、失敗を返す。生成済みの部分は、呼出側のUninitで解放する。
  if(!m_enemyProjectiles.Initialize(*m_modelRegistry)){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"EnemyControllerの敵の弾の初期化に失敗しました。");
    return false;
  }

  // 範囲の予告の描画に使うCube Rendererを借用する。借用できなければ、失敗を返す。生成済みの部分は、呼出側のUninitで解放する。
  if(!m_areaTelegraph.Initialize(*m_modelRegistry)){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"EnemyControllerの範囲の予告の初期化に失敗しました。");
    return false;
  }

  // ボスの更新に使う参照先を渡す。叩きつけの範囲は、上で初期化した範囲の予告へ表示する。失敗は、BossControllerが記録する。
  BOSS_CONTROLLER_LINKS boss_links{};
  boss_links.playerController = m_playerController;
  boss_links.playerHealth = m_playerHealth;
  boss_links.collisionWorld = m_collisionWorld;
  boss_links.areaTelegraph = &m_areaTelegraph;
  return m_bossController.Initialize(boss_links);
}

// 全枠のColliderを解除し、敵の弾・範囲の予告を解放して、参照先を手放す。
void EnemyController::Uninit()
{
  // 登録先のCollisionWorldが分かっている場合だけ、全枠のColliderを解除する。Initialize前は敵が未登録のため、何もしない。
  if(m_collisionWorld != nullptr){
    m_enemyManager.Clear(*m_collisionWorld);
  }

  // 借用したCube Rendererを手放し、参照先と通知先を空に戻す。敵の弾と範囲の予告は、共有Registryより先に解放する。
  // ボスは範囲の予告を借用しているため、範囲の予告より先に参照を手放す。
  m_bossController.Uninit();
  m_enemyProjectiles.Uninit();
  m_areaTelegraph.Uninit();
  m_modelRegistry = nullptr;
  m_collisionWorld = nullptr;
  m_playerController = nullptr;
  m_playerHealth = nullptr;
  m_onAttackEvent = nullptr;
}

// 全枠の攻撃予告と再攻撃待ちを初期状態へ戻し、飛行中の敵の弾と範囲の予告を消し、ボスの攻撃の状態を戻す。
void EnemyController::ResetAttackState()
{
  // 全枠の攻撃予告を破棄し、再配置後の敵が旧予告で攻撃しないようにする。
  for(JustDodgeJudge& judge : m_attackJudges){
    judge.Initialize();
  }
  m_recoverySeconds.fill(0.0f);

  // 旧ウェーブの飛行中の弾と範囲の予告を消す。ボスの予告と待ち時間も戻し、新しいボスが、最初の攻撃までの待ち時間から始まるようにする。
  m_enemyProjectiles.Clear();
  m_areaTelegraph.Clear();
  m_bossController.Reset();
}

//========= 生成関数=========
// 指定した部屋とウェーブを生成し、成功した場合だけ攻撃状態を新しい編成に合わせる。
bool EnemyController::TrySpawnWave(int roomNumber,int waveNumber)
{
  // 登録に失敗した場合は、EnemyManagerが生成済みの枠を解除して空にする。攻撃状態は変えず、失敗を返す。
  if(!m_enemyManager.SpawnWave(roomNumber,waveNumber,*m_modelRegistry,*m_collisionWorld)){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"EnemyControllerの部屋ウェーブの生成に失敗しました。");
    return false;
  }

  // 新しい敵が旧予告・旧ウェーブの弾と範囲の予告を引き継がないよう、攻撃状態を初期化する。
  ResetAttackState();
  return true;
}

//========= 更新・描画関数=========
// 全枠の敵を更新し、続けて敵の弾を進める。
void EnemyController::Update(float worldDeltaTime)
{
  // 敵の行動を先に更新し、射撃型が今回の更新で生成した弾を、同じ更新の中で進める。
  UpdateActions(worldDeltaTime);
  UpdateProjectiles(worldDeltaTime);
}

// 予告中の範囲を、床の上の赤い板として描く。
void EnemyController::DrawAreaTelegraph(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
  m_areaTelegraph.Draw(graphicsSystem,viewMatrix,projectionMatrix);
}

// 全枠の敵を出現順に描き、続けて飛行中の敵の弾を描く。
void EnemyController::DrawEnemies(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
  // 全枠の敵を出現順に描画する。死亡枠はEnemy::Drawが描画を省略する。
  for(const Enemy& enemy : m_enemyManager.GetEnemies()){
    enemy.Draw(graphicsSystem,viewMatrix,projectionMatrix);
  }

  // 敵の後に、飛行中の敵の弾を描画する。
  m_enemyProjectiles.Draw(graphicsSystem,viewMatrix,projectionMatrix);
}

//========= 更新の補助関数=========
// 全枠の敵を出現順に、種類に応じて更新し、予告中の敵の表示を切り替える。
void EnemyController::UpdateActions(float worldDeltaTime)
{
  // 全枠を先頭から更新する。死亡枠や行動不能の敵は、枠ごとの更新の中で予告を中断する。
  for(std::size_t slot_index = 0; slot_index < m_attackJudges.size(); ++slot_index){
    // 敵の種類ごとに、対応する更新を呼ぶ。
    const ENEMY_KIND kind = m_enemyManager.GetEnemies()[slot_index].GetKind();
    switch(kind){
      // 接近型は、追従と近接の攻撃予告・命中を更新する。
    case ENEMY_KIND::e_MELEE:
    UpdateMeleeSlot(slot_index,worldDeltaTime);
    break;

    // 射撃型は、距離維持の移動と、発射の予告と、弾の生成を更新する。
    case ENEMY_KIND::e_RANGED:
    UpdateRangedSlot(slot_index,worldDeltaTime);
    break;

    // 範囲攻撃型は、接近と、床の範囲の予告と、予告の終わりの範囲の判定を更新する。
    case ENEMY_KIND::e_AREA:
    UpdateAreaSlot(slot_index,worldDeltaTime);
    break;

    // ボスは、接近と、振り下ろし・叩きつけの予告と判定を更新する。
    case ENEMY_KIND::e_BOSS:
    UpdateBossSlot(slot_index,worldDeltaTime);
    break;
    }

    // 予告の状態を、その敵の表示へ反映する。死亡・怯みなどで予告が中断された場合は、解除される。ボスの予告は、BossControllerが持つ。
    const bool is_telegraphing = kind == ENEMY_KIND::e_BOSS ? m_bossController.IsTelegraphing() : m_attackJudges[slot_index].HasIncomingAttack();
    m_enemyManager.GetEnemies()[slot_index].SetTelegraphing(is_telegraphing);
  }
}

// 戦闘不能な敵の攻撃予告を取り消し、再攻撃待ちを設定する。
bool EnemyController::CancelIfIncapacitated(std::size_t slotIndex,float recoverySeconds)
{
  // 生きていて、怯み・転倒をしておらず、プレイヤーも死亡していなければ、戦闘不能ではない。
  const Enemy& enemy = m_enemyManager.GetEnemies()[slotIndex];
  if(enemy.IsAlive() && !enemy.IsFlinching() && !enemy.IsKnockedDown() && !m_playerHealth->IsDead())return false;

  // 予告中断時だけ再攻撃待ちを設定し、行動不能が続いても毎回は設定しない。
  JustDodgeJudge& judge = m_attackJudges[slotIndex];
  if(judge.HasIncomingAttack()){
    judge.Initialize();
    m_recoverySeconds[slotIndex] = recoverySeconds;
  }
  return true;
}

// 接近型の敵だけ、予告中は追従を止め、予告が終わったフレームで一度だけ命中を判定する。
void EnemyController::UpdateMeleeSlot(std::size_t slotIndex,float worldDeltaTime)
{
  // 戦闘不能になった敵の予告は、時間停止中でも取り消して持ち越さない。
  if(CancelIfIncapacitated(slotIndex,COMBAT_TABLE::ns_ENEMY_MELEE_RECOVERY_SECONDS))return;

  // ヒットストップで敵側の時間が進まない場合は、新しい予告も始めない。
  if(worldDeltaTime <= 0.0f)return;

  // この枠の敵と、その敵専用の予告・再攻撃待ちを参照する。
  Enemy& enemy = m_enemyManager.GetEnemies()[slotIndex];
  JustDodgeJudge& judge = m_attackJudges[slotIndex];
  float& recovery_seconds = m_recoverySeconds[slotIndex];

  // 追従の停止距離と同じXZ平面で測る。二乗同士を比較し、平方根を取らずに範囲を求める。
  const XMFLOAT3 enemy_position = enemy.GetBodySphere().center;
  const XMFLOAT3& player_position = m_playerController->GetPosition();
  const float offset_x = player_position.x - enemy_position.x;
  const float offset_z = player_position.z - enemy_position.z;
  const float reach = COMBAT_TABLE::ns_ENEMY_MELEE_REACH;
  const bool is_in_reach = offset_x * offset_x + offset_z * offset_z <= reach * reach;

  // 予告中は追従せず、判定が返るまで敵側の時間で予告を進める。
  if(judge.HasIncomingAttack()){
    // 回避以外の無敵も被弾を防ぐが、回避開始時刻としては使わない。
    const bool is_dodging = m_playerController->GetActionState() == PLAYER_ACTION_STATE::e_DODGE;
    const float dodge_age_seconds = is_dodging ? m_playerController->GetSecondsSinceDodgeStart() : COMBAT_TABLE::ns_ENEMY_MELEE_RECOVERY_SECONDS;
    const bool is_invincible = m_playerController->IsInvincible();
    const DODGE_JUDGE_RESULT result = judge.Update(worldDeltaTime,is_invincible,dodge_age_seconds);
    // 予告が続いている間は、命中も再攻撃待ちの開始も行わない。
    if(result == DODGE_JUDGE_RESULT::e_NONE)return;

    // 予告を消費したので、範囲外の空振りでも再攻撃待ちに入る。
    recovery_seconds = COMBAT_TABLE::ns_ENEMY_MELEE_RECOVERY_SECONDS;
    // 予告終了時点で範囲外なら、被弾もジャスト回避のスローも発生しない。
    if(!is_in_reach)return;

    // 範囲内で無敵でなければ、命中として通知し、HP・怯み・ヒットストップは呼出側に任せる。接近型の結果は、表示用の記録に含めない。
    if(result == DODGE_JUDGE_RESULT::e_HIT){
      NotifyAttackEvent(ENEMY_ATTACK_EVENT{result,COMBAT_TABLE::ns_ENEMY_MELEE_DAMAGE,false,false});
    }
    // 実際に回避中のジャスト回避だけが、スローの開始を通知する。
    else if(is_dodging && result == DODGE_JUDGE_RESULT::e_JUST_DODGED){
      NotifyAttackEvent(ENEMY_ATTACK_EVENT{result,0.0f,false,true});
    }
    return;
  }

  // 再攻撃待ちは敵側の時間で減らす。残り時間が負にならないようにする。
  recovery_seconds = std::max(0.0f,recovery_seconds - worldDeltaTime);
  // 射程外では追従だけを行い、予告の開始は次の更新で判定する。
  if(!is_in_reach){
    ENEMY_APPROACH::Update(enemy,player_position,worldDeltaTime,*m_collisionWorld);
    return;
  }

  // 射程内では待ち時間が終わってから、次の攻撃を予告する。
  if(recovery_seconds <= 0.0f){
    judge.StartIncomingAttack(COMBAT_TABLE::ns_ENEMY_MELEE_WINDUP_SECONDS);
  }
}

// 予告中は移動せず、予告が終わったフレームで弾を生成する。予告以外では、距離を保つ。
void EnemyController::UpdateRangedSlot(std::size_t slotIndex,float worldDeltaTime)
{
  // 戦闘不能になった敵の予告は、時間停止中でも取り消して持ち越さない。
  if(CancelIfIncapacitated(slotIndex,ENEMY_RANGED::ns_RECOVERY_SECONDS))return;

  // ヒットストップで敵側の時間が進まない場合は、移動も新しい予告も始めない。
  if(worldDeltaTime <= 0.0f)return;

  // この枠の敵と、その敵専用の予告・再攻撃待ちを参照する。
  Enemy& enemy = m_enemyManager.GetEnemies()[slotIndex];
  JustDodgeJudge& judge = m_attackJudges[slotIndex];
  float& recovery_seconds = m_recoverySeconds[slotIndex];

  // 敵とプレイヤーの水平距離を求める。高さは使わない。
  const XMFLOAT3 enemy_position = enemy.GetBodySphere().center;
  const XMFLOAT3& player_position = m_playerController->GetPosition();
  const float distance = CalculateHorizontalDistance(enemy_position,player_position);

  // 予告中はその場に立ち止まり、予告の終わりだけ、発射のタイミングに達した旨を受け取る。
  if(judge.HasIncomingAttack()){
    // 射撃型は回避の判定を使わないため、無敵でない状態と、十分に大きい回避からの経過時間を渡し、予告の終わりだけを検出する。
    const DODGE_JUDGE_RESULT result = judge.Update(worldDeltaTime,false,ENEMY_RANGED::ns_UNUSED_DODGE_AGE_SECONDS);
    // 予告が続いている間は、何も起こさない。
    if(result == DODGE_JUDGE_RESULT::e_NONE)return;

    // 予告の終わりで、再攻撃待ちに入り、予告の終わりのプレイヤーの位置へ向けて、弾を1つ生成する。弾は、以後追尾しない。
    recovery_seconds = ENEMY_RANGED::ns_RECOVERY_SECONDS;
    m_enemyProjectiles.Spawn(enemy_position,player_position);
    return;
  }

  // 再攻撃待ちは敵側の時間で減らす。残り時間が負にならないようにする。
  recovery_seconds = std::max(0.0f,recovery_seconds - worldDeltaTime);

  // 遠すぎる間は近づき、近すぎる間は離れる。移動した更新では、予告を始めない。
  if(distance > ENEMY_RANGED::ns_FAR_DISTANCE){
    ENEMY_RANGED::Move(enemy,ENEMY_RANGED::MOVE_REQUEST{player_position,1.0f,worldDeltaTime},*m_collisionWorld);
    return;
  }
  // 近すぎる場合は、プレイヤーから離れる。
  if(distance < ENEMY_RANGED::ns_NEAR_DISTANCE){
    ENEMY_RANGED::Move(enemy,ENEMY_RANGED::MOVE_REQUEST{player_position,-1.0f,worldDeltaTime},*m_collisionWorld);
    return;
  }

  // 適切な距離にいて、再攻撃待ちが終わっていれば、立ち止まったまま、次の攻撃を予告する。
  if(recovery_seconds <= 0.0f){
    judge.StartIncomingAttack(ENEMY_RANGED::ns_WINDUP_SECONDS);
  }
}

// 予告中は移動せず、予告の終わりに、固定した範囲の内にいるプレイヤーへ判定する。予告以外では、攻撃を始める距離まで近づく。
void EnemyController::UpdateAreaSlot(std::size_t slotIndex,float worldDeltaTime)
{
  // 戦闘不能になった敵の予告と、床の範囲の表示は、時間停止中でも取り消して持ち越さない。
  if(CancelIfIncapacitated(slotIndex,ENEMY_AREA::ns_RECOVERY_SECONDS)){
    m_areaTelegraph.End(slotIndex);
    return;
  }

  // ヒットストップで敵側の時間が進まない場合は、移動も新しい予告も始めない。
  if(worldDeltaTime <= 0.0f)return;

  // この枠の敵と、その敵専用の予告・再攻撃待ちを参照する。
  Enemy& enemy = m_enemyManager.GetEnemies()[slotIndex];
  JustDodgeJudge& judge = m_attackJudges[slotIndex];
  float& recovery_seconds = m_recoverySeconds[slotIndex];

  // 敵とプレイヤーの位置を取得する。範囲の判定にも、接近の判断にも使う。
  const XMFLOAT3 enemy_position = enemy.GetBodySphere().center;
  const XMFLOAT3& player_position = m_playerController->GetPosition();

  // 予告中はその場に立ち止まり、予告の終わりだけを受け取る。範囲の中心は、予告を始めた時点で固定した位置のままにする。
  if(judge.HasIncomingAttack()){
    // 回避以外の無敵も被弾を防ぐが、回避開始時刻としては使わない。
    const bool is_dodging = m_playerController->GetActionState() == PLAYER_ACTION_STATE::e_DODGE;
    const float dodge_age_seconds = is_dodging ? m_playerController->GetSecondsSinceDodgeStart() : ENEMY_AREA::ns_NON_DODGE_AGE_SECONDS;
    const DODGE_JUDGE_RESULT result = judge.Update(worldDeltaTime,m_playerController->IsInvincible(),dodge_age_seconds);
    // 予告が続いている間は、範囲を表示したまま、何も起こさない。
    if(result == DODGE_JUDGE_RESULT::e_NONE)return;

    // 予告の終わりで、再攻撃待ちに入り、固定した範囲の中心を取り出して、範囲の表示を終える。
    recovery_seconds = ENEMY_AREA::ns_RECOVERY_SECONDS;
    const XMFLOAT3 area_center = m_areaTelegraph.GetCenter(slotIndex);
    m_areaTelegraph.End(slotIndex);

    // 範囲は正方形で、高さを使わない。中心からX・Z方向とも半分の大きさ以内なら、範囲の内とする。しゃがんでも範囲の内にいれば、避けられない。
    const bool is_in_area = std::fabs(player_position.x - area_center.x) <= AREA_TELEGRAPH::ns_HALF_SIZE &&
      std::fabs(player_position.z - area_center.z) <= AREA_TELEGRAPH::ns_HALF_SIZE;
    // 範囲の外にいる場合は、被弾もジャスト回避のスローも発生しない。
    if(!is_in_area)return;

    // 範囲の内にいた場合の結果を、表示用の記録を含めて通知する。実際に回避中のジャスト回避だけが、スローを開始する。
    const bool should_start_slow_motion = is_dodging && result == DODGE_JUDGE_RESULT::e_JUST_DODGED;
    NotifyAttackEvent(ENEMY_ATTACK_EVENT{result,ENEMY_AREA::ns_DAMAGE,true,should_start_slow_motion});
    return;
  }

  // 再攻撃待ちは敵側の時間で減らす。残り時間が負にならないようにする。
  recovery_seconds = std::max(0.0f,recovery_seconds - worldDeltaTime);

  // 攻撃を始める距離より遠い間は近づく。近づいた更新では、予告を始めない。
  if(CalculateHorizontalDistance(enemy_position,player_position) > ENEMY_AREA::ns_ENGAGE_DISTANCE){
    ENEMY_AREA::Approach(enemy,player_position,worldDeltaTime,*m_collisionWorld);
    return;
  }

  // 攻撃を始める距離の内で、再攻撃待ちが終わっていれば、プレイヤーのいる位置へ範囲を固定して予告を始める。範囲は、プレイヤーが動いても追わない。
  if(recovery_seconds <= 0.0f){
    m_areaTelegraph.Begin(slotIndex,player_position);
    judge.StartIncomingAttack(ENEMY_AREA::ns_WINDUP_SECONDS);
  }
}

// ボスをBossControllerに更新させ、攻撃の判定がプレイヤーへ及んだ場合は、攻撃イベントとして通知する。
void EnemyController::UpdateBossSlot(std::size_t slotIndex,float worldDeltaTime)
{
  // 接近・攻撃の予告・判定は、BossControllerが行う。ボスの結果は、表示用の記録に含める。
  const BOSS_UPDATE_RESULT boss_result = m_bossController.Update(m_enemyManager.GetEnemies()[slotIndex],slotIndex,worldDeltaTime);

  // 攻撃の判定がプレイヤーへ及ばなかった（予告中・空振り・攻撃なし）場合は、通知しない。
  if(!boss_result.hasAttackEvent)return;

  NotifyAttackEvent(ENEMY_ATTACK_EVENT{boss_result.result,boss_result.damage,true,boss_result.shouldStartSlowMotion});
}

// 弾を進め、プレイヤーへ命中・回避した弾の結果を、攻撃イベントとして通知する。
void EnemyController::UpdateProjectiles(float worldDeltaTime)
{
  // 回避の動作中だけ、回避の開始からの経過時間がジャスト回避の判定に使われる。蹴り・殴打の無敵は、通常の回避として扱う。
  const bool is_dodging = m_playerController->GetActionState() == PLAYER_ACTION_STATE::e_DODGE;

  // 弾を進め、今回の更新で命中・回避した弾ごとの判定結果を受け取る。
  std::vector<DODGE_JUDGE_RESULT> results;
  // 弾の更新に必要な、経過時間とプレイヤーの状態を1つにまとめる。
  const ENEMY_PROJECTILE_UPDATE_INPUT update_input{worldDeltaTime,m_playerController->GetBodySphere(),m_playerController->IsInvincible(),is_dodging,
    m_playerController->GetSecondsSinceDodgeStart()};
  m_enemyProjectiles.Update(update_input,*m_collisionWorld,results);

  // 受け取った結果を、出た順に通知する。全ての結果を表示用に記録し、ジャスト回避の場合は、スローの開始を求める。連続した場合は、呼出側が時間を数え直す。
  for(const DODGE_JUDGE_RESULT result : results){
    NotifyAttackEvent(ENEMY_ATTACK_EVENT{result,ENEMY_PROJECTILE::ns_DAMAGE,true,result == DODGE_JUDGE_RESULT::e_JUST_DODGED});
  }
}

// 攻撃イベントを通知先へ渡す。
void EnemyController::NotifyAttackEvent(const ENEMY_ATTACK_EVENT& attackEvent) const
{
  // 通知先が空の場合は、空の関数を呼ぶと例外になるため、何もしない。
  if(!m_onAttackEvent)return;

  m_onAttackEvent(attackEvent);
}