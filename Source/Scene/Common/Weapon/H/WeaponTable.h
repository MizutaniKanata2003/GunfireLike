#pragma once

// 武器1種類分の性能値の型と、仮の性能値の表を定義する。数値は企画書で未確定のため、プレイ確認後に調整する仮値。
// 武器の動作や状態は担当しない。性能値を1か所に集約するための定義だけを持つ。

//========= Framework インクルード=========
#include "Framework/Input/H/FpsCamera.h" // 武器ごとの反動の追従・復帰・上限を表すRECOIL_PROFILEを、性能値へ含めるために使用する。

// 武器1種類の性能値。
struct WEAPON_DATA
{
	//========= 発射=========
	// 連射の間隔。単位は秒。この時間が経つまで次の弾を撃てない。
	float fireIntervalSeconds;
	// 命中1発あたりのダメージ。
	float damage;
	// 射撃できる最大距離。単位はワールド距離単位。これを超える相手には命中しない。
	float maxRange;

	//========= 精度=========
	// 構えていない通常射撃の弾のばらつき。視線方向を中心にした円錐の半角で、単位は度。
	float hipSpreadDegrees;
	// 構え射撃の弾のばらつき。円錐の半角で、単位は度。0なら視線方向へ正確に飛ぶ。
	float aimSpreadDegrees;

	//========= 反動=========
	// 1発ごとに視線を上へ動かす角度。単位は度。
	float recoilPitchDegrees;
	// 1発ごとに視線を左右へ動かす角度の最大値。単位は度。実際は±この範囲の乱数。
	float recoilYawDegrees;

	//========= 弾薬=========
	// 装填できる弾数の上限。
	int magazineSize;
	// 開始時に持つ予備弾薬の数。
	int initialReserveAmmo;
	// リロードにかかる時間。単位は秒。
	float reloadSeconds;

	//========= 反動の制御=========
	// 反動によるカメラのずれの、追従・復帰・上限の設定。武器を装備した時に、カメラへ渡す。
	RECOIL_PROFILE recoilProfile;
};

// 武器の性能値の表をまとめる。
namespace WEAPON_TABLE
{
	//========= 反動の制御=========
	// 連射武器の反動の制御。目標へ滑らかに近づき、射撃をやめて少し経つと元へ戻る。D1より前の固定値と同じ。
	// 並び: 追従の速さ(1/秒)、復帰を始めるまでの時間(秒)、復帰の速さ(1/秒)、Pitchの上限(度)、Yawの上限(度)。
	inline constexpr RECOIL_PROFILE ns_RECOIL_PROFILE_ASSAULT_RIFLE{16.0f,0.15f,6.0f,12.0f,6.0f};

	//========= 連射武器=========
	// 初期武器の無強化の連射武器。企画書の方針（無強化では連射が遅く、反動が大きく、リロードが遅い）に沿った仮値。
	// 並び: 発射間隔(秒)、ダメージ、最大射程、通常射撃の散布(度)、構え射撃の散布(度)、反動の上(度)、反動の左右(度)、装弾数、初期予備弾薬、リロード時間(秒)、反動の制御。
	inline constexpr WEAPON_DATA ns_ASSAULT_RIFLE{0.25f,10.0f,100.0f,4.0f,0.3f,1.2f,0.4f,20,120,2.5f,ns_RECOIL_PROFILE_ASSAULT_RIFLE};
}