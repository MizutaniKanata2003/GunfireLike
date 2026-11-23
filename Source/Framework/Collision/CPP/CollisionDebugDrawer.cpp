#include "../H/CollisionDebugDrawer.h"

// 球・AABB・十字を線分の端点へ変換して色ごとに蓄え、DebugLineRendererへ渡して描画する。
// 線分の生成は表示用の近似で、判定の精度には影響しない。

//========= C++標準ライブラリ インクルード=========
#include <cmath> // std::cos・std::sinで球の大円の頂点座標を、角度から求める。
#include <cstdint> // std::uint32_tでレイヤーのbitと色の対応表を保持する。

//========= Debug インクルード=========
#include "Debug/H/DebugLineRenderer.h"

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionWorld.h"

// 表示の形状・色に関する固定設定をまとめる。
namespace
{
	//========= 球の表示設定=========
	// 球の1つの大円を分割する線分の数。大きいほど滑らかになる。
	constexpr int ns_SPHERE_SEGMENT_COUNT = 16;
	// 円周率。大円の角度（ラジアン）を求めるために使う。
	constexpr float ns_PI = 3.14159265358979f;

	//========= AABBの表示設定=========
	// AABBの辺の数。
	constexpr int ns_BOX_EDGE_COUNT = 12;
	// 各辺の2端点を、角の番号で表す対応表。角の番号のbit0がX、bit1がY、bit2がZを表し、1なら最大側、0なら最小側の座標を使う。
	// どの辺も、2つの角の番号が1bitだけ異なる（X方向・Y方向・Z方向の辺が各4本）。
	constexpr int ns_BOX_EDGES[ns_BOX_EDGE_COUNT][2]
	{
		{0,1},{2,3},{4,5},{6,7},
		{0,2},{1,3},{4,6},{5,7},
		{0,4},{1,5},{2,6},{3,7}
	};

	//========= レイヤーの表示色=========
	// レイヤーのbitと、表示色の対応1件分。
	struct LAYER_COLOR
	{
		// 対応するレイヤーのbit。
		std::uint32_t layer;
		// 表示するRGBA色。
		XMFLOAT4 color;
	};

	// レイヤーごとの表示色。上から順に調べ、最初に一致した色を使う。
	constexpr LAYER_COLOR ns_LAYER_COLORS[]
	{
		{COLLISION_LAYER::ns_STATIC,{0.2f,0.9f,0.2f,1.0f}},
		{COLLISION_LAYER::ns_PLAYER,{0.2f,0.6f,1.0f,1.0f}},
		{COLLISION_LAYER::ns_ENEMY,{1.0f,0.2f,0.2f,1.0f}},
		{COLLISION_LAYER::ns_PLAYER_ATTACK,{1.0f,0.8f,0.1f,1.0f}},
		{COLLISION_LAYER::ns_ENEMY_ATTACK,{1.0f,0.4f,0.9f,1.0f}}
	};

	// どのレイヤーにも一致しないColliderを表示する色。白。
	constexpr XMFLOAT4 ns_UNKNOWN_LAYER_COLOR{1.0f,1.0f,1.0f,1.0f};
}

//========= 蓄積関数=========
// 蓄えた線分を全て捨てる。
void CollisionDebugDrawer::Clear()
{
	m_batches.clear();
}

// 1本の線分を、色のまとまりへ追加する。
void CollisionDebugDrawer::AddLine(const XMFLOAT3& from,const XMFLOAT3& to,const XMFLOAT4& color)
{
	std::vector<XMFLOAT3>& points = FindOrAddBatch(color);
	points.push_back(from);
	points.push_back(to);
}

// 3つの大円を、分割した線分として追加する。
void CollisionDebugDrawer::AddSphere(const COLLISION_SPHERE& sphere,const XMFLOAT4& color)
{
	// 色のまとまりを取得する。この後に別のまとまりを追加しないため、参照を使い続けてよい。
	std::vector<XMFLOAT3>& points = FindOrAddBatch(color);

	// 円を分割した各線分について、始点と終点の角度から座標を求め、3つの平面の大円へ追加する。
	for(int segment_index = 0; segment_index < ns_SPHERE_SEGMENT_COUNT; ++segment_index){
		// 線分の始点と終点の角度。円周（2π）をns_SPHERE_SEGMENT_COUNT等分した値。単位はラジアン。
		const float start_angle = 2.0f * ns_PI * static_cast<float>(segment_index) / static_cast<float>(ns_SPHERE_SEGMENT_COUNT);
		const float end_angle = 2.0f * ns_PI * static_cast<float>(segment_index + 1) / static_cast<float>(ns_SPHERE_SEGMENT_COUNT);

		// 角度から、半径1の円上の座標（cosが横、sinが縦）を求める。半径は後で掛ける。
		const float start_cos = std::cos(start_angle);
		const float start_sin = std::sin(start_angle);
		const float end_cos = std::cos(end_angle);
		const float end_sin = std::sin(end_angle);
		const XMFLOAT3& center = sphere.center;
		const float radius = sphere.radius;

		// XY平面（Zが一定）の大円。
		points.push_back(XMFLOAT3{center.x + start_cos * radius,center.y + start_sin * radius,center.z});
		points.push_back(XMFLOAT3{center.x + end_cos * radius,center.y + end_sin * radius,center.z});

		// XZ平面（Yが一定）の大円。
		points.push_back(XMFLOAT3{center.x + start_cos * radius,center.y,center.z + start_sin * radius});
		points.push_back(XMFLOAT3{center.x + end_cos * radius,center.y,center.z + end_sin * radius});

		// YZ平面（Xが一定）の大円。
		points.push_back(XMFLOAT3{center.x,center.y + start_cos * radius,center.z + start_sin * radius});
		points.push_back(XMFLOAT3{center.x,center.y + end_cos * radius,center.z + end_sin * radius});
	}
}

