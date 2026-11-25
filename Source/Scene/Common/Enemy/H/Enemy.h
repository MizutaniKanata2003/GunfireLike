#pragma once

// 敵1体の状態（種類・HP・怯み・転倒・射撃のダメージの印・攻撃予告の表示・生死・向き）と、球のColliderの登録、モデル（読み込めない時はCube）での表示を担当する。
// 種類ごとの数値はEnemyTableから読む。モデルの動きの選択と描画は、EnemyModelPlaybackが担当する。敵の行動（移動・攻撃）は持たない。複数体の管理は、EnemyManagerが担当する。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tで、Colliderへ付ける敵の識別値を保持する。

//========= Framework インクルード=========
#include "Framework/3D/H/CameraMatrices.h"
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyModelLibrary.h"
#include "Scene/Common/Enemy/H/EnemyModelPlayback.h"
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

// 敵1体の簡易アニメーション（待機の揺れ・怯みの傾き・予告の前傾・攻撃の突進）と、向きの追従の状態。Enemyのメンバー変数の数を抑えるため、1つにまとめる。
struct ENEMY_ANIMATION_STATE
{
  // 待機の揺れの位相。単位は周期の数で、0以上1未満。Updateで進める。同時に生成した敵が揃って揺れないよう、Initializeで敵の識別値から初期値を決める。
  float idleBobPhase{};
  // 怯みの傾きの向き。1.0または-1.0。怯みが始まる被弾のたびに反転し、連射で怯みが続く間は変えない。
  float flinchTiltSign{1.0f};
  // 攻撃の予告が続いている時間。単位は秒。予告中だけUpdateで進め、予告でなくなったら0へ戻す。前傾の強さを決める。
  float telegraphSeconds{};
  // 攻撃の突進の残り時間。単位は秒。0より大きい間は、突進中。StartAttackLungeで設定し、Updateで減らす。
  float lungeRemainingSeconds{};
  // 向きの目標。単位はラジアン。0が+Z、πが-Z。SetFacingが設定し、Updateが、旋回の速さの上限で、この向きへ近づける。
  float targetYaw{};
  // 向きの目標を、すでに設定したか。最初の1回は、旋回の途中の姿を見せず、そのまま向ける。
  bool hasFacingTarget{};
  // 倒された時の演出を、まだ出していないか。ApplyDamageで死亡した時にtrueになり、TakeDefeatEffectNoticeで取り出すとfalseへ戻る。Respawnでも、falseへ戻る。
  bool hasPendingDefeatEffect{};
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
  // 怯み・転倒・射撃のダメージの印の残り時間と、待機の揺れの位相・予告の経過時間・攻撃の突進の残り時間を進め、向きを目標へ近づけ、モデルの動きを選んで時刻を進める。
  // deltaTime: この更新で進める経過時間。単位は秒。
  void Update(float deltaTime);
  // 全ての敵が共有する、モデルと動きの読込を設定する。EnemyControllerが、Initialize時に、全ての枠へ渡し、Uninit時にnullptrを渡す。Enemy::Uninit・Respawnでは変えない。
  // modelLibrary: 共有の読込。非所有で、このEnemyが使い終わるまで生存させる。nullptrなら、Cubeで描く。
  void SetModelLibrary(EnemyModelLibrary* modelLibrary) { m_playback.SetLibrary(modelLibrary); }
  // 地形との重なりを補正し、登録済みの球Colliderと描画位置を更新する。
  // targetPosition: 移動先候補のワールド座標。呼出側で1回の移動距離を制限する。
  // collisionWorld: この敵のColliderを登録したCollisionWorld。
  // 戻り値: Colliderと描画位置を更新できればtrue。死亡中または更新失敗時はfalse。
  [[nodiscard]] bool TryMoveTo(const XMFLOAT3& targetPosition,CollisionWorld& collisionWorld);
  // 目標の方へ、水平方向に向ける向きの目標を設定する。実際の向きは、Updateが、旋回の速さの上限で、目標へ近づける（最初の1回だけ、そのまま向ける）。位置とColliderは変えない。
  // 死亡中・転倒中は向きの目標を変えない。目標との水平距離がほぼ0で向きが定まらない場合も、直前の目標を保つ。
  // targetPosition: 向ける先のワールド座標。高さは使わない。
  void SetFacing(const XMFLOAT3& targetPosition);
  // 攻撃の瞬間の突進を始める。向いている方向へ、一定時間、描画だけを前へ出して戻す。Colliderと攻撃の判定は変えない。
  // 死亡している場合は何もしない。すでに突進中の場合は、最初から数え直す。
  void StartAttackLunge();
  // 倒された時の演出を、まだ出していない場合だけ、trueを返し、出す旨を記録する。同じ敵について、二度目以降はfalseを返す。生きている敵は、常にfalse。
  [[nodiscard]] bool TakeDefeatEffectNotice();
  // 生きている場合は、モデルで描く（待機・移動・攻撃・怯み・転倒・起き上がりの動き）。モデルがない種類や、読み込めなかった場合は、Cubeで描く。死亡後は、死亡の動きが終わり、保持の時間が過ぎるまで、モデルだけを描く。
  // 色は、通常は白（モデルの画像の色）、射撃のダメージの印が付いている間は紫、攻撃の予告中は黄、怯み中は橙、転倒中は青みがかった灰色で、画像の色へ乗算する。Cubeの描画では、転倒中に低く潰し、待機の揺れ・前傾・怯みの傾き・突進を加える。
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
  // HPを満タンへ戻して怯みと転倒と射撃のダメージの印を解除する。死亡していた場合はColliderを登録し直して生き返らせる。攻撃の予告の表示と、攻撃の突進も解除する。
  // collisionWorld: Initializeで渡したのと同じCollisionWorld。
  void Respawn(CollisionWorld& collisionWorld);
  // 攻撃の予告中の表示を切り替える。予告の状態そのものは、呼出側が管理し、毎回の更新で現在の状態を渡す。
  // isTelegraphing: trueなら、通常時の色の代わりに予告の色で描き、前へ傾ける。怯み・転倒の表示の方が優先される。
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
  // 倒された敵（初期化済みで、死亡している枠）かを返す。未使用の枠（Uninit済み）は、false。ウェーブの切替で、死体を預ける対象を選ぶために使う。
  [[nodiscard]] bool IsCorpse() const { return !m_isAlive && m_cubeRenderer != nullptr; }
  // モデルの動きの状態を返す。倒された敵の死体を、預けるために使う。
  [[nodiscard]] const EnemyModelPlayback& GetPlayback() const { return m_playback; }
  // モデルの動きの選択と描画に使う、敵の現在の状態を返す。色は白。
  [[nodiscard]] ENEMY_PLAYBACK_STATE GetPlaybackState() const { return BuildPlaybackState(XMFLOAT4{1.0f,1.0f,1.0f,1.0f}); }
  // 追従の移動速度を返す。モデルの歩きの動きから測った、足が地面を滑らない速度で、動きの位相によって変わる。モデルを使えない時は、既定の速度を返す。
  // fallbackSpeed: モデルがない時に返す、既定の速度。単位はワールド距離単位/秒。
  [[nodiscard]] float GetModelMoveSpeed(float fallbackSpeed) const { return m_playback.GetMoveSpeed(BuildPlaybackState(XMFLOAT4{1.0f,1.0f,1.0f,1.0f}),fallbackSpeed); }
private:
  //========= 更新の補助関数=========
  // 怯み・転倒・射撃のダメージの印の残り時間を減らし、0未満にならないようにする。
  // deltaTime: この更新で進める経過時間。単位は秒。
  void UpdateStatusTimers(float deltaTime);
  // 向きを、目標へ、旋回の速さの上限で近づける。目標が未設定、死亡中、転倒中は、何もしない。
  // deltaTime: この更新で進める経過時間。単位は秒。
  void UpdateFacing(float deltaTime);

