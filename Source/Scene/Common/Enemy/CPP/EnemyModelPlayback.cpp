#include "../H/EnemyModelPlayback.h"

// 敵1体のモデルの動きの選択・時刻の進行・描画を実装する。数値は、EnemyAnimationTableと、このファイルの定数にまとめた仮値。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampで、死亡の動きの進行の割合を0〜1へ制限する。

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyAnimationTable.h"

// 動きの切替と、死亡時の高さの補正の数値をまとめる。見た目を確認して、調整する仮値。
namespace
{
  //========= 動きの切替=========
  // 待機と移動の切替を行わない、最短の時間。単位は秒。この時間より前に、待機と移動が入れ替わっても、現在の動きを保つ。
  constexpr float ns_LOCOMOTION_MIN_HOLD_SECONDS = 0.25f;
  // 移動中と判定する、水平の速さの閾値（止まっている状態から）。単位はワールド距離単位/秒。
  constexpr float ns_MOVE_START_SPEED = 0.5f;
  // 止まったと判定する、水平の速さの閾値（移動中の状態から）。開始の閾値より小さくし、境目で判定が入れ替わらないようにする。単位はワールド距離単位/秒。
  constexpr float ns_MOVE_STOP_SPEED = 0.2f;

  //========= 死亡時の高さの補正=========
  // 倒れた体が床に埋まらないよう、死亡の動きの最後に、描画だけを持ち上げる量。単位はワールド距離単位。動きの進行に比例して、0から、この量まで上げる。
  constexpr float ns_DEATH_LIFT_Y = 0.12f;
}

// 動きの再生に使う、補助関数をまとめる。数値はEnemyAnimationTableから読む。
namespace
{
  // 動きを繰り返すかを返す。待機と移動だけが繰り返し、ほかは、最後のポーズで止まる。
  // clip: 動き。
  bool IsLoopingClip(ENEMY_CLIP clip)
  {
    return clip == ENEMY_CLIP::e_IDLE || clip == ENEMY_CLIP::e_WALK;
  }

  // 動きが、待機か移動かを返す。この2つの間の切替だけが、最短の保持の対象になる。
  // clip: 動き。
  bool IsLocomotionClip(ENEMY_CLIP clip)
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

  // 死亡の動きの進行に応じて、描画の高さへ足す量を返す。死亡以外の動きは、0。
  // clip: 再生中の動き。
  // clipTimeSeconds: 動きの時刻。単位は秒。
  // durationSeconds: 動きの長さ。単位は秒。
  // 戻り値: 高さへ足す量。単位はワールド距離単位。
  float CalculateDeathLift(ENEMY_CLIP clip,float clipTimeSeconds,float durationSeconds)
  {
    if(clip != ENEMY_CLIP::e_DEATH || durationSeconds <= 0.0f)return 0.0f;

    return std::clamp(clipTimeSeconds / durationSeconds,0.0f,1.0f) * ns_DEATH_LIFT_Y;
  }
}

//========= 設定・初期化関数=========
// 動きの状態を、待機の最初へ戻す。
void EnemyModelPlayback::Reset(const XMFLOAT3& position,float idleStartSeconds)
{
  m_clip = ENEMY_CLIP::e_IDLE;
  m_clipTimeSeconds = idleStartSeconds;
  m_clipHoldSeconds = 0.0f;
  m_lastPosition = position;
  m_isMoving = false;
  m_wasTelegraphing = false;
}

// 死亡の動きを、最初から始める。
void EnemyModelPlayback::StartDeath()
{
  m_clip = ENEMY_CLIP::e_DEATH;
  m_clipTimeSeconds = 0.0f;
  m_clipHoldSeconds = 0.0f;
}

//========= 更新・描画関数=========
// 状態から動きを選び、時刻を進める。
void EnemyModelPlayback::Update(const ENEMY_PLAYBACK_STATE& state,float deltaTime)
{
  // 共有の読込がない場合と、時間が進まない（ヒットストップ中）場合は、動きも時刻も変えない。
  if(m_library == nullptr || deltaTime <= 0.0f)return;

  // この敵がモデルで描かれる場合だけ、動きを選ぶ。
  ENEMY_APPEARANCE appearance{};
  if(!EnemyModelLibrary::TryGetAppearance(state.kind,state.enemyId,appearance) || !m_library->IsLoaded(appearance))return;

  // 移動中かを更新し、予告が始まった更新を検出する。
  UpdateMovingState(state.position,deltaTime);
  const bool is_telegraph_start = state.isTelegraphing && !m_wasTelegraphing;
  m_wasTelegraphing = state.isTelegraphing;

  // 現在の動きを最後まで再生したかを調べ、次の動きを選ぶ。待機と移動の切替は、最短の保持の時間を守る。
  const SkinnedAnimationPlayer* current_player = m_library->FindPlayer(appearance,m_clip);
  const bool is_finished = current_player == nullptr || m_clipTimeSeconds >= current_player->GetDurationSeconds();
  const ENEMY_CLIP next_clip = ApplyLocomotionHold(ChooseClip(state,is_finished));

  // 動きが変わった時と、攻撃の予告が始まった時は、最初から数え直す。連続した攻撃で、前の攻撃の動きが残らないようにする。
  if(next_clip != m_clip || (is_telegraph_start && next_clip == ENEMY_CLIP::e_ATTACK)){
    m_clip = next_clip;
    m_clipTimeSeconds = 0.0f;
    m_clipHoldSeconds = 0.0f;
  }
  m_clipTimeSeconds += deltaTime * GetClipSpeed(m_clip);
  m_clipHoldSeconds += deltaTime;
}

