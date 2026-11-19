#include "../H/StageController.h"

// ステージの世界の初期化・解放・進行の更新・出口の通過判定・描画を実装する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/InitCheck.h"
#include "Framework/Etc/H/Logger.h"

//========= Scene インクルード=========
#include "Scene/Common/Object/H/SceneObjectFactory.h"
#include "Scene/Common/Stage/H/StageLayout.h"

//========= 初期化・終了関数=========
// 進行状態を初期へ戻し、生成済みObjectとColliderを空にする。
void StageController::Initialize()
{
  // 進行状態を第1部屋の第1ウェーブへ戻し、前回の生成失敗を引き継がない。
  m_roomController.Initialize();
  m_hasWaveSpawnFailed = false;

  // 前回の生成ObjectとColliderを空にする。Rendererの生成、表からのObject生成、出口の配置はInitで行う。
  m_stageObjects.clear();
  m_collisionWorld.Clear();
}

// 共有Renderer・床と壁・出口を、この順に初期化する。
bool StageController::Init(GraphicsSystem& graphicsSystem)
{
  // 3つの初期化を、この順に実行する。最初に失敗した対象のメッセージだけをログへ記録し、後続は実行せずfalseを返す。後始末は、呼出側がUninitで行う。
  // 床と壁と出口は、共有Rendererを借用するため、Rendererの初期化を最初に行う。共有Rendererの初期化失敗を検出する。
  return InitCheck::Succeeded(m_modelRegistry.Initialize(graphicsSystem),e_LogCategory::e_SCENE,L"StageControllerの基本Mesh初期化に失敗しました。")
         // 設定表から床と4面の壁を、表の順に生成し、壁のAABB ColliderをCollisionWorldへ登録する。全件成功しなければ失敗する。
         && InitCheck::Succeeded(SceneObjectFactory::Build(STAGE_LAYOUT::ns_OBJECTS,STAGE_LAYOUT::ns_OBJECT_COUNT,m_modelRegistry,&m_collisionWorld,m_stageObjects),
                                 e_LogCategory::e_SCENE,L"StageControllerのステージObject生成に失敗しました。")
         // 閉じた状態の出口を登録する。Cube Rendererを借用できなければ失敗する。
         && InitCheck::Succeeded(m_exitDoor.Initialize(m_modelRegistry,m_collisionWorld),e_LogCategory::e_SCENE,L"StageControllerの出口の初期化に失敗しました。");
}

// 出口・Object・共有Renderer・Colliderを解放する。
void StageController::Uninit()
{
  // 出口とObjectは、CollisionWorldと共有Rendererを借用しているため、それらを解放する前に先に終了する。
  m_exitDoor.Uninit(m_collisionWorld);
  m_stageObjects.clear();
  m_modelRegistry.Uninit();

  // 登録したColliderを空にする。
  m_collisionWorld.Clear();
}

//========= 進行関数=========
// 最初の部屋の第1ウェーブから、進行をやり直す。
void StageController::Restart()
{
  m_roomController.Initialize();
  m_hasWaveSpawnFailed = false;
}

// 現在のウェーブを生成する必要があるかを返す。
bool StageController::ShouldSpawnWave() const
{
  return m_roomController.GetPhase() == ROOM_PHASE::e_NEEDS_SPAWN && !m_hasWaveSpawnFailed;
}

// 現在のウェーブの生成結果を記録する。
void StageController::RecordSpawnResult(bool isSuccess)
{
  m_hasWaveSpawnFailed = !isSuccess;
}

// 敵の生成が成功したことを、進行管理へ通知する。
void StageController::NotifyWaveSpawned()
{
  m_roomController.NotifyWaveSpawned();
}

// 敵の全滅の結果から進行を更新し、進行状態に合わせて出口を開閉する。
void StageController::UpdateProgress(bool hasLivingEnemies)
{
  // 生存中の敵がいなければウェーブまたは部屋を進める。生成待ちの間は、空の敵一覧を全滅と扱わない。
  m_roomController.Update(hasLivingEnemies);

  // 部屋の進行状態に合わせて、出口を開閉する。出口開放中と攻略完了後は開き、それ以外は閉じる。Restartや部屋の移動で戻った場合も、この更新で閉じる。
  const ROOM_PHASE room_phase = m_roomController.GetPhase();
  m_exitDoor.SetOpen(room_phase == ROOM_PHASE::e_EXIT_OPEN || room_phase == ROOM_PHASE::e_COMPLETE,m_collisionWorld);
}

// 出口の通過を判定し、受理された場合だけ次の部屋へ進める。
bool StageController::TryPassExit(const XMFLOAT3& position)
{
  // 出口が閉じている、または位置が通過範囲の外にある場合は、何もせず、通過していないことを返す。
  if(!m_exitDoor.IsInPassZone(position))return false;

  // 進行管理が出口の通過を受理しなかった場合は、何もせず、通過していないことを返す。最終部屋の通過後や、受理済みの重複した通知では受理されない。
  return m_roomController.TryAdvanceRoom();
}

//========= 描画関数=========
// 設定表から生成した床と壁を、表の順に描画する。
void StageController::DrawObjects(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
  // 生成済みの全Objectを、表の順（床→左壁→右壁→手前壁→奥壁）に描画する。
  for(const SceneObject& object : m_stageObjects){
    object.Draw(graphicsSystem,viewMatrix,projectionMatrix);
  }
}

// 出口を描画する。
void StageController::DrawExit(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
  m_exitDoor.Draw(graphicsSystem,viewMatrix,projectionMatrix);
}
