#include "../H/CollisionMath.h"

// 球・AABB・Rayの交差判定と、Rayの命中計算を実装する。
// 計算は単精度floatで行い、NaNや無限大の入力、方向ベクトルの長さの検証はしない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampで球の中心を各軸のAABB範囲へ制限し、std::swapでslabの近い側と遠い側を入れ替える。
#include <cmath> // std::fabsで方向成分の大きさを調べ、std::sqrtで球とRayの交点の距離を求める。

// Rayの命中計算だけで使う補助関数・構造体・定数をまとめる。
namespace
{
  //========= 判定の閾値=========
  // この値より絶対値が小さいRayの方向成分を、軸と平行とみなす。1で割ると値が極端に大きくなるため、除算の前に分ける。
  constexpr float ns_PARALLEL_EPSILON = 1.0e-8f;

  //========= 構造体=========
  // 1軸分の板（slab）と、その軸でのRayの成分。ClipSlabの入力で、ClipSlabは変更しない。
  struct SLAB_AXIS
  {
    // Rayの始点のこの軸の成分。
    float origin{};
    // Rayの方向のこの軸の成分。
    float direction{};
    // AABBのこの軸の最小値。
    float slabMin{};
    // AABBのこの軸の最大値。
    float slabMax{};
    // この軸の番号。0=X、1=Y、2=Z。入口を更新したとき、入口の面の軸として記録する。
    int axisIndex{};
  };

  // 全軸で重なる区間と、入口の面の情報。ClipSlabが軸ごとに絞り込み、更新する。
  struct SLAB_INTERVAL
  {
    // 全軸で重なる区間の入口の距離。入口が遠くなる場合に更新する。
    float nearDistance{};
    // 全軸で重なる区間の出口の距離。出口が近くなる場合に更新する。
    float farDistance{};
    // 最後に入口を更新した軸の番号。未確定は-1。入口を更新した場合だけ書き換える。
    int hitAxis{-1};
    // 入口の面の法線の向き。最小側の面から入るなら-1、最大側の面から入るなら+1。入口を更新した場合だけ書き換える。
    float hitSign{};
  };

  //========= 補助関数=========
  // 1軸分の板（slab）とRayが重なる区間を求め、AABB全体で重なる区間を絞り込む。
  // slab: この軸の板と、Rayのこの軸の成分。
  // interval: 全軸で重なる区間と、入口の面の情報。軸ごとに更新する。
  // 戻り値: 区間がまだ空でなければtrue。空になればfalse。
  bool ClipSlab(const SLAB_AXIS& slab,SLAB_INTERVAL& interval)
  {
    // 方向成分が0に近い場合、この軸では距離が進んでも位置が変わらない。始点がslabの内側にあるかだけで判定する。
    if(std::fabs(slab.direction) < ns_PARALLEL_EPSILON){
      return slab.origin >= slab.slabMin && slab.origin <= slab.slabMax;
    }

    // slabの2つの面までの距離を求める。位置 = 始点 + 方向 * 距離 を解いた値で、符号は進行方向に依存する。
    const float inverse_direction = 1.0f / slab.direction;
    float slab_near = (slab.slabMin - slab.origin) * inverse_direction;
    float slab_far = (slab.slabMax - slab.origin) * inverse_direction;
    float entry_sign = -1.0f;

    // 方向が負だと最大側の面が先に交差するため、近い側と遠い側を入れ替える。このとき入口は最大側の面で、法線は+向きになる。
    if(slab_near > slab_far){
      std::swap(slab_near,slab_far);
      entry_sign = 1.0f;
    }

    // この軸の入口が、これまでの入口より遠い場合だけ入口を更新し、入口の面の軸と法線の向きを記録する。
    if(slab_near > interval.nearDistance){
      interval.nearDistance = slab_near;
      interval.hitAxis = slab.axisIndex;
      interval.hitSign = entry_sign;
    }

    // この軸の出口が、これまでの出口より近い場合だけ出口を更新する。
    if(slab_far < interval.farDistance){
      interval.farDistance = slab_far;
    }

    // 入口が出口を超えていれば、全軸で同時に重なる区間はなく、命中しない。
    return interval.nearDistance <= interval.farDistance;
  }
}

