#include "../H/Enemy.h"

// 敵1体のHP・怯み・転倒・射撃のダメージの印・向き・Collider登録・表示（モデルの動きの切替と描画、読み込めない時のCube表示）を実装する。
// 種類ごとのHP・当たり・大きさ・色はEnemyTableから、Cubeの簡易アニメーションとモデルの再生の数値はEnemyAnimationTableから読み、怯みと転倒の時間と状態の色は、このファイルの定数にまとめた仮値。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで、怯みの傾きと予告の前傾の強さを1以下に制限する。
#include <cmath> // std::atan2で敵から目標への水平方向をY軸まわりの回転角へ変換し、std::sin・std::cosで待機の揺れ・突進の動き・正面方向を求め、std::floorで揺れの位相を0以上1未満へ戻す。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/3D/H/CameraMatrices.h"
#include "Framework/Collision/H/CollisionResolver.h"
#include "Framework/Collision/H/CollisionWorld.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyAnimationTable.h"
#include "Scene/Common/Enemy/H/EnemyTable.h"
#include "Scene/Common/Object/H/ModelRegistry.h"

// 検証用の敵の状態の数値と色をまとめる。企画書で未確定の数値は仮値。
namespace
{
  //========= 戦闘の仮値=========
  // 被弾後に怯む時間。単位は秒。企画書では「命中時に敵によって怯む」とだけ定められており、具体値は未確定の仮値。
  constexpr float ns_FLINCH_SECONDS = 0.3f;
  // 転倒している時間。単位は秒。企画書で未確定の仮値。
  constexpr float ns_KNOCKDOWN_SECONDS = 1.5f;

  //========= 形状=========
  // 転倒中のCubeの高さの倍率。1辺に掛けて低く潰し、倒れたことを示す。足元の位置は変えず、中心を下げる。仮値。
  constexpr float ns_KNOCKDOWN_HEIGHT_SCALE = 0.4f;

  //========= 向き=========
  // 向きを更新する、目標との水平距離の二乗の下限。この値以下では、目標とほぼ同じ位置にいて向きが定まらないため、向きを変えない。単位はワールド距離単位の二乗。仮値。
  constexpr float ns_FACING_MIN_DISTANCE_SQUARED = 0.0001f;

  //========= 状態の色=========
  // 射撃のダメージの印が付いている間の色。紫で、Object画像の色へ乗算して色味を変える。種類の色の代わりに使い、予告・怯み・転倒の色より後にする。
  constexpr XMFLOAT4 ns_MARK_COLOR{0.9f,0.4f,1.0f,1.0f};
  // モデルの画像に乗算する、通常の色。白で、画像の色をそのまま出す。射撃のダメージの印・予告・怯み・転倒の色は、これを置き換える。
  constexpr XMFLOAT4 ns_MODEL_BASE_COLOR{1.0f,1.0f,1.0f,1.0f};
  // 攻撃の予告中の色。黄で、Object画像の色へ乗算して色味を変える。種類の色と印の色より優先し、怯みの色より後にする。
  constexpr XMFLOAT4 ns_TELEGRAPH_COLOR{1.0f,1.0f,0.3f,1.0f};
  // 怯み中の色。橙で、Object画像の色へ乗算して色味を変える。予告の色より優先する。
  constexpr XMFLOAT4 ns_FLINCH_COLOR{1.0f,0.5f,0.2f,1.0f};
  // 転倒中の色。青みがかった灰色で、Object画像の色へ乗算して色味を変える。怯みの色より優先する。
  constexpr XMFLOAT4 ns_KNOCKDOWN_COLOR{0.5f,0.6f,1.0f,1.0f};
}

// 簡易アニメーションの値を求める補助関数をまとめる。数値はEnemyAnimationTableから読む。
namespace
{
  // 待機の揺れで、高さへ足す量を返す。
  // phase: 揺れの位相。単位は周期の数で、0以上1未満。
  // 戻り値: 高さへ足す量。単位はワールド距離単位。位相に対する正弦波で、中心から上下へ表の振幅だけ動く。
  float CalculateIdleBobOffset(float phase)
  {
    return std::sin(phase * ENEMY_ANIMATION::ns_TWO_PI) * ENEMY_ANIMATION::ns_IDLE_BOB_AMPLITUDE;
  }

