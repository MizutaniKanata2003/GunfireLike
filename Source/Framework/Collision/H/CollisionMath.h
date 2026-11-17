#pragma once

// 球・軸整列AABB・Ray同士の交差判定と、Rayの命中距離・命中点・法線の計算を提供する。
// 状態を持たない静的関数だけで、Colliderの登録・レイヤー選択・描画は担当しない。引数の有限値や形状の正しさは検証しない。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"

// 形状同士の交差判定とRayの命中計算をまとめた静的関数群。
class CollisionMath final
{
public:
  //========= 交差判定関数=========
  // 球と球が重なっているかを返す。接する場合も重なりとする。
  // sphereA: 1つ目の球。
  // sphereB: 2つ目の球。
  [[nodiscard]] static bool IntersectSphereSphere(const COLLISION_SPHERE& sphereA,const COLLISION_SPHERE& sphereB);
  // 球とAABBが重なっているかを返す。接する場合も重なりとする。
  // sphere: 球。
  // aabb: AABB。
  [[nodiscard]] static bool IntersectSphereAABB(const COLLISION_SPHERE& sphere,const COLLISION_AABB& aabb);
  // AABBとAABBが重なっているかを返す。接する場合も重なりとする。
  // aabbA: 1つ目のAABB。
  // aabbB: 2つ目のAABB。
  [[nodiscard]] static bool IntersectAABBAABB(const COLLISION_AABB& aabbA,const COLLISION_AABB& aabbB);

  //========= Ray命中関数=========
  // Rayが球へ命中するかを調べ、命中する場合は最初の命中を返す。
  // ray: Ray。directionは単位ベクトルであること。
  // sphere: 対象の球。
  // maxDistance: 命中を調べる最大距離。始点からの距離がこれを超える命中は無視する。負の値では命中しない。
  // hit: 命中した場合の出力先。命中しない場合は変更しない。
  // 戻り値: 命中すればtrue。始点が球の内側ならdistanceが0の命中として扱う。
  [[nodiscard]] static bool RaycastSphere(const COLLISION_RAY& ray,const COLLISION_SPHERE& sphere,float maxDistance,COLLISION_RAY_HIT& hit);
  // RayがAABBへ命中するかを調べ、命中する場合は最初の命中を返す。
  // ray: Ray。directionは単位ベクトルであること。
  // aabb: 対象のAABB。
  // maxDistance: 命中を調べる最大距離。始点からの距離がこれを超える命中は無視する。負の値では命中しない。
  // hit: 命中した場合の出力先。命中しない場合は変更しない。
  // 戻り値: 命中すればtrue。始点がAABBの内側ならdistanceが0の命中として扱う。
  [[nodiscard]] static bool RaycastAABB(const COLLISION_RAY& ray,const COLLISION_AABB& aabb,float maxDistance,COLLISION_RAY_HIT& hit);

private:
  //========= 生成禁止関数=========
  // 状態を持たず、静的関数だけを使用するため構築を禁止する。
  CollisionMath() = delete;
};