//========= 交差判定関数=========
// 中心間の距離の2乗と、半径の和の2乗を比べる。平方根を使わない。
bool CollisionMath::IntersectSphereSphere(const COLLISION_SPHERE& sphereA,const COLLISION_SPHERE& sphereB)
{
  // 中心間の各軸の差と、その2乗の和（距離の2乗）を求める。
  const float delta_x = sphereA.center.x - sphereB.center.x;
  const float delta_y = sphereA.center.y - sphereB.center.y;
  const float delta_z = sphereA.center.z - sphereB.center.z;
  const float distance_squared = delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;

  // 半径の和の2乗以下なら重なっている。接する場合も重なりとする。
  const float radius_sum = sphereA.radius + sphereB.radius;
  return distance_squared <= radius_sum * radius_sum;
}

// 球の中心に最も近いAABB上の点を求め、その点までの距離の2乗と半径の2乗を比べる。
bool CollisionMath::IntersectSphereAABB(const COLLISION_SPHERE& sphere,const COLLISION_AABB& aabb)
{
  // 球の中心を各軸でAABBの範囲へ制限し、AABB上で最も近い点を求める。中心がAABBの内側ならその中心自身になる。
  const float closest_x = std::clamp(sphere.center.x,aabb.minPoint.x,aabb.maxPoint.x);
  const float closest_y = std::clamp(sphere.center.y,aabb.minPoint.y,aabb.maxPoint.y);
  const float closest_z = std::clamp(sphere.center.z,aabb.minPoint.z,aabb.maxPoint.z);

  // 中心から最も近い点までの距離の2乗を求める。
  const float delta_x = sphere.center.x - closest_x;
  const float delta_y = sphere.center.y - closest_y;
  const float delta_z = sphere.center.z - closest_z;
  const float distance_squared = delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;

  // 半径の2乗以下なら重なっている。接する場合も重なりとする。
  return distance_squared <= sphere.radius * sphere.radius;
}

// 全ての軸で範囲が重なっていれば、AABB同士が重なっている。
bool CollisionMath::IntersectAABBAABB(const COLLISION_AABB& aabbA,const COLLISION_AABB& aabbB)
{
  // X・Y・Zの3軸全てで、一方の最小値が他方の最大値以下であれば重なっている。接する場合も重なりとする。
  return aabbA.minPoint.x <= aabbB.maxPoint.x && aabbA.maxPoint.x >= aabbB.minPoint.x &&
    aabbA.minPoint.y <= aabbB.maxPoint.y && aabbA.maxPoint.y >= aabbB.minPoint.y &&
    aabbA.minPoint.z <= aabbB.maxPoint.z && aabbA.maxPoint.z >= aabbB.minPoint.z;
}

//========= Ray命中関数=========
// Rayの式 位置 = 始点 + 方向 * 距離 と球の式を連立し、距離についての2次方程式の小さい方の解を命中距離にする。
bool CollisionMath::RaycastSphere(const COLLISION_RAY& ray,const COLLISION_SPHERE& sphere,float maxDistance,COLLISION_RAY_HIT& hit)
{
  // 始点から球の中心への向きの逆（中心から始点へのベクトル）を求める。
  const float to_origin_x = ray.origin.x - sphere.center.x;
  const float to_origin_y = ray.origin.y - sphere.center.y;
  const float to_origin_z = ray.origin.z - sphere.center.z;

  // 2次方程式の係数を求める。方向は単位ベクトルのため、距離の2乗の係数は1になる。
  // half_b: 中心から始点へのベクトルと方向の内積。負なら球はRayの進む側にある。
  // constant_term: 始点から中心までの距離の2乗から、半径の2乗を引いた値。正なら始点は球の外側にある。
  const float half_b = to_origin_x * ray.direction.x + to_origin_y * ray.direction.y + to_origin_z * ray.direction.z;
  const float constant_term = to_origin_x * to_origin_x + to_origin_y * to_origin_y + to_origin_z * to_origin_z - sphere.radius * sphere.radius;

  // 始点が球の外側で、Rayが球から遠ざかる向きなら、どの距離でも命中しない。
  if(constant_term > 0.0f && half_b > 0.0f)return false;

  // 判別式。負なら方程式に実数解がなく、Rayは球を外れる。
  const float discriminant = half_b * half_b - constant_term;
  // 判別式が負なら、Rayは球に届かない。平方根を取る前に戻る。
  if(discriminant < 0.0f)return false;

  // 小さい方の解を命中距離にする。始点が球の内側だと負になるため、その場合は距離0（始点での命中）にそろえる。
  const float hit_distance = std::max(0.0f,-half_b - std::sqrt(discriminant));
  // 命中距離が最大距離を超える場合は、範囲外として命中しない。
  if(hit_distance > maxDistance)return false;

  // 命中点を求める。
  hit.distance = hit_distance;
  hit.point = XMFLOAT3{ray.origin.x + ray.direction.x * hit_distance,ray.origin.y + ray.direction.y * hit_distance,
    ray.origin.z + ray.direction.z * hit_distance};

  // 命中点を半径で割り、球の外向き法線（長さ1）を求める。半径が0以下なら割れないため、Rayと逆向きにする。
  // 始点が球の内側（距離0）の場合も、命中点が始点自身であり、中心からの向きが不定になりえるため、Rayと逆向きにする。
  if(sphere.radius > 0.0f && hit_distance > 0.0f){
    hit.normal = XMFLOAT3{(hit.point.x - sphere.center.x) / sphere.radius,(hit.point.y - sphere.center.y) / sphere.radius,
      (hit.point.z - sphere.center.z) / sphere.radius};
  }
  // 法線を決められない場合は、進行方向と逆向きを外向きとして使う。
  else{
    hit.normal = XMFLOAT3{-ray.direction.x,-ray.direction.y,-ray.direction.z};
  }

  return true;
}

