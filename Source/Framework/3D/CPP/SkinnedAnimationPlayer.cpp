#include "../H/SkinnedAnimationPlayer.h"

// ボーン行列の計算を実装する。ノードとチャンネルの対応づけ、キーの補間（位置・拡大は直線、回転は球面）、2つの動きの混ぜ合わせ、階層の変換の合成、水平の位置の固定、逆バインド行列との合成、ノードの位置の計算を行う。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::upper_boundでキーを探し、std::clampで補間の割合を0〜1へ制限する。
#include <cmath> // std::fmodで、繰り返しの時刻を、長さで割った余りにする。
#include <string> // std::stringで、チャンネルの名前を扱い、ログの文字列を組み立てる。
#include <unordered_map> // チャンネルの名前から、チャンネルの番号を引く。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

// キーの探索と補間、姿勢の合成と混ぜ合わせの補助関数をまとめる。
namespace
{
  //========= キーの補助関数=========
  // 時刻以前で、最も新しいキーの添字を返す。最初のキーより前の時刻は、0を返す。キーは、1個以上あること。
  template<class TKey>
  std::size_t FindKeyIndex(const std::vector<TKey>& keys,float timeSeconds)
  {
    const auto upper = std::upper_bound(keys.begin(),keys.end(),timeSeconds,[](float time,const TKey& key){return time < key.timeSeconds; });
    return upper == keys.begin() ? 0 : static_cast<std::size_t>(upper - keys.begin()) - 1;
  }

  // 2つのキーの間の、補間の割合を返す。時刻が同じキーは0。0〜1へ制限する。
  float CalculateRatio(float startSeconds,float endSeconds,float timeSeconds)
  {
    const float span = endSeconds - startSeconds;
    return span > 0.0f ? std::clamp((timeSeconds - startSeconds) / span,0.0f,1.0f) : 0.0f;
  }

  // 位置または拡大を、直線補間で求める。キーがなければ、fallbackを返す。
  XMVECTOR SampleVector(const std::vector<ANIMATION_VECTOR_KEY>& keys,float timeSeconds,XMVECTOR fallback)
  {
    if(keys.empty())return fallback;

    // 最後のキー以降は、最後の値のまま止める。
    const std::size_t index = FindKeyIndex(keys,timeSeconds);
    if(index + 1 >= keys.size())return XMLoadFloat3(&keys[index].value);

    const float ratio = CalculateRatio(keys[index].timeSeconds,keys[index + 1].timeSeconds,timeSeconds);
    return XMVectorLerp(XMLoadFloat3(&keys[index].value),XMLoadFloat3(&keys[index + 1].value),ratio);
  }

  // 回転を、球面補間で求める。キーがなければ、fallbackを返す。
  XMVECTOR SampleRotation(const std::vector<ANIMATION_ROTATION_KEY>& keys,float timeSeconds,XMVECTOR fallback)
  {
    if(keys.empty())return fallback;

    // 最後のキー以降は、最後の値のまま止める。
    const std::size_t index = FindKeyIndex(keys,timeSeconds);
    const XMVECTOR start_rotation = XMLoadFloat4(&keys[index].value);
    if(index + 1 >= keys.size())return XMQuaternionNormalize(start_rotation);

    const float ratio = CalculateRatio(keys[index].timeSeconds,keys[index + 1].timeSeconds,timeSeconds);
    return XMQuaternionNormalize(XMQuaternionSlerp(start_rotation,XMLoadFloat4(&keys[index + 1].value),ratio));
  }

  //========= 姿勢の補助関数=========
  // 姿勢を、行ベクトルの向きで「拡大 × 回転 × 位置」の順に合成した行列にする。
  XMMATRIX ComposePoseMatrix(const ANIMATION_NODE_POSE& pose)
  {
    const XMMATRIX scale_rotation = XMMatrixMultiply(XMMatrixScalingFromVector(XMLoadFloat3(&pose.scale)),XMMatrixRotationQuaternion(XMLoadFloat4(&pose.rotation)));
    return XMMatrixMultiply(scale_rotation,XMMatrixTranslationFromVector(XMLoadFloat3(&pose.translation)));
  }

