#include "../H/EnemyModelPlayback.h"

// 敵1体のモデルの動きの選択（発覚しているかによる、待機と移動の切替を含む）・時刻の進行・動きの切替の補間・移動速度・描画を実装する。数値は、EnemyAnimationTableと、このファイルの定数にまとめた仮値。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::clampで、死亡の動きの進行の割合と、補間の割合と、死体の透明度を0〜1へ制限し、std::maxで、移動速度の下限を守る。

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyAnimationTable.h"

// 動きの切替と、発覚、移動速度、死亡時の高さの補正と、死体のフェードの数値をまとめる。見た目を確認して、調整する仮値。
namespace
{
  //========= 発覚=========
  // プレイヤーに発覚する距離。単位はワールド距離単位。この距離の内にいる間は、移動の動きを再生する。現在のフィールドは狭いため、常に発覚する大きさにした。将来、広いフィールドで、遠くの敵を待機にする時に、小さくする。仮値。
  constexpr float ns_ALERT_DISTANCE = 40.0f;
  // 発覚が解除される距離。単位はワールド距離単位。発覚する距離より大きくし、境目で、待機と移動が入れ替わり続けないようにする。仮値。
  constexpr float ns_LOSE_ALERT_DISTANCE = 50.0f;

  //========= 動きの切替=========
  // 待機と移動の切替を行わない、最短の時間。単位は秒。この時間より前に、待機と移動が入れ替わっても、現在の動きを保つ。
  constexpr float ns_LOCOMOTION_MIN_HOLD_SECONDS = 0.25f;

  //========= 歩きと移動速度=========
  // 歩きの動きの、再生の速さの倍率。足の運びが速くなり、足の速度の表から求めた移動速度も、同じ倍率で速くなるため、足は滑らない。1.0で、動きの本来の速さ（平均0.35程度で、遅い）。仮値。
  constexpr float ns_WALK_PLAYBACK_SPEED = 1.7f;
  // 移動速度の下限。歩きの速度の平均に対する割合。足の動きの位相によって、速度が0に近づいても、敵が止まったままにならないようにする。仮値。
  constexpr float ns_MIN_SPEED_RATIO = 0.3f;

  //========= 動きの補間=========
  // 動きの切替を、前の動きから混ぜる時間。単位は秒。待機・移動・攻撃・起き上がりへの切替に使う。仮値。
  constexpr float ns_BLEND_SECONDS = 0.2f;
  // 怯み・転倒・死亡へ切り替える時の、補間の時間。単位は秒。被弾への反応が遅れないよう、短くする。仮値。
  constexpr float ns_QUICK_BLEND_SECONDS = 0.08f;

  //========= 死亡時の高さの補正=========
  // 倒れた体が床に埋まらないよう、死亡の動きの最後に、描画だけを持ち上げる量。単位はワールド距離単位。動きの進行に比例して、0から、この量まで上げる。
  constexpr float ns_DEATH_LIFT_Y = 0.12f;

  //========= 死体のフェード=========
  // 死亡後の保持の時間が過ぎてから、死体が消えるまでの時間。単位は秒。この間に、画素を市松状に徐々に抜いて、透明にする。仮値。
  constexpr float ns_CORPSE_FADE_SECONDS = 2.0f;
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
    case ENEMY_CLIP::e_WALK:
    return ns_WALK_PLAYBACK_SPEED;

    case ENEMY_CLIP::e_HIT:
    return ENEMY_ANIMATION::ns_MODEL_SPEED_HIT;

    case ENEMY_CLIP::e_DOWN:
    return ENEMY_ANIMATION::ns_MODEL_SPEED_DOWN;

    case ENEMY_CLIP::e_GET_UP:
    return ENEMY_ANIMATION::ns_MODEL_SPEED_GET_UP;