  // 怯みの傾きの角度を返す。怯みの残り時間が短くなるにつれて、傾きを0へ戻す。
  // remainingSeconds: 怯みの残り時間。単位は秒。
  // tiltSign: 傾きの向き。1.0または-1.0。
  // 戻り値: 向きに対する横方向（Roll）の角度。単位はラジアン。
  float CalculateFlinchTilt(float remainingSeconds,float tiltSign)
  {
    // 残り時間が通常の怯みの時間以上の間は、最大の傾きにする。強化で怯みが伸びた場合も、傾きが最大を超えないようにする。
    const float tilt_strength = std::min(1.0f,remainingSeconds / ns_FLINCH_SECONDS);
    return tiltSign * tilt_strength * ENEMY_ANIMATION::ns_FLINCH_TILT_RADIANS;
  }

  // 攻撃の予告中の前傾の角度を返す。予告の開始から、表の時間をかけて、最大の角度へ近づく。
  // telegraphSeconds: 攻撃の予告が続いている時間。単位は秒。
  // 戻り値: 向きに対して前へ傾ける（Pitch）角度。単位はラジアン。正の値で、上部が正面側へ倒れる。
  float CalculateTelegraphLean(float telegraphSeconds)
  {
    // 最大の角度に達した後は、予告が長引いても、それ以上傾けない。
    const float lean_strength = std::min(1.0f,telegraphSeconds / ENEMY_ANIMATION::ns_TELEGRAPH_LEAN_SECONDS);
    return lean_strength * ENEMY_ANIMATION::ns_TELEGRAPH_LEAN_RADIANS;
  }

  // 攻撃の突進で、正面へ出ている距離を返す。突進の開始と終わりは0で、中間で最大になる。
  // remainingSeconds: 突進の残り時間。単位は秒。
  // 戻り値: 正面へ出ている距離。単位はワールド距離単位。突進中でなければ0。
  float CalculateLungeOffset(float remainingSeconds)
  {
    // 突進中でなければ、前へ出さない。
    if(remainingSeconds <= 0.0f)return 0.0f;

    // 経過の割合（0から1）を求め、sinの半周期で、出て戻る動きにする。
    const float progress = 1.0f - remainingSeconds / ENEMY_ANIMATION::ns_ATTACK_LUNGE_SECONDS;
    return std::sin(progress * ENEMY_ANIMATION::ns_PI) * ENEMY_ANIMATION::ns_ATTACK_LUNGE_DISTANCE;
  }
}

// モデルの動きの再生に使う、補助関数をまとめる。数値はEnemyAnimationTableから読む。
namespace
{
  // 動きを繰り返すかを返す。待機と移動だけが繰り返し、ほかは、最後のポーズで止まる。
  // clip: 動き。
  bool IsLoopingClip(ENEMY_CLIP clip)
  {
    return clip == ENEMY_CLIP::e_IDLE || clip == ENEMY_CLIP::e_WALK;
  }

  // 動きの再生の速さの倍率を返す。
  // clip: 動き。
  // 戻り値: 時刻を進める倍率。1.0が等倍。
  float GetClipSpeed(ENEMY_CLIP clip)
  {
    switch(clip){
    case ENEMY_CLIP::e_HIT:
    return ENEMY_ANIMATION::ns_MODEL_SPEED_HIT;

    case ENEMY_CLIP::e_DOWN:
    return ENEMY_ANIMATION::ns_MODEL_SPEED_DOWN;

    case ENEMY_CLIP::e_GET_UP:
    return ENEMY_ANIMATION::ns_MODEL_SPEED_GET_UP;

    // 待機・移動・予告・攻撃・死亡は、等倍。
    default:
    return ENEMY_ANIMATION::ns_MODEL_SPEED_NORMAL;
    }
  }
}