// モデルで描く。
bool EnemyModelPlayback::Draw(GraphicsSystem& graphicsSystem,const CAMERA_MATRICES& camera,const ENEMY_PLAYBACK_STATE& state) const
{
  // 共有の読込がない、この種類にモデルの見た目がない、または読込済みでない場合は、Cubeで描く必要がある。
  ENEMY_APPEARANCE appearance{};
  if(m_library == nullptr || !EnemyModelLibrary::TryGetAppearance(state.kind,state.enemyId,appearance) || !m_library->IsLoaded(appearance))return false;

  // 再生中の動きの再生部と、描画部を取り出す。どちらかがなければ、Cubeで描く必要がある。
  SkinnedAnimationPlayer* player = m_library->FindPlayer(appearance,m_clip);
  SkinnedModelRenderer* renderer = m_library->FindRenderer(appearance);
  if(player == nullptr || renderer == nullptr)return false;

  // 死亡後は、動きが終わり、最後のポーズの保持の時間が過ぎたら、何も描かない。Cubeへは戻さない。
  const float duration_seconds = player->GetDurationSeconds();
  if(!state.isAlive && m_clipTimeSeconds > duration_seconds + ENEMY_ANIMATION::ns_MODEL_DEATH_HOLD_SECONDS)return true;

  // 死亡の動きの間は、倒れた体が床に埋まらないよう、描画の位置だけを持ち上げる。
  XMFLOAT3 draw_position = state.position;
  draw_position.y += CalculateDeathLift(m_clip,m_clipTimeSeconds,duration_seconds);

  // 時刻を整えて、全ボーンの行列を求め、位置・向き・倍率から作ったワールド行列で描く。
  BoneMatrixArray bone_matrices{};
  player->Evaluate(player->NormalizeTime(m_clipTimeSeconds,IsLoopingClip(m_clip)),bone_matrices);
  SKINNED_DRAW_PARAMS draw_params{};
  draw_params.worldMatrix = EnemyModelLibrary::BuildWorldMatrix(appearance,draw_position,state.yawRadians);
  draw_params.boneMatrices = &bone_matrices;
  draw_params.color = state.color;
  renderer->Draw(graphicsSystem,camera,draw_params);
  return true;
}

//========= 更新の補助関数=========
// 移動中かを、開始と終了で別の閾値を使って更新する。
void EnemyModelPlayback::UpdateMovingState(const XMFLOAT3& position,float deltaTime)
{
  // 前回の更新からの水平の移動の距離を、速さ（距離/時間）の二乗として求める。比較は二乗のまま行い、平方根を取らない。
  const float move_x = position.x - m_lastPosition.x;
  const float move_z = position.z - m_lastPosition.z;
  const float speed_squared = (move_x * move_x + move_z * move_z) / (deltaTime * deltaTime);
  m_lastPosition = position;

  // 移動中の間は、低い閾値（止まったか）、止まっている間は、高い閾値（動き出したか）と比べる。境目の速さで、判定が毎フレーム入れ替わるのを防ぐ。
  const float threshold_speed = m_isMoving ? ns_MOVE_STOP_SPEED : ns_MOVE_START_SPEED;
  m_isMoving = speed_squared > threshold_speed * threshold_speed;
}

// 状態から、再生する動きを選ぶ。
ENEMY_CLIP EnemyModelPlayback::ChooseClip(const ENEMY_PLAYBACK_STATE& state,bool isClipFinished) const
{
  // 死亡・転倒・怯みは、状態そのものを表すため、ほかの動きより優先する。
  if(!state.isAlive)return ENEMY_CLIP::e_DEATH;
  if(state.isKnockedDown)return ENEMY_CLIP::e_DOWN;
  if(state.isFlinching)return ENEMY_CLIP::e_HIT;

  // 転倒が終わった直後は、起き上がりを再生し、最後まで続ける。
  if(m_clip == ENEMY_CLIP::e_DOWN)return ENEMY_CLIP::e_GET_UP;
  if(m_clip == ENEMY_CLIP::e_GET_UP && !isClipFinished)return ENEMY_CLIP::e_GET_UP;

  // 攻撃の予告中は、攻撃の動きを再生する。予告が終わっても、動きが終わるまで、続ける。
  if(state.isTelegraphing)return ENEMY_CLIP::e_ATTACK;
  if(m_clip == ENEMY_CLIP::e_ATTACK && !isClipFinished)return ENEMY_CLIP::e_ATTACK;

  return m_isMoving ? ENEMY_CLIP::e_WALK : ENEMY_CLIP::e_IDLE;
}

// 待機と移動の切替を、最短の保持の時間まで、見送る。
ENEMY_CLIP EnemyModelPlayback::ApplyLocomotionHold(ENEMY_CLIP desiredClip) const
{
  // 待機と移動の間の切替だけが、対象。怯み・転倒・攻撃・死亡との切替は、即座に通す。
  const bool is_locomotion_switch = IsLocomotionClip(desiredClip) && IsLocomotionClip(m_clip) && desiredClip != m_clip;
  if(is_locomotion_switch && m_clipHoldSeconds < ns_LOCOMOTION_MIN_HOLD_SECONDS)return m_clip;

  return desiredClip;
}