#include "../H/WeaponSystem.h"

// 連射間隔・リロード・弾薬の管理と、Rayによる射撃の判定（弾ごと）、視線の反動を実装する。
// ダメージの適用は呼出側が行う。数値は性能値の表から受け取り、このファイルでは固定しない。

//========= Windows インクルード=========
#include <windows.h> // VK_LBUTTONで射撃入力、'R'のキーコードでリロード入力を判定する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで連射間隔の残り時間を0以上に保ち、std::minでリロードの補充数と進み具合の上限を制限し、std::clampで弾の数を範囲内に収める。
#include <cmath> // std::tan・std::sqrt・std::cos・std::sinで、弾のばらつきの方向を円錐内から求める。
#include <cstdint> // std::uint32_tで、命中判定の対象にするレイヤーのbitのマスクを保持する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Input/H/FpsCamera.h"
#include "Framework/Input/H/InputSystem.h"

// 射撃の判定に使う固定設定をまとめる。
namespace
{
	//========= 命中判定の対象=========
	// 弾が命中する対象のレイヤー。地形と敵。プレイヤー自身や攻撃判定は対象にしない。
	constexpr std::uint32_t ns_SHOT_LAYER_MASK = COLLISION_LAYER::ns_STATIC | COLLISION_LAYER::ns_ENEMY;

	//========= 数学定数=========
	// 円周率の2倍。円錐内のばらつきの角度（0〜2π）を求めるために使う。
	constexpr float ns_TWO_PI = 6.28318530717959f;
}

//========= 初期化・更新関数=========
// 性能値を登録し、弾薬と時間の状態を初期値へ戻す。
void WeaponSystem::Initialize(const WEAPON_DATA& weaponData)
{
	// 性能値を保持し、装填弾数を上限に、予備弾薬を初期値にする。
	m_weaponData = weaponData;
	m_magazineAmmo = weaponData.magazineSize;
	m_reserveAmmo = weaponData.initialReserveAmmo;

	// 連射間隔とリロードの状態を解除し、弾薬を消費しないデバッグ設定も無効へ戻す。
	m_fireCooldownSeconds = 0.0f;
	m_isReloading = false;
	m_reloadElapsedSeconds = 0.0f;
	m_isInfiniteAmmo = false;
}

// リロード中なら時間を進め、そうでなければリロード開始と射撃を判定する。
WEAPON_UPDATE_RESULT WeaponSystem::Update(const WEAPON_UPDATE_REQUEST& request,const InputSystem& input,FpsCamera& camera,const CollisionWorld& collisionWorld)
{
	// 戻す結果。発射しなければ全て初期値のまま。
	WEAPON_UPDATE_RESULT result{};

	// 射撃を続けている最中かを、カメラの反動の制御へ伝える。射撃が許可され、左クリックを押し続け、リロード中でなく、撃てる弾がある間だけtrue。
	// 連射方式の反動は、trueの間は戻らず、falseになった更新から戻り始める。弾が尽きた時や、リロード、射撃の禁止でも、falseになる。
	const bool is_firing = request.isShootingAllowed && input.IsKeyPressed(VK_LBUTTON) && !m_isReloading && (m_magazineAmmo > 0 || m_isInfiniteAmmo);
	camera.SetRecoilHold(is_firing);

	// 連射間隔の残り時間を進める。0未満にはせず、発射可能の状態を0で表す。
	m_fireCooldownSeconds = std::max(0.0f,m_fireCooldownSeconds - request.deltaTime);

	// リロード中は発射せず、経過時間を進めて、完了していれば弾を補充する。
	if(m_isReloading){
		m_reloadElapsedSeconds += request.deltaTime;

		// 経過時間がリロード時間に達したら、不足分と予備弾薬の少ない方だけ補充してリロードを終える。
		if(m_reloadElapsedSeconds >= m_weaponData.reloadSeconds){
			const int needed_ammo = m_weaponData.magazineSize - m_magazineAmmo;
			const int refill_ammo = std::min(needed_ammo,m_reserveAmmo);
			m_magazineAmmo += refill_ammo;
			m_reserveAmmo -= refill_ammo;
			m_isReloading = false;
			m_reloadElapsedSeconds = 0.0f;
		}

		return result;
	}

	// Rキーを押した瞬間に、装填弾数が満タンでなく、予備弾薬が残っている場合だけリロードを始める。始めたフレームは発射しない。
	if(input.IsKeyTriggered('R') && m_magazineAmmo < m_weaponData.magazineSize && m_reserveAmmo > 0){
		m_isReloading = true;
		m_reloadElapsedSeconds = 0.0f;
		return result;
	}

	// 射撃が許可されていない、左クリックを押し続けていない、連射間隔が残っている場合は発射しない。
	// 装填弾がない場合も発射しないが、弾薬を消費しないデバッグ設定が有効なら、装填弾数に関係なく発射できる。
	if(!request.isShootingAllowed || !input.IsKeyPressed(VK_LBUTTON) || m_fireCooldownSeconds > 0.0f)return result;
	if(m_magazineAmmo <= 0 && !m_isInfiniteAmmo)return result;

	// 弾薬を1つ消費し、次の発射までの連射間隔を設定する。弾薬を消費しないデバッグ設定が有効なら、弾は減らさない。
	// 複数の弾（ペレット）を撃つ武器でも、減るのは1回の発射につき1つ。
	if(!m_isInfiniteAmmo){
		--m_magazineAmmo;
	}
	m_fireCooldownSeconds = m_weaponData.fireIntervalSeconds;

	// 構えているかに応じた散布で、弾ごとの方向と命中を決める。反動は方向を決めた後に、1回の発射につき1回だけ加える。
	const float spread_degrees = request.isAiming ? m_weaponData.aimSpreadDegrees : m_weaponData.hipSpreadDegrees;
	result.didFire = true;
	CastPellets(camera,spread_degrees,collisionWorld,result);

	// 反動として視線を上へ動かし、左右へは乱数で振る。度をラジアンへ換算して渡す。
	std::uniform_real_distribution<float> yaw_distribution(-m_weaponData.recoilYawDegrees,m_weaponData.recoilYawDegrees);
	camera.ApplyRecoil(XMConvertToRadians(yaw_distribution(m_randomEngine)),XMConvertToRadians(m_weaponData.recoilPitchDegrees));

	return result;
}