//========= 初期化・終了関数=========
// Cube Rendererを借用し、種類の表からHPと大きさを設定して、球のColliderを登録する。
bool Enemy::Initialize(ModelRegistry& registry,CollisionWorld& collisionWorld,const ENEMY_INIT_PARAMS& initParams)
{
  // 共有のCube Rendererを借用する。
  m_cubeRenderer = registry.GetCubeRenderer();
  // Registryが未初期化ならnullptrが返るため、Colliderを登録せず失敗とする。
  if(m_cubeRenderer == nullptr)return false;

  // 種類・位置・識別値・大きさを設定する。Cubeは1辺1のため、倍率がそのまま1辺の長さになる。向きは、SetFacingが呼ばれるまで、回転なしにする。
  const ENEMY_PARAMS& params = ENEMY_TABLE::GetParams(initParams.kind);
  m_kind = initParams.kind;
  m_position = initParams.position;
  m_enemyId = initParams.enemyId;
  m_transform.SetPosition(initParams.position);
  m_transform.SetRotation(XMFLOAT3{});
  m_transform.SetScale(XMFLOAT3{params.cubeSize,params.cubeSize,params.cubeSize});

  // HPを満タンにして生きている状態にし、球のColliderを敵レイヤーで登録する。
  m_currentHP = params.maxHP;
  m_flinchRemainingSeconds = 0.0f;
  m_knockdownRemainingSeconds = 0.0f;
  m_shotDamageMarkRemainingSeconds = 0.0f;
  m_shotDamageScale = 1.0f;
  m_isTelegraphing = false;
  m_isAlive = true;
  m_colliderHandle = collisionWorld.AddSphere(COLLISION_SPHERE{initParams.position,params.colliderRadius},COLLISION_LAYER::ns_ENEMY,initParams.enemyId);

  // 簡易アニメーションの状態を初期値へ戻す。待機の揺れの位相の初期値は、敵の識別値から決め、同時に生成した敵が揃って揺れないよう、1周を等分した位置へずらす。
  m_animation = ENEMY_ANIMATION_STATE{};
  const std::uint32_t phase_step = initParams.enemyId % ENEMY_ANIMATION::ns_IDLE_PHASE_STEP_COUNT;
  m_animation.idleBobPhase = static_cast<float>(phase_step) / static_cast<float>(ENEMY_ANIMATION::ns_IDLE_PHASE_STEP_COUNT);
  // モデルの動きを、待機の最初へ戻す。待機の動きの開始位置は、揺れの位相と同じ段数で、敵ごとにずらし、同時に生成した敵が揃って動かないようにする。
  ResetModelState(static_cast<float>(phase_step) * ENEMY_ANIMATION::ns_MODEL_IDLE_STEP_SECONDS);
  return true;
}

// Colliderを解除し、状態を未初期化へ戻す。
void Enemy::Uninit(CollisionWorld& collisionWorld)
{
  // 登録中のColliderがある場合だけ解除する。死亡中や未登録の場合は、解除済みのため何もしない。
  if(m_colliderHandle != COLLIDER_HANDLE::ns_INVALID){
    collisionWorld.Remove(m_colliderHandle);
  }

  // 借用した参照と状態を手放す。
  m_colliderHandle = COLLIDER_HANDLE::ns_INVALID;
  m_cubeRenderer = nullptr;
  m_isAlive = false;
  m_currentHP = 0.0f;
  m_flinchRemainingSeconds = 0.0f;
  m_knockdownRemainingSeconds = 0.0f;
  m_shotDamageMarkRemainingSeconds = 0.0f;
  m_shotDamageScale = 1.0f;
  m_isTelegraphing = false;
}

