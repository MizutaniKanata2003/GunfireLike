#pragma once

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/H/Transform.h"

//========= 前方宣言=========
class GraphicsSystem;

// Stage移動またはShop移動に使用する3D Gateを管理する。
class StageGate final
{
public:
	//========= 列挙型=========
	// Gateが持つ遷移先の種類。
	enum class GateType
	{
		e_PREVIOUS_STAGE,
		e_NEXT_STAGE,
		e_SHOP
	};

	//========= 初期化関数=========
	// Gateの種類、Transform、表示色を初期化する。
	void Initialize( GateType gateType, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& scale, const DirectX::XMFLOAT4& color );

	//========= 更新関数=========
	// Gateの回転アニメーションを更新する。
	void Update( float deltaTime );

	//========= 判定関数=========
	// PlayerがGateの操作範囲内にいるかを返す。
	[[nodiscard]] bool IsPlayerNear( const DirectX::XMFLOAT3& playerPosition, float interactionRadiusSquared ) const;

	//========= 描画関数=========
	// GateをOpaque Passで描画する。
	void Draw( BasicMeshRenderer& basicMeshRenderer, GraphicsSystem& graphicsSystem, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix ) const;

	//========= Getter関数=========
	// Gateの種類を返す。
	[[nodiscard]] GateType GetGateType() const { return m_GateType; }
	// GateのWorld座標を返す。
	[[nodiscard]] const DirectX::XMFLOAT3& GetPosition() const { return  m_Transform.GetPosition(); }
private:
	//========= Gate状態=========
	// Gateの遷移先種別。
	GateType m_GateType{ GateType::e_SHOP };
	// Gateの位置、回転、Scaleを管理するTransform。
	Transform m_Transform{};
	// Gateの単色描画に使用する色。
	DirectX::XMFLOAT4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	// Gateの回転アニメーションに使用する累計時間。
	float m_AnimationTime{};
};