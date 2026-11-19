#pragma once

// プレイヤー側一式（プレイヤー・HP・回復アイテム・カメラ・近接攻撃と接触と回復・射撃・視野角）を所有し、初期化・解放・更新・開始位置への復帰を担当する。
// 敵の生成と更新・ステージ・時間（ヒットストップとスロー）の管理・描画・デバッグ操作は担当しない。ヒットストップは、通知先へ求めるだけで、残り時間は持たない。
// 出口の通過の判定と更新順序の仲介は呼出側が行う。移動と視野角の更新を分けているのは、呼出側が、その間に出口の通過を判定するため。

//========= C++標準ライブラリ インクルード=========
#include <functional> // std::functionで、ヒットストップの通知先（呼出側の関数）を受け取り、プレイヤーの戦闘へ渡す。

//========= Framework インクルード=========
#include "Framework/Input/H/FpsCamera.h"

//========= Scene インクルード=========
#include "Scene/Common/Camera/H/CameraFovController.h"
#include "Scene/Common/Combat/H/PlayerCombatController.h"
#include "Scene/Common/Player/H/HealItems.h"
#include "Scene/Common/Player/H/PlayerController.h"
#include "Scene/Common/Player/H/PlayerHealth.h"
#include "Scene/Common/Weapon/H/ShotController.h"

//========= 前方宣言=========
class CollisionDebugView;
class CollisionWorld;
class EnemyManager;
class InputSystem;

// PlayerSuiteが借用する参照先と、ヒットストップの通知先をまとめる。参照先は、Uninitまで有効であること。
struct PLAYER_SUITE_LINKS
{
  // 視点・移動・近接攻撃・回復・射撃の入力の取得元。
  InputSystem* inputSystem{};
  // 壁からの押し戻し・命中判定・敵のCollider解除に使うCollisionWorld。
  CollisionWorld* collisionWorld{};
  // 近接攻撃・接触・射撃の対象検索と、ダメージの反映先となる敵の所有者。
  EnemyManager* enemyManager{};
  // 弾道の終点の通知先。
  CollisionDebugView* collisionDebugView{};
  // ヒットストップの通知先。引数は求めるヒットストップの長さ（単位は秒）。空のままではInitが失敗する。
  std::function<void(float)> onHitStopRequested{};
};

// プレイヤー側の状態と更新を、1つのまとまりとして扱う。
class PlayerSuite final
{
public:
  //========= 初期化・終了関数=========
  // プレイヤー・HP・回復アイテム・カメラ・武器・視野角・戦闘の状態を既定へ戻し、カメラをプレイヤーの開始位置へ合わせる。参照先は変えない。
  void Initialize();
  // 参照先を登録し、プレイヤーの戦闘と射撃へ、使用する参照を渡す。最初に失敗した対象をログへ記録し、後続は実行せずfalseを返す。後始末は、呼出側がUninitで行う。
  // links: 借用する参照先と通知先。1つでも欠けている場合は失敗する。
  // 戻り値: 全て成功したらtrue。
  [[nodiscard]] bool Init(const PLAYER_SUITE_LINKS& links);
  // 射撃とプレイヤーの戦闘の参照先を手放し、このクラスの参照先を空にする。未初期化でも、繰り返しても呼出可能。
  void Uninit();

  //========= 更新関数=========
  // 回復アイテムの間隔・命中表示・反動のずれを更新する。Captureに関係なく呼ぶ。
  // deltaTime: 実時間の経過時間。単位は秒。命中表示と反動に使う。
  // gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。回復アイテムの間隔に使う。
  void UpdateTimers(float deltaTime,float gameplayDeltaTime);
  // 視線を更新し、蹴れる敵の有無を調べてから、プレイヤーの動作と位置を更新して、カメラ位置を合わせる。Capture中だけ呼ぶ。
  // gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。
  void UpdateMovement(float gameplayDeltaTime);
  // 動作の状態を記録し、殴打の開始でリロードをキャンセルし、動作の状態と構えに応じた視野角へ近づける。UpdateMovementの後に呼ぶ。
  // deltaTime: 実時間の経過時間。単位は秒。視野角の補間に使う。
  void UpdateView(float deltaTime);
  // 近接攻撃・接触・回復・射撃を、この順に更新する。Capture中だけ呼ぶ。
  // gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。
  void UpdateCombat(float gameplayDeltaTime);
  // プレイヤーを開始位置へ戻し、カメラ位置を合わせる。次の部屋へ進んだ時に使う。
  void ReturnToStart();