// 8個の角から12本の辺を作って追加する。
void CollisionDebugDrawer::AddAABB(const COLLISION_AABB& aabb,const XMFLOAT4& color)
{
	// 色のまとまりを取得する。
	std::vector<XMFLOAT3>& points = FindOrAddBatch(color);

	// 角の番号0〜7から、角のワールド座標を求めて並べる。番号のbit0・bit1・bit2が1なら、X・Y・Zの最大側の値を使う。
	XMFLOAT3 corners[8]{};
	// 8個の角を順に計算する。
	for(int corner_index = 0; corner_index < 8; ++corner_index){
		// 論理積でbitを取り出し、0以外なら最大側、0なら最小側の座標を選ぶ。
		corners[corner_index].x = (corner_index & 1) != 0 ? aabb.maxPoint.x : aabb.minPoint.x;
		corners[corner_index].y = (corner_index & 2) != 0 ? aabb.maxPoint.y : aabb.minPoint.y;
		corners[corner_index].z = (corner_index & 4) != 0 ? aabb.maxPoint.z : aabb.minPoint.z;
	}

	// 対応表の各辺について、2つの角の座標を線分の端点として追加する。
	for(int edge_index = 0; edge_index < ns_BOX_EDGE_COUNT; ++edge_index){
		points.push_back(corners[ns_BOX_EDGES[edge_index][0]]);
		points.push_back(corners[ns_BOX_EDGES[edge_index][1]]);
	}
}

// 中心から3軸の正負方向へ、halfSizeの長さの線を3本追加する。
void CollisionDebugDrawer::AddCross(const XMFLOAT3& center,float halfSize,const XMFLOAT4& color)
{
	// 色のまとまりを取得し、X軸・Y軸・Z軸それぞれの方向に、中心をはさむ線を1本ずつ追加する。
	std::vector<XMFLOAT3>& points = FindOrAddBatch(color);
	points.push_back(XMFLOAT3{center.x - halfSize,center.y,center.z});
	points.push_back(XMFLOAT3{center.x + halfSize,center.y,center.z});
	points.push_back(XMFLOAT3{center.x,center.y - halfSize,center.z});
	points.push_back(XMFLOAT3{center.x,center.y + halfSize,center.z});
	points.push_back(XMFLOAT3{center.x,center.y,center.z - halfSize});
	points.push_back(XMFLOAT3{center.x,center.y,center.z + halfSize});
}

// 登録中の全Colliderを、レイヤーの色で形状に応じて追加する。
void CollisionDebugDrawer::AddWorld(const CollisionWorld& world)
{
	// 登録順に全Colliderを調べる。
	for(const COLLIDER_ENTRY& entry : world.GetColliders()){
		// 色の初期値は未対応レイヤー用の白。対応表を上から調べ、レイヤーのbitが一致した最初の色で置き換える。
		XMFLOAT4 color = ns_UNKNOWN_LAYER_COLOR;
		// 対応表の全項目を調べ、最初に一致した項目で打ち切る。
		for(const LAYER_COLOR& layer_color : ns_LAYER_COLORS){
			// Colliderのレイヤーのbitが、この項目のbitと一致すれば、その色を採用して調べ終える。
			if((entry.layer & layer_color.layer) != 0){
				color = layer_color.color;
				break;
			}
		}

		// 形状の種類に応じて、球またはAABBのワイヤーを追加する。
		switch(entry.shapeType){
			// 球はワイヤーの大円として追加する。
		case COLLISION_SHAPE_TYPE::e_SPHERE:
		AddSphere(entry.sphere,color);
		break;

		// AABBは12本の辺として追加する。
		case COLLISION_SHAPE_TYPE::e_AABB:
		AddAABB(entry.aabb,color);
		break;
		}
	}
}

//========= 描画関数=========
// 色ごとのまとまりを順に、線描画Rendererへ渡す。
void CollisionDebugDrawer::Draw(GraphicsSystem& graphicsSystem,DebugLineRenderer& lineRenderer,const XMMATRIX& viewMatrix,
																const XMMATRIX& projectionMatrix) const
{
	// 蓄えた全ての色のまとまりを順に描画する。色ごとに1回の描画になる。
	for(const LINE_BATCH& batch : m_batches){
		lineRenderer.DrawLines(graphicsSystem,batch.points,batch.color,viewMatrix,projectionMatrix);
	}
}

//========= 補助関数=========
// 指定した色のまとまりを探し、なければ末尾に作って、その端点配列を返す。
std::vector<XMFLOAT3>& CollisionDebugDrawer::FindOrAddBatch(const XMFLOAT4& color)
{
	// 既存のまとまりを先頭から調べる。色の4成分が完全に一致するものを同じ色とする。同じ定数から渡す前提のため、誤差は考慮しない。
	for(LINE_BATCH& batch : m_batches){
		// 4成分が全て一致する場合だけ、そのまとまりの端点配列を返す。
		if(batch.color.x == color.x && batch.color.y == color.y && batch.color.z == color.z && batch.color.w == color.w){
			return batch.points;
		}
	}

	// 一致するまとまりがなければ、この色の新しいまとまりを末尾に追加して返す。
	LINE_BATCH new_batch{};
	new_batch.color = color;
	m_batches.push_back(new_batch);
	return m_batches.back().points;
}
