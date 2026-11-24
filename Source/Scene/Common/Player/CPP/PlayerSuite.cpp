#include "../H/PlayerSuite.h"

// プレイヤー側一式の初期化・解放・更新・開始位置への復帰を実装する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Etc/H/InitCheck.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/CombatTable.h"
#include "Scene/Common/Enemy/H/EnemyManager.h"

// プレイヤーの開始位置に使用する固定設定をまとめる。戦闘の数値はCombatTableにある。
namespace
{
  //========= 配置の設定=========
  // プレイヤーの開始位置。ワールド座標(0,1.6,0)で、床の手前寄りの中央。Yは立っている時のカメラの高さ。壁の内側にある。
  constexpr XMFLOAT3 ns_PLAYER_START_POSITION{0.0f,1.6f,0.0f};
}

//========= 初期化・終了関数=========
// プレイヤー側の状態を既定へ戻し、カメラをプレイヤーの開始位置へ合わせる。
void PlayerSuite::Initialize()
{
  // プレイヤー・HP・回復アイテム・カメラ・武器・視野角を既定へ戻す。
  m_playerController.Initialize(ns_PLAYER_START_POSITION);
  m_playerHealth.Initialize(COMBAT_TABLE::ns_PLAYER_MAX_HP);
  m_healItems.Initialize(COMBAT_TABLE::ns_HEAL_INITIAL_COUNT,COMBAT_TABLE::ns_HEAL_MAX_COUNT);
  m_FPSCamera = FpsCamera{};
  m_shotController.Reset();
  m_cameraFov.Reset();

  // 戦闘の状態と、動作の記録を既定へ戻す。
  m_playerCombat.Reset();
  m_previousActionState = PLAYER_ACTION_STATE::e_NORMAL;

  // カメラの位置をプレイヤーの開始位置へ合わせる。壁のColliderはステージのInitで登録するため、押し戻しはInit後の更新から始まる。
  m_FPSCamera.SetPosition(m_playerController.GetPosition());
}

// 参照先を登録し、プレイヤーの戦闘と射撃へ、使用する参照を渡す。
bool PlayerSuite::Init(const PLAYER_SUITE_LINKS& links)
{
  // 参照先を登録する。欠けている場合は、下の判定で失敗を返すため、登録された値は使われない。
  m_inputSystem = links.inputSystem;
  m_collisionWorld = links.collisionWorld;
  m_enemyManager = links.enemyManager;

  // プレイヤーの戦闘に使う参照と、ヒットストップの通知先を用意する。
  PLAYER_COMBAT_LINKS player_combat_links{};
  player_combat_links.playerController = &m_playerController;
  player_combat_links.playerHealth = &m_playerHealth;
  player_combat_links.healItems = &m_healItems;
  player_combat_links.enemyManager = links.enemyManager;
  player_combat_links.collisionWorld = links.collisionWorld;
  player_combat_links.camera = &m_FPSCamera;
  player_combat_links.inputSystem = links.inputSystem;
  player_combat_links.onHitStopRequested = links.onHitStopRequested;

  // 射撃に使う参照を用意する。
  SHOT_CONTROLLER_LINKS shot_links{};
  shot_links.playerController = &m_playerController;
  shot_links.camera = &m_FPSCamera;
  shot_links.inputSystem = links.inputSystem;
  shot_links.collisionWorld = links.collisionWorld;
  shot_links.enemyManager = links.enemyManager;
  shot_links.playerCombat = &m_playerCombat;
  shot_links.collisionDebugView = links.collisionDebugView;

  // 参照先の確認、プレイヤーの戦闘、射撃の順に登録する。最初に失敗した対象のメッセージだけをログへ記録し、後続は実行せずfalseを返す。
  // 参照先が1つでも欠けている場合は、以後の更新で空の参照を使ってしまうため、失敗にする。
  return InitCheck::Succeeded(links.inputSystem != nullptr && links.collisionWorld != nullptr && links.enemyManager != nullptr &&
                              links.collisionDebugView != nullptr && links.onHitStopRequested,
                              e_LogCategory::e_SCENE,L"PlayerSuiteの参照先の登録に失敗しました。")
    // 参照先の登録に失敗した場合は、プレイヤーの戦闘の登録が失敗する。
    && InitCheck::Succeeded(m_playerCombat.Initialize(player_combat_links),e_LogCategory::e_SCENE,L"PlayerSuiteのプレイヤー戦闘の初期化に失敗しました。")
    // 参照先の登録に失敗した場合は、射撃の登録が失敗する。
    && InitCheck::Succeeded(m_shotController.Initialize(shot_links),e_LogCategory::e_SCENE,L"PlayerSuiteの射撃の初期化に失敗しました。");
}

