#pragma once

// 範囲攻撃型の攻撃の予告として、床へ表示する正方形の範囲を、敵の固定枠ごとに保持して描画する。
// 範囲の中心は、予告を始めた時点で固定し、以後は動かさない。範囲の判定・敵の行動・予告の時間管理は担当しない。
// 範囲の大きさは、判定にも同じ値を使うため、AREA_TELEGRAPHにまとめる。単位はワールド距離単位。

//========= C++標準ライブラリ インクルード=========
#include <array> // 敵の固定枠ごとの予告を、枠数と同じ長さの配列で保持する。
#include <cstddef> // std::size_tで、敵の固定枠の番号を受け取る。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyWaveTable.h"

//========= 前方宣言=========
class BasicMeshRenderer;
class GraphicsSystem;
class ModelRegistry;

// 範囲の予告の大きさ・高さ・色をまとめる。
namespace AREA_TELEGRAPH
{
	//========= 範囲の大きさ=========
	// 範囲の中心から辺までの距離。範囲は、X・Z方向とも中心から±この値の正方形。単位はワールド距離単位。試遊用の仮値。
	inline constexpr float ns_HALF_SIZE = 2.0f;

	//========= 表示=========
	// 表示する板の中心のY座標。床の上面（-0.5）に板の下面が接する値。
	inline constexpr float ns_CENTER_Y = -0.475f;
	// 表示する板の厚み。単位はワールド距離単位。床と重なって見えなくならない薄さの仮値。
	inline constexpr float ns_THICKNESS = 0.05f;
	// 予告の色。赤で、Object画像の色へ乗算して、危険な範囲を示す。
	inline constexpr XMFLOAT4 ns_COLOR{1.0f,0.2f,0.2f,1.0f};
}

// 敵の固定枠ごとの範囲の予告を保持し、予告中の範囲を床へ描画する。
class AreaTelegraph final
{
public:
	//========= 初期化・終了関数=========
	// 共有のCube Rendererを借用し、全枠の予告を解除する。
	// registry: Cube Rendererの借用元。Initialize済みであること。このAreaTelegraphより長く生存させる。
	// 戻り値: Cube Rendererを借用できればtrue。Registryが未初期化ならfalse。
	[[nodiscard]] bool Initialize(ModelRegistry& registry);
	// 全枠の予告を解除し、借用した参照を手放す。未初期化でも呼出可能。
	void Uninit();
	// 全枠の予告を解除する。借用した参照は変えない。ウェーブの切替や再開で、旧ウェーブの予告を持ち越さないために使う。
	void Clear();

	//========= 更新・描画関数=========
	// 指定した枠の予告を始め、範囲の中心を固定する。すでに予告中なら、中心を置き換える。
	// slotIndex: 敵の固定枠の番号。0始まりで、枠数未満であること。範囲外なら何もしない。
	// center: 範囲の中心のワールド座標。XとZだけを使い、高さは使わない。
	void Begin(std::size_t slotIndex,const XMFLOAT3& center);
	// 指定した枠の予告を終える。予告中でなくても呼出可能。
	// slotIndex: 敵の固定枠の番号。範囲外なら何もしない。
	void End(std::size_t slotIndex);
	// 予告中の全ての範囲を、床の上の赤い板として描く。未初期化なら何も描かない。
	// graphicsSystem: Rendererが使う描画基盤。
	// viewMatrix: World空間からView空間への変換行列。
	// projectionMatrix: View空間からClip空間への変換行列。
	void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

	//========= Getter関数=========
	// 指定した枠が予告中かを返す。枠の番号が範囲外ならfalse。
	// slotIndex: 敵の固定枠の番号。
	[[nodiscard]] bool IsActive(std::size_t slotIndex) const;
	// 指定した枠の範囲の中心を返す。予告中でない枠や、範囲外の番号では、原点を返す。
	// slotIndex: 敵の固定枠の番号。
	[[nodiscard]] XMFLOAT3 GetCenter(std::size_t slotIndex) const;

private:
	//========= 内部の型=========
	// 1枠分の予告の状態。
	struct SLOT_STATE
	{
		// 範囲の中心。XとZが有効で、Yは使わない。
		XMFLOAT3 center{};
		// 予告中か。
		bool isActive{};
	};

	//========= 状態=========
	// Cube描画に使う共有Renderer。非所有で、ModelRegistryが所有する。未取得はnullptr。
	BasicMeshRenderer* m_cubeRenderer{};
	// 敵の固定枠ごとの予告。枠の番号と同じ順に並ぶ。
	std::array<SLOT_STATE,ENEMY_WAVE_TABLE::ns_SLOT_COUNT> m_slots{};
};
