#pragma once

// 敵1体の状態（種類・HP・怯み・転倒・射撃のダメージの印・攻撃予告の表示・生死・向き）と、球のColliderの登録、Cubeでの仮表示を担当する。
// 種類ごとの数値はEnemyTableから読む。敵の行動（移動・攻撃）は持たない。複数体の管理は、EnemyManagerが担当する。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tで、Colliderへ付ける敵の識別値を保持する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyWaveTable.h"
#include "Scene/Common/Object/H/Transform.h"

//========= 前方宣言=========
class BasicMeshRenderer;
class CollisionWorld;
class GraphicsSystem;
class ModelRegistry;

// 敵1体の初期化に使う、種類・位置・識別値。Enemy::Initializeの引数の数を減らすための値の束で、初期化は変更しない。
struct ENEMY_INIT_PARAMS
{
  // 敵の種類。HP・当たりの大きさ・表示の大きさと色を決める。
  ENEMY_KIND kind{ENEMY_KIND::e_MELEE};
  // 球の中心のワールド座標。
  XMFLOAT3 position{};
  // Colliderへ付ける識別値。命中結果のuserDataとして返る。
  std::uint32_t enemyId{};
};

// 種類・HP・怯み・転倒を持ち、球のColliderで被弾を受ける敵1体。
class Enemy final
{
public:
  //========= 初期化・終了関数=========
  // 種類・位置・識別値を設定し、HPを満タンにして、球のColliderを登録する。表示用のCube Rendererを借用する。
  // registry: Cube Rendererの借用元。Initialize済みであること。このEnemyより長く生存させる。
  // collisionWorld: Colliderの登録先。このEnemyより長く生存させ、Uninitまで同じ実体を渡す。
  // initParams: 敵の種類・位置・Colliderへ付ける識別値。
  // 戻り値: Cube Rendererを借用できて登録できればtrue。Registryが未初期化ならfalse。
  [[nodiscard]] bool Initialize(ModelRegistry& registry,CollisionWorld& collisionWorld,const ENEMY_INIT_PARAMS& initParams);
  // 登録したColliderを解除し、借用した参照を手放す。未初期化・解除済みでも呼出可能。
  // collisionWorld: Initializeで渡したのと同じCollisionWorld。
  void Uninit(CollisionWorld& collisionWorld);

  //========= 更新・描画関数=========
  // 怯み・転倒・射撃のダメージの印の残り時間を進める。
  // deltaTime: この更新で進める経過時間。単位は秒。
  void Update(float deltaTime);
  // 地形との重なりを補正し、登録済みの球Colliderと描画位置を更新する。
  // targetPosition: 移動先候補のワールド座標。呼出側で1回の移動距離を制限する。
  // collisionWorld: この敵のColliderを登録したCollisionWorld。
  // 戻り値: Colliderと描画位置を更新できればtrue。死亡中または更新失敗時はfalse。
  [[nodiscard]] bool TryMoveTo(const XMFLOAT3& targetPosition,CollisionWorld& collisionWorld);
  // 目標の方へ、水平方向に向ける。Y軸まわりの回転だけを変え、位置とColliderは変えない。
  // 死亡中・転倒中は向きを変えない。目標との水平距離がほぼ0で向きが定まらない場合も、直前の向きを保つ。
  // targetPosition: 向ける先のワールド座標。高さは使わない。
  void SetFacing(const XMFLOAT3& targetPosition);
  // 生きている場合だけ、Cubeで描画する。通常は種類の色、射撃のダメージの印が付いている間は紫、攻撃の予告中は黄、怯み中・転倒中は色を変え、転倒中は低く潰して描く。
  // graphicsSystem: Rendererが使う描画基盤。
  // viewMatrix: World空間からView空間への変換行列。
  // projectionMatrix: View空間からClip空間への変換行列。
  void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