  // 2つの姿勢を混ぜる。拡大と位置は直線補間、回転は球面補間。
  // from: 割合0の姿勢。
  // to: 割合1の姿勢。
  // weight: 割合。0〜1。
  ANIMATION_NODE_POSE BlendPose(const ANIMATION_NODE_POSE& from,const ANIMATION_NODE_POSE& to,float weight)
  {
    ANIMATION_NODE_POSE blended{};
    XMStoreFloat3(&blended.scale,XMVectorLerp(XMLoadFloat3(&from.scale),XMLoadFloat3(&to.scale),weight));
    XMStoreFloat4(&blended.rotation,XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&from.rotation),XMLoadFloat4(&to.rotation),weight)));
    XMStoreFloat3(&blended.translation,XMVectorLerp(XMLoadFloat3(&from.translation),XMLoadFloat3(&to.translation),weight));
    return blended;
  }
}

//========= 初期化・終了関数=========
// モデルとアニメーションを借用し、ノードとチャンネルを対応づける。
bool SkinnedAnimationPlayer::Initialize(const SKINNED_MODEL_DATA& model,const SKINNED_ANIMATION_CLIP& clip)
{
  Uninit();
  if(model.nodes.empty()){
    Logger::Write(e_LogLevel::e_WARNING,e_LogCategory::e_ASSET,L"SkinnedAnimationPlayerの初期化に失敗しました。モデルにノードがありません。");
    return false;
  }

  // チャンネルの名前から番号を引く表を作り、ノードごとに、同じ名前のチャンネルの番号を調べる。対応がなければ-1。
  std::unordered_map<std::string,int> channel_lookup;
  for(std::size_t channel_index = 0; channel_index < clip.channels.size(); ++channel_index){
    channel_lookup[clip.channels[channel_index].nodeName] = static_cast<int>(channel_index);
  }
  std::vector<int> channel_index_by_node(model.nodes.size(),-1);
  std::size_t matched_node_count = 0;
  for(std::size_t node_index = 0; node_index < model.nodes.size(); ++node_index){
    const auto found = channel_lookup.find(model.nodes[node_index].name);
    if(found == channel_lookup.end())continue;
    channel_index_by_node[node_index] = found->second;
    ++matched_node_count;
  }
  if(matched_node_count == 0){
    Logger::Write(e_LogLevel::e_WARNING,e_LogCategory::e_ASSET,L"SkinnedAnimationPlayerの初期化に失敗しました。ノードに対応するチャンネルがありません。");
    return false;
  }

  // 動かされるボーンの数を数えて、対応の状況を出す。
  std::size_t animated_bone_count = 0;
  for(const SKINNED_BONE& bone : model.bones){
    if(channel_index_by_node[static_cast<std::size_t>(bone.nodeIndex)] >= 0)++animated_bone_count;
  }
  Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,
                L"アニメーションを対応づけました: チャンネル " + std::to_wstring(clip.channels.size())
                + L" 中 " + std::to_wstring(matched_node_count) + L" がノードに対応 / ボーン " + std::to_wstring(model.bones.size())
                + L" 中 " + std::to_wstring(animated_bone_count) + L" が動く");

  m_model = &model;
  m_clip = &clip;
  m_channelIndexByNode = std::move(channel_index_by_node);
  m_globalTransforms.assign(model.nodes.size(),XMFLOAT4X4{});
  return true;
}

// 借用した参照と、作業領域を手放す。
void SkinnedAnimationPlayer::Uninit()
{
  m_model = nullptr;
  m_clip = nullptr;
  m_channelIndexByNode.clear();
  m_globalTransforms.clear();
  m_horizontalLockNodeIndex = -1;
}

