#include "../H/GameDebugController.h"

// デバッグ操作の反映、確認用の攻撃の判定、デバッグUIの情報の組立と描画を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_F3・VK_F5〜VK_F9・VK_F11・VK_INSERTの仮想キーコードで、デバッグ操作のキーを指定する。

//========= Debug インクルード=========
#include "Debug/H/CollisionDebugView.h"
#include "Debug/H/GameDebugUi.h"

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/CombatTable.h"
#include "Scene/Common/Combat/H/PlayerCombatController.h"
#include "Scene/Common/Combat/H/TimeScaleController.h"
#include "Scene/Common/Enemy/H/Enemy.h"
#include "Scene/Common/Enemy/H/EnemyController.h"
#include "Scene/Common/Enemy/H/EnemyManager.h"
#include "Scene/Common/Player/H/HealItems.h"
#include "Scene/Common/Player/H/PlayerController.h"
#include "Scene/Common/Player/H/PlayerHealth.h"
#include "Scene/Common/Stage/H/RoomController.h"
#include "Scene/Common/Weapon/H/ShotController.h"

//========= 初期化・終了関数=========
// 参照先を登録する。
// 13.8の例外：参照先の検査と登録の並びは、PlayerCombatController::Initializeと、メンバー名と型だけが違う。
// 参照先の型と個数がクラスごとに異なり、各クラスのメンバーへ直接代入するため、共通の関数にはまとめない。
bool GameDebugController::Initialize(const GAME_DEBUG_CONTROLLER_LINKS& links)
{
	// 参照先が1つでも欠けている場合は、以後の更新で空の参照を使ってしまうため、何も登録せず失敗を返す。
	if(links.inputSystem == nullptr || links.collisionDebugView == nullptr || links.playerController == nullptr || links.playerHealth == nullptr ||
		 links.healItems == nullptr || links.playerCombat == nullptr || links.roomController == nullptr || links.enemyController == nullptr ||
		 links.shotController == nullptr || links.timeScale == nullptr){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameDebugControllerの参照先の登録に失敗しました。");
		return false;
	}

	// 参照先を登録する。以後の更新と描画は、この参照先を使う。
	m_inputSystem = links.inputSystem;
	m_collisionDebugView = links.collisionDebugView;
	m_playerController = links.playerController;
	m_playerHealth = links.playerHealth;
	m_healItems = links.healItems;
	m_playerCombat = links.playerCombat;
	m_roomController = links.roomController;
	m_enemyController = links.enemyController;
	m_shotController = links.shotController;
	m_timeScale = links.timeScale;
	return true;
}

// 参照先を手放す。
void GameDebugController::Uninit()
{
	m_inputSystem = nullptr;
	m_collisionDebugView = nullptr;
	m_playerController = nullptr;
	m_playerHealth = nullptr;
	m_healItems = nullptr;
	m_playerCombat = nullptr;
	m_roomController = nullptr;
	m_enemyController = nullptr;
	m_shotController = nullptr;
	m_timeScale = nullptr;
}

// 確認用の攻撃の判定と、直近の判定結果を、初期状態へ戻す。
void GameDebugController::Reset()
{
	m_dodgeJudge.Initialize();
	m_lastJudgeResult = DODGE_JUDGE_RESULT::e_NONE;
}

//========= 更新関数=========
// F3・F5〜F9・Insertのデバッグ操作を反映し、F11が押されたかを返す。
bool GameDebugController::UpdateCommands()
{
	// F3が押された瞬間は、Collision可視化の表示を反転する。Capture解除中も切り替えられるよう、呼出側はCaptureの判定より前に呼ぶ。
	if(m_inputSystem->IsKeyTriggered(VK_F3)){
		m_collisionDebugView->ToggleVisible();
	}

	// F5が押された瞬間は、HPバーと低HP警告の確認のため、プレイヤーへ確認用のダメージを与える。ヒットストップや怯みは与えない。
	if(m_inputSystem->IsKeyTriggered(VK_F5)){
		m_playerHealth->ApplyDamage(COMBAT_TABLE::ns_DEBUG_DAMAGE);
	}

	// F6が押された瞬間は、プレイヤーのHPを満タンへ戻す。
	if(m_inputSystem->IsKeyTriggered(VK_F6)){
		m_playerHealth->HealFull();
	}

	// F7が押された瞬間は、回避とジャスト回避の確認のため、一定時間後に当たる攻撃を予告する。敵の攻撃の実装までの暫定の操作。
	if(m_inputSystem->IsKeyTriggered(VK_F7)){
		m_dodgeJudge.StartIncomingAttack(COMBAT_TABLE::ns_SIMULATED_ATTACK_DELAY_SECONDS);
	}

	// F8が押された瞬間は、回復アイテムを1個増やす。入手の実装までの暫定の操作。上限を超えない。
	if(m_inputSystem->IsKeyTriggered(VK_F8)){
		m_healItems->Add(1);
	}

	// F9が押された瞬間は、デバッグ用に、一撃で敵を倒す設定を切り替える。Capture解除中も切り替えられるよう、呼出側はCaptureの判定より前に呼ぶ。
	if(m_inputSystem->IsKeyTriggered(VK_F9)){
		m_playerCombat->ToggleOneHitKill();
	}

	// Insertが押された瞬間は、デバッグ用に、弾薬を消費せずに撃ち続けられる設定を切り替え、切替後の状態をログへ記録する。
	// F10はWindowsのメニュー操作、F12はデバッガーの中断に使われるため、割り当てない。
	if(m_inputSystem->IsKeyTriggered(VK_INSERT)){
		m_shotController->ToggleInfiniteAmmo();
		Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,
									m_shotController->IsInfiniteAmmoEnabled() ? L"弾薬を消費しない設定を有効にしました。" : L"弾薬を消費しない設定を無効にしました。");
	}

	// F11が押された瞬間かを返す。現在のウェーブの敵を倒す処理は、敵の処理を持つ呼出側が行う。
	return m_inputSystem->IsKeyTriggered(VK_F11);
}