// 射撃とプレイヤーの戦闘の参照先を手放し、このクラスの参照先を空にする。
void PlayerSuite::Uninit()
{
  m_shotController.Uninit();
  m_playerCombat.Uninit();
  m_inputSystem = nullptr;
  m_collisionWorld = nullptr;
  m_enemyManager = nullptr;
}

//========= 更新関数=========
// 回復アイテムの間隔・命中表示・反動のずれを更新する。
void PlayerSuite::UpdateTimers(float deltaTime,float gameplayDeltaTime)
{
  // 回復アイテムの間隔は、ヒットストップを反映した時間で進める。命中表示は、実時間で進め、0未満にしない。
  m_healItems.Update(gameplayDeltaTime);
  m_shotController.UpdateHitMarker(deltaTime);

  // 反動によるカメラのずれを、実時間で目標へ滑らかに近づけ、射撃をやめて少し経つと、元の視線へ戻す。
  m_FPSCamera.UpdateRecoil(deltaTime);
}

// 視線・プレイヤーの動作と位置・カメラ位置を更新する。
void PlayerSuite::UpdateMovement(float gameplayDeltaTime)
{
  // 視線を先に更新し、その向きで、蹴れる敵がいるかを調べてから、プレイヤーを更新する。ヒットストップ中も視線は動かせる。
  m_FPSCamera.Update(*m_inputSystem);
  const bool can_kick = m_enemyManager->FindMeleeTarget(m_FPSCamera.GetPosition(),m_FPSCamera.GetForward(),true) != nullptr;

  // 経過時間と、蹴れる相手がいるかを、1つの値にまとめて渡す。
  const PLAYER_UPDATE_REQUEST update_request{gameplayDeltaTime,can_kick};
  m_playerController.Update(update_request,*m_inputSystem,m_FPSCamera,*m_collisionWorld);
  m_FPSCamera.SetPosition(m_playerController.GetPosition());
}

// 動作の状態の記録・殴打でのリロードのキャンセル・視野角を更新する。
void PlayerSuite::UpdateView(float deltaTime)
{
  // 現在の動作の状態を取得し、殴打を始めた更新では、リロードをキャンセルする。弾は補充しない。殴打の開始は、1回前の状態が殴打ではなかったことで検出する。
  const PLAYER_ACTION_STATE action_state = m_playerController.GetActionState();
  if(action_state == PLAYER_ACTION_STATE::e_PUNCH && m_previousActionState != PLAYER_ACTION_STATE::e_PUNCH){
    m_shotController.CancelReload();
  }
  m_previousActionState = action_state;

  // 動作の状態と構えに応じた視野角の目標値へ、滑らかに近づける。
  m_cameraFov.Update(deltaTime,action_state,m_playerController.IsAiming());
}

// 近接攻撃・接触・回復・射撃を更新する。
void PlayerSuite::UpdateCombat(float gameplayDeltaTime)
{
  // 近接攻撃・接触・回復・射撃を、この順に更新する。
  m_playerCombat.Update(gameplayDeltaTime);
  m_shotController.Update(gameplayDeltaTime);
}

// プレイヤーを開始位置へ戻し、カメラ位置を合わせる。
void PlayerSuite::ReturnToStart()
{
  m_playerController.Initialize(ns_PLAYER_START_POSITION);
  m_FPSCamera.SetPosition(m_playerController.GetPosition());
}