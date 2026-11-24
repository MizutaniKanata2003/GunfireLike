#pragma once

// 武器1丁の弾薬・連射間隔・リロードの状態を保持し、射撃入力に応じてRayによる命中判定（HitScan）と視線の反動を行う。
// 1回の発射で撃つ弾（ペレット）の数は性能値が決め、弾ごとに独立した方向のRayで判定する。
// ダメージの適用や敵の状態更新は担当せず、弾ごとに命中した相手の情報を結果として返す。
// 武器の切替・購入・強化は担当しないが、購入した予備弾薬の加算と、強化後の性能値への差し替えと、持ち替えた時の弾薬の復元は受け付ける。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minとstd::maxで、予備弾薬の加算を上限内に収め、装填弾数を装弾数の上限内に収める。
#include <array> // std::arrayで、1回の発射の弾ごとの結果を、固定長で保持する。
#include <random> // std::mt19937で弾のばらつきと反動の左右の乱数を生成する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Weapon/H/WeaponTable.h"

//========= 前方宣言=========
class CollisionWorld;
class FpsCamera;
class InputSystem;

// 1回の発射で撃った、弾（ペレット）1つ分の結果。
struct SHOT_PELLET_RESULT
{
	//========= 発射=========
	// 弾のRay。始点はカメラ位置、方向はばらつきを反映した単位ベクトル。
	COLLISION_RAY ray{};

	//========= 命中=========
	// 弾が、いずれかのColliderへ命中したか。
	bool didHit{};
	// 命中の距離・命中点・法線。didHitがtrueの場合だけ有効。
	COLLISION_RAY_HIT rayHit{};
	// 命中したColliderの情報。didHitがtrueの場合だけ有効。
	COLLISION_HIT_INFO hitInfo{};
};

// 1回の更新で起きた射撃の結果。
struct WEAPON_UPDATE_RESULT
{
	//========= 射撃=========
	// この更新で弾を発射したか。
	bool didFire{};
	// この発射で撃った弾（ペレット）の数。1以上で、WEAPON_TABLE::ns_MAX_PELLET_COUNT以下。didFireがtrueの場合だけ有効。
	int pelletCount{};

	//========= 弾ごとの結果=========
	// 弾ごとのRayと命中の結果。先頭からpelletCount個だけ有効。didFireがtrueの場合だけ有効。
	std::array<SHOT_PELLET_RESULT,WEAPON_TABLE::ns_MAX_PELLET_COUNT> pellets{};
};

// 1回の更新で、呼出側が渡す、時間と射撃の条件。WeaponSystem::Updateの引数の数を減らすための値の束で、更新は変更しない。
struct WEAPON_UPDATE_REQUEST
{
	// この更新で進める経過時間。単位は秒。
	float deltaTime{};
	// 構えているか。trueなら構え射撃の散布、falseなら通常射撃の散布を使う。
	bool isAiming{};
	// 射撃してよい状態か。falseの間は、左クリックを押していても発射しない。リロードの開始と進行には影響しない。
	bool isShootingAllowed{};
};

