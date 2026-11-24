#include "../H/ShotController.h"

// 射撃の更新、武器の装備、弾道の終点の通知、命中した敵へのダメージと怯み（弾ごと。特殊強化の効果を含む）、命中表示の時間管理を実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで、命中表示とダメージの増加の残り時間を0未満にしない。

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

// 射撃の表示に使用する固定設定と、弾道の終点を求める補助関数をまとめる。
namespace
{
	//========= 射撃の表示設定=========
	// 命中しない場合に、弾道の線を伸ばす長さ。単位はワールド距離単位。
	constexpr float ns_SHOT_TRACE_DISPLAY_LENGTH = 30.0f;
	// 命中を示す表示を続ける時間。単位は秒。
	constexpr float ns_HIT_MARKER_SECONDS = 0.3f;

	//========= 弾道の補助関数=========
	// 弾1つの弾道の終点を求める。命中していれば命中点、していなければ弾の方向へ表示用の長さだけ進んだ点。
	// pellet: 終点を求める弾の結果。
	// 戻り値: 弾道の終点のワールド座標。
	XMFLOAT3 GetShotTraceEnd(const SHOT_PELLET_RESULT& pellet)
	{
		// 命中した場合は、終点を命中点にする。
		if(pellet.didHit)return pellet.rayHit.point;

		// 命中しなかった場合は、弾の方向へ表示用の長さだけ進んだ点を終点にする。
		const XMFLOAT3& origin = pellet.ray.origin;
		const XMFLOAT3& direction = pellet.ray.direction;
		return XMFLOAT3{origin.x + direction.x * ns_SHOT_TRACE_DISPLAY_LENGTH,origin.y + direction.y * ns_SHOT_TRACE_DISPLAY_LENGTH,
			origin.z + direction.z * ns_SHOT_TRACE_DISPLAY_LENGTH};
	}
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
// 射撃・リロードを更新し、敵へ命中していればダメージと怯みを与える。
void ShotController::Update(float gameplayDeltaTime)
{
	// ジャスト回避の後のダメージの増加の残り時間を進め、0未満にしない。このフレームの射撃にも、残りがあれば反映する。
	m_damageBoostRemainingSeconds = std::max(0.0f,m_damageBoostRemainingSeconds - gameplayDeltaTime);

	// 射撃を更新する。リロード・連射間隔・弾薬の処理と、弾ごとの命中判定は武器が行い、ダメージの反映だけをここで行う。
	// 経過時間・構えているか・射撃してよいかを、1つの構造体にまとめて渡す。回避・蹴り・殴打の間は射撃できない。
	const WEAPON_UPDATE_REQUEST weapon_request{gameplayDeltaTime,m_playerController->IsAiming(),m_playerController->IsShootingAllowed()};
	const WEAPON_UPDATE_RESULT weapon_result = m_weaponSystem.Update(weapon_request,*m_inputSystem,*m_camera,*m_collisionWorld);

	// 発射していない更新は、弾道の通知もダメージもないため、ここで戻る。
	if(!weapon_result.didFire)return;

	// 弾道の終点を可視化へ伝える。可視化は弾道を1本だけ表示するため、複数の弾を撃つ武器では、先頭の弾の終点を使う。
	m_collisionDebugView->NotifyShot(GetShotTraceEnd(weapon_result.pellets[0]));

	// 弾ごとに、敵レイヤーへ命中した場合だけ、ダメージを与える。ダメージは、使用中の武器の弾1つあたりの値（強化を含む）に、
	// ジャスト回避の後の増加（回避反撃）と、敵に付いた印（殴打追撃）の倍率を掛ける。怯み時間は、制圧射撃の倍率を掛ける。
	// 全ての弾は、ダメージを与える前に判定済みで、先に倒した敵への弾は、生存中の敵として見つからず、何もしない。
	const float boost_scale = IsDamageBoostActive() ? m_damageBoostScale : 1.0f;
	for(int pellet_index = 0; pellet_index < weapon_result.pelletCount; ++pellet_index){
		const SHOT_PELLET_RESULT& pellet = weapon_result.pellets[pellet_index];
		// 命中しなかった弾と、敵以外（地形）へ命中した弾は、ダメージを与えず、次の弾へ進む。
		if(!pellet.didHit || pellet.hitInfo.layer != COLLISION_LAYER::ns_ENEMY)continue;

		// Colliderの識別値から、ダメージを与える相手を探す。対応する生存中の敵がいなければ、次の弾へ進む。
		Enemy* const hit_enemy = m_enemyManager->FindLivingEnemy(pellet.hitInfo.userData);
		if(hit_enemy == nullptr)continue;

		const float weapon_damage = m_weaponSystem.GetWeaponData().damage * boost_scale * hit_enemy->GetShotDamageScale();
		hit_enemy->ApplyDamage(m_playerCombat->GetAttackDamage(weapon_damage,*hit_enemy),*m_collisionWorld,m_flinchScale);
		m_hitMarkerRemainingSeconds = ns_HIT_MARKER_SECONDS;
	}
}

// 命中表示の残り時間を進める。
void ShotController::UpdateHitMarker(float deltaTime)
{
	m_hitMarkerRemainingSeconds = std::max(0.0f,m_hitMarkerRemainingSeconds - deltaTime);
}

//========= 武器の装備関数=========
// 表の基本の性能値で、武器を装備する。
void ShotController::EquipWeapon(WEAPON_TYPE weaponType)
{
	// 表の範囲外の種類は装備しない。通常は、呼出側が範囲内の値を渡す。
	const int weapon_index = static_cast<int>(weaponType);
	if(weapon_index < 0 || weapon_index >= WEAPON_TABLE::ns_WEAPON_COUNT)return;

	EquipWeapon(weaponType,WEAPON_TABLE::ns_WEAPONS[weapon_index]);
}

// 指定した性能値で、武器を装備する。
void ShotController::EquipWeapon(WEAPON_TYPE weaponType,const WEAPON_DATA& weaponData)
{
	// 表の範囲外の種類は装備しない。通常は、呼出側が範囲内の値を渡す。
	const int weapon_index = static_cast<int>(weaponType);
	if(weapon_index < 0 || weapon_index >= WEAPON_TABLE::ns_WEAPON_COUNT)return;

	// 弾薬を消費しないデバッグ設定は、武器を切り替えても引き継ぐ。Initializeは、この設定を無効へ戻すため、切替の前の状態を覚えて、戻す。
	const bool was_infinite_ammo = m_weaponSystem.IsInfiniteAmmoEnabled();
	m_weaponSystem.Initialize(weaponData);
	if(was_infinite_ammo){
		m_weaponSystem.ToggleInfiniteAmmo();
	}

	// 参照先の登録後なら、この武器の反動の設定を、カメラへ渡す。登録の前（Reset）では、Initializeが渡す。
	if(m_camera != nullptr){
		m_camera->SetRecoilProfile(weaponData.recoilProfile);
	}
}