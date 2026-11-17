#include "../H/CollisionResolver.h"

// 球とAABBの押し戻し量と、複数のAABBに対する繰り返しの解消を実装する。
// 単精度floatで計算し、NaNや無限大の入力は検証しない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampで球の中心を各軸のAABB範囲へ制限して、最も近い点を求める。
#include <cmath> // std::sqrtで、AABB上の最も近い点から球の中心までの距離を求める。

// 押し戻しの計算だけで使う閾値と繰り返し回数をまとめる。
namespace
{
  //========= 判定の閾値=========
  // 球の中心からAABB上の最も近い点までの距離の2乗が、この値以下なら、中心がAABBの内側（または表面上）にあるとみなす。
  // 距離で割って方向を求める際の、0除算を避けるために使う。
  constexpr float ns_INSIDE_EPSILON = 1.0e-10f;

  //========= 繰り返し回数=========
  // 複数のAABBの押し戻しを繰り返す最大回数。壁の角で2枚の壁から同時に押される場合を解消するための回数。
  constexpr int ns_MAX_ITERATIONS = 4;
}

//========= 押し戻し関数=========
// 中心が外側にある場合は最も近い点から離す方向、内側にある場合は最も近い面の外へ出す方向に、押し戻し量を求める。
bool CollisionResolver::PushOutSphereFromAABB(const COLLISION_SPHERE& sphere,const COLLISION_AABB& aabb,XMFLOAT3& pushOut)
{
  // 球の中心を各軸でAABBの範囲へ制限し、AABB上で最も近い点を求める。中心がAABBの内側ならその中心自身になる。
  const float closest_x = std::clamp(sphere.center.x,aabb.minPoint.x,aabb.maxPoint.x);
  const float closest_y = std::clamp(sphere.center.y,aabb.minPoint.y,aabb.maxPoint.y);
  const float closest_z = std::clamp(sphere.center.z,aabb.minPoint.z,aabb.maxPoint.z);

  // 最も近い点から中心へのベクトルと、その長さの2乗を求める。
  const float delta_x = sphere.center.x - closest_x;
  const float delta_y = sphere.center.y - closest_y;
  const float delta_z = sphere.center.z - closest_z;
  const float distance_squared = delta_x * delta_x + delta_y * delta_y + delta_z * delta_z;

  // 距離が半径以上なら、重なっていない（接するだけを含む）ため、押し戻しは不要。
  if(distance_squared >= sphere.radius * sphere.radius)return false;

  // 中心がAABBの外側にある場合は、最も近い点から中心の向きへ、食い込んだ量（半径 - 距離）だけ押し出す。
  if(distance_squared > ns_INSIDE_EPSILON){
    const float distance = std::sqrt(distance_squared);
    const float push_scale = (sphere.radius - distance) / distance;
    pushOut = XMFLOAT3{delta_x * push_scale,delta_y * push_scale,delta_z * push_scale};
    return true;
  }

  // 中心がAABBの内側にある場合は、方向が定まらないため、6つの面のうち、球が外へ出るまでの移動量が最も小さい面を選ぶ。
  // 各面について、面の外側に球が接するまで中心を動かす量を求める。最小側は中心と最小値の差に半径を足した量、最大側はその逆。
  float best_distance = sphere.center.x - aabb.minPoint.x + sphere.radius;
  pushOut = XMFLOAT3{-best_distance,0.0f,0.0f};

  // +X側の面から出す場合の量を求め、これまでの最小より小さければ採用する。
  const float distance_plus_x = aabb.maxPoint.x - sphere.center.x + sphere.radius;
  // これまでの最小より小さい場合だけ、この面から出す方向へ置き換える。
  if(distance_plus_x < best_distance){
    best_distance = distance_plus_x;
    pushOut = XMFLOAT3{distance_plus_x,0.0f,0.0f};
  }

  // -Y側の面から出す場合の量を求め、これまでの最小より小さければ採用する。
  const float distance_minus_y = sphere.center.y - aabb.minPoint.y + sphere.radius;
  // これまでの最小より小さい場合だけ、この面から出す方向へ置き換える。
  if(distance_minus_y < best_distance){
    best_distance = distance_minus_y;
    pushOut = XMFLOAT3{0.0f,-distance_minus_y,0.0f};
  }

  // +Y側の面から出す場合の量を求め、これまでの最小より小さければ採用する。
  const float distance_plus_y = aabb.maxPoint.y - sphere.center.y + sphere.radius;
  // これまでの最小より小さい場合だけ、この面から出す方向へ置き換える。
  if(distance_plus_y < best_distance){
    best_distance = distance_plus_y;
    pushOut = XMFLOAT3{0.0f,distance_plus_y,0.0f};
  }

  // -Z側の面から出す場合の量を求め、これまでの最小より小さければ採用する。
  const float distance_minus_z = sphere.center.z - aabb.minPoint.z + sphere.radius;
  // これまでの最小より小さい場合だけ、この面から出す方向へ置き換える。
  if(distance_minus_z < best_distance){
    best_distance = distance_minus_z;
    pushOut = XMFLOAT3{0.0f,0.0f,-distance_minus_z};
  }

  // +Z側の面から出す場合の量を求め、これまでの最小より小さければ採用する。
  const float distance_plus_z = aabb.maxPoint.z - sphere.center.z + sphere.radius;
  // これまでの最小より小さい場合だけ、この面から出す方向へ置き換える。
  if(distance_plus_z < best_distance){
    pushOut = XMFLOAT3{0.0f,0.0f,distance_plus_z};
  }

  return true;
}

// 登録中のAABBを順に調べて押し戻し、動かなくなるか回数の上限に達するまで繰り返す。
XMFLOAT3 CollisionResolver::ResolveSphere(const CollisionWorld& world,const COLLISION_SPHERE& sphere,std::uint32_t layerMask)
{
  // 押し戻しながら更新する球。元の引数は変更しない。
  COLLISION_SPHERE resolved_sphere = sphere;

  // 解消の繰り返し。1回の走査で、全AABBに対する押し戻しを順に適用する。
  for(int iteration_index = 0; iteration_index < ns_MAX_ITERATIONS; ++iteration_index){
    // この走査で、1回でも押し戻したかを記録する。
    bool has_moved = false;

    // 登録順に全Colliderを調べ、対象レイヤーのAABBだけ押し戻す。
    for(const COLLIDER_ENTRY& entry : world.GetColliders()){
      // 対象外のレイヤー、またはAABBではないColliderは、押し戻しの対象にしないため次へ進む。
      if((entry.layer & layerMask) == 0 || entry.shapeType != COLLISION_SHAPE_TYPE::e_AABB)continue;

      // 押し戻し量の出力先を用意する。
      XMFLOAT3 push_out{};
      // 重なっている場合だけ、球の中心へ押し戻し量を加える。後続のAABBは、押し戻し後の位置で判定する。
      if(PushOutSphereFromAABB(resolved_sphere,entry.aabb,push_out)){
        resolved_sphere.center.x += push_out.x;
        resolved_sphere.center.y += push_out.y;
        resolved_sphere.center.z += push_out.z;
        has_moved = true;
      }
    }

    // 1回も押し戻さなかったなら、全AABBとの重なりが解消されているため、繰り返しを終える。
    if(!has_moved)break;
  }

  return resolved_sphere.center;
}
