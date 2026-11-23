#pragma once

// Collisionの可視化を担当する。登録中のColliderの線、カメラから視線方向へのRayの結果、直近の弾道の線を、線描画で表示する。
// 表示の切替・Rayの判定・線の描画を持つ。Colliderの登録やゲームの進行は担当しない。

//========= Debug インクルード=========
#include "Debug/H/DebugLineRenderer.h"

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionDebugDrawer.h"
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class CollisionWorld;
class FpsCamera;
class GraphicsSystem;

// Collisionの可視化の状態と描画。
class CollisionDebugView final
{
public:
	//========= 初期化・終了関数=========
	// 線描画Rendererを初期化し、表示状態を非表示へ戻す。
	// graphicsSystem: Resource生成に使う描画基盤。Uninitまで有効であること。
	// 戻り値: 線描画Rendererの初期化に成功すればtrue。失敗した場合は解放してfalse。
	[[nodiscard]] bool Initialize(GraphicsSystem& graphicsSystem);
	// 線描画Rendererを解放し、蓄えた線と状態を空にする。未初期化・終了済みでも呼出可能。
	void Uninit();

	//========= 更新関数=========
	// 表示用の残り時間を進める。Captureに関係なく毎フレーム呼ぶ。
	// deltaTime: この更新で進める経過時間。単位は秒。
	void Update(float deltaTime);
	// 表示中だけ、カメラ位置から視線方向へRayを飛ばして、最も近い命中を調べる。視線と位置の更新後に呼ぶ。
	// camera: Rayの始点と方向の取得元。
	// collisionWorld: 命中判定を行うCollisionWorld。読み取りだけを行う。
	void UpdateRay(const FpsCamera& camera,const CollisionWorld& collisionWorld);
	// 直近の弾道の終点を記録し、弾道の線を表示する時間を数え直す。
	// traceEnd: 弾の終点のワールド座標。命中していれば命中点、していなければ表示用の長さまで進んだ点。
	void NotifyShot(const XMFLOAT3& traceEnd);
	// 表示と非表示を切り替える。
	void ToggleVisible() { m_isVisible = !m_isVisible; }

	//========= 描画関数=========
	// 表示中だけ、登録中のColliderと視線のRay・弾道を、深度比較なしで描画する。描画後は深度比較を既定へ戻す。
	// graphicsSystem: 描画基盤。Render Passは呼出側が設定しておくこと。
	// collisionWorld: 表示するColliderを持つCollisionWorld。読み取りだけを行う。
	// camera: 線の始点を求めるための位置と向きの取得元。
	// viewMatrix: World空間からView空間への変換行列。
	// projectionMatrix: View空間からClip空間への変換行列。
	void Draw(GraphicsSystem& graphicsSystem,const CollisionWorld& collisionWorld,const FpsCamera& camera,const XMMATRIX& viewMatrix,
						const XMMATRIX& projectionMatrix);

	//========= Getter関数=========
	// 表示中かを返す。
	[[nodiscard]] bool IsVisible() const { return m_isVisible; }
	// 視線のRayが命中しているかを返す。表示中のUpdateRayだけが更新する。
	[[nodiscard]] bool HasRayHit() const { return m_hasRayHit; }
	// 視線のRayの命中の距離・命中点・法線を返す。HasRayHitがtrueの間だけ有効。
	[[nodiscard]] const COLLISION_RAY_HIT& GetRayHit() const { return m_rayHit; }
	// 視線のRayが命中したColliderの情報を返す。HasRayHitがtrueの間だけ有効。
	[[nodiscard]] const COLLISION_HIT_INFO& GetRayHitInfo() const { return m_rayHitInfo; }

private:
	//========= 描画Resource=========
	// 線分を蓄え、色ごとに描画する。
	CollisionDebugDrawer m_drawer{};
	// 線分の描画に使うGPU Resourceを所有する。
	DebugLineRenderer m_lineRenderer{};

	//========= 表示状態=========
	// 表示するか。既定は非表示。
	bool m_isVisible{};
	// 表示中にUpdateRayで求めた、カメラから視線方向へのRay。
	COLLISION_RAY m_ray{};
	// m_rayがColliderへ命中したか。
	bool m_hasRayHit{};
	// 命中した場合の距離・命中点・法線。m_hasRayHitがtrueの間だけ有効。
	COLLISION_RAY_HIT m_rayHit{};
	// 命中したColliderの情報。m_hasRayHitがtrueの間だけ有効。
	COLLISION_HIT_INFO m_rayHitInfo{};

	//========= 弾道の表示状態=========
	// 直近の弾の終点のワールド座標。
	XMFLOAT3 m_shotTraceEnd{};
	// 弾道を表示する残り時間。単位は秒。0より大きい間、弾道の線を描く。
	float m_shotTraceRemainingSeconds{};
};
