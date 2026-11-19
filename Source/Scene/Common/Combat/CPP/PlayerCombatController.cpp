#include "../H/PlayerCombatController.h"

// 近接攻撃の当たりによる敵へのダメージと転倒、敵との接触の反応、スライディングによる転倒、回復アイテムの使用、被弾の反応を実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで、接触の反応の間隔の残り時間を0未満にしない。
#include <cstdint> // std::uint32_tで、敵IDに対応するビットを作り、転倒の記録と照合する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionMath.h"
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/FpsCamera.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/CombatTable.h"
#include "Scene/Common/Enemy/H/Enemy.h"
#include "Scene/Common/Enemy/H/EnemyManager.h"
#include "Scene/Common/Player/H/HealItems.h"
#include "Scene/Common/Player/H/PlayerController.h"
#include "Scene/Common/Player/H/PlayerHealth.h"

//========= 初期化・終了関数=========
// 参照先と通知先を登録する。
bool PlayerCombatController::Initialize(const PLAYER_COMBAT_LINKS& links)
{
	// 参照先または通知先が1つでも欠けている場合は、以後の更新で空の参照を使ってしまうため、何も登録せず失敗を返す。
	if(links.playerController == nullptr || links.playerHealth == nullptr || links.healItems == nullptr || links.enemyManager == nullptr ||
		 links.collisionWorld == nullptr || links.camera == nullptr || links.inputSystem == nullptr || !links.onHitStopRequested){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"PlayerCombatControllerの参照先の登録に失敗しました。");
		return false;
	}

	// 参照先と通知先を登録する。以後の更新は、この参照先を使う。
	m_playerController = links.playerController;
	m_playerHealth = links.playerHealth;
	m_healItems = links.healItems;
	m_enemyManager = links.enemyManager;
	m_collisionWorld = links.collisionWorld;
	m_camera = links.camera;
	m_inputSystem = links.inputSystem;
	m_onHitStopRequested = links.onHitStopRequested;
	return true;
}

// 参照先と通知先を手放す。
void PlayerCombatController::Uninit()
{
	m_playerController = nullptr;
	m_playerHealth = nullptr;
	m_healItems = nullptr;
	m_enemyManager = nullptr;
	m_collisionWorld = nullptr;
	m_camera = nullptr;
	m_inputSystem = nullptr;
	m_onHitStopRequested = nullptr;
}

// 接触の反応の間隔・スライディングの転倒記録・一撃で倒す設定を、初期状態へ戻す。
void PlayerCombatController::Reset()
{
	m_contactCooldownRemainingSeconds = 0.0f;
	ClearSlideKnockdownRecord();
	m_isOneHitKillEnabled = false;
}

//========= 更新関数=========
// 近接攻撃の当たり・敵との接触・回復の入力を、この順に更新する。
void PlayerCombatController::Update(float gameplayDeltaTime)
{
	// 近接攻撃の当たりを先に反映し、続けて接触と回復を、元のGameSceneと同じ順に更新する。
	UpdateMelee();
	UpdateContact(gameplayDeltaTime);
	UpdateHeal();
}

// 被弾・接触の反応として、ダメージ（あれば）・怯み・ヒットストップを与える。
void PlayerCombatController::ApplyHitReaction(float damage,float hitStopSeconds)
{
	// ダメージが正の場合だけHPを減らす。接触は0のため、HPは変わらない。
	if(damage > 0.0f){
		m_playerHealth->ApplyDamage(damage);
	}

	// プレイヤーを怯ませ、ヒットストップを求める。すでに残っている場合に長い方を使うかどうかは、通知先が決める。
	m_playerController->ApplyFlinch(COMBAT_TABLE::ns_PLAYER_FLINCH_SECONDS);
	RequestHitStop(hitStopSeconds);
}

//========= ダメージ・デバッグ設定関数=========
// 一撃で倒す設定が有効なら敵の残りHPを、無効なら基本のダメージを返す。
float PlayerCombatController::GetAttackDamage(float baseDamage,const Enemy& target) const
{
	return m_isOneHitKillEnabled ? target.GetCurrentHP() : baseDamage;
}