//========= Getter関数=========
// リロード時間に対する経過時間の割合を求める。
float WeaponSystem::GetReloadProgress() const
{
	// リロード中でない、またはリロード時間が0以下なら、0除算を避けて0を返す。
	if(!m_isReloading || m_weaponData.reloadSeconds <= 0.0f)return 0.0f;

	return std::min(1.0f,m_reloadElapsedSeconds / m_weaponData.reloadSeconds);
}

//========= 補助関数=========
// 弾（ペレット）ごとに、別の方向のRayを作り、命中を調べる。
void WeaponSystem::CastPellets(const FpsCamera& camera,float spreadDegrees,const CollisionWorld& collisionWorld,WEAPON_UPDATE_RESULT& result)
{
	// 撃つ弾の数を、1から上限の範囲へ収める。性能値の誤りで、結果の配列の外へ書き込まないため。
	result.pelletCount = std::clamp(m_weaponData.pelletCount,1,WEAPON_TABLE::ns_MAX_PELLET_COUNT);

	// 弾の数だけ、始点はカメラ位置、方向は散布の範囲から弾ごとに選んで、最も近い地形または敵への命中を調べる。
	// 弾の方向は互いに独立で、命中しなかった弾は、命中情報が初期値のまま残る。
	for(int pellet_index = 0; pellet_index < result.pelletCount; ++pellet_index){
		SHOT_PELLET_RESULT& pellet = result.pellets[pellet_index];
		pellet.ray.origin = camera.GetPosition();
		pellet.ray.direction = BuildShotDirection(camera,spreadDegrees);

		// Ray・最大距離・対象のレイヤーを、1つの問い合わせにまとめて、命中を調べる。
		const COLLISION_RAY_QUERY query{pellet.ray,m_weaponData.maxRange,ns_SHOT_LAYER_MASK};
		pellet.didHit = collisionWorld.Raycast(query,pellet.rayHit,pellet.hitInfo);
	}
}

// 視線方向・右方向・上方向の3つで作る円錐の内側から、方向を1つ選ぶ。
XMFLOAT3 WeaponSystem::BuildShotDirection(const FpsCamera& camera,float spreadDegrees)
{
	// 視線方向と右方向を取得し、外積（前方向×右方向）でカメラの上方向を求める。3つはほぼ直交している。
	const XMFLOAT3 forward = camera.GetForward();
	const XMFLOAT3 right = camera.GetRight();
	const XMFLOAT3 up{forward.y * right.z - forward.z * right.y,forward.z * right.x - forward.x * right.z,forward.x * right.y - forward.y * right.x};

	// 円錐の半角の正接を求める。視線方向に長さ1だけ進んだ位置での、円の半径になる。
	const float spread_tangent = std::tan(XMConvertToRadians(spreadDegrees));

	// 円の内側を一様に選ぶため、角度は一様乱数、半径は一様乱数の平方根に比例させる。面積あたりの密度がそろう。
	std::uniform_real_distribution<float> unit_distribution(0.0f,1.0f);
	const float angle = unit_distribution(m_randomEngine) * ns_TWO_PI;
	const float radius = spread_tangent * std::sqrt(unit_distribution(m_randomEngine));

	// 視線方向へ、右方向と上方向のずれを加え、長さ1へ揃える。ずれが0なら視線方向そのまま。
	const float offset_right = radius * std::cos(angle);
	const float offset_up = radius * std::sin(angle);
	XMFLOAT3 direction{forward.x + right.x * offset_right + up.x * offset_up,forward.y + right.y * offset_right + up.y * offset_up,
		forward.z + right.z * offset_right + up.z * offset_up};
	const float direction_length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);

	// 長さが正の場合だけ単位化する。長さが0の場合は割らず、視線方向を返す。
	if(direction_length > 0.0f){
		direction.x /= direction_length;
		direction.y /= direction_length;
		direction.z /= direction_length;
		return direction;
	}

	return forward;
}