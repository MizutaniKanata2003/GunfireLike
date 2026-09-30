#pragma once

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/H/CombatSystem.h"
#include "Scene/Common/H/Transform.h"

//========= 前方宣言=========
class GraphicsSystem;

// Shop内の遷移先、Raycast判定、回転描画を管理する3D Gate。
class ShopGate final
{
public:
	//========= 列挙型=========
	// Shop Gateが持つ遷移先の種類。
	enum class GateType
	{
		e_CHALLENGE,
		e_TITLE
	};

	//========= 初期化関数=========
	// Gateの種類、Transform、表示色、Raycast Sphere半径を初期化する。
	void Initialize( GateType gateType, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT4& color, float hitSphereRadius );

	//========= 更新関数=========
	// GateのY軸回転Animationを更新する。
	void Update( float deltaTime );

	//========= 判定関数=========
	// 指定したRayがGateのSphereへ命中した場合、命中距離を返す。
	[[nodiscard]] RaycastResult Raycast( const CombatSystem& combatSystem, const DirectX::XMFLOAT3& rayOrigin,
										 const DirectX::XMFLOAT3& rayDirection, float maxDistance ) const;

	//========= 描画関数=========
	// 照準状態に応じたScaleでGateをOpaque Passに描画する。
	void Draw( BasicMeshRenderer& basicMeshRenderer, GraphicsSystem& graphicsSystem, const DirectX::XMMATRIX& viewMatrix,
			   const DirectX::XMMATRIX& projectionMatrix, bool isAimed ) const;

	//========= Getter関数=========
	// Gateの種類を返す。
	[[nodiscard]] GateType GetGateType() const { return m_GateType; }
private:
	//========= Gate状態=========
	// Gateの遷移先種別。
	GateType m_GateType { GateType::e_CHALLENGE };
	// Gateの位置、回転、Scaleを管理するTransform。
	Transform m_Transform {};
	// Gateの単色描画に使用する色。
	DirectX::XMFLOAT4 m_Color { 1.0f,1.0f,1.0f,1.0f };
	// Raycast判定に使用するSphere半径。
	float m_HitSphereRadius {};
	// Gateの回転Animationに使用する累計時間。
	float m_AnimationTime {};
};