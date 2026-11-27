#pragma once

// 敵の攻撃の結果と、確認用の攻撃の結果を、プレイヤーへの被弾とジャスト回避の効果へ反映する。GameSessionの更新から、被弾とジャスト回避の反映の部分を切り出したもの。
// 時間・演出・プレイヤーの戦闘・射撃・報酬・デバッグ操作への参照を借用し、所有はしない。敵の攻撃の判定そのものと、時間の停止・部屋の進行は、呼出側が決める。

//========= 前方宣言=========
class EconomyController;
class EffectController;
class GameDebugController;
class PlayerCombatController;
class PlayerController;
class ShotController;
class TimeScaleController;
struct ENEMY_ATTACK_EVENT;

// SessionDamageFlowが借用する参照先をまとめる。参照先は、Unlinkまで有効であること。
struct SESSION_DAMAGE_FLOW_LINKS
{
  // ジャスト回避のスローを開始する、ヒットストップとスロー。
  TimeScaleController* timeScale{};
  // ジャスト回避の演出を出す、演出の粒。
  EffectController* effectController{};
  // ジャスト回避の演出を出す位置（プレイヤーの位置）の読み取り元。読み取りだけを行う。
  const PlayerController* playerController{};
  // 被弾の反応（ダメージ・怯み・ヒットストップ）の反映先となるプレイヤーの戦闘。
  PlayerCombatController* playerCombat{};
  // 回避反撃（射撃のダメージの増加）の反映先となる射撃。
  ShotController* shotController{};
  // 回避反撃の倍率を持つ、通貨と報酬。読み取りだけを行う。
  EconomyController* economyController{};
  // 確認用の攻撃の判定と、直近の判定結果の記録先となるデバッグ操作。
  GameDebugController* debugController{};
};

// 被弾とジャスト回避の効果の反映を担当するクラス。
class SessionDamageFlow final
{
public:
  //========= 初期化・終了関数=========
  // 参照先を登録する。
  // links: 借用する参照先。1つでも欠けている場合は、何も登録せず失敗する。
  // 戻り値: 登録できたらtrue。失敗した場合は、ログへ記録する。
  [[nodiscard]] bool Link(const SESSION_DAMAGE_FLOW_LINKS& links);
  // 参照先を手放す。未登録でも、繰り返しても呼出可能。
  void Unlink();

  //========= 更新関数=========
  // 確認用の攻撃が当たる瞬間を判定し、結果に応じて、プレイヤーへのダメージとジャスト回避の効果を反映する。Link成功後に呼ぶ。
  // worldDeltaTime: スローを反映した経過時間。単位は秒。ヒットストップ中は0。
  void UpdateDodgeJudge(float worldDeltaTime);
  // 敵の攻撃イベントを受け取り、命中ならプレイヤーへの被弾の反応を、ジャスト回避ならその効果を反映し、求められた場合は直近の判定結果を記録する。Link成功後に呼ぶ。
  // attackEvent: EnemyControllerが通知した攻撃イベント。敵の更新の途中で呼ばれる。
  void ApplyEnemyAttackEvent(const ENEMY_ATTACK_EVENT& attackEvent);

private:
  //========= 反映の補助関数=========
  // ジャスト回避が成立した時の効果を反映する。スローを開始し、演出を出し、回避反撃を取得していれば、一定時間、射撃のダメージを増やす。
  void StartJustDodgeEffects();

  //========= 参照先=========
  // ヒットストップとスロー。非所有。未登録はnullptr。
  TimeScaleController* m_timeScale{};
  // 演出の粒。非所有。未登録はnullptr。
  EffectController* m_effectController{};
  // プレイヤーの位置の読み取り元。非所有。未登録はnullptr。
  const PlayerController* m_playerController{};
  // 被弾の反応の反映先。非所有。未登録はnullptr。
  PlayerCombatController* m_playerCombat{};
  // 回避反撃の反映先となる射撃。非所有。未登録はnullptr。
  ShotController* m_shotController{};
  // 回避反撃の倍率の取得元。非所有。未登録はnullptr。
  EconomyController* m_economyController{};
  // 確認用の攻撃の判定と、判定結果の記録先。非所有。未登録はnullptr。
  GameDebugController* m_debugController{};
};
