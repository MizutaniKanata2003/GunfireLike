#pragma once

// 移動・視点・構え・射撃・リロードと、設定表から生成した床・壁、検証用の敵1体、Collisionの可視化を行う最小構成のGameSceneを宣言する。
// 各クラスの更新を仲介し、共有Renderer・生成したObject・CollisionWorldを所有する。Scene切替はManagerへ予約する。

//========= C++標準ライブラリ インクルード=========
#include <vector> // 設定表から生成したSceneObjectを、表の順に保持する可変長配列として使用する。

//========= Framework インクルード=========
#include "Framework/3D/H/DebugLineRenderer.h"
#include "Framework/Collision/H/CollisionDebugDrawer.h"
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Input/H/FpsCamera.h"

//========= Scene インクルード=========
#include "Scene/Common/H/IScene.h"
#include "Scene/Common/H/ModelRegistry.h"
#include "Scene/Common/H/SceneObject.h"
#include "Scene/GameScene/H/Enemy.h"
#include "Scene/GameScene/H/PlayerController.h"
#include "Scene/GameScene/H/WeaponSystem.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// テスト用の操作・3D描画・デバッグ表示と、タイトルへの遷移予約を担当するScene。
class GameScene final: public IScene
{
public:
	//========= 生成関数=========
	// Sceneで使用する非所有参照を登録する。Resource生成はInitで行う。
	// sceneManager: タイトルへの遷移予約先。Sceneの使用期間中は有効であること。
	// inputSystem: キー入力の取得先とCapture設定先。Sceneより長く生存すること。
	// graphicsSystem: Resource生成・描画先サイズ取得・描画の基盤。Resource終了まで有効であること。
	// audioSystem: Game用BGMの再生先。Sceneの使用期間中は有効であること。
	GameScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

	//========= Sceneライフサイクル関数=========
	// プレイヤー・カメラ・武器・Collision・表示状態を初期状態へ戻し、生成済みObjectを空にして、カメラをプレイヤー位置へ合わせる。
	void Initialize() override;
	// 共有Rendererと線描画Rendererを初期化し、設定表から床と壁（とそのCollider）を生成し、敵を配置して、成功後にFPS操作とGame用BGMを有効にする。
	// 戻り値: Registry初期化・線描画の初期化・Object生成・敵の配置が成功したらtrue。falseを検出した場合はFinalize後にfalseを返す。
	// 下位処理の例外は捕捉しない。
	bool Init() override;
	// F2の遷移予約、F3のCollision可視化の切替、F4の敵の再配置、F1のCapture切替、視点・移動・構え・射撃・リロードの更新と、命中の反映を行う。
	// deltaTime: 各更新へ渡す経過時間。単位は秒。
	// Capture解除中は視点・移動・射撃の更新を行わない。F2で予約した場合は後続処理を行わない。
	void Update(float deltaTime) override;
	// 描画先サイズに合わせて行列を作り、生成済みのObjectと敵を描画して、Collisionの可視化と読み取り専用のデバッグUIを描画する。
	// 幅または高さが0なら何もしない。ImGuiフレーム開始後、描画データ確定前に呼ぶ。
	void Draw() override;
	// 敵・生成したObjectを先に破棄し、その後に共有Renderer・線描画Renderer・Collisionを解放する。繰り返し呼出可能。CaptureやBGMは変更しない。
	void Finalize() override;

private:
	//========= 補助関数=========
	// 登録中のColliderと、視線のRay・直近の弾道の結果を線分へ変換し、深度比較なしで描画する。描画後は深度比較を既定へ戻す。
	// viewMatrix: World空間からView空間への変換行列。
	// projectionMatrix: View空間からClip空間への変換行列。
	void DrawCollisionDebug(const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix);

	//========= System・Scene参照=========
	// Scene遷移の予約先となる非所有参照。
	SceneManager& m_sceneManager;
	// 操作入力とCapture設定に使用する非所有参照。
	InputSystem& m_inputSystem;
	// Resource初期化と描画に使用する非所有参照。
	GraphicsSystem& m_graphicsSystem;
	// Init成功後のGame用BGM再生に使用する非所有参照。
	AudioSystem& m_audioSystem;

	//========= Scene所有状態=========
	// 位置・構え・壁からの押し戻しを管理するプレイヤー。
	PlayerController m_playerController{};
	// マウスによる視線方向と、プレイヤー位置に追従するView行列の生成状態。
	FpsCamera m_FPSCamera{};
	// 弾薬・連射・リロード・射撃判定を管理する武器。
	WeaponSystem m_weaponSystem{};
	// 検証用の敵1体。ModelRegistryとCollisionWorldを借用するため、Finalizeでそれらより先にUninitを呼ぶ。
	Enemy m_enemy{};
	// 現在の視野角。単位は度。構えている間は狭い値へ滑らかに近づく。
	float m_fovDegrees{};

	//========= 描画ResourceとObject=========
	// Cube・OBJの共有Rendererを所有する。m_stageObjectsとm_enemyが借用するため、このメンバーを先に宣言し、後に破棄する。
	ModelRegistry m_modelRegistry{};
	// STAGE_LAYOUTの表から生成した床と壁。Rendererを借用するだけで所有しない。表の順が描画順になる。
	std::vector<SceneObject> m_stageObjects{};

	//========= Collision=========
	// 床・壁のAABBと、敵の球を登録するCollision。
	CollisionWorld m_collisionWorld{};
	// Collisionの形状を表示する線分を蓄え、色ごとに描画する。
	CollisionDebugDrawer m_collisionDebugDrawer{};
	// 線分の描画に使うGPU Resourceを所有する。
	DebugLineRenderer m_debugLineRenderer{};

	//========= Collision可視化の状態=========
	// Collisionの可視化を表示するか。F3で切り替え、既定は非表示。
	bool m_isCollisionDebugVisible{};
	// 表示中にUpdateで求めた、カメラから視線方向へのRay。
	COLLISION_RAY m_debugRay{};
	// m_debugRayがColliderへ命中したか。表示中だけUpdateで更新する。
	bool m_hasDebugRayHit{};
	// 命中した場合の距離・命中点・法線。m_hasDebugRayHitがtrueの間だけ有効。
	COLLISION_RAY_HIT m_debugRayHit{};
	// 命中したColliderの情報。m_hasDebugRayHitがtrueの間だけ有効。
	COLLISION_HIT_INFO m_debugRayHitInfo{};

	//========= 射撃の表示状態=========
	// 直近の弾の終点。命中していれば命中点、命中しなければ射程または表示用の長さまで進んだ点。
	XMFLOAT3 m_shotTraceEnd{};
	// 直近の弾道を表示する残り時間。単位は秒。0より大きい間、可視化の表示中に弾道の線を描く。
	float m_shotTraceRemainingSeconds{};
	// 直近の命中を表示する残り時間。単位は秒。0より大きい間、命中した旨をデバッグUIへ表示する。
	float m_hitMarkerRemainingSeconds{};
};