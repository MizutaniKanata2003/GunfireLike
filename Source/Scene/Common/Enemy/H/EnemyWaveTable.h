#pragma once

// 部屋ごと・ウェーブごとの敵の編成（種類・数・初期位置）を表としてまとめる。敵の行動やColliderの登録は担当しない。
// 編成の数値は、企画書で未確定の試遊用の仮値。表を変えるだけで、編成と難度を調整できる。座標・距離の単位はワールド距離単位。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、部屋・ウェーブ・出現枠の数を固定した編成の表を作る。
#include <cstddef> // std::size_tで、出現枠の数と、ウェーブごとの敵の数を表す。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// 出現する敵の行動種別。
enum class ENEMY_KIND
{
	e_MELEE,  // プレイヤーへ接近して攻撃する敵。
	e_RANGED, // 離れた位置から弾を撃つ敵。
	e_AREA,   // 範囲を予告して攻撃する敵。
	e_BOSS    // 最後の部屋に1体だけ出る、近接型のボス。行動は、F4で追加するまで、立ったまま動かない。
};

// 敵1体の初期配置。
struct ENEMY_SPAWN
{
	// 行動種別。
	ENEMY_KIND kind;
	// 球Colliderの中心。Xは左右、Yは床からの高さ、Zは奥行きのワールド座標。壁の内側（X:±9.5、Z:-1.5〜17.5）で、出口の手前（Z:17より奥のX:±1.5）を避ける。
	XMFLOAT3 position;
};

// 部屋とウェーブの編成をまとめる。
namespace ENEMY_WAVE_TABLE
{
	//========= 編成の規模=========
	// 1ウェーブに同時に出現できる敵の最大数。固定枠の数でもあり、GameSessionの攻撃状態の配列とIDの割り当ての上限になる。
	inline constexpr std::size_t ns_SLOT_COUNT = 4;
	// 部屋の数。最後の部屋は、ボスの部屋。
	inline constexpr std::size_t ns_ROOM_COUNT = 4;
	// 1部屋が持てるウェーブの最大数。表の各部屋の配列の長さで、実際に使うウェーブの数は、ns_WAVE_COUNTSが決める。
	inline constexpr std::size_t ns_WAVES_PER_ROOM = 2;

	//========= ウェーブの数=========
	// 部屋ごとに実際に使うウェーブの数。添字は、部屋番号から1を引いた値。各部屋の最後のウェーブの全滅で出口が開く。
	// 1〜3部屋目は2ウェーブ、ボスの部屋は1ウェーブ。
	inline constexpr std::array<int,ns_ROOM_COUNT> ns_WAVE_COUNTS{2,2,2,1};

	//========= 編成の型=========
	// 1ウェーブ分の編成。
	struct WAVE_DEFINITION
	{
		// 出現させる敵の数。spawnsの先頭からこの数だけを使う。ns_SLOT_COUNT以下であること。
		std::size_t enemyCount;
		// 敵の配置。先頭からenemyCount個だけが有効で、残りは使わない。
		std::array<ENEMY_SPAWN,ns_SLOT_COUNT> spawns;
	};

	//========= 編成の表=========
	// 部屋→ウェーブの順に並ぶ編成。部屋が進むほど、敵の数と、射撃型・範囲攻撃型の割合を増やす試遊用の仮の配置。
	// 1部屋目は接近型が中心、2部屋目は射撃型が加わり、3部屋目は範囲攻撃型が加わり、4部屋目はボスだけになる。
	inline const std::array<std::array<WAVE_DEFINITION,ns_WAVES_PER_ROOM>,ns_ROOM_COUNT> ns_ROOMS{{
			// 1部屋目。1ウェーブ目は接近型2体、2ウェーブ目は接近型3体と射撃型1体。
		{{
			{2,{{{ENEMY_KIND::e_MELEE,{-4.0f,1.0f,9.0f}},{ENEMY_KIND::e_MELEE,{4.0f,1.0f,9.0f}},{},{}}}},
		{4,{{{ENEMY_KIND::e_MELEE,{-5.0f,1.0f,8.0f}},{ENEMY_KIND::e_MELEE,{0.0f,1.0f,11.0f}},{ENEMY_KIND::e_MELEE,{5.0f,1.0f,8.0f}},
		{ENEMY_KIND::e_RANGED,{0.0f,1.0f,15.0f}}}}}
			}},
		// 2部屋目。1ウェーブ目は接近型2体と射撃型2体、2ウェーブ目は射撃型2体と接近型1体と範囲攻撃型1体。
		{{
			{4,{{{ENEMY_KIND::e_MELEE,{-4.0f,1.0f,8.0f}},{ENEMY_KIND::e_MELEE,{4.0f,1.0f,8.0f}},{ENEMY_KIND::e_RANGED,{-7.0f,1.0f,14.0f}},{ENEMY_KIND::e_RANGED,{7.0f,1.0f,14.0f}}}}},
		{4,{{{ENEMY_KIND::e_RANGED,{-6.0f,1.0f,14.0f}},{ENEMY_KIND::e_RANGED,{6.0f,1.0f,14.0f}},{ENEMY_KIND::e_MELEE,{0.0f,1.0f,9.0f}},{ENEMY_KIND::e_AREA,{0.0f,1.0f,13.0f}}}}}
			}},
		// 3部屋目。1ウェーブ目は接近型2体と範囲攻撃型1体と射撃型1体、2ウェーブ目は範囲攻撃型2体と射撃型1体と接近型1体。
		{{
			{4,{{{ENEMY_KIND::e_MELEE,{-5.0f,1.0f,8.0f}},{ENEMY_KIND::e_MELEE,{5.0f,1.0f,8.0f}},{ENEMY_KIND::e_AREA,{0.0f,1.0f,13.0f}},{ENEMY_KIND::e_RANGED,{0.0f,1.0f,16.0f}}}}},
		{4,{{{ENEMY_KIND::e_AREA,{-5.0f,1.0f,12.0f}},{ENEMY_KIND::e_AREA,{5.0f,1.0f,12.0f}},{ENEMY_KIND::e_RANGED,{0.0f,1.0f,16.0f}},{ENEMY_KIND::e_MELEE,{0.0f,1.0f,8.0f}}}}}
			}},
		// 4部屋目（ボスの部屋）。1ウェーブ目はボス1体を、部屋の奥の中央へ置く。球の中心の高さは、ボスの球の半径と同じにする。
		// 2ウェーブ目は、この部屋が使わない（ns_WAVE_COUNTSが1）ため、敵の数0の空の編成を置く。
		{{
			{1,{{{ENEMY_KIND::e_BOSS,{0.0f,1.5f,12.0f}},{},{},{}}}},
		{0,{}}
			}}
		}};

	//========= 取得関数=========
	// 部屋が使うウェーブの数を返す。
	// roomNumber: 1始まりの部屋番号。
	// 戻り値: その部屋のウェーブの数。部屋番号が範囲外の場合は0。
	[[nodiscard]] inline constexpr int GetWaveCount(int roomNumber)
	{
		// 部屋番号が範囲外の場合は、配列の外を読まないよう0を返す。
		if(roomNumber < 1 || roomNumber > static_cast<int>(ns_ROOM_COUNT))return 0;

		return ns_WAVE_COUNTS[static_cast<std::size_t>(roomNumber - 1)];
	}
}