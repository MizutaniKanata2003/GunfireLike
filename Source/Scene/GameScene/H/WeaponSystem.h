#pragma once

// 武器1丁の弾薬・連射間隔・リロードの状態を保持し、射撃入力に応じてRayによる命中判定（HitScan）と視線の反動を行う。
// ダメージの適用や敵の状態更新は担当せず、命中した相手の情報を結果として返す。武器の切替・購入・強化は担当しない。

//========= C++標準ライブラリ インクルード=========
#include <random> // std::mt19937で弾のばらつきと反動の左右の乱数を生成する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/GameScene/H/WeaponTable.h"

//========= 前方宣言=========
class CollisionWorld;
class FpsCamera;
class InputSystem;

// 1回の更新で起きた射撃の結果。
struct WEAPON_UPDATE_RESULT
{
	//========= 射撃=========
	// この更新で弾を発射したか。
	bool didFire{};
	// 発射した弾が、いずれかのColliderへ命中したか。didFireがtrueの場合だけ意味を持つ。
	bool didHit{};
	// 発射した弾のRay。始点はカメラ位置、方向はばらつきを反映した単位ベクトル。didFireがtrueの場合だけ有効。
	COLLISION_RAY shotRay{};

	//========= 命中=========
	// 命中の距離・命中点・法線。didHitがtrueの場合だけ有効。
	COLLISION_RAY_HIT rayHit{};
	// 命中したColliderの情報。didHitがtrueの場合だけ有効。
	COLLISION_HIT_INFO hitInfo{};
};

// 弾薬・連射間隔・リロードの状態を管理し、射撃の判定を行う。
class WeaponSystem final
{
public:
	//========= 初期化・更新関数=========
	// 性能値を登録し、弾薬を満タン・予備弾薬を初期値へ戻し、連射間隔とリロードの状態を解除する。
	// weaponData: 使用する武器の性能値。値をコピーする。
	void Initialize(const WEAPON_DATA& weaponData);
	// 連射間隔とリロードの時間を進め、リロード開始と射撃の入力を処理する。
	// deltaTime: この更新で進める経過時間。単位は秒。
	// input: 左クリック（射撃）とRキー（リロード）の取得先。参照は保持しない。
	// isAiming: 構えているか。trueなら構え射撃の散布、falseなら通常射撃の散布を使う。
	// isShootingAllowed: 射撃してよい状態か。falseの間は、左クリックを押していても発射しない。リロードの開始と進行には影響しない。
	// camera: 発射の始点と方向の取得元で、発射時に反動を加える先。呼出側が先に視線と位置を更新しておくこと。
	// collisionWorld: 命中判定を行うCollisionWorld。読み取りだけを行う。
	// 戻り値: この更新で起きた射撃の結果。リロード中は発射しない。
	[[nodiscard]] WEAPON_UPDATE_RESULT Update(float deltaTime,const InputSystem& input,bool isAiming,bool isShootingAllowed,FpsCamera& camera,
																						const CollisionWorld& collisionWorld);

	//========= Getter関数=========
	// 現在の装填弾数を返す。
	[[nodiscard]] int GetMagazineAmmo() const { return m_magazineAmmo; }
	// 現在の予備弾薬の数を返す。
	[[nodiscard]] int GetReserveAmmo() const { return m_reserveAmmo; }
	// 装弾数の上限を返す。
	[[nodiscard]] int GetMagazineSize() const { return m_weaponData.magazineSize; }
	// リロード中かを返す。
	[[nodiscard]] bool IsReloading() const { return m_isReloading; }
	// リロードの進み具合を0〜1で返す。リロード中でなければ0。
	[[nodiscard]] float GetReloadProgress() const;

private:
	//========= 補助関数=========
	// 視線方向を中心にした円錐の内側から、一様な乱数で弾の方向を選ぶ。
	// camera: 視線方向と右方向の取得元。
	// spreadDegrees: 円錐の半角。単位は度。0なら視線方向をそのまま返す。
	// 戻り値: 長さ1の弾の方向。
	[[nodiscard]] XMFLOAT3 BuildShotDirection(const FpsCamera& camera,float spreadDegrees);

	//========= 性能値=========
	// 使用中の武器の性能値のコピー。
	WEAPON_DATA m_weaponData{};

	//========= 弾薬・時間の状態=========
	// 現在の装填弾数。0以上で装弾数の上限以下。
	int m_magazineAmmo{};
	// 現在の予備弾薬の数。0以上。
	int m_reserveAmmo{};
	// 次の弾を撃てるまでの残り時間。単位は秒。0以下なら発射できる。
	float m_fireCooldownSeconds{};
	// リロード中か。
	bool m_isReloading{};
	// リロード開始からの経過時間。単位は秒。リロード中だけ進む。
	float m_reloadElapsedSeconds{};

	//========= 乱数=========
	// 弾のばらつきと反動の左右に使う乱数生成器。再現性は不要なため、固定の初期値を使う。
	std::mt19937 m_randomEngine{12345u};
};