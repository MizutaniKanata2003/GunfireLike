#include "../H/GameRenderer.h"

// ゲーム中の3Dのワールド、一人称の銃、画像のHUD、Collision可視化、演出の初期化・描画・解放を実装する。

//========= Framework インクルード=========
#include "Framework/3D/H/CameraMatrices.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/InitCheck.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Input/H/FpsCamera.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyController.h"
#include "Scene/Common/Stage/H/StageController.h"
#include "Scene/Common/Weapon/H/ShotController.h"

//========= 初期化・終了関数=========
// 参照先を登録し、Collision可視化とHUDと演出と一人称の銃を初期化する。
bool GameRenderer::Initialize(GraphicsSystem& graphicsSystem,const GAME_RENDERER_LINKS& links)
{
  // 参照先が1つでも欠けている場合は、以後の描画で空の参照を使ってしまうため、何も登録せず失敗を返す。
  if(links.stage == nullptr || links.enemyController == nullptr || links.camera == nullptr || links.shotController == nullptr){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameRendererの参照先の登録に失敗しました。");
    return false;
  }

  // 参照先を登録する。以後の描画は、この参照先を使う。
  m_stage = links.stage;
  m_enemyController = links.enemyController;
  m_camera = links.camera;
  m_shotController = links.shotController;

  // 一人称の銃の表示の位置を設定し、4種のモデルを順に読み込む。読み込めない武器があっても、その武器を描かないだけで、ゲームは続ける。失敗の理由は、WeaponViewと各Loaderがログへ記録する。
  m_weaponView.Prepare();
  // 武器の種類の数だけ、先頭から順に、1種類ずつ読み込む。1種類の失敗は、残りの武器の読込を止めない。
  for(int weapon_index = 0; weapon_index < WEAPON_TABLE::ns_WEAPON_COUNT; ++weapon_index){
    static_cast<void>(m_weaponView.LoadWeapon(graphicsSystem,weapon_index));
  }

  // 3つの初期化を、この順に実行する。最初に失敗した対象のメッセージだけをログへ記録し、後続は実行せずfalseを返す。
  // 後始末は、呼出側のUninitで行う。
  // Collision可視化の初期化失敗を検出する。
  return InitCheck::Succeeded(m_collisionDebugView.Initialize(graphicsSystem),e_LogCategory::e_SCENE,L"GameRendererの線描画Renderer初期化に失敗しました。")
    // 画像のHUDの初期化失敗を検出する。個々の画像の読込失敗はここでは失敗にならない。
    && InitCheck::Succeeded(m_gameHud.Initialize(graphicsSystem),e_LogCategory::e_SCENE,L"GameRendererのHUD初期化に失敗しました。")
    // 演出のCube Renderer借用の失敗を検出する。ステージの初期化の後に呼ぶため、共有Registryは初期化済み。
    && InitCheck::Succeeded(m_effectController.Initialize(m_stage->GetModelRegistry()),e_LogCategory::e_SCENE,L"GameRendererの演出の初期化に失敗しました。");
}

// HUDとCollision可視化と演出と一人称の銃を解放し、参照先を手放す。
void GameRenderer::Uninit()
{
  // 銃のモデルと、演出の借用を手放し、HUDと線描画Rendererを解放する。
  m_weaponView.Uninit();
  m_effectController.Uninit();
  m_gameHud.Uninit();
  m_collisionDebugView.Uninit();

  m_stage = nullptr;
  m_enemyController = nullptr;
  m_camera = nullptr;
  m_shotController = nullptr;
}

//========= 描画関数=========
// 床・壁・範囲の予告・出口・敵・敵の弾・演出・Collisionの線・一人称の銃を、この順に3Dで描画する。
void GameRenderer::DrawWorld(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix)
{
  // 参照先が未登録の場合は、描画しない。
  if(m_stage == nullptr || m_enemyController == nullptr || m_camera == nullptr || m_shotController == nullptr)return;

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

  // 演出の後に、表示中ならCollisionの線を重ねる。View行列とProjection行列は、1つの構造体にまとめて渡す。
  const CAMERA_MATRICES camera_matrices{viewMatrix,projectionMatrix};
  m_collisionDebugView.Draw(graphicsSystem,m_stage->GetCollisionWorld(),*m_camera,camera_matrices);

  // 最後に、一人称の銃を、ワールドの上に重ねて描く。
  DrawWeaponView(graphicsSystem,projectionMatrix);
}

// 3Dの上に、画像のHUDを描画する。
void GameRenderer::DrawHud(GraphicsSystem& graphicsSystem,const GAME_HUD_STATE& hudState)
{
  // 3Dの上に画像のHUDを重ねるため、深度比較なし・アルファ合成の画面UI用Passへ切り替える。
  graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);
  m_gameHud.Draw(graphicsSystem,hudState);
}

//========= 描画の補助関数=========
// 装備中の武器を、一人称の銃として描く。
void GameRenderer::DrawWeaponView(GraphicsSystem& graphicsSystem,const XMMATRIX& projectionMatrix)
{
  // 装備中の武器を選び、深度を消して、不透明用のPassで描く。深度を消すことで、壁や敵に近づいても、銃が、めり込んだり、隠れたりしない。
  m_weaponView.SelectWeapon(m_shotController->GetWeaponSystem().GetWeaponData());
  graphicsSystem.ClearDepth();
  graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);
  m_weaponView.Draw(graphicsSystem,projectionMatrix);

  // 調整の画面を出す。表示の位置が決まったら、この呼出しを削除する。
  m_weaponView.DrawTuningUi();
}