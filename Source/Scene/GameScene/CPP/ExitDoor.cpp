#include "../H/ExitDoor.h"

// 出口の扉のCollider登録・解除と、開閉に応じたCube表示を実装する。
// 数値と色はExitDoor.hのEXIT_DOORにまとめた。Colliderは扉の見た目と同じ範囲のAABBで、STATICレイヤーに登録する。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/Collision/H/CollisionWorld.h"

//========= Scene インクルード=========
#include "Scene/Common/H/ModelRegistry.h"

// 扉のAABBを、中心と寸法から求める補助関数をまとめる。
namespace
{
  //========= 補助関数=========
  // 閉じた扉と同じ範囲のAABBを返す。
  // 戻り値: 最小点は中心から各軸の寸法の半分を引いた点、最大点は足した点。
  COLLISION_AABB BuildDoorAABB()
  {
    const XMFLOAT3& center = EXIT_DOOR::ns_DOOR_CENTER;
    const XMFLOAT3& size = EXIT_DOOR::ns_DOOR_SIZE;
    return COLLISION_AABB{XMFLOAT3{center.x - size.x * 0.5f,center.y - size.y * 0.5f,center.z - size.z * 0.5f},
                          XMFLOAT3{center.x + size.x * 0.5f,center.y + size.y * 0.5f,center.z + size.z * 0.5f}};
  }
}

//========= 初期化・終了関数=========
// Cube Rendererを借用し、閉じた扉のColliderを登録する。
bool ExitDoor::Initialize(ModelRegistry& registry,CollisionWorld& collisionWorld)
{
  // 共有のCube Rendererを借用する。
  m_cubeRenderer = registry.GetCubeRenderer();
  // Registryが未初期化ならnullptrが返るため、Colliderを登録せず失敗とする。
  if(m_cubeRenderer == nullptr)return false;

  // 扉と目印の位置・大きさを設定する。Cubeは1辺1のため、倍率がそのまま各軸の寸法になる。回転はしない。
  m_doorTransform.SetPosition(EXIT_DOOR::ns_DOOR_CENTER);
  m_doorTransform.SetRotation(XMFLOAT3{});
  m_doorTransform.SetScale(EXIT_DOOR::ns_DOOR_SIZE);
  m_padTransform.SetPosition(EXIT_DOOR::ns_PAD_CENTER);
  m_padTransform.SetRotation(XMFLOAT3{});
  m_padTransform.SetScale(EXIT_DOOR::ns_PAD_SIZE);

  // 閉じた状態で開始し、扉と同じ範囲のAABBをSTATICレイヤーに登録する。userDataは使わないため0。
  m_isOpen = false;
  m_colliderHandle = collisionWorld.AddAABB(BuildDoorAABB(),COLLISION_LAYER::ns_STATIC,0);
  return true;
}

// Colliderを解除し、状態を未初期化へ戻す。
void ExitDoor::Uninit(CollisionWorld& collisionWorld)
{
  // 登録中のColliderがある場合だけ解除する。開放中や未登録の場合は、解除済みのため何もしない。
  if(m_colliderHandle != COLLIDER_HANDLE::ns_INVALID){
    collisionWorld.Remove(m_colliderHandle);
  }

  // 借用した参照と状態を手放す。
  m_colliderHandle = COLLIDER_HANDLE::ns_INVALID;
  m_cubeRenderer = nullptr;
  m_isOpen = false;
}

//========= 更新・描画関数=========
// 開閉を切り替え、閉じている間だけ扉のColliderを登録する。
void ExitDoor::SetOpen(bool isOpen,CollisionWorld& collisionWorld)
{
  // 未初期化、または現在と同じ状態の指示では、Colliderを二重に登録・解除しないよう何もしない。
  if(m_cubeRenderer == nullptr || isOpen == m_isOpen)return;

  // 開く場合は扉のColliderを解除して通れるようにし、閉じる場合は同じ範囲を登録し直す。
  if(isOpen){
    collisionWorld.Remove(m_colliderHandle);
    m_colliderHandle = COLLIDER_HANDLE::ns_INVALID;
  }
  // 閉じる場合は、通行を止めるため、扉と同じ範囲のAABBを登録する。
  else{
    m_colliderHandle = collisionWorld.AddAABB(BuildDoorAABB(),COLLISION_LAYER::ns_STATIC,0);
  }
  m_isOpen = isOpen;
}

// 開閉の状態に合う、扉または目印をCubeで描く。
void ExitDoor::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
  // Rendererを借用していない場合は、描画しない。
  if(m_cubeRenderer == nullptr)return;

  // 開いている間は床の緑の目印を、閉じている間は赤い扉を描く。扉の色は画像の色へ乗算して、通行可否を示す。
  if(m_isOpen){
    m_cubeRenderer->DrawCube(graphicsSystem,m_padTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,EXIT_DOOR::ns_OPEN_COLOR,XMFLOAT2{1.0f,1.0f},
                             BasicMeshRenderer::TextureType::Object);
  }
  // 閉じている間は、赤い扉を描く。
  else{
    m_cubeRenderer->DrawCube(graphicsSystem,m_doorTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,EXIT_DOOR::ns_CLOSED_COLOR,XMFLOAT2{1.0f,1.0f},
                             BasicMeshRenderer::TextureType::Object);
  }
}