    // 待機・予告・攻撃・死亡は、等倍。
    default:
    return ENEMY_ANIMATION::ns_MODEL_SPEED_NORMAL;
    }
  }

  // 切り替える先の動きに応じた、補間の時間を返す。怯み・転倒・死亡は、短い。
  // nextClip: 切り替える先の動き。
  // 戻り値: 補間の時間。単位は秒。
  float GetBlendSeconds(ENEMY_CLIP nextClip)
  {
    const bool is_quick = nextClip == ENEMY_CLIP::e_HIT || nextClip == ENEMY_CLIP::e_DOWN || nextClip == ENEMY_CLIP::e_DEATH;
    return is_quick ? ns_QUICK_BLEND_SECONDS : ns_BLEND_SECONDS;
  }

  // 補間の経過から、今の動きの割合を返す。始まりと終わりが滑らかになるよう、3次の補間（smoothstep）にする。
  // blend: 補間の状態。durationSecondsが正であること。
  // 戻り値: 今の動きの割合。0〜1。
  float CalculateBlendWeight(const ENEMY_CLIP_BLEND& blend)
  {
    const float ratio = std::clamp(blend.elapsedSeconds / blend.durationSeconds,0.0f,1.0f);
    return ratio * ratio * (3.0f - 2.0f * ratio);
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

  // 死体の透明度を返す。死亡の動きが終わり、保持の時間が過ぎるまでは1で、その後、フェードの時間をかけて、0へ下がる。
  // clipTimeSeconds: 死亡の動きの時刻。単位は秒。
  // durationSeconds: 死亡の動きの長さ。単位は秒。
  // 戻り値: 透明度。1で不透明、0で完全に透明。
  float CalculateCorpseAlpha(float clipTimeSeconds,float durationSeconds)
  {
    const float fade_elapsed_seconds = clipTimeSeconds - (durationSeconds + ENEMY_ANIMATION::ns_MODEL_DEATH_HOLD_SECONDS);
    return 1.0f - std::clamp(fade_elapsed_seconds / ns_CORPSE_FADE_SECONDS,0.0f,1.0f);
  }
}

//========= 設定・初期化関数=========
// 動きの状態を、発覚している状態の、移動の動きの最初へ戻し、補間を止める。
void EnemyModelPlayback::Reset(float startSeconds)
{
  m_clip = ENEMY_CLIP::e_WALK;
  m_clipTimeSeconds = startSeconds;
  m_clipHoldSeconds = 0.0f;
  m_blend = ENEMY_CLIP_BLEND{};
  m_isAlert = true;
  m_wasTelegraphing = false;
}

// 死亡の動きを、最初から始める。
void EnemyModelPlayback::StartDeath()
{
  SwitchClip(ENEMY_CLIP::e_DEATH);
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

  // 発覚しているかを更新し、予告が始まった更新を検出する。
  UpdateAlertState(state.playerDistance);
  const bool is_telegraph_start = state.isTelegraphing && !m_wasTelegraphing;
  m_wasTelegraphing = state.isTelegraphing;

  // 現在の動きを最後まで再生したかを調べ、次の動きを選ぶ。待機と移動の切替は、最短の保持の時間を守る。
  const SkinnedAnimationPlayer* current_player = m_library->FindPlayer(appearance,m_clip);
  const bool is_finished = current_player == nullptr || m_clipTimeSeconds >= current_player->GetDurationSeconds();
  const ENEMY_CLIP next_clip = ApplyLocomotionHold(ChooseClip(state,is_finished));

  // 動きが変わった時と、攻撃の予告が始まった時は、補間を始めて、最初から数え直す。連続した攻撃で、前の攻撃の動きが残らないようにする。
  if(next_clip != m_clip || (is_telegraph_start && next_clip == ENEMY_CLIP::e_ATTACK)){
    SwitchClip(next_clip);
  }
  m_clipTimeSeconds += deltaTime * GetClipSpeed(m_clip);
  m_clipHoldSeconds += deltaTime;
  AdvanceBlend(deltaTime);
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

  // 死亡後は、保持の時間が過ぎてから、画素を市松状に抜いて、徐々に消す。完全に透明になったら、何も描かない。Cubeへは戻さない。
  const float duration_seconds = player->GetDurationSeconds();
  const float corpse_alpha = state.isAlive ? 1.0f : CalculateCorpseAlpha(m_clipTimeSeconds,duration_seconds);
  if(corpse_alpha <= 0.0f)return true;

  // 死亡の動きの間は、倒れた体が床に埋まらないよう、描画の位置だけを持ち上げる。
  XMFLOAT3 draw_position = state.position;
  draw_position.y += CalculateDeathLift(m_clip,m_clipTimeSeconds,duration_seconds);

  // 全ボーンの行列を求め（補間中は直前の動きと混ぜ）、位置・向き・倍率から作ったワールド行列で描く。色のAlphaが、Pixel Shaderの、市松状に抜く濃さになる。
  BoneMatrixArray bone_matrices{};
  EvaluateBoneMatrices(appearance,*player,bone_matrices);
  SKINNED_DRAW_PARAMS draw_params{};
  draw_params.worldMatrix = EnemyModelLibrary::BuildWorldMatrix(appearance,draw_position,state.yawRadians);
  draw_params.boneMatrices = &bone_matrices;
  draw_params.color = state.color;
  draw_params.color.w *= corpse_alpha;
  renderer->Draw(graphicsSystem,camera,draw_params);
  return true;
}