//========= 計算関数=========
// 時刻を、アニメーションの範囲へ整える。
float SkinnedAnimationPlayer::NormalizeTime(float timeSeconds,bool isLooping) const
{
  const float duration = GetDurationSeconds();
  if(duration <= 0.0f)return 0.0f;

  // 繰り返す場合は、長さで割った余り（負の時刻は、後ろから数える）、繰り返さない場合は、範囲へ制限する。
  if(!isLooping)return std::clamp(timeSeconds,0.0f,duration);
  const float wrapped = std::fmod(timeSeconds,duration);
  return wrapped < 0.0f ? wrapped + duration : wrapped;
}

// 指定した時刻の、全ボーンの最終の行列を計算する。
void SkinnedAnimationPlayer::Evaluate(float timeSeconds,BoneMatrixArray& outBoneMatrices)
{
  // 先に全て単位行列にして、使わない分と、借用していない場合に備える。
  XMFLOAT4X4 identity{};
  XMStoreFloat4x4(&identity,XMMatrixIdentity());
  outBoneMatrices.fill(identity);
  if(m_model == nullptr)return;

  ComputeGlobalTransforms(timeSeconds,nullptr);
  ComposeBoneMatrices(outBoneMatrices);
}

// 直前の動きと混ぜた、全ボーンの最終の行列を計算する。
void SkinnedAnimationPlayer::EvaluateBlended(float timeSeconds,const ANIMATION_BLEND_INPUT& blendInput,BoneMatrixArray& outBoneMatrices)
{
  // 先に全て単位行列にして、使わない分と、借用していない場合に備える。
  XMFLOAT4X4 identity{};
  XMStoreFloat4x4(&identity,XMMatrixIdentity());
  outBoneMatrices.fill(identity);
  if(m_model == nullptr)return;

  // 混ぜる相手がない、または、別のモデルの場合は、この動きだけで計算する。
  const SkinnedAnimationPlayer* previous_player = blendInput.previousPlayer;
  const bool can_blend = previous_player != nullptr && previous_player->m_model == m_model;
  ComputeGlobalTransforms(timeSeconds,can_blend ? &blendInput : nullptr);
  ComposeBoneMatrices(outBoneMatrices);
}

// 指定した時刻の、ノードの位置を、モデル空間で求める。
void SkinnedAnimationPlayer::EvaluateNodePositions(float timeSeconds,const std::vector<int>& nodeIndices,std::vector<XMFLOAT3>& outPositions)
{
  // 先に、全て原点にする。借用していない場合や、範囲外の番号は、原点のまま。
  outPositions.assign(nodeIndices.size(),XMFLOAT3{});
  if(m_model == nullptr)return;

  // 全体の変換を求め、ボーン行列と同じように、根の逆行列を掛けて、スキニング後の頂点と同じ空間にする。位置は、行列の4行目。
  ComputeGlobalTransforms(timeSeconds,nullptr);
  const XMMATRIX global_inverse = XMLoadFloat4x4(&m_model->globalInverseTransform);
  for(std::size_t position_index = 0; position_index < nodeIndices.size(); ++position_index){
    const int node_index = nodeIndices[position_index];
    if(node_index < 0 || static_cast<std::size_t>(node_index) >= m_globalTransforms.size())continue;

    const XMMATRIX node_in_model = XMMatrixMultiply(XMLoadFloat4x4(&m_globalTransforms[static_cast<std::size_t>(node_index)]),global_inverse);
    XMStoreFloat3(&outPositions[position_index],node_in_model.r[3]);
  }
}

// アニメーションの長さを返す。
float SkinnedAnimationPlayer::GetDurationSeconds() const
{
  return m_clip != nullptr ? m_clip->durationSeconds : 0.0f;
}