  //========= Getter関数=========
  // プレイヤーの参照を返す。敵・デバッグなどが借用する。戻り値の参照は、このPlayerSuiteの寿命を超えて保持しない。
  [[nodiscard]] PlayerController& GetPlayerController() { return m_playerController; }
  // プレイヤーのHPの参照を返す。敵・デバッグ・HUDが借用する。戻り値の参照は、このPlayerSuiteの寿命を超えて保持しない。
  [[nodiscard]] PlayerHealth& GetPlayerHealth() { return m_playerHealth; }
  // 回復アイテムの参照を返す。デバッグ・HUDが借用する。戻り値の参照は、このPlayerSuiteの寿命を超えて保持しない。
  [[nodiscard]] HealItems& GetHealItems() { return m_healItems; }
  // プレイヤーの戦闘の参照を返す。被弾の反映・転倒記録の初期化・デバッグが使う。戻り値の参照は、このPlayerSuiteの寿命を超えて保持しない。
  [[nodiscard]] PlayerCombatController& GetPlayerCombat() { return m_playerCombat; }
  // 射撃の参照を返す。デバッグ・HUDが借用する。戻り値の参照は、このPlayerSuiteの寿命を超えて保持しない。
  [[nodiscard]] ShotController& GetShotController() { return m_shotController; }
  // カメラの参照を返す。描画のView行列とCollision可視化が使う。戻り値の参照は、このPlayerSuiteの寿命を超えて保持しない。
  [[nodiscard]] FpsCamera& GetCamera() { return m_FPSCamera; }
  // 現在の垂直方向の視野角を返す。単位は度。
  [[nodiscard]] float GetFovDegrees() const { return m_cameraFov.GetFovDegrees(); }

private:
  //========= 参照先=========
  // 視点・移動・戦闘・射撃の入力の取得元。非所有。未登録はnullptr。
  InputSystem* m_inputSystem{};
  // 壁からの押し戻しに使うCollisionWorld。非所有。未登録はnullptr。
  CollisionWorld* m_collisionWorld{};
  // 蹴れる敵の検索に使う敵の所有者。非所有。未登録はnullptr。
  EnemyManager* m_enemyManager{};

  //========= プレイヤー側の状態=========
  // 位置・動作の状態・構え・近接攻撃・怯み・壁からの押し戻しを管理するプレイヤー。
  PlayerController m_playerController{};
  // プレイヤーのHP。
  PlayerHealth m_playerHealth{};
  // 回復アイテムの所持数と、連続使用の間隔。
  HealItems m_healItems{};
  // マウスによる視線方向と、プレイヤー位置に追従するView行列の生成状態。
  FpsCamera m_FPSCamera{};
  // 蹴り・殴打の当たり・敵との接触・スライディングの転倒・回復アイテムの使用・被弾の反応と、一撃で倒すデバッグ設定を担当する。
  PlayerCombatController m_playerCombat{};
  // 射撃・リロードの更新、弾道の終点の通知、命中した敵へのダメージ、命中表示の時間を担当し、武器を所有する。
  ShotController m_shotController{};
  // 動作の状態に応じた視野角の目標値と、現在の視野角の補間を担当する。
  CameraFovController m_cameraFov{};
  // 1回前の更新での、プレイヤーの動作の状態。殴打の開始を検出するために使う。
  PLAYER_ACTION_STATE m_previousActionState{PLAYER_ACTION_STATE::e_NORMAL};
};
