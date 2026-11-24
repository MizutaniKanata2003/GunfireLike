#pragma once

// ボスの行動（プレイヤーへの接近、攻撃の選択と予告、攻撃の判定、攻撃の後の待ち時間）を担当する。
// 近接の振り下ろし（ボスの近くの水平距離だけが当たる）と、範囲の叩きつけ（床へ予告した正方形の範囲だけが当たる）の、2種類の攻撃を持つ。
// ボスの体力・怯み・転倒・Colliderは、Enemyが持つ。ダメージ・ヒットストップ・スローの反映は担当せず、攻撃の結果を返して、呼出側（EnemyController）が通知する。
// 数値は企画書で未確定の試遊用の仮値。距離の単位はワールド距離単位、時間の単位は秒。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // std::size_tで、敵の固定枠の番号を受け取る。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/JustDodgeJudge.h"

//========= 前方宣言=========
class AreaTelegraph;
class CollisionWorld;
class Enemy;
class PlayerController;
class PlayerHealth;

// ボスの移動・攻撃の数値をまとめる。
namespace BOSS_TABLE
{
  //========= 移動の設定=========
  // 移動速度。単位はワールド距離単位/秒。接近型の追従（2.4）より遅い仮値。
  inline constexpr float ns_SPEED = 2.0f;
  // 近づくのを止める水平中心間距離。ボスの球の半径（1.5）より大きく、振り下ろしの届く距離（ns_SWING_REACH）より小さい。
  inline constexpr float ns_STOP_DISTANCE = 3.2f;

  //========= 攻撃の選択=========
  // 振り下ろしを始められる水平距離の上限。この距離の内で、直前の攻撃が振り下ろしでなければ、振り下ろしを選ぶ。
  inline constexpr float ns_SWING_ENGAGE_DISTANCE = 3.6f;
  // 叩きつけを始められる水平距離の上限。振り下ろしを選ばない場合に、この距離の内なら、叩きつけを選ぶ。これより遠い間は、近づく。
  inline constexpr float ns_SLAM_ENGAGE_DISTANCE = 10.0f;
  // 出現してから、最初の攻撃を予告できるまでの時間。敵側の時間。仮値。
  inline constexpr float ns_FIRST_ATTACK_DELAY_SECONDS = 1.5f;

  //========= 振り下ろし=========
  // 予告を始めてから、判定までの時間。敵側の時間。仮値。
  inline constexpr float ns_SWING_WINDUP_SECONDS = 0.8f;
  // 判定の瞬間に、ボスの中心とプレイヤーの水平距離がこの値以内なら、当たる。ボスの外へ離れれば、避けられる。
  inline constexpr float ns_SWING_REACH = 3.8f;
  // 当たった場合にプレイヤーへ与えるダメージ。仮値。
  inline constexpr float ns_SWING_DAMAGE = 20.0f;
  // 判定の後、次の攻撃を予告できるまでの時間。敵側の時間。仮値。
  inline constexpr float ns_SWING_RECOVERY_SECONDS = 1.2f;

  //========= 叩きつけ=========
  // 予告を始めてから、判定までの時間。敵側の時間。予告した範囲は、プレイヤーが動いても追わない。仮値。
  inline constexpr float ns_SLAM_WINDUP_SECONDS = 1.5f;
  // 当たった場合にプレイヤーへ与えるダメージ。範囲の大きさは、予告の表示と同じ値（AREA_TELEGRAPH::ns_HALF_SIZE）を使う。仮値。
  inline constexpr float ns_SLAM_DAMAGE = 30.0f;
  // 判定の後、次の攻撃を予告できるまでの時間。敵側の時間。仮値。
  inline constexpr float ns_SLAM_RECOVERY_SECONDS = 2.0f;

  //========= 共通の設定=========
  // 転倒などで予告が中断された後、次の攻撃を予告できるまでの時間。敵側の時間。仮値。
  inline constexpr float ns_INTERRUPT_RECOVERY_SECONDS = 1.5f;
  // 回避の動作中ではない場合に、回避の開始からの経過時間として渡す値。単位は秒。ジャスト回避の閾値より十分に大きく、蹴り・殴打の無敵をジャスト回避にしない。
  inline constexpr float ns_NON_DODGE_AGE_SECONDS = 1000.0f;
}

// ボスの攻撃の種類。
enum class BOSS_ATTACK
{
  e_NONE,  // 攻撃していない。または、まだ攻撃をしたことがない。
  e_SWING, // 近接の振り下ろし。
  e_SLAM   // 床へ範囲を予告する叩きつけ。
};

// BossControllerが借用する参照先をまとめる。参照先は、Uninitまで有効であること。
struct BOSS_CONTROLLER_LINKS
{
  // 位置・動作の状態・無敵・回避の経過時間の読み取り元となるプレイヤー。読み取りだけを行う。
  const PlayerController* playerController{};
  // 死亡の確認に使うプレイヤーのHP。読み取りだけを行う。
  const PlayerHealth* playerHealth{};
  // ボスの移動の押し戻しに使うCollisionWorld。
  CollisionWorld* collisionWorld{};
  // 叩きつけの予告の範囲を、床へ表示する先。ボスの枠の番号で使う。
  AreaTelegraph* areaTelegraph{};
};