// 名前の末尾が一致する、最初のノードの番号を返す。
int SkinnedAnimationPlayer::FindNodeIndexBySuffix(const std::string& suffix) const
{
  if(m_model == nullptr || suffix.empty())return -1;

  for(std::size_t node_index = 0; node_index < m_model->nodes.size(); ++node_index){
    const std::string& name = m_model->nodes[node_index].name;
    if(name.size() >= suffix.size() && name.compare(name.size() - suffix.size(),suffix.size(),suffix) == 0){
      return static_cast<int>(node_index);
    }
  }
  return -1;
}

//========= 計算の補助関数=========
// 全てのノードの、親を含めた全体の変換を計算する。ノードは親が先に並ぶため、先頭から順に、親の結果を使える。
void SkinnedAnimationPlayer::ComputeGlobalTransforms(float timeSeconds,const ANIMATION_BLEND_INPUT* blend)
{
  // 水平の位置を固定するノードを決める。固定がなければ、-1で、どのノードにも一致しない。
  const int lock_node_index = DecideHorizontalLockNode(blend);

  for(std::size_t node_index = 0; node_index < m_model->nodes.size(); ++node_index){
    const XMMATRIX local_transform = BuildLocalTransform(static_cast<int>(node_index),timeSeconds,blend);
    const int parent_index = m_model->nodes[node_index].parentIndex;

    // 根は、親から見た変換が、そのまま全体の変換。それ以外は、行ベクトルの向きで、「自分 × 親」。
    if(parent_index < 0){
      XMStoreFloat4x4(&m_globalTransforms[node_index],local_transform);
    }
    else{
      const XMMATRIX parent_global = XMLoadFloat4x4(&m_globalTransforms[static_cast<std::size_t>(parent_index)]);
      XMStoreFloat4x4(&m_globalTransforms[node_index],XMMatrixMultiply(local_transform,parent_global));
    }

    // 固定するノードは、子の計算より先に、水平の位置を戻す。子は、戻した後の位置を、親として使う。
    if(static_cast<int>(node_index) == lock_node_index){
      ApplyHorizontalLock(node_index);
    }
  }
}

// 水平の位置を固定するノードの番号を決める。
int SkinnedAnimationPlayer::DecideHorizontalLockNode(const ANIMATION_BLEND_INPUT* blend) const
{
  // この動きに固定があれば、それを使う。
  if(m_horizontalLockNodeIndex >= 0)return m_horizontalLockNodeIndex;

  // この動きになければ、混ぜている間だけ、直前の動きの固定を使う。直前の動きのずれが、混ぜている間に残らないようにする。
  return blend != nullptr ? blend->previousPlayer->m_horizontalLockNodeIndex : -1;
}

// ノード1つの、全体の変換の位置を、モデル空間で水平が原点になるよう、ずらす。
void SkinnedAnimationPlayer::ApplyHorizontalLock(std::size_t nodeIndex)
{
  // ノードの位置を、スキニング後の頂点と同じ空間（モデル空間）で求める。位置は、行列の4行目。
  const XMMATRIX global_inverse = XMLoadFloat4x4(&m_model->globalInverseTransform);
  XMMATRIX node_global = XMLoadFloat4x4(&m_globalTransforms[nodeIndex]);
  const XMVECTOR model_position = XMMatrixMultiply(node_global,global_inverse).r[3];

  // モデル空間で、水平（XとZ）を原点へ戻すずれを、全体の変換の空間へ直し、ノードの位置へ足す。高さと向きは、変えない。
  const XMVECTOR model_offset = XMVectorSet(-XMVectorGetX(model_position),0.0f,-XMVectorGetZ(model_position),0.0f);
  const XMVECTOR global_offset = XMVector3TransformNormal(model_offset,XMMatrixInverse(nullptr,global_inverse));
  node_global.r[3] = XMVectorAdd(node_global.r[3],global_offset);
  XMStoreFloat4x4(&m_globalTransforms[nodeIndex],node_global);
}