//========= 更新・描画関数=========
// 怯み・転倒・射撃のダメージの印の残り時間を減らし、待機の揺れの位相・予告の経過時間・攻撃の突進の残り時間を進める。
void Enemy::Update(float deltaTime)
{
  // 待機の揺れの位相を進め、1周を超えた分を除いて、0以上1未満に保つ。転倒中の揺れは、描画側で止めるため、位相は進め続ける。
  m_animation.idleBobPhase += deltaTime * ENEMY_ANIMATION::ns_IDLE_BOB_CYCLES_PER_SECOND;
  m_animation.idleBobPhase -= std::floor(m_animation.idleBobPhase);

  // 攻撃の予告中の間だけ、前傾に使う経過時間を進める。予告でなくなったら0へ戻し、次の予告を最初から数える。
  if(m_isTelegraphing){
    m_animation.telegraphSeconds += deltaTime;
  }
  else{
    m_animation.telegraphSeconds = 0.0f;
  }

  // 突進中の場合だけ時間を進め、0未満にならないようにする。突進中でなければ何もしない。
  if(m_animation.lungeRemainingSeconds > 0.0f){
    m_animation.lungeRemainingSeconds -= deltaTime;
    // 残り時間が0以下になったら、突進を終えて0にそろえる。
    if(m_animation.lungeRemainingSeconds < 0.0f){
      m_animation.lungeRemainingSeconds = 0.0f;
    }
  }

  // 怯み中の場合だけ時間を進め、0未満にならないようにする。怯みでなければ何もしない。
  if(m_flinchRemainingSeconds > 0.0f){
    m_flinchRemainingSeconds -= deltaTime;
    // 残り時間が0以下になったら、怯みを終えて0にそろえる。
    if(m_flinchRemainingSeconds < 0.0f){
      m_flinchRemainingSeconds = 0.0f;
    }
  }

  // 転倒中の場合だけ時間を進め、0未満にならないようにする。転倒でなければ何もしない。
  if(m_knockdownRemainingSeconds > 0.0f){
    m_knockdownRemainingSeconds -= deltaTime;
    // 残り時間が0以下になったら、転倒を終えて0にそろえる。
    if(m_knockdownRemainingSeconds < 0.0f){
      m_knockdownRemainingSeconds = 0.0f;
    }
  }

  // 射撃のダメージの印が付いている場合だけ時間を進め、0未満にならないようにする。印がなければ何もしない。
  if(m_shotDamageMarkRemainingSeconds > 0.0f){
    m_shotDamageMarkRemainingSeconds -= deltaTime;
    // 残り時間が0以下になったら、印を外して0にそろえる。
    if(m_shotDamageMarkRemainingSeconds < 0.0f){
      m_shotDamageMarkRemainingSeconds = 0.0f;
    }
  }
  // 状態から、モデルの動きを選び、時刻を進める。
  UpdateModelClip(deltaTime);
}

// 移動先の球を地形から押し戻し、Colliderの更新に成功した座標を描画にも使う。
bool Enemy::TryMoveTo(const XMFLOAT3& targetPosition,CollisionWorld& collisionWorld)
{
  // 死亡した敵には有効なColliderがないため、位置を変更しない。
  if(!m_isAlive)return false;

  // 現在の球から半径を引き継ぎ、移動先の中心を地形のAABBから押し戻す。
  COLLISION_SPHERE body_sphere = GetBodySphere();
  body_sphere.center = targetPosition;
  body_sphere.center = CollisionResolver::ResolveSphere(collisionWorld,body_sphere,COLLISION_LAYER::ns_STATIC);

  // 登録済みの球を更新できなければ、保持座標も描画用Transformも変更しない。
  if(!collisionWorld.UpdateSphere(m_colliderHandle,body_sphere))return false;

  // 判定に反映された中心を、敵の保持座標と描画の両方に反映する。
  m_position = body_sphere.center;
  m_transform.SetPosition(m_position);
  return true;
}

// 目標の方へ、Y軸まわりの回転だけで向ける。
void Enemy::SetFacing(const XMFLOAT3& targetPosition)
{
  // 死亡中は描画されず、転倒中は倒れたまま向きを変えないため、更新しない。
  if(!m_isAlive || IsKnockedDown())return;

  // 敵の位置から目標までの水平方向の差を求める。高さは使わない。
  const float offset_x = targetPosition.x - m_position.x;
  const float offset_z = targetPosition.z - m_position.z;
  // 水平距離がほぼ0の場合は、向きが定まらず、わずかな位置の違いで向きが急に変わってしまうため、直前の向きを保つ。
  if(offset_x * offset_x + offset_z * offset_z <= ns_FACING_MIN_DISTANCE_SQUARED)return;

  // +Z方向を基準に、Y軸まわりの回転角を求める。atan2(X成分,Z成分)は、回転後の+Z方向が目標の方を向く角度（ラジアン）になる。
  // X（Pitch）とZ（Roll）の回転は0のままにし、Colliderと位置は変えない。
  m_transform.SetRotation(XMFLOAT3{0.0f,std::atan2(offset_x,offset_z),0.0f});
}

