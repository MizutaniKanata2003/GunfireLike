#include "../H/CollisionDebugView.h"

// Collision可視化の初期化・更新と、Collider・Ray・弾道の線の生成・描画を実装する。
// 表示の色・長さは、このファイルの定数にまとめた。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで、弾道を表示する残り時間を0未満にしない。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Input/H/FpsCamera.h"

// 可視化に使う距離・長さ・色の固定設定をまとめる。
namespace
{
  //========= Rayの判定=========
  // 視線のRayを調べる最大距離。単位はワールド距離単位。ステージの奥行き（約20）より十分長い値を使う。
  constexpr float ns_RAY_MAX_DISTANCE = 100.0f;

  //========= 線の表示=========
  // 命中しない場合に、線を伸ばす長さ。単位はワールド距離単位。
  constexpr float ns_RAY_DISPLAY_LENGTH = 30.0f;
  // 線の始点を、カメラ位置から右方向へずらす量。画面中央の1点に線が重ならないようにする。単位はワールド距離単位。
  constexpr float ns_LINE_OFFSET_RIGHT = 0.25f;
  // 線の始点を、カメラ位置から下方向（-Y）へずらす量。単位はワールド距離単位。
  constexpr float ns_LINE_OFFSET_DOWN = 0.3f;
  // 命中点の十字の、中心から端までの長さ。単位はワールド距離単位。
  constexpr float ns_HIT_CROSS_HALF_SIZE = 0.2f;
  // 命中点から伸ばす法線の線の長さ。単位はワールド距離単位。
  constexpr float ns_NORMAL_LENGTH = 0.5f;
  // 直近の弾道を表示し続ける時間。単位は秒。連射中は撃つたびに数え直す。
  constexpr float ns_SHOT_TRACE_SECONDS = 0.15f;

  //========= 線の色=========
  // 視線のRayの線の色。黄色。
  constexpr XMFLOAT4 ns_RAY_COLOR{1.0f,1.0f,0.0f,1.0f};
  // 命中点の十字の色。白。
  constexpr XMFLOAT4 ns_HIT_CROSS_COLOR{1.0f,1.0f,1.0f,1.0f};
  // 命中点の法線の線の色。水色。
  constexpr XMFLOAT4 ns_NORMAL_COLOR{0.0f,1.0f,1.0f,1.0f};
  // 弾道の線の色。橙。
  constexpr XMFLOAT4 ns_SHOT_TRACE_COLOR{1.0f,0.5f,0.0f,1.0f};
}

//========= 初期化・終了関数=========
// 線描画Rendererを初期化し、表示状態を既定へ戻す。
bool CollisionDebugView::Initialize(GraphicsSystem& graphicsSystem)
{
  // 表示状態と蓄えた線を既定へ戻す。
  m_isVisible = false;
  m_hasRayHit = false;
  m_shotTraceRemainingSeconds = 0.0f;
  m_drawer.Clear();

  // 線描画Rendererの初期化結果をそのまま返す。失敗時の部分解放は、Renderer側が行う。
  return m_lineRenderer.Initialize(graphicsSystem);
}

// 線描画Rendererを解放し、状態を空にする。
void CollisionDebugView::Uninit()
{
  m_lineRenderer.Uninit();
  m_drawer.Clear();
  m_hasRayHit = false;
  m_shotTraceRemainingSeconds = 0.0f;
}

//========= 更新関数=========
// 弾道の表示時間を進める。
void CollisionDebugView::Update(float deltaTime)
{
  // 残り時間を減らし、0未満にならないよう制限する。
  m_shotTraceRemainingSeconds = std::max(0.0f,m_shotTraceRemainingSeconds - deltaTime);
}

// 表示中だけ、視線のRayを調べる。
void CollisionDebugView::UpdateRay(const FpsCamera& camera,const CollisionWorld& collisionWorld)
{
  // 非表示の間は、判定を行わず、命中なしの状態にする。
  if(!m_isVisible){
    m_hasRayHit = false;
    return;
  }

  // カメラ位置から視線方向へRayを飛ばし、最も近い命中を調べる。Ray・最大距離・対象のレイヤーは、1つの問い合わせにまとめる。
  m_ray.origin = camera.GetPosition();
  m_ray.direction = camera.GetForward();
  const COLLISION_RAY_QUERY query{m_ray,ns_RAY_MAX_DISTANCE,COLLISION_LAYER::ns_ALL};
  m_hasRayHit = collisionWorld.Raycast(query,m_rayHit,m_rayHitInfo);
}