//========= 更新の補助関数=========
// 近接攻撃の当たりを取り出し、範囲内の敵へ効果を与える。
void PlayerCombatController::UpdateMelee()
{
	// 当たりの種類の出力先を用意する。
	PLAYER_MELEE_TYPE melee_type{};
	// 当たりが発生していなければ、何もしない。
	if(!m_playerController->TryConsumeMeleeImpact(melee_type))return;

	// 当たる瞬間に、視点から射程と範囲の内で最も近い生存中の敵を選ぶ。いなければ空振りとして何もしない。
	Enemy* const target_enemy = m_enemyManager->FindMeleeTarget(m_camera->GetPosition(),m_camera->GetForward(),false);
	// 対象がいない場合は、空振りとして効果を与えずに戻る。
	if(target_enemy == nullptr)return;

	// 蹴りはダメージだけを与える。殴打はダメージを与えた後に、転倒させる。ダメージで死亡した場合、転倒は何も起こらない。
	if(melee_type == PLAYER_MELEE_TYPE::e_KICK){
		target_enemy->ApplyDamage(GetAttackDamage(COMBAT_TABLE::ns_KICK_DAMAGE,*target_enemy),*m_collisionWorld);
	}
	// 殴打の場合は、ダメージを与えた後に転倒させる。範囲攻撃型は、転倒しない。
	else{
		target_enemy->ApplyDamage(GetAttackDamage(COMBAT_TABLE::ns_PUNCH_DAMAGE,*target_enemy),*m_collisionWorld);
		target_enemy->ApplyKnockdown();
	}

	// 当たった手応えとして、短いヒットストップを求める。
	RequestHitStop(COMBAT_TABLE::ns_MELEE_HIT_STOP_SECONDS);
}

// 敵との接触を調べ、スライディングなら転倒、それ以外は条件を満たす場合に接触の反応を与える。
void PlayerCombatController::UpdateContact(float gameplayDeltaTime)
{
	// 接触の反応の間隔を進める。0未満にはしない。
	m_contactCooldownRemainingSeconds = std::max(0.0f,m_contactCooldownRemainingSeconds - gameplayDeltaTime);

	// 現在の動作の状態を取得する。
	const PLAYER_ACTION_STATE action_state = m_playerController->GetActionState();
	// スライディングでなくなったら、次のスライディングで再び転倒させられるよう、記録を戻す。
	if(action_state != PLAYER_ACTION_STATE::e_SLIDE){
		ClearSlideKnockdownRecord();
	}

	// 全枠を出現順に調べ、体が重なった敵ごとに、スライディングの転倒か接触の反応を判定する。
	for(Enemy& enemy : m_enemyManager->GetEnemies()){
		// 死亡中、または体が重なっていない敵は、接触していないため次の枠へ進む。
		if(!enemy.IsAlive() || !CollisionMath::IntersectSphereSphere(m_playerController->GetBodySphere(),enemy.GetBodySphere()))continue;

		// スライディング中は、この敵をまだ転倒させていない場合だけ転倒させる。接触の反応は与えず、他の敵の判定を続ける。範囲攻撃型は、転倒しない。
		if(action_state == PLAYER_ACTION_STATE::e_SLIDE){
			const std::uint32_t enemy_bit = 1u << enemy.GetEnemyId();
			// 未記録で、実際に転倒させられた場合だけ、この敵への記録を付ける。すでに転倒中や、転倒しない種類なら何もしない。
			if((m_slideKnockedDownEnemyMask & enemy_bit) == 0 && enemy.ApplyKnockdown()){
				m_slideKnockedDownEnemyMask |= enemy_bit;
			}
			continue;
		}

		// 殴打中・蹴り中・無敵中・敵が転倒中・反応の間隔が残っている間は、接触の反応を与えない。
		if(action_state == PLAYER_ACTION_STATE::e_PUNCH || action_state == PLAYER_ACTION_STATE::e_KICK || m_playerController->IsInvincible() ||
			 enemy.IsKnockedDown() || m_contactCooldownRemainingSeconds > 0.0f)continue;

		// 条件を満たした接触は、ダメージなしの怯みとヒットストップを与え、間隔を設定する。以後の枠は、間隔が残るため反応しない。
		ApplyHitReaction(0.0f,COMBAT_TABLE::ns_CONTACT_HIT_STOP_SECONDS);
		m_contactCooldownRemainingSeconds = COMBAT_TABLE::ns_CONTACT_COOLDOWN_SECONDS;
	}
}

// Qを押した瞬間に、HPが満タンでなく、回復アイテムを使えれば、HPを回復する。動作の状態に関係なく使え、他の動作は中断しない。
void PlayerCombatController::UpdateHeal()
{
	// Qを押した瞬間でなければ、何もしない。
	if(!m_inputSystem->IsKeyTriggered('Q'))return;

	// HPが満タンなら、使っても効果がないため、アイテムを消費しない。
	if(m_playerHealth->IsFull())return;

	// アイテムを使えた（所持数があり、連続使用の間隔が空いている）場合だけ、HPを回復する。
	if(m_healItems->TryUse(COMBAT_TABLE::ns_HEAL_COOLDOWN_SECONDS)){
		m_playerHealth->Heal(COMBAT_TABLE::ns_HEAL_AMOUNT);
	}
}

// ヒットストップを通知先へ求める。
void PlayerCombatController::RequestHitStop(float hitStopSeconds) const
{
	// 通知先が空の場合は、空の関数を呼ぶと例外になるため、何もしない。
	if(!m_onHitStopRequested)return;

	m_onHitStopRequested(hitStopSeconds);
}