// 攻撃の突進を、最初から始める。
void Enemy::StartAttackLunge()
{
  // 死亡している敵は描画されないため、突進を始めない。
  if(!m_isAlive)return;

  // 突進の残り時間を、表の時間へ設定する。すでに突進中の場合も、最初から数え直す。
  m_animation.lungeRemainingSeconds = ENEMY_ANIMATION::ns_ATTACK_LUNGE_SECONDS;
}

// 倒された時の演出を、まだ出していない場合だけ、trueを返し、出す旨を記録する。
bool Enemy::TakeDefeatEffectNotice()
{
  // 生きている敵と、演出をすでに出した敵は、出す対象ではない。
  if(m_isAlive || !m_animation.hasPendingDefeatEffect)return false;

  // 出す旨を記録して、同じ敵の演出を二度出さない。
  m_animation.hasPendingDefeatEffect = false;
  return true;
}

// 生きている間は、モデルで描き、描けない時はCubeで描く。死亡後は、モデルの死亡の動きだけを描く。
void Enemy::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
  // Rendererを借用していない（未初期化・Uninit済み）場合は、描画しない。
  if(m_cubeRenderer == nullptr)return;

  // View行列とProjection行列を、1つの構造体にまとめる。
  const CAMERA_MATRICES camera{viewMatrix,projectionMatrix};

  // 死亡後は、モデルの死亡の動きだけを描く。モデルを使えない場合は、何も描かない。
  if(!m_isAlive){
    static_cast<void>(DrawModel(graphicsSystem,camera,ns_MODEL_BASE_COLOR));
    return;
  }

  // 生きている間は、モデルで描く。射撃のダメージの印は、基準の色を置き換え、予告・怯み・転倒の色が、それより優先する。
  const XMFLOAT4 base_color = IsShotDamageMarked() ? ns_MARK_COLOR : ns_MODEL_BASE_COLOR;
  if(DrawModel(graphicsSystem,camera,DecideStateColor(base_color)))return;

  // モデルを使えない場合は、Cubeで描く。
  DrawCube(graphicsSystem,camera);
}

//========= 戦闘関数=========
// ダメージを受けてHPを減らし、怯ませ、0以下なら死亡させる。
bool Enemy::ApplyDamage(float damage,CollisionWorld& collisionWorld,float flinchScale)
{
  // すでに死亡している場合は、ダメージも怯みも与えずfalseを返す。
  if(!m_isAlive)return false;

  // 怯みが始まる被弾の時だけ、傾きの向きを反転する。連射で怯みが続く間は向きを変えず、傾きが左右に細かく振れるのを避ける。
  if(m_flinchRemainingSeconds <= 0.0f){
    m_animation.flinchTiltSign = -m_animation.flinchTiltSign;
  }

  // HPを減らし、怯みの時間を、倍率を掛けた長さで最初から数え直す。
  m_currentHP -= damage;
  m_flinchRemainingSeconds = ns_FLINCH_SECONDS * flinchScale;

  // HPが残っていれば、死亡せずに戻る。
  if(m_currentHP > 0.0f)return false;

  // HPが0以下になったため、Colliderを解除して死亡にする。以後の弾は、この敵を通り抜ける。倒された時の演出を、出す対象として記録する。
  m_currentHP = 0.0f;
  m_isAlive = false;
  m_animation.hasPendingDefeatEffect = true;
  // 死亡の動きを、最初から始める。次のUpdateを待たず、倒された最初の描画から、死亡の動きになる。
  m_modelState.clip = ENEMY_CLIP::e_DEATH;
  m_modelState.clipTimeSeconds = 0.0f;
  collisionWorld.Remove(m_colliderHandle);
  m_colliderHandle = COLLIDER_HANDLE::ns_INVALID;
  return true;
}

// 転倒させ、怯みを解除する。範囲攻撃型は、転倒しない。
bool Enemy::ApplyKnockdown()
{
  // 死亡している、すでに転倒中、または範囲攻撃型（転倒しない種類）の場合は、転倒させず何もしない。
  if(!m_isAlive || IsKnockedDown() || m_kind == ENEMY_KIND::e_AREA)return false;

  // 転倒の時間を設定し、怯みは転倒へ置き換えるため解除する。
  m_knockdownRemainingSeconds = ns_KNOCKDOWN_SECONDS;
  m_flinchRemainingSeconds = 0.0f;
  return true;
}

