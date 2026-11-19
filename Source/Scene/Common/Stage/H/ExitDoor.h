#pragma once

// 戦闘部屋の出口（閉じた扉と、開放後に床へ表示する目印）を管理する。閉じている間は、AABB Colliderで通行を止める。
// 開放後は、プレイヤーが通過範囲に入ったかを判定する。敵の全滅判定・部屋の遷移は担当しない。呼出側が進行状態に合わせて、開閉を指示する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Object/H/Transform.h"

//========= 前方宣言=========
class BasicMeshRenderer;
class CollisionWorld;
class GraphicsSystem;
class ModelRegistry;

// 出口の位置・大きさ・色・通過範囲をまとめる。値は、奥壁（中心Z=18、厚み0.2）の手前に収まるよう決めた試遊用の仮値。
namespace EXIT_DOOR
{
	//========= 扉の形状=========
	// 閉じた扉の中心のワールド座標。奥壁の内側の面（Z=17.9）に、扉の奥側の面が接する位置。Yは床の上面（-0.5）から扉の上端までの中央。
	inline constexpr XMFLOAT3 ns_DOOR_CENTER{0.0f,1.1f,17.7f};
	// 閉じた扉の寸法。X方向が幅、Y方向が高さ、Z方向が厚み。単位はワールド距離単位。壁の高さ（3.2）より低く、床の上面から壁の上端付近まで届く。
	inline constexpr XMFLOAT3 ns_DOOR_SIZE{3.0f,3.2f,0.4f};

	//========= 開放後の目印=========
	// 開放後に床へ表示する目印の中心のワールド座標。床の上面（Y=-0.5）の少し上に置き、扉の手前の通過位置を示す。
	inline constexpr XMFLOAT3 ns_PAD_CENTER{0.0f,-0.475f,16.6f};
	// 開放後の目印の寸法。X方向が幅（扉と同じ）、Y方向が厚み、Z方向が奥行き。単位はワールド距離単位。
	inline constexpr XMFLOAT3 ns_PAD_SIZE{3.0f,0.05f,2.0f};

	//========= 通過範囲=========
	// 通過とみなすX方向の範囲の半分の幅。中心X=0から左右へこの距離まで。単位はワールド距離単位。扉の幅（3.0）の半分で、目印の幅と同じ。
	inline constexpr float ns_PASS_HALF_WIDTH_X = 1.5f;
	// 通過とみなすZ方向の下限。プレイヤーの中心がこのZ以上になると通過とする。単位はワールド距離単位。目印の中心（Z=16.6）に合わせた値。
	// プレイヤーの中心は壁の押し戻しにより、奥壁の手前のZ=17.5までしか進めないため、この下限は到達できる範囲に収まる。
	inline constexpr float ns_PASS_MIN_Z = 16.6f;

	//========= 色=========
	// 閉じた扉の色。赤で、Object画像の色へ乗算して、通れないことを示す。
	inline constexpr XMFLOAT4 ns_CLOSED_COLOR{0.9f,0.25f,0.25f,1.0f};
	// 開放後の目印の色。緑で、Object画像の色へ乗算して、通れることを示す。
	inline constexpr XMFLOAT4 ns_OPEN_COLOR{0.2f,0.9f,0.3f,1.0f};
}

// 閉じている間だけ通行を止める出口。開閉の状態を持ち、状態に合った見た目を描き、開放後の通過を判定する。
class ExitDoor final
{
public:
	//========= 初期化・終了関数=========
	// 共有のCube Rendererを借用し、閉じた状態の扉のColliderを登録する。
	// registry: Cube Rendererの借用元。Initialize済みであること。このExitDoorより長く生存させる。
	// collisionWorld: Colliderの登録先。このExitDoorより長く生存させ、Uninitまで同じ実体を渡す。
	// 戻り値: Cube Rendererを借用でき、閉じた扉のColliderを登録できればtrue。Registryが未初期化ならfalse。
	[[nodiscard]] bool Initialize(ModelRegistry& registry,CollisionWorld& collisionWorld);
	// 登録中のColliderを解除し、借用した参照を手放す。未初期化・解除済みでも呼出可能。
	// collisionWorld: Initializeで渡したのと同じCollisionWorld。
	void Uninit(CollisionWorld& collisionWorld);

	//========= 更新・描画関数=========
	// 出口の開閉を指示する。現在と同じ状態なら何もしない。開く場合はColliderを解除し、閉じる場合は登録し直す。
	// isOpen: trueで開き、falseで閉じる。
	// collisionWorld: Initializeで渡したのと同じCollisionWorld。
	void SetOpen(bool isOpen,CollisionWorld& collisionWorld);
	// 閉じている間は赤い扉を、開いている間は床の緑の目印を、Cubeで描く。未初期化なら何も描かない。
	// graphicsSystem: Rendererが使う描画基盤。
	// viewMatrix: World空間からView空間への変換行列。
	// projectionMatrix: View空間からClip空間への変換行列。
	void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

	//========= Getter関数=========
	// 出口が開いているかを返す。
	[[nodiscard]] bool IsOpen() const { return m_isOpen; }
	// 出口が開いていて、指定した位置が通過範囲の内にあるかを返す。閉じている間は、範囲の内でもfalse。
	// position: 判定する位置のワールド座標。XとZだけを使い、高さは使わない。プレイヤーの中心を渡す。
	// 戻り値: 開いていて、Xが中心から範囲の半分の幅以内、かつZが下限以上ならtrue。
	[[nodiscard]] bool IsInPassZone(const XMFLOAT3& position) const;

private:
	//========= 表示=========
	// Cube描画に使う共有Renderer。非所有で、ModelRegistryが所有する。未取得はnullptr。
	BasicMeshRenderer* m_cubeRenderer{};
	// 閉じた扉の位置と大きさを保持し、World行列を作るTransform。
	Transform m_doorTransform{};
	// 開放後の目印の位置と大きさを保持し、World行列を作るTransform。
	Transform m_padTransform{};

	//========= 状態=========
	// 閉じた扉のColliderの識別値。開いている間や未登録は無効値。
	ColliderHandle m_colliderHandle{COLLIDER_HANDLE::ns_INVALID};
	// 出口が開いているか。初期状態は閉じている。
	bool m_isOpen{};
};