// 弾薬・連射間隔・リロードの状態を管理し、射撃の判定を行う。
class WeaponSystem final
{
public:
	//========= 初期化・更新関数=========
	// 性能値を登録し、弾薬を満タン・予備弾薬を初期値へ戻し、連射間隔とリロードの状態を解除する。弾薬を消費しないデバッグ設定も、無効へ戻す。
	// weaponData: 使用する武器の性能値。値をコピーする。
	void Initialize(const WEAPON_DATA& weaponData);
	// 連射間隔とリロードの時間を進め、リロード開始と射撃の入力を処理する。射撃を続けている最中かを、カメラの反動の制御へ伝える。
	// 発射した場合は、1回の発射で、弾薬を1つだけ減らし、反動を1回だけ加え、性能値の数の弾（ペレット）を、それぞれ別の方向へ判定する。
	// request: この更新で進める経過時間と、構えているか、射撃してよいか。
	// input: 左クリック（射撃）とRキー（リロード）の取得先。参照は保持しない。
	// camera: 発射の始点と方向の取得元で、発射時に反動を加える先。呼出側が先に視線と位置を更新しておくこと。
	// collisionWorld: 命中判定を行うCollisionWorld。読み取りだけを行う。
	// 戻り値: この更新で起きた射撃の結果。リロード中は発射しない。
	[[nodiscard]] WEAPON_UPDATE_RESULT Update(const WEAPON_UPDATE_REQUEST& request,const InputSystem& input,FpsCamera& camera,const CollisionWorld& collisionWorld);
	// リロードを途中でやめる。弾は補充せず、装填弾数と予備弾薬は変わらない。殴打など、リロードをキャンセルする動作から呼ぶ。
	// リロード中でなければ何もしない。
	void CancelReload()
	{
		m_isReloading = false;
		m_reloadElapsedSeconds = 0.0f;
	}
	// 予備弾薬を増やす。上限を超える分は加えない。装填弾数とリロードの状態は変えない。弾薬の購入から呼ぶ。
	// amount: 増やす弾数。0以下の場合は何もしない。
	// maxReserve: 予備弾薬の上限。現在の予備弾薬が上限以上なら、何も加えない。
	// 戻り値: 実際に加えた弾数。0以上。
	int AddReserveAmmo(int amount,int maxReserve)
	{
		// 増やす数が正でない場合は、何も加えない。
		if(amount <= 0)return 0;

		// 上限までの空きと、増やす数の少ない方だけ加える。空きがなければ0になる。
		const int addable_ammo = std::max(0,maxReserve - m_reserveAmmo);
		const int added_ammo = std::min(amount,addable_ammo);
		m_reserveAmmo += added_ammo;
		return added_ammo;
	}
	// 装填弾数と予備弾薬を、指定した値へ設定する。持ち替える前に保存した弾薬を、戻すために使う。
	// 装填弾数は、0〜装弾数の上限へ収め、予備弾薬は、0以上へ収める。リロードの進行は、解除する。
	// magazineAmmo: 設定する装填弾数。
	// reserveAmmo: 設定する予備弾薬の数。
	void SetAmmo(int magazineAmmo,int reserveAmmo)
	{
		m_magazineAmmo = std::clamp(magazineAmmo,0,m_weaponData.magazineSize);
		m_reserveAmmo = std::max(0,reserveAmmo);
		CancelReload();
	}
	// 性能値を、強化後の値へ差し替える。装填弾数・予備弾薬・連射間隔・リロードの進行は、そのまま引き継ぐ。
	// 装填弾数が、新しい装弾数の上限を超える場合だけ、上限へ収める。強化の購入から呼ぶ。
	// weaponData: 差し替える性能値。同じ武器の強化後の値であること。値をコピーする。
	void ReplaceWeaponData(const WEAPON_DATA& weaponData)
	{
		m_weaponData = weaponData;
		m_magazineAmmo = std::min(m_magazineAmmo,m_weaponData.magazineSize);
	}
	// デバッグ用に、弾薬を消費しない設定を切り替える。有効の間は、装填弾数が0でも発射でき、発射しても弾薬が減らない。
	void ToggleInfiniteAmmo() { m_isInfiniteAmmo = !m_isInfiniteAmmo; }

	//========= Getter関数=========
	// 使用中の武器の性能値への参照を返す。戻り値の参照は、次のInitializeまで有効。
	[[nodiscard]] const WEAPON_DATA& GetWeaponData() const { return m_weaponData; }
	// 現在の装填弾数を返す。
	[[nodiscard]] int GetMagazineAmmo() const { return m_magazineAmmo; }
	// 現在の予備弾薬の数を返す。
	[[nodiscard]] int GetReserveAmmo() const { return m_reserveAmmo; }
	// 装弾数の上限を返す。
	[[nodiscard]] int GetMagazineSize() const { return m_weaponData.magazineSize; }
	// リロード中かを返す。
	[[nodiscard]] bool IsReloading() const { return m_isReloading; }
	// 弾薬を消費しないデバッグ設定が有効かを返す。
	[[nodiscard]] bool IsInfiniteAmmoEnabled() const { return m_isInfiniteAmmo; }
	// リロードの進み具合を0〜1で返す。リロード中でなければ0。
	[[nodiscard]] float GetReloadProgress() const;

private:
	//========= 補助関数=========
	// 1回の発射で撃つ弾（ペレット）の数だけ、弾ごとに別の方向のRayを作り、最も近い地形または敵への命中を調べて、結果へ書き込む。
	// camera: 発射の始点と、視線方向・右方向の取得元。
	// spreadDegrees: 弾のばらつきの円錐の半角。単位は度。弾ごとに、この範囲から方向を選ぶ。
	// collisionWorld: 命中判定を行うCollisionWorld。読み取りだけを行う。
	// result: 弾の数と、弾ごとの結果の書込み先。
	void CastPellets(const FpsCamera& camera,float spreadDegrees,const CollisionWorld& collisionWorld,WEAPON_UPDATE_RESULT& result);
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

	//========= デバッグ設定=========
	// 弾薬を消費しないか。デバッグ用。Initializeで無効へ戻る。
	bool m_isInfiniteAmmo{};

	//========= 乱数=========
	// 弾のばらつきと反動の左右に使う乱数生成器。再現性は不要なため、固定の初期値を使う。
	std::mt19937 m_randomEngine{12345u};
};