// 射撃のダメージの印を付ける。
void Enemy::ApplyShotDamageMark(float damageScale,float durationSeconds)
{
  // 死亡している敵には、印を付けない。
  if(!m_isAlive)return;

  // 倍率と時間を、新しい値へ置き換える。すでに印がある場合も、時間を数え直す。
  m_shotDamageScale = damageScale;
  m_shotDamageMarkRemainingSeconds = durationSeconds;
}

// HPと怯み・転倒・印を戻し、死亡中ならColliderを登録し直す。
void Enemy::Respawn(CollisionWorld& collisionWorld)
{
  // 未初期化（Rendererを借用していない）の場合は、生き返らせる対象がないため戻る。
  if(m_cubeRenderer == nullptr)return;

  // HPを満タンにして、怯みと転倒と射撃のダメージの印と、攻撃の予告の表示と、予告の前傾・攻撃の突進を解除する。
  const ENEMY_PARAMS& params = ENEMY_TABLE::GetParams(m_kind);
  m_currentHP = params.maxHP;
  m_flinchRemainingSeconds = 0.0f;
  m_knockdownRemainingSeconds = 0.0f;
  m_shotDamageMarkRemainingSeconds = 0.0f;
  m_shotDamageScale = 1.0f;
  m_isTelegraphing = false;
  m_animation.telegraphSeconds = 0.0f;
  m_animation.lungeRemainingSeconds = 0.0f;
  m_animation.hasPendingDefeatEffect = false;
  ResetModelState(0.0f);

  // 死亡中だった場合だけ、Colliderを同じ位置へ登録し直して生きている状態にする。生きている場合は二重登録しない。
  if(!m_isAlive){
    m_colliderHandle = collisionWorld.AddSphere(COLLISION_SPHERE{m_position,params.colliderRadius},COLLISION_LAYER::ns_ENEMY,m_enemyId);
    m_isAlive = true;
  }
}

//========= Getter関数=========
// 最大HPを返す。
float Enemy::GetMaxHP() const
{
  return ENEMY_TABLE::GetParams(m_kind).maxHP;
}

// 敵の位置を中心にした、Colliderと同じ半径の球を返す。
COLLISION_SPHERE Enemy::GetBodySphere() const
{
  return COLLISION_SPHERE{m_position,ENEMY_TABLE::GetParams(m_kind).colliderRadius};
}

// Cubeで描く。
void Enemy::DrawCube(GraphicsSystem& graphicsSystem,const CAMERA_MATRICES& camera) const
{
  // 描画用のTransformと色を決める。通常は保持しているTransformと、種類の色。射撃のダメージの印が付いている間は、印の色にする。予告・怯み・転倒の色が、それより優先する。
  const ENEMY_PARAMS& params = ENEMY_TABLE::GetParams(m_kind);
  Transform draw_transform = m_transform;
  const XMFLOAT4 color = DecideStateColor(IsShotDamageMarked() ? ns_MARK_COLOR : params.normalColor);

  // 転倒中は、高さを低く潰し、足元の位置が変わらないよう中心を下げる。中心を下げる量は、潰した高さの分の半分。
  if(IsKnockedDown()){
    const float squashed_height = params.cubeSize * ns_KNOCKDOWN_HEIGHT_SCALE;
    draw_transform.SetScale(XMFLOAT3{params.cubeSize,squashed_height,params.cubeSize});
    draw_transform.SetPosition(XMFLOAT3{m_position.x,m_position.y - (params.cubeSize - squashed_height) * 0.5f,m_position.z});
  }

  // 簡易アニメーション（揺れ・突進・傾き・前傾）を、描画用のTransformへ反映する。転倒中は、反映の中で何もしない。
  ApplyAnimation(draw_transform);

  // 決めた色・UV倍率1・Object画像を、1つの構造体にまとめて、Cubeを描く。
  const BasicMeshRenderer::CUBE_APPEARANCE appearance{color,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Object};
  m_cubeRenderer->DrawCube(graphicsSystem,draw_transform.GetWorldMatrix(),camera,appearance);
}

