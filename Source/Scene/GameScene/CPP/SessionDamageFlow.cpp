#include "../H/SessionDamageFlow.h"

// 確認用の攻撃と敵の攻撃の結果を、プレイヤーへの被弾とジャスト回避の効果（スロー・演出・回避反撃）へ反映する処理を実装する。

//========= Debug インクルード=========
#include "Debug/H/GameDebugController.h"

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/CombatTable.h"
#include "Scene/Common/Combat/H/PlayerCombatController.h"
#include "Scene/Common/Combat/H/TimeScaleController.h"
#include "Scene/Common/Enemy/H/EnemyController.h"
#include "Scene/Common/Player/H/PlayerController.h"
#include "Scene/Common/Weapon/H/ShotController.h"
#include "Scene/GameScene/H/EconomyController.h"
#include "Scene/GameScene/H/EffectController.h"

//========= 初期化・終了関数=========
// 参照先を登録する。
bool SessionDamageFlow::Link(const SESSION_DAMAGE_FLOW_LINKS& links)
{
  // 参照先が1つでも欠けている場合は、以後の更新で空の参照を使ってしまうため、何も登録せず失敗を返す。
  if(links.timeScale == nullptr || links.effectController == nullptr || links.playerController == nullptr || links.playerCombat == nullptr ||
     links.shotController == nullptr || links.economyController == nullptr || links.debugController == nullptr){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"SessionDamageFlowの参照先の登録に失敗しました。");
    return false;
  }

  // 参照先を登録する。以後の更新は、この参照先を使う。
  m_timeScale = links.timeScale;
  m_effectController = links.effectController;
  m_playerController = links.playerController;
  m_playerCombat = links.playerCombat;
  m_shotController = links.shotController;
  m_economyController = links.economyController;
  m_debugController = links.debugController;
  return true;
}

// 参照先を手放す。
void SessionDamageFlow::Unlink()
{
  m_timeScale = nullptr;
  m_effectController = nullptr;
  m_playerController = nullptr;
  m_playerCombat = nullptr;
  m_shotController = nullptr;
  m_economyController = nullptr;
  m_debugController = nullptr;
}

//========= 更新関数=========
// 確認用の攻撃が当たる瞬間の判定結果を、プレイヤーへのダメージとジャスト回避の効果へ反映する。
void SessionDamageFlow::UpdateDodgeJudge(float worldDeltaTime)
{
  // 予告した攻撃が当たる瞬間だけ、結果が返る。結果の記録は、GameDebugControllerが行う。
  const DODGE_JUDGE_RESULT judge_result = m_debugController->UpdateDodgeJudge(worldDeltaTime);

  // 命中した場合は、ダメージ・怯み・ヒットストップを与える。
  if(judge_result == DODGE_JUDGE_RESULT::e_HIT){
    m_playerCombat->ApplyHitReaction(COMBAT_TABLE::ns_SIMULATED_ATTACK_DAMAGE,COMBAT_TABLE::ns_DAMAGE_HIT_STOP_SECONDS);
  }
  // ジャスト回避の場合は、スローと回避反撃を開始する。
  else if(judge_result == DODGE_JUDGE_RESULT::e_JUST_DODGED){
    StartJustDodgeEffects();
  }
}

// 敵の攻撃イベントを、プレイヤーへの被弾・ジャスト回避の効果・表示用の記録へ反映する。
void SessionDamageFlow::ApplyEnemyAttackEvent(const ENEMY_ATTACK_EVENT& attackEvent)
{
  // 表示用の記録が求められた場合だけ、直近の判定結果を更新する。接近型の結果は求めない。
  if(attackEvent.shouldRecordResult)m_debugController->RecordJudgeResult(attackEvent.result);

  // 無敵でなく命中した場合は、ダメージ・怯み・ヒットストップを与える。
  if(attackEvent.result == DODGE_JUDGE_RESULT::e_HIT){
    m_playerCombat->ApplyHitReaction(attackEvent.damage,COMBAT_TABLE::ns_DAMAGE_HIT_STOP_SECONDS);
  }
  // 実際に回避中のジャスト回避の場合は、スローと回避反撃を開始する。
  else if(attackEvent.shouldStartSlowMotion){
    StartJustDodgeEffects();
  }
}

//========= 反映の補助関数=========
// ジャスト回避が成立した時の効果を反映する。
void SessionDamageFlow::StartJustDodgeEffects()
{
  // ジャスト回避のスローを開始する。連続した場合は、時間を数え直す。
  m_timeScale->StartSlowMotion();

  // ジャスト回避の演出を、プレイヤーの付近から広げて出す。連続した場合も、そのたびに出す。
  m_effectController->SpawnJustDodgeEffect(m_playerController->GetPosition());

  // 回避反撃を取得している（倍率が1.0より大きい）場合だけ、一定時間、射撃のダメージを増やす。未取得なら、何も増やさない。
  const float damage_scale = m_economyController->GetReward().GetCounterDodgeDamageScale();
  if(damage_scale > 1.0f)m_shotController->ActivateDamageBoost(damage_scale,REWARD_TABLE::ns_COUNTER_DODGE_SECONDS);
}
