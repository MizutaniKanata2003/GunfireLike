#include "../H/ShotController.h"

// 射撃の更新、弾道の終点の通知、命中した敵へのダメージ、命中表示の時間管理を実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで、命中表示の残り時間を0未満にしない。

//========= Debug インクルード=========
#include "Debug/H/CollisionDebugView.h"

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/FpsCamera.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/PlayerCombatController.h"
#include "Scene/Common/Enemy/H/Enemy.h"
#include "Scene/Common/Enemy/H/EnemyManager.h"
#include "Scene/Common/Player/H/PlayerController.h"
#include "Scene/Common/Weapon/H/WeaponTable.h"

// 射撃の表示に使用する固定設定をまとめる。
namespace
{
	//========= 射撃の表示設定=========
	// 命中しない場合に、弾道の線を伸ばす長さ。単位はワールド距離単位。
	constexpr float ns_SHOT_TRACE_DISPLAY_LENGTH = 30.0f;
	// 命中を示す表示を続ける時間。単位は秒。
	constexpr float ns_HIT_MARKER_SECONDS = 0.3f;
}

//========= 初期化・終了関数=========
// 参照先を登録し、使用する武器の反動の設定をカメラへ渡す。
bool ShotController::Initialize(const SHOT_CONTROLLER_LINKS& links)
{
	// 参照先が1つでも欠けている場合は、以後の更新で空の参照を使ってしまうため、何も登録せず失敗を返す。
	if(links.playerController == nullptr || links.camera == nullptr || links.inputSystem == nullptr || links.collisionWorld == nullptr ||
		 links.enemyManager == nullptr || links.playerCombat == nullptr || links.collisionDebugView == nullptr){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"ShotControllerの参照先の登録に失敗しました。");
		return false;
	}

	// 参照先を登録する。以後の更新は、この参照先を使う。
	m_playerController = links.playerController;
	m_camera = links.camera;
	m_inputSystem = links.inputSystem;
	m_collisionWorld = links.collisionWorld;
	m_enemyManager = links.enemyManager;
	m_playerCombat = links.playerCombat;
	m_collisionDebugView = links.collisionDebugView;

	// 使用する武器の反動の設定を、カメラへ渡す。武器を切り替える工程では、切替のたびに、この設定を渡し直す。
	m_camera->SetRecoilProfile(WEAPON_TABLE::ns_ASSAULT_RIFLE.recoilProfile);
	return true;
}

// 参照先を手放す。
void ShotController::Uninit()
{
	m_playerController = nullptr;
	m_camera = nullptr;
	m_inputSystem = nullptr;
	m_collisionWorld = nullptr;
	m_enemyManager = nullptr;
	m_playerCombat = nullptr;
	m_collisionDebugView = nullptr;
}

// 武器を初期の性能値・満タンの弾薬へ戻し、命中表示を消す。
void ShotController::Reset()
{
	m_weaponSystem.Initialize(WEAPON_TABLE::ns_ASSAULT_RIFLE);
	m_hitMarkerRemainingSeconds = 0.0f;
}

//========= 更新関数=========
// 射撃・リロードを更新し、敵へ命中していればダメージを与える。
void ShotController::Update(float gameplayDeltaTime)
{
	// 射撃を更新する。リロード・連射間隔・弾薬の処理と、命中判定は武器が行い、ダメージの反映だけをここで行う。回避・蹴り・殴打の間は射撃できない。
	const WEAPON_UPDATE_RESULT weapon_result = m_weaponSystem.Update(gameplayDeltaTime,*m_inputSystem,m_playerController->IsAiming(),
																																	 m_playerController->IsShootingAllowed(),*m_camera,*m_collisionWorld);

	// 弾を発射した場合は、弾道の終点を可視化へ伝える。命中していれば命中点、していなければ弾の方向へ表示用の長さだけ進んだ点。
	if(weapon_result.didFire){
		const XMFLOAT3& shot_origin = weapon_result.shotRay.origin;
		const XMFLOAT3& shot_direction = weapon_result.shotRay.direction;
		// 命中した場合は、終点を命中点にする。
		if(weapon_result.didHit){
			m_collisionDebugView->NotifyShot(weapon_result.rayHit.point);
		}
		// 命中しなかった場合は、弾の方向へ表示用の長さだけ進んだ点を終点にする。
		else{
			m_collisionDebugView->NotifyShot(XMFLOAT3{shot_origin.x + shot_direction.x * ns_SHOT_TRACE_DISPLAY_LENGTH,
																								shot_origin.y + shot_direction.y * ns_SHOT_TRACE_DISPLAY_LENGTH,
																								shot_origin.z + shot_direction.z * ns_SHOT_TRACE_DISPLAY_LENGTH});
		}
	}

	// 敵レイヤーに命中した場合だけ、Colliderの識別値からダメージを与える相手を探す。
	if(weapon_result.didHit && weapon_result.hitInfo.layer == COLLISION_LAYER::ns_ENEMY){
		Enemy* const hit_enemy = m_enemyManager->FindLivingEnemy(weapon_result.hitInfo.userData);
		// 対応する生存中の敵が見つかった場合だけ、ダメージと命中表示を反映する。
		if(hit_enemy != nullptr){
			hit_enemy->ApplyDamage(m_playerCombat->GetAttackDamage(WEAPON_TABLE::ns_ASSAULT_RIFLE.damage,*hit_enemy),*m_collisionWorld);
			m_hitMarkerRemainingSeconds = ns_HIT_MARKER_SECONDS;
		}
	}
}

// 命中表示の残り時間を進める。
void ShotController::UpdateHitMarker(float deltaTime)
{
	m_hitMarkerRemainingSeconds = std::max(0.0f,m_hitMarkerRemainingSeconds - deltaTime);
}