  //========= 戦闘関数=========
  // ダメージを受け、HPを減らして怯ませる。HPが0以下になったらColliderを解除して死亡にする。
  // damage: 受けるダメージ。正の値。
  // collisionWorld: Initializeで渡したのと同じCollisionWorld。死亡時にColliderを解除する。
  // flinchScale: 怯みの時間に掛ける倍率。1.0で通常。特殊強化（制圧射撃）で、射撃の怯みを伸ばす時に1.0より大きい値を渡す。正の値であること。省略すると1.0。
  // 戻り値: このダメージで死亡したらtrue。すでに死亡している場合はダメージを受けずfalse。
  bool ApplyDamage(float damage,CollisionWorld& collisionWorld,float flinchScale = 1.0f);
  // 転倒させる。転倒中は怯みも解除され、蹴りの対象にならない。すでに転倒中、死亡している、または範囲攻撃型（転倒しない種類）の場合は何もしない。
  // 戻り値: 新しく転倒させたらtrue。何もしなかった場合はfalse。
  bool ApplyKnockdown();
  // 一定時間、この敵が射撃で受けるダメージを増やす印を付ける。特殊強化（殴打追撃）の効果。すでに印が付いている場合は、倍率と時間を新しい値へ置き換える。死亡している場合は何もしない。
  // damageScale: 射撃で受けるダメージの倍率。1.0より大きい値。
  // durationSeconds: 印が続く時間。単位は秒。この敵の側の時間（スローの影響を受ける時間）で数える。
  void ApplyShotDamageMark(float damageScale,float durationSeconds);
  // HPを満タンへ戻して怯みと転倒と射撃のダメージの印を解除する。死亡していた場合はColliderを登録し直して生き返らせる。攻撃の予告の表示も解除する。
  // collisionWorld: Initializeで渡したのと同じCollisionWorld。
  void Respawn(CollisionWorld& collisionWorld);
  // 攻撃の予告中の表示を切り替える。予告の状態そのものは、呼出側が管理し、毎回の更新で現在の状態を渡す。
  // isTelegraphing: trueなら、通常時の色の代わりに予告の色で描く。怯み・転倒の色の方が優先される。
  void SetTelegraphing(bool isTelegraphing) { m_isTelegraphing = isTelegraphing; }

  //========= Getter関数=========
  // 生きているかを返す。
  [[nodiscard]] bool IsAlive() const { return m_isAlive; }
  // 怯み中かを返す。怯みは被弾から一定時間続く。
  [[nodiscard]] bool IsFlinching() const { return m_flinchRemainingSeconds > 0.0f; }
  // 転倒中かを返す。転倒は一定時間続き、その間は蹴れない。
  [[nodiscard]] bool IsKnockedDown() const { return m_knockdownRemainingSeconds > 0.0f; }
  // 攻撃の予告中の表示になっているかを返す。
  [[nodiscard]] bool IsTelegraphing() const { return m_isTelegraphing; }
  // 射撃のダメージの印が付いているかを返す。
  [[nodiscard]] bool IsShotDamageMarked() const { return m_shotDamageMarkRemainingSeconds > 0.0f; }
  // この敵が射撃で受けるダメージの倍率を返す。印が付いている間は、その倍率で、付いていなければ1.0。
  [[nodiscard]] float GetShotDamageScale() const { return IsShotDamageMarked() ? m_shotDamageScale : 1.0f; }
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
  // 位置・向き・大きさを保持し、World行列を作るTransform。向きは、SetFacingがY軸まわりの回転だけを更新する。
  Transform m_transform{};
  // 攻撃の予告中の表示か。呼出側がSetTelegraphingで毎回設定する。
  bool m_isTelegraphing{};

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
  // 射撃のダメージの印の残り時間。単位は秒。0より大きい間は、印が付いている。
  float m_shotDamageMarkRemainingSeconds{};
  // 印が付いている間に、射撃で受けるダメージの倍率。印が付いていない間は使わない。
  float m_shotDamageScale{1.0f};
  // 生きているか。
  bool m_isAlive{};
};