  //========= 描画の補助関数=========
  // Cubeで描く。モデルを描けない時の代わり。生きている時だけ呼ぶ。
  // graphicsSystem: Rendererが使う描画基盤。
  // camera: View行列とProjection行列。
  void DrawCube(GraphicsSystem& graphicsSystem,const CAMERA_MATRICES& camera) const;
  // モデルの動きの選択と描画に渡す、敵の現在の状態を作る。
  // color: モデルの画像に乗算する色。
  // 戻り値: 現在の状態を詰めた構造体。
  [[nodiscard]] ENEMY_PLAYBACK_STATE BuildPlaybackState(const XMFLOAT4& color) const;
  // 転倒・怯み・予告の状態に応じた色を返す。優先順は、転倒、怯み、予告、基準の色。
  // baseColor: どの状態でもない時の色。
  // 戻り値: 乗算する色。
  [[nodiscard]] XMFLOAT4 DecideStateColor(const XMFLOAT4& baseColor) const;
  // 簡易アニメーション（待機の揺れ・突進・怯みの傾き・予告の前傾）を、描画用のTransformへ反映する。転倒中は何もしない。
  // drawTransform: 反映先の描画用Transform。位置は保持座標から作り直し、回転は保持している向きに、傾きを足す。Colliderと保持座標は変えない。
  void ApplyAnimation(Transform& drawTransform) const;

  //========= 表示=========
  // Cube描画に使う共有Renderer。非所有で、ModelRegistryが所有する。未取得はnullptr。
  BasicMeshRenderer* m_cubeRenderer{};
  // 位置・向き・大きさを保持し、World行列を作るTransform。向きは、UpdateFacingが、Y軸まわりの回転だけを更新する。
  Transform m_transform{};
  // 攻撃の予告中の表示か。呼出側がSetTelegraphingで毎回設定する。
  bool m_isTelegraphing{};

  //========= アニメーション=========
  // 待機の揺れの位相・怯みの傾きの向き・予告の経過時間・突進の残り時間・向きの目標。簡易アニメーションと向きの状態をまとめて保持する。
  ENEMY_ANIMATION_STATE m_animation{};
  // モデルの動きの選択・時刻の進行・描画を担当する。共有の読込への参照は、SetModelLibraryで設定し、Uninit・Respawnでは変えない。
  EnemyModelPlayback m_playback{};

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