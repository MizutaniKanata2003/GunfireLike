#pragma once

// 確認用のデバッグ操作（F3・F5〜F9・F11）、確認用の攻撃の判定（F7）、直近の判定結果の記録、読み取り専用のデバッグUIの情報の組立と描画を担当する。
// ゲームの進行・Scene遷移・Capture切替・F4の再開は担当しない。F11のウェーブを飛ばす操作は、要求を呼出側へ返し、敵の処理は呼出側が行う。

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/JustDodgeJudge.h"

//========= 前方宣言=========
class CollisionDebugView;
class EnemyController;
class HealItems;
class InputSystem;
class PlayerCombatController;
class PlayerController;
class PlayerHealth;
class RoomController;
class ShotController;
class TimeScaleController;

// GameDebugControllerが借用する参照先をまとめる。参照先は、Uninitまで有効であること。
struct GAME_DEBUG_CONTROLLER_LINKS
{
  // デバッグ操作のキー入力と、Capture状態の取得元。読み取りだけを行う。
  InputSystem* inputSystem{};
  // F3で表示を切り替える、Collision可視化。
  CollisionDebugView* collisionDebugView{};
  // 確認用の攻撃の判定に、無敵と回避の経過時間を渡す取得元。デバッグUIの表示にも使う。読み取りだけを行う。
  PlayerController* playerController{};
  // F5の確認用ダメージとF6の全回復の反映先となるプレイヤーのHP。デバッグUIの表示にも使う。
  PlayerHealth* playerHealth{};
  // F8で1個増やす回復アイテム。デバッグUIの表示にも使う。
  HealItems* healItems{};
  // F9で切り替える、一撃で倒す設定の持ち主。デバッグUIの表示にも使う。
  PlayerCombatController* playerCombat{};
  // デバッグUIに表示する、部屋とウェーブの進行状態。読み取りだけを行う。
  RoomController* roomController{};
  // デバッグUIに表示する、敵の状態の取得元。読み取りだけを行う。
  EnemyController* enemyController{};
  // デバッグUIに表示する、命中表示の状態の取得元。読み取りだけを行う。
  const ShotController* shotController{};
  // デバッグUIに表示する、ヒットストップとスローの状態の取得元。読み取りだけを行う。
  const TimeScaleController* timeScale{};
};

// デバッグ操作・確認用の攻撃の判定・デバッグUIの情報の組立を扱う。
class GameDebugController final
{
public:
  //========= 初期化・終了関数=========
  // 参照先を登録する。
  // links: 借用する参照先。1つでも欠けている場合は何も登録しない。
  // 戻り値: 全ての参照先が揃っていて登録できたらtrue。
  [[nodiscard]] bool Initialize(const GAME_DEBUG_CONTROLLER_LINKS& links);
  // 参照先を手放す。未初期化でも、繰り返しても呼出可能。
  void Uninit();
  // 確認用の攻撃の判定と、直近の判定結果を、初期状態へ戻す。参照先は変えない。
  void Reset();

  //========= 更新関数=========
  // F3・F5〜F9のデバッグ操作を反映し、F11が押されたかを返す。Initialize成功後に呼ぶ。
  // 戻り値: F11が押された瞬間ならtrue。現在のウェーブを飛ばす処理は、呼出側が行う。
  [[nodiscard]] bool UpdateCommands();
  // 確認用の攻撃が当たる瞬間を判定する。結果が出た場合は、デバッグUIの表示用に記録する。Initialize成功後に呼ぶ。
  // worldDeltaTime: スローを反映した経過時間。単位は秒。ヒットストップ中は0。
  // 戻り値: 攻撃が当たる瞬間の結果。それ以外の更新ではe_NONE。ダメージやスローの反映は、呼出側が行う。
  [[nodiscard]] DODGE_JUDGE_RESULT UpdateDodgeJudge(float worldDeltaTime);
  // 敵の攻撃の結果を、直近の判定結果として記録する。
  // result: 記録する判定結果。
  void RecordJudgeResult(DODGE_JUDGE_RESULT result) { m_lastJudgeResult = result; }

  //========= 描画関数=========
  // デバッグUIに表示する値を集めて描画する。デバッグUIはゲームの状態を変更しない。ImGuiフレーム開始後に呼ぶ。Initialize成功後に呼ぶ。
  // hasWaveSpawnFailed: 現在のウェーブの生成に失敗したか。
  void DrawUi(bool hasWaveSpawnFailed);

private:
  //========= 参照先=========
  // キー入力とCapture状態の取得元。非所有。未登録はnullptr。
  InputSystem* m_inputSystem{};
  // Collision可視化。非所有。未登録はnullptr。
  CollisionDebugView* m_collisionDebugView{};
  // プレイヤー。非所有。未登録はnullptr。
  PlayerController* m_playerController{};
  // プレイヤーのHP。非所有。未登録はnullptr。
  PlayerHealth* m_playerHealth{};
  // 回復アイテム。非所有。未登録はnullptr。
  HealItems* m_healItems{};
  // プレイヤーの戦闘。非所有。未登録はnullptr。
  PlayerCombatController* m_playerCombat{};
  // 部屋とウェーブの進行。非所有。未登録はnullptr。
  RoomController* m_roomController{};
  // 敵の更新。非所有。未登録はnullptr。
  EnemyController* m_enemyController{};
  // 命中表示。非所有。未登録はnullptr。
  const ShotController* m_shotController{};
  // ヒットストップとスロー。非所有。未登録はnullptr。
  const TimeScaleController* m_timeScale{};

  //========= 判定の状態=========
  // 確認用に発生させた攻撃の、命中の瞬間の判定（無敵・ジャスト回避）。
  JustDodgeJudge m_dodgeJudge{};
  // 直近の判定結果。デバッグUIへの表示に使う。
  DODGE_JUDGE_RESULT m_lastJudgeResult{DODGE_JUDGE_RESULT::e_NONE};
};