// 足が地面を滑らない、追従の移動速度を返す。
float EnemyModelPlayback::GetMoveSpeed(const ENEMY_PLAYBACK_STATE& state,float fallbackSpeed) const
{
  // 歩きの速度の表がない（共有の読込がない・見た目がない・読込済みでない・足のノードが見つからなかった）場合は、既定の速度を使う。
  ENEMY_APPEARANCE appearance{};
  float average_speed = 0.0f;
  if(m_library == nullptr || !EnemyModelLibrary::TryGetAppearance(state.kind,state.enemyId,appearance) || !m_library->IsLoaded(appearance)
     || !m_library->TryGetWalkSpeedAverage(appearance,average_speed))return fallbackSpeed;

  // 歩きの動きの再生中は、動きの位相に対応する速度、それ以外（怯みの後など）は、平均の速度を使う。
  float ground_speed = average_speed;
  const SkinnedAnimationPlayer* walk_player = m_clip == ENEMY_CLIP::e_WALK ? m_library->FindPlayer(appearance,ENEMY_CLIP::e_WALK) : nullptr;
  if(walk_player != nullptr && walk_player->GetDurationSeconds() > 0.0f){
    const float phase = walk_player->NormalizeTime(m_clipTimeSeconds,true) / walk_player->GetDurationSeconds();
    static_cast<void>(m_library->TryGetWalkSpeed(appearance,phase,ground_speed));
  }

  // 位相によって速度が0に近づいても止まったままにならないよう、平均に対する下限を守り、歩きの再生の倍率を掛ける。
  return std::max(ground_speed,average_speed * ns_MIN_SPEED_RATIO) * GetClipSpeed(ENEMY_CLIP::e_WALK);
}

// 死体が、完全に消えたかを返す。
bool EnemyModelPlayback::IsCorpseGone(const ENEMY_PLAYBACK_STATE& state) const
{
  // 生きている敵は、死体ではない。
  if(state.isAlive)return false;

  // モデルで描けない場合（読込がない・見た目がない・動きがない）は、預かっても描けないため、消えたものとして扱う。
  ENEMY_APPEARANCE appearance{};
  if(m_library == nullptr || !EnemyModelLibrary::TryGetAppearance(state.kind,state.enemyId,appearance) || !m_library->IsLoaded(appearance))return true;
  const SkinnedAnimationPlayer* player = m_library->FindPlayer(appearance,m_clip);
  if(player == nullptr)return true;

  // 透明度が0以下になったら、消えた。
  return CalculateCorpseAlpha(m_clipTimeSeconds,player->GetDurationSeconds()) <= 0.0f;
}

