#pragma once

// 敵1体の状態（種類・HP・怯み・転倒・生死）と、球のColliderの登録、Cubeでの仮表示を担当する。
// 種類ごとの数値はEnemyTableから読む。敵の行動（移動・攻撃）は持たない。複数体の管理は、EnemyManagerが担当する。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tで、Colliderへ付ける敵の識別値を保持する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/H/Transform.h"
#include "Scene/GameScene/H/EnemyWaveTable.h"

//========= 前方宣言=========
class BasicMeshRenderer;
class CollisionWorld;
class GraphicsSystem;
class ModelRegistry;

// 種類・HP・怯み・転倒を持ち、球のColliderで被弾を受ける敵1体。
class Enemy final
{
public:
	//========= 初期化・終了関数=========
	// 種類・位置・識別値を設定し、HPを満タンにして、球のColliderを登録する。表示用のCube Rendererを借用する。
	// registry: Cube Rendererの借用元。Initialize済みであること。このEnemyより長く生存させる。
	// collisionWorld: Colliderの登録先。このEnemyより長く生存させ、Uninitまで同じ実体を渡す。
	// kind: 敵の種類。HP・当たりの大きさ・表示の大きさと色を決める。
	// position: 球の中心のワールド座標。
	// enemyId: Colliderへ付ける識別値。命中結果のuserDataとして返る。
	// 戻り値: Cube Rendererを借用できて登録できればtrue。Registryが未初期化ならfalse。
	[[nodiscard]] bool Initialize(ModelRegistry& registry,CollisionWorld& collisionWorld,ENEMY_KIND kind,const XMFLOAT3& position,std::uint32_t enemyId);
	// 登録したColliderを解除し、借用した参照を手放す。未初期化・解除済みでも呼出可能。
	// collisionWorld: Initializeで渡したのと同じCollisionWorld。
	void Uninit(CollisionWorld& collisionWorld);

	//========= 更新・描画関数=========
	// 怯みと転倒の残り時間を進める。
	// deltaTime: この更新で進める経過時間。単位は秒。
	void Update(float deltaTime);
	// 地形との重なりを補正し、登録済みの球Colliderと描画位置を更新する。
	// targetPosition: 移動先候補のワールド座標。呼出側で1回の移動距離を制限する。
	// collisionWorld: この敵のColliderを登録したCollisionWorld。
	// 戻り値: Colliderと描画位置を更新できればtrue。死亡中または更新失敗時はfalse。
	[[nodiscard]] bool TryMoveTo(const XMFLOAT3& targetPosition,CollisionWorld& collisionWorld);
	// 生きている場合だけ、Cubeで描画する。通常は種類の色、怯み中・転倒中は色を変え、転倒中は低く潰して描く。
	// graphicsSystem: Rendererが使う描画基盤。
	// viewMatrix: World空間からView空間への変換行列。
	// projectionMatrix: View空間からClip空間への変換行列。
	void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

	//========= 戦闘関数=========
	// ダメージを受け、HPを減らして怯ませる。HPが0以下になったらColliderを解除して死亡にする。
	// damage: 受けるダメージ。正の値。
	// collisionWorld: Initializeで渡したのと同じCollisionWorld。死亡時にColliderを解除する。
	// 戻り値: このダメージで死亡したらtrue。すでに死亡している場合はダメージを受けずfalse。
	bool ApplyDamage(float damage,CollisionWorld& collisionWorld);
	// 転倒させる。転倒中は怯みも解除され、蹴りの対象にならない。すでに転倒中、または死亡している場合は何もしない。
	// 戻り値: 新しく転倒させたらtrue。何もしなかった場合はfalse。
	bool ApplyKnockdown();
	// HPを満タンへ戻して怯みと転倒を解除する。死亡していた場合はColliderを登録し直して生き返らせる。
	// collisionWorld: Initializeで渡したのと同じCollisionWorld。
	void Respawn(CollisionWorld& collisionWorld);

	//========= Getter関数=========
	// 生きているかを返す。
	[[nodiscard]] bool IsAlive() const { return m_isAlive; }
	// 怯み中かを返す。怯みは被弾から一定時間続く。
	[[nodiscard]] bool IsFlinching() const { return m_flinchRemainingSeconds > 0.0f; }
	// 転倒中かを返す。転倒は一定時間続き、その間は蹴れない。
	[[nodiscard]] bool IsKnockedDown() const { return m_knockdownRemainingSeconds > 0.0f; }
	// 現在のHPを返す。
	[[nodiscard]] float GetCurrentHP() const { return m_currentHP; }
	// 最大HPを返す。種類の表の値。
	[[nodiscard]] float GetMaxHP() const;
	// 敵の種類を返す。
	[[nodiscard]] ENEMY_KIND GetKind() const { return m_kind; }
	// Colliderへ付けた識別値を返す。
	[[nodiscard]] std::uint32_t GetEnemyId() const { return m_enemyId; }
	// 敵の体の当たりを表す球を返す。中心は敵の位置、半径は種類の表の値でColliderと同じ。
	[[nodiscard]] COLLISION_SPHERE GetBodySphere() const;

private:
	//========= 表示=========
	// Cube描画に使う共有Renderer。非所有で、ModelRegistryが所有する。未取得はnullptr。
	BasicMeshRenderer* m_cubeRenderer{};
	// 位置と大きさを保持し、World行列を作るTransform。
	Transform m_transform{};

	//========= 種類=========
	// 敵の種類。Initializeで設定する。未初期化の間は接近型として扱う。
	ENEMY_KIND m_kind{ENEMY_KIND::e_MELEE};

	//========= Collision=========
	// 球のColliderの中心。生き返らせる際に同じ位置へ登録し直す。
	XMFLOAT3 m_position{};
	// 登録中のColliderの識別値。死亡中や未登録は無効値。
	ColliderHandle m_colliderHandle{COLLIDER_HANDLE::ns_INVALID};
	// Colliderへ付ける識別値。
	std::uint32_t m_enemyId{};

	//========= 戦闘状態=========
	// 現在のHP。0以下で死亡。
	float m_currentHP{};
	// 怯みの残り時間。単位は秒。0より大きい間は怯み中。
	float m_flinchRemainingSeconds{};
	// 転倒の残り時間。単位は秒。0より大きい間は転倒中。
	float m_knockdownRemainingSeconds{};
	// 生きているか。
	bool m_isAlive{};
};