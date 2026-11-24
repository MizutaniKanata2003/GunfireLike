#include "../H/AreaTelegraph.h"

// 範囲攻撃型の予告の枠ごとの保持と、床の上の板としての描画を実装する。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/3D/H/CameraMatrices.h"

//========= Scene インクルード=========
#include "Scene/Common/Object/H/ModelRegistry.h"
#include "Scene/Common/Object/H/Transform.h"

//========= 初期化・終了関数=========
// Cube Rendererを借用し、全枠の予告を解除する。
bool AreaTelegraph::Initialize(ModelRegistry& registry)
{
  // 共有のCube Rendererを借用する。
  m_cubeRenderer = registry.GetCubeRenderer();
  // Registryが未初期化ならnullptrが返るため、借用できず、失敗とする。
  if(m_cubeRenderer == nullptr)return false;

  // 前回の予告を持ち越さないよう、全枠を解除する。
  Clear();
  return true;
}

// 全枠の予告を解除し、借用した参照を手放す。
void AreaTelegraph::Uninit()
{
  Clear();
  m_cubeRenderer = nullptr;
}

// 全枠の予告を解除する。
void AreaTelegraph::Clear()
{
  // 全枠を先頭から解除する。
  for(SLOT_STATE& slot : m_slots){
    slot = SLOT_STATE{};
  }
}

//========= 更新・描画関数=========
// 指定した枠の中心を固定して、予告を始める。
void AreaTelegraph::Begin(std::size_t slotIndex,const XMFLOAT3& center)
{
  // 枠の番号が範囲外の場合は、配列の外へ書かないよう、何もしない。
  if(slotIndex >= m_slots.size())return;

  // 中心を固定して予告中にする。高さは使わないため、0にそろえる。
  m_slots[slotIndex].center = XMFLOAT3{center.x,0.0f,center.z};
  m_slots[slotIndex].isActive = true;
}

// 指定した枠の予告を終える。
void AreaTelegraph::End(std::size_t slotIndex)
{
  // 枠の番号が範囲外の場合は、配列の外へ触れないよう、何もしない。
  if(slotIndex >= m_slots.size())return;

  m_slots[slotIndex].isActive = false;
}

// 予告中の枠だけ、範囲と同じ大きさの板を、床の上に描く。
void AreaTelegraph::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
  // Rendererを借用していない場合は、描画しない。
  if(m_cubeRenderer == nullptr)return;

  // 全ての板に共通の、回転と大きさを設定する。Cubeは1辺1のため、倍率がそのまま寸法になる。範囲は、中心から辺までの距離の2倍の正方形。
  const float side_length = AREA_TELEGRAPH::ns_HALF_SIZE * 2.0f;
  Transform transform;
  transform.SetRotation(XMFLOAT3{});
  transform.SetScale(XMFLOAT3{side_length,AREA_TELEGRAPH::ns_THICKNESS,side_length});

  // 全ての板に共通の、視点の行列と、色・UV倍率・Texture種別を、1つずつの構造体にまとめる。
  const CAMERA_MATRICES camera{viewMatrix,projectionMatrix};
  const BasicMeshRenderer::CUBE_APPEARANCE appearance{AREA_TELEGRAPH::ns_COLOR,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Object};

  // 全枠を先頭から調べ、予告中の枠だけ、範囲の中心へ板を置いて描く。
  for(const SLOT_STATE& slot : m_slots){
    // 予告中でない枠は、描かずに次の枠へ進む。
    if(!slot.isActive)continue;

    transform.SetPosition(XMFLOAT3{slot.center.x,AREA_TELEGRAPH::ns_CENTER_Y,slot.center.z});
    m_cubeRenderer->DrawCube(graphicsSystem,transform.GetWorldMatrix(),camera,appearance);
  }
}

//========= Getter関数=========
// 枠の番号が範囲内で、予告中の場合だけtrueを返す。
bool AreaTelegraph::IsActive(std::size_t slotIndex) const
{
  // 範囲外の番号は、予告中ではないものとして扱う。
  if(slotIndex >= m_slots.size())return false;

  return m_slots[slotIndex].isActive;
}

// 予告中の枠の中心を返す。それ以外は、原点を返す。
XMFLOAT3 AreaTelegraph::GetCenter(std::size_t slotIndex) const
{
  // 範囲外の番号、または予告中でない枠は、中心を持たないため、原点を返す。
  if(slotIndex >= m_slots.size() || !m_slots[slotIndex].isActive)return XMFLOAT3{};

  return m_slots[slotIndex].center;
}