//========= 更新の補助関数=========
// プレイヤーとの距離から、発覚しているかを、発覚する距離と解除される距離で別の閾値を使って更新する。
void EnemyModelPlayback::UpdateAlertState(float playerDistance)
{
  // 発覚している間は、遠い閾値（見失ったか）、していない間は、近い閾値（気づいたか）と比べる。境目の距離で、判定が毎フレーム入れ替わるのを防ぐ。
  const float threshold_distance = m_isAlert ? ns_LOSE_ALERT_DISTANCE : ns_ALERT_DISTANCE;
  m_isAlert = playerDistance <= threshold_distance;
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

  // 発覚している間は、立ち止まっている間も含めて、移動の動きを再生する。発覚していない間だけ、待機を再生する。
  return m_isAlert ? ENEMY_CLIP::e_WALK : ENEMY_CLIP::e_IDLE;
}

// 待機と移動の切替を、最短の保持の時間まで、見送る。
ENEMY_CLIP EnemyModelPlayback::ApplyLocomotionHold(ENEMY_CLIP desiredClip) const
{
  // 待機と移動の間の切替だけが、対象。怯み・転倒・攻撃・死亡との切替は、即座に通す。
  const bool is_locomotion_switch = IsLocomotionClip(desiredClip) && IsLocomotionClip(m_clip) && desiredClip != m_clip;
  if(is_locomotion_switch && m_clipHoldSeconds < ns_LOCOMOTION_MIN_HOLD_SECONDS)return m_clip;

  return desiredClip;
}

// 動きを切り替え、直前の動きから、短い時間で混ぜる。
void EnemyModelPlayback::SwitchClip(ENEMY_CLIP nextClip)
{
  // 直前の動きと、その時刻を、補間の元として残す。補間の途中で、さらに切り替わった場合は、その時点の動きだけを、元にする。
  m_blend.isActive = true;
  m_blend.previousClip = m_clip;
  m_blend.previousTimeSeconds = m_clipTimeSeconds;
  m_blend.elapsedSeconds = 0.0f;
  m_blend.durationSeconds = GetBlendSeconds(nextClip);

  // 今の動きを切り替え、時刻と保持の時間を、0へ戻す。
  m_clip = nextClip;
  m_clipTimeSeconds = 0.0f;
  m_clipHoldSeconds = 0.0f;
}

// 補間を進める。
void EnemyModelPlayback::AdvanceBlend(float deltaTime)
{
  if(!m_blend.isActive)return;

  // 直前の動きの時刻も、補間の間は進め、動きが止まって見えないようにする。補間の時間が過ぎたら、終える。
  m_blend.previousTimeSeconds += deltaTime * GetClipSpeed(m_blend.previousClip);
  m_blend.elapsedSeconds += deltaTime;
  if(m_blend.elapsedSeconds >= m_blend.durationSeconds){
    m_blend.isActive = false;
  }
}

//========= 描画の補助関数=========
// 全ボーンの最終の行列を求める。
void EnemyModelPlayback::EvaluateBoneMatrices(ENEMY_APPEARANCE appearance,SkinnedAnimationPlayer& player,BoneMatrixArray& outBoneMatrices) const
{
  // 今の動きの時刻を、動きの範囲へ整える。
  const float current_time = player.NormalizeTime(m_clipTimeSeconds,IsLoopingClip(m_clip));

  // 補間中でなければ、または、直前の動きの再生部がなければ、今の動きだけで求める。
  SkinnedAnimationPlayer* previous_player = m_blend.isActive ? m_library->FindPlayer(appearance,m_blend.previousClip) : nullptr;
  if(previous_player == nullptr){
    player.Evaluate(current_time,outBoneMatrices);
    return;
  }

  // 直前の動きと、その時刻、今の動きの割合を渡して、混ぜる。
  const float previous_time = previous_player->NormalizeTime(m_blend.previousTimeSeconds,IsLoopingClip(m_blend.previousClip));
  player.EvaluateBlended(current_time,ANIMATION_BLEND_INPUT{previous_player,previous_time,CalculateBlendWeight(m_blend)},outBoneMatrices);
}