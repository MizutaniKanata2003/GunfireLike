#include "../H/CollisionWorld.h"

// Colliderの登録・更新・解除と、Ray・球による問い合わせを実装する。
// 全Colliderを順に調べるため、登録数が増えた場合の高速化（空間分割など）は行わない。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionMath.h"

//========= 登録・解除関数=========
// 球を登録し、新しい識別値を割り当てる。
ColliderHandle CollisionWorld::AddSphere(const COLLISION_SPHERE& sphere,std::uint32_t layer,std::uint32_t userData)
{
  // 識別値・分類・形状をまとめた登録情報を作り、登録順の末尾へ追加する。
  COLLIDER_ENTRY entry{};
  entry.handle = m_nextHandle;
  entry.userData = userData;
  entry.layer = layer;
  entry.shapeType = COLLISION_SHAPE_TYPE::e_SPHERE;
  entry.sphere = sphere;
  m_colliders.push_back(entry);

  // 割り当てた識別値を返し、次の識別値へ進める。解除後も同じ値を再利用しない。
  return m_nextHandle++;
}

// AABBを登録し、新しい識別値を割り当てる。
ColliderHandle CollisionWorld::AddAABB(const COLLISION_AABB& aabb,std::uint32_t layer,std::uint32_t userData)
{
  // 識別値・分類・形状をまとめた登録情報を作り、登録順の末尾へ追加する。
  COLLIDER_ENTRY entry{};
  entry.handle = m_nextHandle;
  entry.userData = userData;
  entry.layer = layer;
  entry.shapeType = COLLISION_SHAPE_TYPE::e_AABB;
  entry.aabb = aabb;
  m_colliders.push_back(entry);

  // 割り当てた識別値を返し、次の識別値へ進める。解除後も同じ値を再利用しない。
  return m_nextHandle++;
}

// 識別値に対応するColliderを探して取り除く。
bool CollisionWorld::Remove(ColliderHandle handle)
{
  // 識別値から位置を求める。見つからなければ何も変更せずfalseを返す。
  const std::size_t index = FindIndex(handle);
  // 位置が登録数と同じなら未登録または解除済みのため、何も取り除かずに戻る。
  if(index == m_colliders.size())return false;

  // 登録順を保ったまま、該当のColliderだけを取り除く。
  m_colliders.erase(m_colliders.begin() + static_cast<std::ptrdiff_t>(index));
  return true;
}

// 登録を全て取り除く。識別値の採番は戻さない。
void CollisionWorld::Clear()
{
  m_colliders.clear();
}

//========= 更新関数=========
// 球のColliderの形状だけを置き換える。
bool CollisionWorld::UpdateSphere(ColliderHandle handle,const COLLISION_SPHERE& sphere)
{
  // 識別値から位置を求める。
  const std::size_t index = FindIndex(handle);
  // 未登録なら更新せず戻る。
  if(index == m_colliders.size())return false;

  // 形状が球以外のColliderは、有効な形状のメンバーが異なるため更新しない。
  if(m_colliders[index].shapeType != COLLISION_SHAPE_TYPE::e_SPHERE)return false;

  m_colliders[index].sphere = sphere;
  return true;
}

// AABBのColliderの形状だけを置き換える。
bool CollisionWorld::UpdateAABB(ColliderHandle handle,const COLLISION_AABB& aabb)
{
  // 識別値から位置を求める。
  const std::size_t index = FindIndex(handle);
  // 未登録なら更新せず戻る。
  if(index == m_colliders.size())return false;

  // 形状がAABB以外のColliderは、有効な形状のメンバーが異なるため更新しない。
  if(m_colliders[index].shapeType != COLLISION_SHAPE_TYPE::e_AABB)return false;

  m_colliders[index].aabb = aabb;
  return true;
}

