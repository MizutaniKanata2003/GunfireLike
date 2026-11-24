#pragma once

// Collisionの形状・Rayなどを、色ごとの線分の端点へ変換して蓄え、DebugLineRendererで描画する。
// Collision自体の判定や、描画StateのDepth設定は担当しない。端点は毎フレームClearして作り直す使い方を想定する。

//========= C++標準ライブラリ インクルード=========
#include <vector> // 色ごとの線分の端点の列と、色ごとのまとまりを可変長配列で保持する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class CollisionWorld;
class DebugLineRenderer;
class GraphicsSystem;

// 形状を線分へ変換して蓄え、色ごとにまとめて描画する。
class CollisionDebugDrawer final
{
public:
  //========= 蓄積関数=========
  // 蓄えた線分を全て捨てる。毎フレームの描画前に呼ぶ。
  void Clear();
  // 1本の線分を追加する。
  // from: 始点のワールド座標。
  // to: 終点のワールド座標。
  // color: 線分のRGBA色。同じ色の線分は同じ描画にまとめる。
  void AddLine(const XMFLOAT3& from,const XMFLOAT3& to,const XMFLOAT4& color);
  // 球を、3つの大円（XY・YZ・XZ平面）のワイヤーとして追加する。
  // sphere: 表示する球。
  // color: 線分のRGBA色。
  void AddSphere(const COLLISION_SPHERE& sphere,const XMFLOAT4& color);
  // AABBを、12本の辺のワイヤーとして追加する。
  // aabb: 表示するAABB。
  // color: 線分のRGBA色。
  void AddAABB(const COLLISION_AABB& aabb,const XMFLOAT4& color);
  // 点の位置を示す、3軸方向の十字を追加する。
  // center: 十字の中心のワールド座標。
  // halfSize: 各軸方向の、中心から端までの長さ。単位はワールド距離単位。
  // color: 線分のRGBA色。
  void AddCross(const XMFLOAT3& center,float halfSize,const XMFLOAT4& color);
  // 登録中の全Colliderを、レイヤーごとの色で追加する。
  // world: 表示するCollisionWorld。読み取りだけを行い、参照は保持しない。
  void AddWorld(const CollisionWorld& world);

  //========= 描画関数=========
  // 蓄えた線分を、色ごとにまとめて描画する。
  // graphicsSystem: DebugLineRendererへ渡す描画基盤。Depth・Blend・Render Passは呼出側が設定する。
  // lineRenderer: 初期化済みの線描画Renderer。借用し、保持しない。
  // viewMatrix: World空間からView空間への変換行列。
  // projectionMatrix: View空間からClip空間への変換行列。
  void Draw(GraphicsSystem& graphicsSystem,DebugLineRenderer& lineRenderer,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

private:
  //========= 構造体=========
  // 同じ色の線分の端点をまとめた描画単位。
  struct LINE_BATCH
  {
    // このまとまりに共通のRGBA色。
    XMFLOAT4 color{};
    // 線分の端点の列。2点ずつが1本の線分。
    std::vector<XMFLOAT3> points{};
  };

  //========= 補助関数=========
  // 指定した色のまとまりの端点配列を返す。なければ新しく作る。
  // color: 探す色。4成分が完全に一致するまとまりを同じ色として扱う。
  // 戻り値: その色の端点配列。次の追加でこの配列が移動する場合があるため、取得後すぐに使い、保持しない。
  [[nodiscard]] std::vector<XMFLOAT3>& FindOrAddBatch(const XMFLOAT4& color);

  //========= 蓄積状態=========
  // 色ごとの描画単位。蓄積順に並ぶ。
  std::vector<LINE_BATCH> m_batches{};
};
