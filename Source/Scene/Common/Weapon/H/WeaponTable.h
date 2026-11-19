#pragma once

// 武器1種類分の性能値の型と、仮の性能値の表を定義する。数値は企画書で未確定のため、プレイ確認後に調整する仮値。
// 武器の動作や状態は担当しない。性能値を1か所に集約するための定義だけを持つ。

//========= Framework インクルード=========
#include "Framework/Input/H/FpsCamera.h" // 武器ごとの反動の制御方式・追従・復帰・上限を表すRECOIL_PROFILEを、性能値へ含めるために使用する。

// 武器の種類。表（WEAPON_TABLE::ns_WEAPONS）の並びと対応し、値は表の添字になる。
enum class WEAPON_TYPE
{
	e_ASSAULT_RIFLE, // 連射銃。連射が遅く、反動が大きい初期武器。
	e_SHOTGUN,       // 散弾銃。発射間隔が長く、1発の反動が大きい。複数の弾（ペレット）への対応は、F2で行う。
	e_HAND_CANNON    // 単発高威力銃。1発のダメージが大きく、反動は単発方式で戻る。
};

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
	// 反動によるカメラのずれの、制御方式・追従・復帰・上限の設定。武器を装備した時に、カメラへ渡す。
	RECOIL_PROFILE recoilProfile;

	//========= 表示=========
	// 武器の表示名。静的な文字列定数を指し、所有しない。ログや、今後の表示に使う。
	const wchar_t* displayName;
};

// 武器の性能値の表をまとめる。
namespace WEAPON_TABLE
{
	//========= 反動の調整値=========
	// 反動の体感を調整する値を、ここへ集める。基準の値は、D1より前の固定値（1.0が基準）。武器の追加や、他の武器の調整では、この2つを使い回す。
	// 1発ごとの反動量（上と左右）に掛ける倍率。大きいほど、撃った時の揺れが大きくなる。仮値。
	inline constexpr float ns_RECOIL_KICK_SCALE = 1.5f;
	// 反動のずれの上限（PitchとYaw）に掛ける倍率。大きいほど、連射で視線が上がり続ける範囲が広がる。仮値。
	inline constexpr float ns_RECOIL_LIMIT_SCALE = 1.25f;

	//========= 反動の制御=========
	// 連射武器の反動の制御。射撃を続けている間は、視線のずれを戻さず（上限まで上がり続け）、射撃をやめた時に、すぐ元へ戻り始める。
	// 1発だけ撃ってやめた場合（タップ撃ち）も、やめた時から戻り始めるため、次の発砲までに、ずれの大半が戻る。
	// 上限は、基準値（Pitch 12度・Yaw 6度）に、上限の倍率を掛けた値。
	// 並び: 制御方式、追従の速さ(1/秒)、やめてから戻り始めるまでの時間(秒)、復帰の速さ(1/秒)、Pitchの上限(度)、Yawの上限(度)。
	inline constexpr RECOIL_PROFILE ns_RECOIL_PROFILE_ASSAULT_RIFLE{RECOIL_CONTROL::e_HOLD_UNTIL_RELEASE,16.0f,0.0f,6.0f,
		12.0f * ns_RECOIL_LIMIT_SCALE,6.0f * ns_RECOIL_LIMIT_SCALE};
	// 単発武器の反動の制御。1発ごとに、視線のずれの目標へ加え、実際の視線は目標へ滑らかに近づく。
	// 最後の発砲から待ち時間が過ぎると、元へ戻る。追従・待ち時間・復帰の速さは、D1より前の固定値と同じ。上限は、上限の倍率を掛けた値。
	// 並び: 制御方式、追従の速さ(1/秒)、最後の反動から戻り始めるまでの時間(秒)、復帰の速さ(1/秒)、Pitchの上限(度)、Yawの上限(度)。
	inline constexpr RECOIL_PROFILE ns_RECOIL_PROFILE_SINGLE_SHOT{RECOIL_CONTROL::e_TIMED_RETURN,16.0f,0.15f,6.0f,
		12.0f * ns_RECOIL_LIMIT_SCALE,6.0f * ns_RECOIL_LIMIT_SCALE};
	// 散弾銃の反動の制御。1発の反動が大きく、発射間隔も長いため、単発方式で、戻り始めるまでをやや長くする。仮値。
	// 並び: 上の単発武器と同じ。
	inline constexpr RECOIL_PROFILE ns_RECOIL_PROFILE_SHOTGUN{RECOIL_CONTROL::e_TIMED_RETURN,20.0f,0.35f,5.0f,
		12.0f * ns_RECOIL_LIMIT_SCALE,6.0f * ns_RECOIL_LIMIT_SCALE};

	//========= 武器の性能値=========
	// 並び: 発射間隔(秒)、ダメージ、最大射程、通常射撃の散布(度)、構え射撃の散布(度)、反動の上(度)、反動の左右(度)、装弾数、初期予備弾薬、リロード時間(秒)、反動の制御、表示名。
	// 1発ごとの反動量は、基準値に、反動量の倍率を掛けた値。

	// 連射銃。初期武器の無強化の連射武器。企画書の方針（無強化では連射が遅く、反動が大きく、リロードが遅い）に沿った仮値。基準の反動は、上1.2度・左右0.4度。
	inline constexpr WEAPON_DATA ns_ASSAULT_RIFLE{0.25f,10.0f,100.0f,4.0f,0.3f,1.2f * ns_RECOIL_KICK_SCALE,0.4f * ns_RECOIL_KICK_SCALE,20,120,2.5f,
		ns_RECOIL_PROFILE_ASSAULT_RIFLE,L"連射銃"};
	// 散弾銃。近距離向けで、散布が大きく、射程が短い。ダメージは、1発（1ペレット）あたりで、複数のペレットへの対応は、F2で行う。基準の反動は、上5.0度・左右0.8度。全て仮値。
	inline constexpr WEAPON_DATA ns_SHOTGUN{0.9f,12.0f,25.0f,6.0f,4.0f,5.0f * ns_RECOIL_KICK_SCALE,0.8f * ns_RECOIL_KICK_SCALE,6,36,3.0f,
		ns_RECOIL_PROFILE_SHOTGUN,L"散弾銃"};
	// 単発高威力銃。1発のダメージが大きく、構え射撃では正確に飛ぶ。基準の反動は、上3.5度・左右0.5度。全て仮値。
	inline constexpr WEAPON_DATA ns_HAND_CANNON{0.6f,60.0f,100.0f,2.5f,0.0f,3.5f * ns_RECOIL_KICK_SCALE,0.5f * ns_RECOIL_KICK_SCALE,6,30,2.0f,
		ns_RECOIL_PROFILE_SINGLE_SHOT,L"単発高威力銃"};

	//========= 武器の表=========
	// 武器の種類（WEAPON_TYPE）の値を添字にする表。並びは、WEAPON_TYPEの並びと同じにする。
	inline constexpr WEAPON_DATA ns_WEAPONS[]{ns_ASSAULT_RIFLE,ns_SHOTGUN,ns_HAND_CANNON};
	// 表の武器の数。
	inline constexpr int ns_WEAPON_COUNT = static_cast<int>(sizeof(ns_WEAPONS) / sizeof(ns_WEAPONS[0]));
}