// 1回の更新で起きた、ボスの攻撃の結果。
struct BOSS_UPDATE_RESULT
{
  // この更新で、攻撃の判定がプレイヤーへ及んだか。範囲の外や、距離の外で空振りした場合は、false。
  bool hasAttackEvent{};
  // 判定の結果。hasAttackEventがtrueの場合だけ有効。e_HITは命中、e_DODGEDは通常の回避、e_JUST_DODGEDはジャスト回避。
  DODGE_JUDGE_RESULT result{DODGE_JUDGE_RESULT::e_NONE};
  // 命中した場合にプレイヤーへ与えるダメージ。resultがe_HITの場合だけ使う。
  float damage{};
  // スローを開始するか。実際に回避中のジャスト回避の場合だけtrue。
  bool shouldStartSlowMotion{};
};

// ボスの接近・攻撃の選択・予告・判定を更新する。
class BossController final
{
public:
  //========= 初期化・終了関数=========
  // 参照先を登録する。
  // links: 借用する参照先。1つでも欠けている場合は、何も登録せず失敗する。
  // 戻り値: 登録できたらtrue。
  [[nodiscard]] bool Initialize(const BOSS_CONTROLLER_LINKS& links);
  // 参照先を手放す。未初期化でも、繰り返しても呼出可能。
  void Uninit();
  // 攻撃の予告と待ち時間を初期状態へ戻し、最初の攻撃までの待ち時間を設定する。ボスの生成の直後や、再開で呼ぶ。範囲の表示は、呼出側が消す。
  void Reset();

  //========= 更新関数=========
  // ボスを1回分更新する。攻撃の予告中は、その場に立ち止まって予告を進め、判定の瞬間に結果を返す。
  // 予告中でなければ、待ち時間を減らし、攻撃を選べる距離なら予告を始め、そうでなければプレイヤーへ近づく。
  // 怯みでは、攻撃も移動も止まらない。死亡・転倒・プレイヤーの死亡では、予告を取り消す。
  // boss: 更新するボス。
  // slotIndex: ボスがいる敵の固定枠の番号。範囲の表示に使う。
  // worldDeltaTime: 敵側の経過時間。単位は秒。0以下（ヒットストップ中）では、何も進めない。
  // 戻り値: この更新で起きた攻撃の結果。
  [[nodiscard]] BOSS_UPDATE_RESULT Update(Enemy& boss,std::size_t slotIndex,float worldDeltaTime);

  //========= Getter関数=========
  // 攻撃を予告中かを返す。ボスの表示を、予告の色にするために使う。
  [[nodiscard]] bool IsTelegraphing() const { return m_judge.HasIncomingAttack(); }

private:
  //========= 補助関数=========
  // 距離と直前の攻撃から、次に始める攻撃を選ぶ。
  // horizontalDistance: ボスの中心とプレイヤーの水平距離。
  // 戻り値: 選んだ攻撃。どの攻撃も始められない距離ならe_NONE。
  [[nodiscard]] BOSS_ATTACK ChooseAttack(float horizontalDistance) const;
  // 攻撃の予告を始める。叩きつけなら、プレイヤーのいる位置へ範囲を固定して表示する。
  // attack: 始める攻撃。e_NONEは指定しない。
  // slotIndex: ボスがいる敵の固定枠の番号。
  // playerPosition: プレイヤーのワールド座標。叩きつけの範囲の中心になる。
  void StartAttack(BOSS_ATTACK attack,std::size_t slotIndex,const XMFLOAT3& playerPosition);
  // 予告中の攻撃を進め、判定の瞬間に、範囲の内にいるかを調べて、結果を返す。判定の後は、攻撃を終えて、待ち時間に入る。
  // slotIndex: ボスがいる敵の固定枠の番号。
  // bossPosition: ボスの中心のワールド座標。
  // playerPosition: プレイヤーのワールド座標。
  // worldDeltaTime: 敵側の経過時間。単位は秒。正の値。
  // 戻り値: 判定の瞬間の攻撃の結果。予告が続いている間や、範囲の外で空振りした場合は、hasAttackEventがfalse。
  [[nodiscard]] BOSS_UPDATE_RESULT UpdateAttack(std::size_t slotIndex,const XMFLOAT3& bossPosition,const XMFLOAT3& playerPosition,float worldDeltaTime);
  // 予告中の攻撃を取り消し、範囲の表示を消す。予告中だった場合だけ、待ち時間を設定する。
  // slotIndex: ボスがいる敵の固定枠の番号。
  void CancelAttack(std::size_t slotIndex);

  //========= 参照先=========
  // プレイヤーの位置・状態の読み取り元。非所有。未登録はnullptr。
  const PlayerController* m_playerController{};
  // プレイヤーの死亡の確認元。非所有。未登録はnullptr。
  const PlayerHealth* m_playerHealth{};
  // ボスの移動の押し戻しに使うCollisionWorld。非所有。未登録はnullptr。
  CollisionWorld* m_collisionWorld{};
  // 叩きつけの範囲の表示先。非所有。未登録はnullptr。
  AreaTelegraph* m_areaTelegraph{};

  //========= 攻撃の状態=========
  // 攻撃の予告と、判定の瞬間の回避の判定。
  JustDodgeJudge m_judge{};
  // 次の攻撃を予告できるまでの残り時間。単位は敵側の秒。0以下なら、予告できる。
  float m_recoverySeconds{};
  // 予告中の攻撃。予告中でなければe_NONE。
  BOSS_ATTACK m_currentAttack{BOSS_ATTACK::e_NONE};
  // 直前に判定まで終えた攻撃。同じ攻撃が、近距離で続かないよう、選ぶ時に使う。
  BOSS_ATTACK m_lastAttack{BOSS_ATTACK::e_NONE};
};