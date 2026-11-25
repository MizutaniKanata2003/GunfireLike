#include "../H/EffectController.h"

// 演出の粒の生成・更新・描画を実装する。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/3D/H/CameraMatrices.h"

//========= Scene インクルード=========
#include "Scene/Common/Object/H/ModelRegistry.h"
#include "Scene/Common/Object/H/Transform.h"

//========= 初期化・終了関数=========
// Cube Rendererを借用し、全ての粒を消す。
bool EffectController::Initialize(ModelRegistry& registry)
{
  // 共有のCube Rendererを借用する。
  m_cubeRenderer = registry.GetCubeRenderer();
  // Registryが未初期化ならnullptrが返るため、借用できず、失敗とする。
  if(m_cubeRenderer == nullptr)return false;

  // 前回の粒を持ち越さないよう、全ての粒を消す。
  Clear();
  return true;
}

// 全ての粒を消し、借用した参照を手放す。
void EffectController::Uninit()
{
  Clear();
  m_cubeRenderer = nullptr;
}

// 全ての粒を消し、生成の位置を先頭へ戻す。
void EffectController::Clear()
{
  // 全ての粒を先頭から、使われていない枠へ戻す。
  for(EFFECT_STATE& effect : m_effects){
    effect = EFFECT_STATE{};
  }
  m_nextIndex = 0;
}

//========= 更新・描画関数=========
// 粒を1個生成する。
void EffectController::Spawn(const EFFECT_SPAWN_PARAMS& params)
{
  // 残り時間が0以下の粒は、使われていない枠と区別できず、割合の計算で0による除算にもなるため、生成しない。
  if(params.lifetimeSeconds <= 0.0f)return;

  // 生成の位置から順に、使われていない枠を探す。見つからなければ、生成の位置の粒（最も古い）を置き換える。
  std::size_t target_index = m_nextIndex;
  for(std::size_t offset = 0; offset < m_effects.size(); ++offset){
    const std::size_t candidate_index = (m_nextIndex + offset) % m_effects.size();
    // 使われていない枠が見つかったら、探すのを止める。
    if(m_effects[candidate_index].remainingSeconds <= 0.0f){
      target_index = candidate_index;
      break;
    }
  }

  // 選んだ枠へ粒を設定し、次の探索の開始位置を、選んだ枠の次へ進める。
  m_effects[target_index] = EFFECT_STATE{params.position,params.velocity,params.color,params.size,params.lifetimeSeconds,params.lifetimeSeconds};
  m_nextIndex = (target_index + 1) % m_effects.size();
}

// 全ての粒の位置を進め、残り時間を減らし、終わった粒を消す。
void EffectController::Update(float deltaTime)
{
  // 全ての粒を先頭から更新する。
  for(EFFECT_STATE& effect : m_effects){
    // 使われていない枠は、更新せずに次へ進む。
    if(effect.remainingSeconds <= 0.0f)continue;

    // 残り時間を減らし、0以下になった粒は、使われていない枠へ戻して、次へ進む。
    effect.remainingSeconds -= deltaTime;
    if(effect.remainingSeconds <= 0.0f){
      effect = EFFECT_STATE{};
      continue;
    }

    // 残った粒は、速度の分だけ位置を進める。
    effect.position.x += effect.velocity.x * deltaTime;
    effect.position.y += effect.velocity.y * deltaTime;
    effect.position.z += effect.velocity.z * deltaTime;
  }
}

// 残っている全ての粒を、残り時間の割合に応じた大きさで描く。
void EffectController::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
  // Rendererを借用していない場合は、描画しない。
  if(m_cubeRenderer == nullptr)return;

  // 全ての粒に共通の、視点の行列を、1つの構造体にまとめる。回転はなし。
  const CAMERA_MATRICES camera{viewMatrix,projectionMatrix};
  Transform transform;
  transform.SetRotation(XMFLOAT3{});

  // 全ての粒を先頭から調べ、残っている粒だけ描く。
  for(const EFFECT_STATE& effect : m_effects){
    // 使われていない枠は、描かずに次へ進む。
    if(effect.remainingSeconds <= 0.0f)continue;

    // 生成した時の大きさに、残り時間の割合を掛け、時間とともに小さくする。Cubeは1辺1のため、倍率がそのまま1辺の長さになる。
    const float current_size = effect.size * (effect.remainingSeconds / effect.lifetimeSeconds);
    transform.SetPosition(effect.position);
    transform.SetScale(XMFLOAT3{current_size,current_size,current_size});

    // 画像を使わない単色（Color）で、粒の色・UV倍率1を指定して描く。
    const BasicMeshRenderer::CUBE_APPEARANCE appearance{effect.color,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Color};
    m_cubeRenderer->DrawCube(graphicsSystem,transform.GetWorldMatrix(),camera,appearance);
  }
}
