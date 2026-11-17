#pragma once

// 衝突判定に使う形状（球・軸整列AABB・Ray）、判定対象を分けるレイヤー、問い合わせ結果の型を定義する。
// 判定の計算はCollisionMath、登録と問い合わせはCollisionWorldが担当する。この型は描画やGameの進行を担当しない。
// 座標は全てワールド空間で、距離の単位はワールド距離単位。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tでColliderの識別値とレイヤーのbitを保持する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Colliderの識別値=========
// 登録したColliderを指す識別値。0は無効値で、登録のたびに1から増える値を割り当て、解除後も再利用しない。
using ColliderHandle = std::uint32_t;

// 無効なColliderHandleに関する定数をまとめる。
namespace COLLIDER_HANDLE
{
  //========= 無効値=========
  // どのColliderも指さない識別値。登録に使う値としては割り当てない。
  inline constexpr ColliderHandle ns_INVALID = 0;
}

// Colliderが属する分類のbitをまとめる。Colliderは1個だけ立てたbitを持ち、問い合わせは複数bitを組み合わせたマスクで対象を選ぶ。
namespace COLLISION_LAYER
{
  //========= レイヤーのbit=========
  // 床・壁など動かない地形。
  inline constexpr std::uint32_t ns_STATIC = 1u << 0;
  // プレイヤー本体。
  inline constexpr std::uint32_t ns_PLAYER = 1u << 1;
  // 敵本体。
  inline constexpr std::uint32_t ns_ENEMY = 1u << 2;
  // プレイヤーの攻撃判定。
  inline constexpr std::uint32_t ns_PLAYER_ATTACK = 1u << 3;
  // 敵の攻撃判定。
  inline constexpr std::uint32_t ns_ENEMY_ATTACK = 1u << 4;

  //========= マスク=========
  // 全レイヤーを対象にするマスク。32bit全てが1。
  inline constexpr std::uint32_t ns_ALL = 0xFFFFFFFFu;
}

//========= 形状=========
// 球。
struct COLLISION_SPHERE
{
  // 中心のワールド座標。
  XMFLOAT3 center{};
  // 半径。単位はワールド距離単位。0以下の球は、他の形状と重ならない扱いに近くなるため、正の値を設定する。
  float radius{};
};

// 軸（X・Y・Z）に平行な直方体。回転した箱は表せない。
struct COLLISION_AABB
{
  // 各軸の最小値を持つ角のワールド座標。各成分が maxPoint 以下であること。
  XMFLOAT3 minPoint{};
  // 各軸の最大値を持つ角のワールド座標。各成分が minPoint 以上であること。
  XMFLOAT3 maxPoint{};
};

// 始点と方向で表す半直線。
struct COLLISION_RAY
{
  // 始点のワールド座標。
  XMFLOAT3 origin{};
  // 進む方向。長さ1の単位ベクトルであること。判定側は正規化や長さの検証をしない。既定は+Z方向。
  XMFLOAT3 direction{0.0f,0.0f,1.0f};
};

//========= 問い合わせ結果=========
// Rayが形状へ命中した結果。
struct COLLISION_RAY_HIT
{
  // 始点から命中点までの距離。始点が形状の内側にある場合は0。
  float distance{};
  // 命中点のワールド座標。
  XMFLOAT3 point{};
  // 命中した面の外向き法線。長さ1。始点が形状の内側にある場合は、Rayの進行方向と逆向きにする。
  XMFLOAT3 normal{};
};

// 命中した、または重なったColliderの情報。
struct COLLISION_HIT_INFO
{
  // 命中したColliderの識別値。
  ColliderHandle handle{};
  // 命中したColliderが属するレイヤーのbit。
  std::uint32_t layer{};
  // 登録時に呼出側が付けた識別値。敵の番号などに使う。
  std::uint32_t userData{};
};

//========= 登録されるCollider=========
// Colliderが持つ形状の種類。
enum class COLLISION_SHAPE_TYPE
{
  e_SPHERE, // 球。COLLIDER_ENTRYのsphereが有効。
  e_AABB    // 軸整列AABB。COLLIDER_ENTRYのaabbが有効。
};

// CollisionWorldへ登録した1個のCollider。
struct COLLIDER_ENTRY
{
  //========= 識別=========
  // 登録時に割り当てた識別値。
  ColliderHandle handle{};
  // 登録時に呼出側が付けた識別値。
  std::uint32_t userData{};
  // 属するレイヤーのbit。1個だけ立てる。
  std::uint32_t layer{};

  //========= 形状=========
  // 形状の種類。有効な形状のメンバーを選ぶ。
  COLLISION_SHAPE_TYPE shapeType{COLLISION_SHAPE_TYPE::e_SPHERE};
  // 形状が球の場合の値。
  COLLISION_SPHERE sphere{};
  // 形状がAABBの場合の値。
  COLLISION_AABB aabb{};
};
