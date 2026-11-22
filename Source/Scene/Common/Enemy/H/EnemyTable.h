#pragma once

// 敵の種類（接近型・射撃型・範囲攻撃型・ボス）ごとの、体力・当たりの大きさ・表示の大きさと色を表としてまとめる。
// 数値の定義と取得だけを持ち、敵の行動や状態は担当しない。企画書で未確定の数値は、プレイ確認後に調整する仮値。単位は、特に記載がない限り、ワールド距離単位。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyWaveTable.h"

// 敵の種類1つ分の性能と見た目。
struct ENEMY_PARAMS
{
	//========= 戦闘=========
	// 最大HP。
	float maxHP;
	// 被弾判定の球の半径。
	float colliderRadius;

	//========= 表示=========
	// 表示するCubeの1辺の長さ。球の内側に収まる大きさを目安にした仮の大きさ。
	float cubeSize;
	// 通常時の色。Object画像の色へ乗算して、種類を見分けられるようにする。RGBAで、各成分は0〜1。
	XMFLOAT4 normalColor;
};

// 敵の種類ごとの数値をまとめる。
namespace ENEMY_TABLE
{
	//========= 接近型=========
	// プレイヤーへ接近して攻撃する敵。標準的なHPと大きさで、赤みのある色。
	inline constexpr ENEMY_PARAMS ns_MELEE{100.0f,0.8f,1.0f,XMFLOAT4{1.0f,0.6f,0.6f,1.0f}};

	//========= 射撃型=========
	// 離れた位置から弾を撃つ敵。接近型より低いHPと小さい体で、青みのある色。
	inline constexpr ENEMY_PARAMS ns_RANGED{60.0f,0.7f,0.9f,XMFLOAT4{0.6f,0.8f,1.0f,1.0f}};

	//========= 範囲攻撃型=========
	// 範囲を予告して攻撃する敵。接近型より高いHPと大きい体で、紫がかった色。
	inline constexpr ENEMY_PARAMS ns_AREA{160.0f,1.0f,1.3f,XMFLOAT4{0.85f,0.6f,1.0f,1.0f}};

	//========= ボス=========
	// 最後の部屋に1体だけ出る、近接型のボス。ここまでのどの敵よりも高いHPと大きい体で、他の種類と見分けられる濃い赤。
	// HPは、連射銃の無強化（1発10）で約60発、単発高威力銃の無強化（1発60）で10発に当たる仮値。球の半径1.5は、表示のCubeの半分（1.2）より大きい。
	inline constexpr ENEMY_PARAMS ns_BOSS{600.0f,1.5f,2.4f,XMFLOAT4{0.8f,0.25f,0.25f,1.0f}};

	//========= 取得関数=========
	// 敵の種類に対応する数値を返す。
	// kind: 種類。未定義の値は、接近型として扱う。
	// 戻り値: 対応する数値への参照。静的な定数への参照のため、寿命の制約はない。
	inline const ENEMY_PARAMS& GetParams(ENEMY_KIND kind)
	{
		// 種類ごとに対応する表を選ぶ。
		switch(kind){
			// 射撃型。
		case ENEMY_KIND::e_RANGED:
		return ns_RANGED;

		// 範囲攻撃型。
		case ENEMY_KIND::e_AREA:
		return ns_AREA;

		// ボス。
		case ENEMY_KIND::e_BOSS:
		return ns_BOSS;

		// 接近型と、未定義の値。
		default:
		return ns_MELEE;
		}
	}
}