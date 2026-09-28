#pragma once

//========= Framework インクルード=========
#include "Framework/3D/H/ObjModelRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/H/Transform.h"

//========= 前方宣言=========
class GraphicsSystem;

// EnemyのTransform、浮遊・回転Animation、Model描画、Hit位置を管理する。
class EnemyVisual final
{
public:
	//========= 初期化関数=========
	// EnemyのTransform、浮遊・回転Animation状態を初期化する。
	void Initialize( const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& scale );

	//========= 更新関数=========
	// Enemyの浮遊・回転Animationを更新する。
	void Update( float deltaTime );

	//========= 描画関数=========
	// Enemy OBJ ModelをOpaque Passで描画する。
	void Draw( ObjModelRenderer& enemyModelRenderer, GraphicsSystem& graphicsSystem,
	const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix ) const;

	//========= Getter関数=========
	// Enemyの現在World座標を返す。
	[[nodiscard]] const DirectX::XMFLOAT3& GetPosition() const { return m_Transform.GetPosition(); }
	// EnemyのRay命中判定に使用するSphere中心座標を返す。
	[[nodiscard]] DirectX::XMFLOAT3 GetHitSphereCenter( float hitCenterYOffset ) const;
	// Enemy頭上HPバーのWorld座標を返す。
	[[nodiscard]] DirectX::XMFLOAT3 GetHealthBarPosition( float healthBarYOffset ) const;
private:
	//========= Transform・Animation状態=========
	// Enemyの位置、回転、Scale、World行列を管理する。
	Transform m_Transform{};
	// Enemyの初期World座標。
	DirectX::XMFLOAT3 m_BasePosition{};
	// Enemy浮遊・回転Animationに使用する累計時間。
	float m_AnimationTime{};
};