// モデルで描く。
bool Enemy::DrawModel(GraphicsSystem& graphicsSystem,const CAMERA_MATRICES& camera,const XMFLOAT4& color) const
{
  // 共有の読込がない、この種類にモデルの見た目がない、または読込済みでない場合は、Cubeで描く必要がある。
  EnemyModelLibrary* library = m_modelState.library;
  ENEMY_APPEARANCE appearance{};
  if(library == nullptr || !EnemyModelLibrary::TryGetAppearance(m_kind,m_enemyId,appearance) || !library->IsLoaded(appearance))return false;

  // 再生中の動きの再生部と、描画部を取り出す。どちらかがなければ、Cubeで描く必要がある。
  SkinnedAnimationPlayer* player = library->FindPlayer(appearance,m_modelState.clip);
  SkinnedModelRenderer* renderer = library->FindRenderer(appearance);
  if(player == nullptr || renderer == nullptr)return false;

  // 死亡後は、動きが終わり、最後のポーズの保持の時間が過ぎたら、何も描かない。Cubeへは戻さない。
  if(!m_isAlive && m_modelState.clipTimeSeconds > player->GetDurationSeconds() + ENEMY_ANIMATION::ns_MODEL_DEATH_HOLD_SECONDS)return true;

  // 時刻を整えて、全ボーンの行列を求め、位置・向き・倍率から作ったワールド行列で描く。
  BoneMatrixArray bone_matrices{};
  player->Evaluate(player->NormalizeTime(m_modelState.clipTimeSeconds,IsLoopingClip(m_modelState.clip)),bone_matrices);
  SKINNED_DRAW_PARAMS draw_params{};
  draw_params.worldMatrix = EnemyModelLibrary::BuildWorldMatrix(appearance,m_position,m_transform.GetRotation().y);
  draw_params.boneMatrices = &bone_matrices;
  draw_params.color = color;
  renderer->Draw(graphicsSystem,camera,draw_params);
  return true;
}

// 状態に応じた色を返す。
XMFLOAT4 Enemy::DecideStateColor(const XMFLOAT4& baseColor) const
{
  // 転倒の表示を最優先にし、次に怯み、次に攻撃の予告とする。
  if(IsKnockedDown())return ns_KNOCKDOWN_COLOR;
  if(IsFlinching())return ns_FLINCH_COLOR;
  if(m_isTelegraphing)return ns_TELEGRAPH_COLOR;
  return baseColor;
}

//========= モデルの動きの補助関数=========
// 状態から動きを選び、時刻を進める。
void Enemy::UpdateModelClip(float deltaTime)
{
  // 共有の読込がない場合と、時間が進まない（ヒットストップ中）場合は、動きも時刻も変えない。
  if(m_modelState.library == nullptr || deltaTime <= 0.0f)return;

  // この敵がモデルで描かれる場合だけ、動きを選ぶ。
  ENEMY_APPEARANCE appearance{};
  if(!EnemyModelLibrary::TryGetAppearance(m_kind,m_enemyId,appearance) || !m_modelState.library->IsLoaded(appearance))return;

  // 前回の更新からの水平の移動の距離を、移動中と判定する下限の距離と比べる。比較は二乗のまま行い、平方根を取らない。
  const float move_x = m_position.x - m_modelState.lastPosition.x;
  const float move_z = m_position.z - m_modelState.lastPosition.z;
  const float min_distance = ENEMY_ANIMATION::ns_MODEL_WALK_MIN_SPEED * deltaTime;
  const bool is_moving = move_x * move_x + move_z * move_z > min_distance * min_distance;
  m_modelState.lastPosition = m_position;

  // 予告が始まった更新を検出する。
  const bool is_telegraph_start = m_isTelegraphing && !m_modelState.wasTelegraphing;
  m_modelState.wasTelegraphing = m_isTelegraphing;

  // 現在の動きを最後まで再生したかを調べ、次の動きを選ぶ。
  const SkinnedAnimationPlayer* current_player = m_modelState.library->FindPlayer(appearance,m_modelState.clip);
  const bool is_finished = current_player == nullptr || m_modelState.clipTimeSeconds >= current_player->GetDurationSeconds();
  const ENEMY_CLIP next_clip = ChooseClip(is_moving,is_finished);

  // 動きが変わった時と、攻撃の予告が始まった時は、最初から数え直す。連続した攻撃で、前の攻撃の動きが残らないようにする。
  if(next_clip != m_modelState.clip || (is_telegraph_start && next_clip == ENEMY_CLIP::e_ATTACK)){
    m_modelState.clip = next_clip;
    m_modelState.clipTimeSeconds = 0.0f;
  }
  m_modelState.clipTimeSeconds += deltaTime * GetClipSpeed(m_modelState.clip);
}