// ボーンごとに「逆バインド行列 × ノードの全体の変換 × 根の逆行列」を合成して出力する。
void SkinnedAnimationPlayer::ComposeBoneMatrices(BoneMatrixArray& outBoneMatrices) const
{
  const XMMATRIX global_inverse = XMLoadFloat4x4(&m_model->globalInverseTransform);
  for(std::size_t bone_index = 0; bone_index < m_model->bones.size(); ++bone_index){
    const SKINNED_BONE& bone = m_model->bones[bone_index];
    const XMMATRIX offset = XMLoadFloat4x4(&bone.offsetMatrix);
    const XMMATRIX node_global = XMLoadFloat4x4(&m_globalTransforms[static_cast<std::size_t>(bone.nodeIndex)]);
    XMStoreFloat4x4(&outBoneMatrices[bone_index],XMMatrixMultiply(XMMatrixMultiply(offset,node_global),global_inverse));
  }
}

// ノード1つの、親から見た変換を作る。
XMMATRIX SkinnedAnimationPlayer::BuildLocalTransform(int nodeIndex,float timeSeconds,const ANIMATION_BLEND_INPUT* blend) const
{
  // どちらの動きにもチャンネルがないノードは、基準の姿勢のまま。基準の行列を、分解と合成で変えないよう、そのまま返す。
  const std::size_t node_slot = static_cast<std::size_t>(nodeIndex);
  const XMMATRIX bind_transform = XMLoadFloat4x4(&m_model->nodes[node_slot].localTransform);
  const bool has_current_channel = m_channelIndexByNode[node_slot] >= 0;

  // 混ぜない場合は、この動きだけの姿勢から作る。
  if(blend == nullptr)return has_current_channel ? ComposePoseMatrix(SampleNodePose(nodeIndex,timeSeconds)) : bind_transform;

  // 混ぜる場合は、直前の動きの姿勢から、今の動きの姿勢へ、割合で混ぜる。
  const SkinnedAnimationPlayer& previous = *blend->previousPlayer;
  if(!has_current_channel && previous.m_channelIndexByNode[node_slot] < 0)return bind_transform;

  const ANIMATION_NODE_POSE previous_pose = previous.SampleNodePose(nodeIndex,blend->previousTimeSeconds);
  return ComposePoseMatrix(BlendPose(previous_pose,SampleNodePose(nodeIndex,timeSeconds),blend->currentWeight));
}

// ノード1つの、親から見た姿勢を、この動きだけで求める。
ANIMATION_NODE_POSE SkinnedAnimationPlayer::SampleNodePose(int nodeIndex,float timeSeconds) const
{
  // 基準の姿勢を、拡大・回転・位置へ分け、キーのない種類の代わりに使う。
  const std::size_t node_slot = static_cast<std::size_t>(nodeIndex);
  XMVECTOR scale{};
  XMVECTOR rotation{};
  XMVECTOR translation{};
  XMMatrixDecompose(&scale,&rotation,&translation,XMLoadFloat4x4(&m_model->nodes[node_slot].localTransform));

  // チャンネルがあるノードだけ、時刻の拡大・回転・位置を、補間で求める。
  const int channel_index = m_channelIndexByNode[node_slot];
  if(channel_index >= 0){
    const ANIMATION_CHANNEL& channel = m_clip->channels[static_cast<std::size_t>(channel_index)];
    scale = SampleVector(channel.scaleKeys,timeSeconds,scale);
    rotation = SampleRotation(channel.rotationKeys,timeSeconds,rotation);
    translation = SampleVector(channel.positionKeys,timeSeconds,translation);
  }

  ANIMATION_NODE_POSE pose{};
  XMStoreFloat3(&pose.scale,scale);
  XMStoreFloat4(&pose.rotation,rotation);
  XMStoreFloat3(&pose.translation,translation);
  return pose;
}