//========= 問い合わせ関数=========
// 対象レイヤーの全Colliderへ順にRayを当て、最も近い命中を採用する。
bool CollisionWorld::Raycast(const COLLISION_RAY_QUERY& query,COLLISION_RAY_HIT& rayHit,COLLISION_HIT_INFO& hitInfo) const
{
  // 命中の有無と、現時点で最も近い命中を保持する。出力先は、最後に命中が確定した場合だけ書き込む。
  bool has_hit = false;
  COLLISION_RAY_HIT nearest_ray_hit{};
  COLLISION_HIT_INFO nearest_hit_info{};

  // 登録順に全Colliderを調べる。同じ距離の命中は、先に登録したものを残す。
  for(const COLLIDER_ENTRY& entry : m_colliders){
    // マスクに含まれないレイヤーのColliderは、形状の判定を行わず次へ進む。
    if((entry.layer & query.layerMask) == 0)continue;

    // このColliderへの命中結果の出力先と、命中の有無を用意する。
    COLLISION_RAY_HIT candidate{};
    bool is_hit = false;
    // 形状の種類に応じて、このColliderへのRayの命中を調べる。
    switch(entry.shapeType){
      // 球のColliderは、球との命中計算を使う。
    case COLLISION_SHAPE_TYPE::e_SPHERE:
    is_hit = CollisionMath::RaycastSphere(query.ray,entry.sphere,query.maxDistance,candidate);
    break;

    // AABBのColliderは、AABBとの命中計算を使う。
    case COLLISION_SHAPE_TYPE::e_AABB:
    is_hit = CollisionMath::RaycastAABB(query.ray,entry.aabb,query.maxDistance,candidate);
    break;
    }

    // 命中し、かつこれまでの最短より厳密に近い場合だけ、最短の命中を置き換える。
    if(is_hit && (!has_hit || candidate.distance < nearest_ray_hit.distance)){
      has_hit = true;
      nearest_ray_hit = candidate;
      nearest_hit_info.handle = entry.handle;
      nearest_hit_info.layer = entry.layer;
      nearest_hit_info.userData = entry.userData;
    }
  }

  // 命中がなければ出力先を変更せず、命中が確定した場合だけ結果をコピーして返す。
  if(!has_hit)return false;

  rayHit = nearest_ray_hit;
  hitInfo = nearest_hit_info;
  return true;
}

// 対象レイヤーの全Colliderのうち、球と重なるものを登録順に集める。
void CollisionWorld::OverlapSphere(const COLLISION_SPHERE& sphere,std::uint32_t layerMask,std::vector<COLLISION_HIT_INFO>& hits) const
{
  // 前回の結果を残さないよう、出力先を空にする。
  hits.clear();

  // 登録順に全Colliderを調べ、重なったものを追加する。
  for(const COLLIDER_ENTRY& entry : m_colliders){
    // マスクに含まれないレイヤーのColliderは、形状の判定を行わず次へ進む。
    if((entry.layer & layerMask) == 0)continue;

    // 重なりの有無を用意する。
    bool is_overlapping = false;
    // 形状の種類に応じて、球との重なりを調べる。
    switch(entry.shapeType){
      // 球のColliderは、球と球の判定を使う。
    case COLLISION_SHAPE_TYPE::e_SPHERE:
    is_overlapping = CollisionMath::IntersectSphereSphere(sphere,entry.sphere);
    break;

    // AABBのColliderは、球とAABBの判定を使う。
    case COLLISION_SHAPE_TYPE::e_AABB:
    is_overlapping = CollisionMath::IntersectSphereAABB(sphere,entry.aabb);
    break;
    }

    // 重なっている場合だけ、このColliderの情報を結果へ追加する。
    if(is_overlapping){
      hits.push_back(COLLISION_HIT_INFO{entry.handle,entry.layer,entry.userData});
    }
  }
}

//========= 補助関数=========
// 登録順に先頭から調べ、識別値が一致する最初の位置を返す。
std::size_t CollisionWorld::FindIndex(ColliderHandle handle) const
{
  // 先頭から末尾まで走査し、識別値が一致したColliderの位置を返す。識別値は一意のため、最初の一致で終える。
  for(std::size_t index = 0; index < m_colliders.size(); ++index){
    // 識別値が一致した位置が、探しているColliderの位置。
    if(m_colliders[index].handle == handle)return index;
  }

  // 一致するものがなければ、登録数と同じ値を「見つからない」の印として返す。
  return m_colliders.size();
}