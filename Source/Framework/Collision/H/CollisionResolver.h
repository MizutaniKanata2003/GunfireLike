#pragma once

// 球をAABBから押し戻して、重ならない位置を求める。プレイヤーなど、動く球が壁や床を通り抜けないようにするために使う。
// 状態を持たない静的関数だけで、Colliderの登録は担当しない。登録中のAABBを読み取るだけで、CollisionWorldの内容は変更しない。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tで、対象にするレイヤーのbitのマスクを受け取る。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Collision/H/CollisionWorld.h"

// 球とAABBの重なりを解消する静的関数群。
class CollisionResolver final
{
public:
  //========= 押し戻し関数=========
  // 球がAABBと重なっている場合に、重なりを最小の移動で解消するために必要な移動量を求める。
  // sphere: 調べる球。
  // aabb: 押し戻しの基準にするAABB。
  // pushOut: 重なっている場合の出力先。球の中心へ加える移動量。重なっていない場合は変更しない。
  // 戻り値: 重なっていて移動が必要ならtrue。接するだけ、または離れている場合はfalse。
  // 中心がAABBの内側にある場合は、最も近い面の外側まで出す方向を選ぶ。
  [[nodiscard]] static bool PushOutSphereFromAABB(const COLLISION_SPHERE& sphere,const COLLISION_AABB& aabb,XMFLOAT3& pushOut);
  // 登録中の全AABBに対して、球の重なりを解消した中心の位置を求める。球のColliderは対象にしない。
  // world: 押し戻しの基準にするAABBを持つCollisionWorld。読み取りだけを行う。
  // sphere: 押し戻す球。
  // layerMask: 対象にするレイヤーのbitの組合せ。
  // 戻り値: 重なりを解消した後の球の中心。重なりがなければsphereの中心と同じ。
  // 複数のAABBの角で押し戻しが互いに影響するため、解消できなくなるまで一定回数だけ繰り返す。回数を超えた重なりは残り得る。
  [[nodiscard]] static XMFLOAT3 ResolveSphere(const CollisionWorld& world,const COLLISION_SPHERE& sphere,std::uint32_t layerMask);

private:
  //========= 生成禁止関数=========
  // 状態を持たず、静的関数だけを使用するため構築を禁止する。
  CollisionResolver() = delete;
};
