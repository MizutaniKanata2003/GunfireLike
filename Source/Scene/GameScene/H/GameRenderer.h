#pragma once

// ゲーム中の描画（3Dのワールド、画像のHUD、Collisionの可視化）をまとめて担当する。
// 画像のHUD（GameHud）と、Collision可視化（CollisionDebugView）と、演出（EffectController）を所有し、床と壁・範囲の予告・出口・敵・敵の弾・演出・Collisionの線を3Dで描画し、HUDを描画する。
// 描画する値（HUDの状態）の決定・ゲームの進行・入力・演出を出す場面の判断は担当しない。ステージ・敵・カメラは、借用する参照として受け取る。

//========= Debug インクルード=========
#include "Debug/H/CollisionDebugView.h"

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/GameScene/H/EffectController.h"
#include "Scene/GameScene/H/GameHud.h"

//========= 前方宣言=========
class EnemyController;
class FpsCamera;
class GraphicsSystem;
class StageController;

// GameRendererが借用する参照先をまとめる。参照先は、Uninitまで有効であること。
struct GAME_RENDERER_LINKS
{
  // 床・壁・出口の描画と、Collisionの取得元となるステージ。
  StageController* stage{};
  // 範囲の予告・敵・敵の弾の描画元。
  EnemyController* enemyController{};
  // Collisionの線の始点を求めるための、位置と向きの取得元となるカメラ。読み取りだけを行う。
  const FpsCamera* camera{};
};

// ゲーム中の3Dとワールドの描画、HUDの描画、Collision可視化、演出を担当するクラス。
class GameRenderer final
{
public:
  //========= 初期化・終了関数=========
  // 参照先を登録し、Collision可視化とHUDと演出を初期化する。ステージの初期化の後に呼ぶ。
  // graphicsSystem: Resource生成に使う描画基盤。Uninitまで有効であること。
  // links: 借用する参照先。1つでも欠けている場合は、何も登録せず失敗する。
  // 戻り値: 登録と初期化に成功したらtrue。失敗した場合は、失敗した対象をログへ記録してfalse。後始末は、呼出側がUninitで行う。
  [[nodiscard]] bool Initialize(GraphicsSystem& graphicsSystem,const GAME_RENDERER_LINKS& links);
  // HUDとCollision可視化と演出を解放し、参照先を手放す。未初期化でも、繰り返しても呼出可能。
  void Uninit();

  //========= 描画関数=========
  // 床・壁・範囲の予告・出口・敵・敵の弾・演出・Collisionの線を、この順に3Dで描画する。参照先が未登録の場合は何もしない。
  // graphicsSystem: 描画基盤。この関数の中で、Render Passを不透明用へ設定する。
  // viewMatrix: World空間からView空間への変換行列。
  // projectionMatrix: View空間からClip空間への変換行列。
  void DrawWorld(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix);
  // 3Dの上に、画像のHUDを描画する。この関数の中で、Render Passを画面UI用へ設定する。
  // graphicsSystem: 描画基盤。
  // hudState: 描画する値。参照は保持しない。
  void DrawHud(GraphicsSystem& graphicsSystem,const GAME_HUD_STATE& hudState);

  //========= Getter関数=========
  // Collision可視化を返す。表示の切替・弾道の通知・視線のRayの更新に、プレイヤー・デバッグ・GameSessionが使う。
  // 戻り値の参照は、このGameRendererの寿命を超えて保持しない。
  [[nodiscard]] CollisionDebugView& GetCollisionDebugView() { return m_collisionDebugView; }
  // 演出を返す。粒の生成と更新を、GameSessionが行う。
  // 戻り値の参照は、このGameRendererの寿命を超えて保持しない。
  [[nodiscard]] EffectController& GetEffectController() { return m_effectController; }

private:
  //========= 参照先=========
  // 床・壁・出口・Collisionの取得元。非所有。未登録はnullptr。
  StageController* m_stage{};
  // 範囲の予告・敵・敵の弾の描画元。非所有。未登録はnullptr。
  EnemyController* m_enemyController{};
  // Collisionの線の始点を求めるカメラ。非所有。未登録はnullptr。
  const FpsCamera* m_camera{};

  //========= 所有Resource=========
  // 画像で描くHUD。
  GameHud m_gameHud{};
  // Collisionの可視化（Colliderの線・視線のRay・弾道）。F3で表示を切り替える。
  CollisionDebugView m_collisionDebugView{};
  // 演出の粒（発砲・命中・撃破などの小さなCube）。Cube Rendererは、ステージの共有Registryから借用する。
  EffectController m_effectController{};
};