// 確認用の攻撃が当たる瞬間に、無敵かどうかと回避の開始からの経過時間で判定し、結果を記録して返す。
DODGE_JUDGE_RESULT GameDebugController::UpdateDodgeJudge(float worldDeltaTime)
{
	// 予告した攻撃が当たる瞬間だけ、結果が返る。それ以外の更新では、何も返らない。
	const DODGE_JUDGE_RESULT judge_result = m_dodgeJudge.Update(worldDeltaTime,m_playerController->IsInvincible(),
																															m_playerController->GetSecondsSinceDodgeStart());
	// 判定結果が出た場合だけ、表示用に記録する。
	if(judge_result != DODGE_JUDGE_RESULT::e_NONE){
		m_lastJudgeResult = judge_result;
	}
	return judge_result;
}

//========= 描画関数=========
// デバッグUIに表示する値を集めて描画する。
void GameDebugController::DrawUi(bool hasWaveSpawnFailed)
{
	// 表示する先頭の敵を取得し、プレイヤー・回復アイテム・部屋の状態を集める。
	auto& first_enemy = m_enemyController->GetEnemyManager().GetFirstEnemy();
	GAME_DEBUG_INFO debug_info{};
	debug_info.playerPosition = m_playerController->GetPosition();
	debug_info.playerHP = m_playerHealth->GetCurrentHP();
	debug_info.playerMaxHP = m_playerHealth->GetMaxHP();
	debug_info.actionState = m_playerController->GetActionState();
	debug_info.isInvincible = m_playerController->IsInvincible();
	debug_info.isPlayerFlinching = m_playerController->IsFlinching();
	debug_info.healItemCount = m_healItems->GetCount();
	debug_info.roomNumber = m_roomController->GetRoomNumber();
	debug_info.waveNumber = m_roomController->GetWaveNumber();
	debug_info.roomPhase = m_roomController->GetPhase();
	debug_info.hasWaveSpawnFailed = hasWaveSpawnFailed;
	debug_info.isOneHitKillEnabled = m_playerCombat->IsOneHitKillEnabled();

	// 先頭の敵の状態と、命中表示・ヒットストップ・スロー・確認用の攻撃の判定の状態を集める。
	debug_info.isEnemyAlive = first_enemy.IsAlive();
	debug_info.enemyHP = first_enemy.GetCurrentHP();
	debug_info.enemyMaxHP = first_enemy.GetMaxHP();
	debug_info.isEnemyFlinching = first_enemy.IsFlinching();
	debug_info.isEnemyKnockedDown = first_enemy.IsKnockedDown();
	debug_info.isHitMarkerActive = m_shotController->IsHitMarkerActive();
	debug_info.isCaptureEnabled = m_inputSystem->IsMouseCaptureEnabled();
	debug_info.isHitStopActive = m_timeScale->IsHitStopActive();
	debug_info.isSlowMotionActive = m_timeScale->IsSlowMotionActive();
	debug_info.hasIncomingAttack = m_dodgeJudge.HasIncomingAttack();
	debug_info.secondsUntilHit = m_dodgeJudge.GetSecondsUntilHit();
	debug_info.lastJudgeResult = m_lastJudgeResult;

	// Collision可視化の状態を集め、デバッグUIへ渡す。
	debug_info.isCollisionDebugVisible = m_collisionDebugView->IsVisible();
	debug_info.hasRayHit = m_collisionDebugView->HasRayHit();
	debug_info.rayHitLayer = m_collisionDebugView->GetRayHitInfo().layer;
	debug_info.rayHitDistance = m_collisionDebugView->GetRayHit().distance;
	GameDebugUi::Draw(debug_info);
}