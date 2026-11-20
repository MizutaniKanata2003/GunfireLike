#include "../H/ShotController.h"

// 射撃の更新、武器の装備、弾道の終点の通知、命中した敵へのダメージと怯み（特殊強化の効果を含む）、命中表示の時間管理を実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで、命中表示とダメージの増加の残り時間を0未満にしない。
#include <string> // std::wstringで、武器の切替のログに、武器の表示名を連結する。

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

// 射撃の表示と、暫定の武器切替に使用する固定設定をまとめる。
namespace
{
	//========= 射撃の表示設定=========
	// 命中しない場合に、弾道の線を伸ばす長さ。単位はワールド距離単位。
	constexpr float ns_SHOT_TRACE_DISPLAY_LENGTH = 30.0f;
	// 命中を示す表示を続ける時間。単位は秒。
	constexpr float ns_HIT_MARKER_SECONDS = 0.3f;

	//========= 暫定の武器切替=========
	// 武器を切り替える数字キー。表の武器の並び（連射銃・散弾銃・単発高威力銃）と同じ順で、1・2・3。武器の所持・購入の実装までの暫定の操作。
	constexpr unsigned char ns_WEAPON_SELECT_KEYS[]{'1','2','3'};
}

//========= 初期化・終了関数=========
// 参照先を登録し、使用中の武器の反動の設定をカメラへ渡す。
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

	// 使用中の武器の反動の設定を、カメラへ渡す。カメラは、登録の前の準備で作り直されているため、ここで渡し直す。
	m_camera->SetRecoilProfile(m_weaponSystem.GetWeaponData().recoilProfile);
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

// 初期武器を満タンの弾薬で装備し直し、命中表示を消し、特殊強化の効果を通常へ戻す。
void ShotController::Reset()
{
	m_weaponSystem.Initialize(WEAPON_TABLE::ns_ASSAULT_RIFLE);
	m_hitMarkerRemainingSeconds = 0.0f;
	m_flinchScale = 1.0f;
	m_damageBoostScale = 1.0f;
	m_damageBoostRemainingSeconds = 0.0f;
}

//========= 更新関数=========
// 武器の切替・射撃・リロードを更新し、敵へ命中していればダメージと怯みを与える。
void ShotController::Update(float gameplayDeltaTime)
{
	// ジャスト回避の後のダメージの増加の残り時間を進め、0未満にしない。このフレームの射撃にも、残りがあれば反映する。
	m_damageBoostRemainingSeconds = std::max(0.0f,m_damageBoostRemainingSeconds - gameplayDeltaTime);

	// 数字キーによる暫定の武器切替を、射撃の更新より先に処理する。切り替えたフレームから、新しい武器で射撃する。
	UpdateWeaponSelection();

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
	// ダメージは、使用中の武器の値に、ジャスト回避の後の増加（回避反撃）と、敵に付いた印（殴打追撃）の倍率を掛け、怯み時間は、制圧射撃の倍率を掛ける。
	if(weapon_result.didHit && weapon_result.hitInfo.layer == COLLISION_LAYER::ns_ENEMY){
		Enemy* const hit_enemy = m_enemyManager->FindLivingEnemy(weapon_result.hitInfo.userData);
		// 対応する生存中の敵が見つかった場合だけ、ダメージと命中表示を反映する。
		if(hit_enemy != nullptr){
			const float boost_scale = IsDamageBoostActive() ? m_damageBoostScale : 1.0f;
			const float weapon_damage = m_weaponSystem.GetWeaponData().damage * boost_scale * hit_enemy->GetShotDamageScale();
			hit_enemy->ApplyDamage(m_playerCombat->GetAttackDamage(weapon_damage,*hit_enemy),*m_collisionWorld,m_flinchScale);
			m_hitMarkerRemainingSeconds = ns_HIT_MARKER_SECONDS;
		}
	}
}

// 命中表示の残り時間を進める。
void ShotController::UpdateHitMarker(float deltaTime)
{
	m_hitMarkerRemainingSeconds = std::max(0.0f,m_hitMarkerRemainingSeconds - deltaTime);
}

//========= 武器の装備関数=========
// 武器の性能値とカメラの反動の設定を切り替える。
void ShotController::EquipWeapon(WEAPON_TYPE weaponType)
{
	// 表の範囲外の種類は装備しない。通常は、呼出側が範囲内の値を渡す。
	const int weapon_index = static_cast<int>(weaponType);
	if(weapon_index < 0 || weapon_index >= WEAPON_TABLE::ns_WEAPON_COUNT)return;

	// 弾薬を消費しないデバッグ設定は、武器を切り替えても引き継ぐ。Initializeは、この設定を無効へ戻すため、切替の前の状態を覚えて、戻す。
	const WEAPON_DATA& weapon_data = WEAPON_TABLE::ns_WEAPONS[weapon_index];
	const bool was_infinite_ammo = m_weaponSystem.IsInfiniteAmmoEnabled();
	m_weaponSystem.Initialize(weapon_data);
	if(was_infinite_ammo){
		m_weaponSystem.ToggleInfiniteAmmo();
	}

	// 参照先の登録後なら、新しい武器の反動の設定を、カメラへ渡す。登録の前（Reset）では、Initializeが渡す。
	if(m_camera != nullptr){
		m_camera->SetRecoilProfile(weapon_data.recoilProfile);
	}
}

//========= 補助関数=========
// 数字キーが押された瞬間に、対応する武器を装備する。
void ShotController::UpdateWeaponSelection()
{
	// 切替のキーを順に調べる。表の武器の数より多いキーは、対応する武器がないため、調べない。
	for(int key_index = 0; key_index < WEAPON_TABLE::ns_WEAPON_COUNT && key_index < static_cast<int>(sizeof(ns_WEAPON_SELECT_KEYS)); ++key_index){
		// 押された瞬間のキーに対応する武器を装備し、切替をログへ記録して、この更新の切替を終える。
		if(m_inputSystem->IsKeyTriggered(ns_WEAPON_SELECT_KEYS[key_index])){
			EquipWeapon(static_cast<WEAPON_TYPE>(key_index));
			Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,std::wstring{L"武器を切り替えました: "} + WEAPON_TABLE::ns_WEAPONS[key_index].displayName);
			return;
		}
	}
}