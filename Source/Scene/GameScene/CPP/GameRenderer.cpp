#include "../H/GameRenderer.h"

// ゲーム中の3Dのワールド、画像のHUD、Collision可視化、演出の初期化・描画・解放を実装する。

//========= Framework インクルード=========
#include "Framework/3D/H/CameraMatrices.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/InitCheck.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Input/H/FpsCamera.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyController.h"
#include "Scene/Common/Stage/H/StageController.h"

#include <windows.h>
#include "Framework/3D/H/SkinnedAnimationLoader.h"
#include "Framework/3D/H/SkinnedAnimationPlayer.h"
#include "Framework/3D/H/SkinnedModelLoader.h"
#include "Framework/3D/H/SkinnedModelRenderer.h"
//========= 初期化・終了関数=========
// 参照先を登録し、Collision可視化とHUDと演出を初期化する。
bool GameRenderer::Initialize(GraphicsSystem& graphicsSystem,const GAME_RENDERER_LINKS& links)
{
  // 参照先が1つでも欠けている場合は、以後の描画で空の参照を使ってしまうため、何も登録せず失敗を返す。
  if(links.stage == nullptr || links.enemyController == nullptr || links.camera == nullptr){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameRendererの参照先の登録に失敗しました。");
    return false;
  }

  // 参照先を登録する。以後の描画は、この参照先を使う。
  m_stage = links.stage;
  m_enemyController = links.enemyController;
  m_camera = links.camera;

  // 3つの初期化を、この順に実行する。最初に失敗した対象のメッセージだけをログへ記録し、後続は実行せずfalseを返す。
  // 後始末は、呼出側のUninitで行う。
  // Collision可視化の初期化失敗を検出する。
  return InitCheck::Succeeded(m_collisionDebugView.Initialize(graphicsSystem),e_LogCategory::e_SCENE,L"GameRendererの線描画Renderer初期化に失敗しました。")
    // 画像のHUDの初期化失敗を検出する。個々の画像の読込失敗はここでは失敗にならない。
    && InitCheck::Succeeded(m_gameHud.Initialize(graphicsSystem),e_LogCategory::e_SCENE,L"GameRendererのHUD初期化に失敗しました。")
    // 演出のCube Renderer借用の失敗を検出する。ステージの初期化の後に呼ぶため、共有Registryは初期化済み。
    && InitCheck::Succeeded(m_effectController.Initialize(m_stage->GetModelRegistry()),e_LogCategory::e_SCENE,L"GameRendererの演出の初期化に失敗しました。");
}

// HUDとCollision可視化と演出を解放し、参照先を手放す。
void GameRenderer::Uninit()
{
  // 演出の借用を手放し、HUDと線描画Rendererを解放する。
  m_effectController.Uninit();
  m_gameHud.Uninit();
  m_collisionDebugView.Uninit();

  m_stage = nullptr;
  m_enemyController = nullptr;
  m_camera = nullptr;
}

//========= 描画関数=========
// 床・壁・範囲の予告・出口・敵・敵の弾・演出・Collisionの線を、この順に3Dで描画する。
void GameRenderer::DrawWorld(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix)
{
  // 参照先が未登録の場合は、描画しない。
  if(m_stage == nullptr || m_enemyController == nullptr || m_camera == nullptr)return;

  // 床・壁・範囲の予告・出口・敵・敵の弾・演出はOPAQUE Passで描画するため、描画前にPassを設定する。
  graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);

  // 生成済みの全Objectを、表の順（床→左壁→右壁→手前壁→奥壁）に描画する。
  m_stage->DrawObjects(graphicsSystem,viewMatrix,projectionMatrix);

  // 床の上に、範囲の予告を描画する。床の後に描き、敵や出口より先に描くことで、敵の体が予告の上に重なる。続けて出口を描画する。
  m_enemyController->DrawAreaTelegraph(graphicsSystem,viewMatrix,projectionMatrix);
  m_stage->DrawExit(graphicsSystem,viewMatrix,projectionMatrix);
  // 続けて全枠の敵を出現順に描画し、その後に飛行中の敵の弾を描画する。死亡枠はEnemy::Drawが描画を省略する。
  m_enemyController->DrawEnemies(graphicsSystem,viewMatrix,projectionMatrix);

  // 敵と弾の後に、演出の粒を描画する。残っている粒がなければ、何も描かない。
  m_effectController.Draw(graphicsSystem,viewMatrix,projectionMatrix);
  // 一時確認用。確認後に削除する。
  {
    static SKINNED_MODEL_DATA test_model;
    static SKINNED_ANIMATION_CLIP test_clip;
    static SkinnedAnimationPlayer test_player;
    static SkinnedModelRenderer test_renderer;
    static bool is_tried = false;
    static bool is_ready = false;
    if(!is_tried){
      is_tried = true;
      SkinnedModelLoader model_loader;
      SkinnedAnimationLoader animation_loader;
      is_ready = model_loader.Load(L"Assets/Models/Enemy/Melee/Model/MeleeA.fbx",test_model)
        && animation_loader.Load(L"Assets/Models/Enemy/Melee/Animation/Idle.fbx",test_clip)
        && test_player.Initialize(test_model,test_clip)
        && test_renderer.Initialize(graphicsSystem,test_model);
    }
    if(is_ready){
      const float test_yaw = 0.0f;
      BoneMatrixArray bone_matrices;
      const float test_time = static_cast<float>((GetTickCount64() % 1000000) / 1000.0);
      test_player.Evaluate(test_player.NormalizeTime(test_time,true),bone_matrices);

      SKINNED_DRAW_PARAMS draw_params{};
      XMStoreFloat4x4(&draw_params.worldMatrix,XMMatrixMultiply(XMMatrixMultiply(XMMatrixScaling(0.01f,0.01f,0.01f),
                      XMMatrixRotationRollPitchYaw(0.0f,test_yaw,0.0f)),XMMatrixTranslation(0.0f,0.0f,4.0f)));
      draw_params.boneMatrices = &bone_matrices;
      draw_params.color = XMFLOAT4{0.8f,0.4f,0.4f,1.0f};
      test_renderer.Draw(graphicsSystem,CAMERA_MATRICES{viewMatrix,projectionMatrix},draw_params);
    }
  }
  // 演出の後に、表示中ならCollisionの線を重ねる。View行列とProjection行列は、1つの構造体にまとめて渡す。
  const CAMERA_MATRICES camera_matrices{viewMatrix,projectionMatrix};
  m_collisionDebugView.Draw(graphicsSystem,m_stage->GetCollisionWorld(),*m_camera,camera_matrices);
}

// 3Dの上に、画像のHUDを描画する。
void GameRenderer::DrawHud(GraphicsSystem& graphicsSystem,const GAME_HUD_STATE& hudState)
{
  // 3Dの上に画像のHUDを重ねるため、深度比較なし・アルファ合成の画面UI用Passへ切り替える。
  graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);
  m_gameHud.Draw(graphicsSystem,hudState);
}