// 3軸のslabそれぞれの重なる区間を順に絞り込み、最後に残った区間の入口を命中距離にする（slab法）。
bool CollisionMath::RaycastAABB(const COLLISION_RAY& ray,const COLLISION_AABB& aabb,float maxDistance,COLLISION_RAY_HIT& hit)
{
  // 重なる区間の初期値。入口は始点（距離0）、出口は最大距離。入口の面は未確定（軸番号-1、向き0）にする。
  SLAB_INTERVAL interval{};
  interval.nearDistance = 0.0f;
  interval.farDistance = maxDistance;

  // 各軸のRayの成分と、AABBの範囲を、軸ごとにまとめる。
  const SLAB_AXIS slab_x{ray.origin.x,ray.direction.x,aabb.minPoint.x,aabb.maxPoint.x,0};
  const SLAB_AXIS slab_y{ray.origin.y,ray.direction.y,aabb.minPoint.y,aabb.maxPoint.y,1};
  const SLAB_AXIS slab_z{ray.origin.z,ray.direction.z,aabb.minPoint.z,aabb.maxPoint.z,2};

  // X・Y・Zの順に区間を絞り込む。途中で区間が空になった軸があれば、以降の軸は調べずに命中なしとして戻る（短絡評価）。
  if(!ClipSlab(slab_x,interval) || !ClipSlab(slab_y,interval) || !ClipSlab(slab_z,interval)){
    return false;
  }

  // 全軸が平行で始点が内側にあり、最大距離が負の場合は、区間の更新が行われないため、最後に入口と出口の関係を確認する。
  if(interval.nearDistance > interval.farDistance)return false;

  // 命中距離と命中点を求める。入口を更新しなかった場合（始点が内側）は、距離0で始点自身が命中点になる。
  hit.distance = interval.nearDistance;
  hit.point = XMFLOAT3{ray.origin.x + ray.direction.x * interval.nearDistance,ray.origin.y + ray.direction.y * interval.nearDistance,
    ray.origin.z + ray.direction.z * interval.nearDistance};

  // 入口の面の軸に応じて、外向き法線を決める。符号は入口がどちら側の面かで決まる。
  switch(interval.hitAxis){
    // X軸の面から入った場合の法線。
  case 0:
  hit.normal = XMFLOAT3{interval.hitSign,0.0f,0.0f};
  break;

  // Y軸の面から入った場合の法線。
  case 1:
  hit.normal = XMFLOAT3{0.0f,interval.hitSign,0.0f};
  break;

  // Z軸の面から入った場合の法線。
  case 2:
  hit.normal = XMFLOAT3{0.0f,0.0f,interval.hitSign};
  break;

  // 入口を更新していない（始点がAABBの内側）場合は、進行方向と逆向きを法線として使う。
  default:
  hit.normal = XMFLOAT3{-ray.direction.x,-ray.direction.y,-ray.direction.z};
  break;
  }

  return true;
}