// 状態から、再生する動きを選ぶ。
ENEMY_CLIP Enemy::ChooseClip(bool isMoving,bool isClipFinished) const
{
  const ENEMY_CLIP current_clip = m_modelState.clip;

  // 死亡・転倒・怯みは、状態そのものを表すため、ほかの動きより優先する。
  if(!m_isAlive)return ENEMY_CLIP::e_DEATH;
  if(IsKnockedDown())return ENEMY_CLIP::e_DOWN;
  if(IsFlinching())return ENEMY_CLIP::e_HIT;

  // 転倒が終わった直後は、起き上がりを再生し、最後まで続ける。
  if(current_clip == ENEMY_CLIP::e_DOWN)return ENEMY_CLIP::e_GET_UP;
  if(current_clip == ENEMY_CLIP::e_GET_UP && !isClipFinished)return ENEMY_CLIP::e_GET_UP;

  // 攻撃の予告中は、攻撃の動きを再生する。予告が終わっても、動きが終わるまで、続ける。
  if(m_isTelegraphing)return ENEMY_CLIP::e_ATTACK;
  if(current_clip == ENEMY_CLIP::e_ATTACK && !isClipFinished)return ENEMY_CLIP::e_ATTACK;

  return isMoving ? ENEMY_CLIP::e_WALK : ENEMY_CLIP::e_IDLE;
}

// モデルの動きの状態を、待機の最初へ戻す。
void Enemy::ResetModelState(float idleStartSeconds)
{
  m_modelState.clip = ENEMY_CLIP::e_IDLE;
  m_modelState.clipTimeSeconds = idleStartSeconds;
  m_modelState.lastPosition = m_position;
  m_modelState.wasTelegraphing = false;
}

//========= 描画の補助関数=========
// 簡易アニメーションを、描画用のTransformへ反映する。
void Enemy::ApplyAnimation(Transform& drawTransform) const
{
  // 転倒中は、倒れたまま動かさないため、揺れ・突進・傾きを全て止める。
  if(IsKnockedDown())return;

  // 保持している向き（Yaw）を、正面方向の計算と、回転の基準に使う。
  const XMFLOAT3& facing_rotation = m_transform.GetRotation();

  // 待機の揺れを高さへ、突進を正面方向の水平位置へ足す。正面方向は、Yawから求める。位置を変えるのは描画用のTransformだけで、Colliderと保持座標は変えない。
  const float lunge_offset = CalculateLungeOffset(m_animation.lungeRemainingSeconds);
  const float lunge_x = std::sin(facing_rotation.y) * lunge_offset;
  const float lunge_z = std::cos(facing_rotation.y) * lunge_offset;
  drawTransform.SetPosition(XMFLOAT3{m_position.x + lunge_x,m_position.y + CalculateIdleBobOffset(m_animation.idleBobPhase),m_position.z + lunge_z});

  // 怯み中は、向き（PitchとYaw）を保ったまま、Rollだけを傾ける。Z軸は敵の正面方向のため、傾きは正面から見て横方向になる。
  if(IsFlinching()){
    drawTransform.SetRotation(XMFLOAT3{facing_rotation.x,facing_rotation.y,CalculateFlinchTilt(m_flinchRemainingSeconds,m_animation.flinchTiltSign)});
  }
  // 攻撃の予告中は、向き（YawとRoll）を保ったまま、Pitchで前へ傾ける。怯みの傾きを優先する。
  else if(m_isTelegraphing){
    drawTransform.SetRotation(XMFLOAT3{CalculateTelegraphLean(m_animation.telegraphSeconds),facing_rotation.y,facing_rotation.z});
  }
}