// 弾道の終点と、表示時間を記録する。
void CollisionDebugView::NotifyShot(const XMFLOAT3& traceEnd)
{
  m_shotTraceEnd = traceEnd;
  m_shotTraceRemainingSeconds = ns_SHOT_TRACE_SECONDS;
}

//========= 描画関数=========
// Collider・Ray・弾道の線を作り、深度比較を切って描画する。
void CollisionDebugView::Draw(GraphicsSystem& graphicsSystem,const CollisionWorld& collisionWorld,const FpsCamera& camera,const CAMERA_MATRICES& cameraMatrices)
{
  // 非表示なら、線を作らず描画しない。
  if(!m_isVisible)return;

  // 前フレームの線を捨て、登録中の全Colliderをレイヤーごとの色で追加する。
  m_drawer.Clear();
  m_drawer.AddWorld(collisionWorld);

  // 線の始点を求める。カメラ位置のままでは画面中央の1点に重なり線が見えないため、右方向と下方向（-Y）へずらす。
  // 右方向はカメラの右ベクトルに距離を掛け、下方向はワールドのY座標から引く。
  const XMFLOAT3& camera_position = camera.GetPosition();
  const XMFLOAT3 camera_right = camera.GetRight();
  const XMFLOAT3 line_start{camera_position.x + camera_right.x * ns_LINE_OFFSET_RIGHT,
    camera_position.y + camera_right.y * ns_LINE_OFFSET_RIGHT - ns_LINE_OFFSET_DOWN,
    camera_position.z + camera_right.z * ns_LINE_OFFSET_RIGHT};

  // 視線のRayの線の終点を決める。
  XMFLOAT3 line_end{};
  // 命中している場合は、終点を命中点にして、命中点の十字と、命中面の法線の線も追加する。
  if(m_hasRayHit){
    line_end = m_rayHit.point;
    m_drawer.AddCross(m_rayHit.point,ns_HIT_CROSS_HALF_SIZE,ns_HIT_CROSS_COLOR);
    m_drawer.AddLine(m_rayHit.point,
                     XMFLOAT3{m_rayHit.point.x + m_rayHit.normal.x * ns_NORMAL_LENGTH,m_rayHit.point.y + m_rayHit.normal.y * ns_NORMAL_LENGTH,
                     m_rayHit.point.z + m_rayHit.normal.z * ns_NORMAL_LENGTH},
                     ns_NORMAL_COLOR);
  }
  // 命中していない場合は、カメラ位置から視線方向へ表示用の長さだけ進んだ点を終点にする。
  else{
    const XMFLOAT3 camera_forward = camera.GetForward();
    line_end = XMFLOAT3{camera_position.x + camera_forward.x * ns_RAY_DISPLAY_LENGTH,camera_position.y + camera_forward.y * ns_RAY_DISPLAY_LENGTH,
      camera_position.z + camera_forward.z * ns_RAY_DISPLAY_LENGTH};
  }
  m_drawer.AddLine(line_start,line_end,ns_RAY_COLOR);

  // 直近の弾道の表示時間が残っている間だけ、同じ始点から弾の終点までの線を追加する。ばらつきのある弾道を確認できる。
  if(m_shotTraceRemainingSeconds > 0.0f){
    m_drawer.AddLine(line_start,m_shotTraceEnd,ns_SHOT_TRACE_COLOR);
  }

  // 壁面と同じ位置のAABBが深度比較でちらつかないよう、深度比較を切って描画し、描画後に既定へ戻す。
  graphicsSystem.SetDepthTestEnabled(false);
  m_drawer.Draw(graphicsSystem,m_lineRenderer,cameraMatrices.view,cameraMatrices.projection);
  graphicsSystem.SetDepthTestEnabled(true);
}