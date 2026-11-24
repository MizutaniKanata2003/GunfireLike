#pragma once

// 球・軸整列AABBのColliderを登録して保持し、Rayの命中と球の重なりを問い合わせる。
// 登録数が少ない前提で、問い合わせは全Colliderを順に調べる。Colliderの移動・押し戻し・描画・Game側の反応は担当しない。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // std::size_tでColliderの位置を表す。
#include <cstdint> // std::uint32_tでレイヤーのbitと呼出側の識別値を受け取る。
#include <vector> // 登録したColliderと、問い合わせ結果を可変長配列で保持・返却する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"

// Rayによる問い合わせの条件。CollisionWorld::Raycastの引数の数を減らすための値の束で、問い合わせは変更しない。
struct COLLISION_RAY_QUERY
{
	// Ray。directionは単位ベクトルであること。
	COLLISION_RAY ray{};
	// 命中を調べる最大距離。これを超える命中は無視する。
	float maxDistance{};
	// 対象にするレイヤーのbitの組合せ。Colliderのレイヤーとの論理積が0でないものを調べる。
	std::uint32_t layerMask{};
};

// Colliderの登録・更新・解除と、Ray・球による問い合わせを提供する。
class CollisionWorld final
{
public:
	//========= 登録・解除関数=========
	// 球のColliderを登録する。
	// sphere: 登録する球。値をコピーする。
	// layer: 属するレイヤーのbit。COLLISION_LAYERの値を1個だけ指定する。
	// userData: 呼出側が後で識別するための値。敵の番号などを指定する。使わなければ0。
	// 戻り値: 登録したColliderの識別値。解除するまで有効で、再利用しない。
	ColliderHandle AddSphere(const COLLISION_SPHERE& sphere,std::uint32_t layer,std::uint32_t userData);
	// AABBのColliderを登録する。
	// aabb: 登録するAABB。値をコピーする。
	// layer: 属するレイヤーのbit。COLLISION_LAYERの値を1個だけ指定する。
	// userData: 呼出側が後で識別するための値。使わなければ0。
	// 戻り値: 登録したColliderの識別値。解除するまで有効で、再利用しない。
	ColliderHandle AddAABB(const COLLISION_AABB& aabb,std::uint32_t layer,std::uint32_t userData);
	// 指定したColliderを解除する。他のColliderの識別値は変わらない。
	// handle: 解除するColliderの識別値。
	// 戻り値: 解除できればtrue。未登録または解除済みならfalse。
	bool Remove(ColliderHandle handle);
	// 登録した全Colliderを解除する。識別値の採番は続け、以前の識別値を再利用しない。
	void Clear();

	//========= 更新関数=========
	// 球のColliderの形状を置き換える。移動する敵やプレイヤーの追従に使う。
	// handle: 更新するColliderの識別値。
	// sphere: 新しい球。値をコピーする。
	// 戻り値: 更新できればtrue。未登録、または形状が球でなければfalseで、何も変更しない。
	bool UpdateSphere(ColliderHandle handle,const COLLISION_SPHERE& sphere);
	// AABBのColliderの形状を置き換える。
	// handle: 更新するColliderの識別値。
	// aabb: 新しいAABB。値をコピーする。
	// 戻り値: 更新できればtrue。未登録、または形状がAABBでなければfalseで、何も変更しない。
	bool UpdateAABB(ColliderHandle handle,const COLLISION_AABB& aabb);

	//========= 問い合わせ関数=========
	// Rayが最初に命中するColliderを調べる。
	// query: Ray・最大距離・対象のレイヤーのbit。最大距離を超える命中は無視する。
	// rayHit: 命中した場合の出力先。命中しない場合は変更しない。
	// hitInfo: 命中したColliderの情報の出力先。命中しない場合は変更しない。
	// 戻り値: いずれかのColliderに命中すればtrue。同じ距離なら先に登録したColliderを採用する。
	[[nodiscard]] bool Raycast(const COLLISION_RAY_QUERY& query,COLLISION_RAY_HIT& rayHit,COLLISION_HIT_INFO& hitInfo) const;
	// 球と重なる全Colliderを調べる。
	// sphere: 調べる範囲を表す球。
	// layerMask: 対象にするレイヤーのbitの組合せ。
	// hits: 重なったColliderの情報の出力先。先にclearし、登録順に追加する。
	void OverlapSphere(const COLLISION_SPHERE& sphere,std::uint32_t layerMask,std::vector<COLLISION_HIT_INFO>& hits) const;

	//========= Getter関数=========
	// 登録中のColliderの一覧を返す。可視化など、読み取り専用の用途に使う。登録・解除・Clearの後は参照が無効になる。
	[[nodiscard]] const std::vector<COLLIDER_ENTRY>& GetColliders() const { return m_colliders; }

private:
	//========= 補助関数=========
	// 識別値に対応するColliderの位置を線形探索で求める。
	// handle: 探すColliderの識別値。
	// 戻り値: 見つかった位置。見つからなければ登録数と同じ値を返す。
	[[nodiscard]] std::size_t FindIndex(ColliderHandle handle) const;

	//========= 登録状態=========
	// 登録したColliderを登録順に保持する。
	std::vector<COLLIDER_ENTRY> m_colliders{};
	// 次に割り当てるColliderの識別値。0は無効値のため1から始める。
	ColliderHandle m_nextHandle{1};
};