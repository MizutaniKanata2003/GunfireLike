#pragma once

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/H/Transform.h"

//========= 前方宣言=========
class GraphicsSystem;

// 静的なFloorとWallで構成されるGame Stageの3Dフィールドを管理する。
class StageField final
{
public:
	//========= 初期化関数=========
	// Floorと4面WallのTransformを初期化する。
	void Initialize();

	//========= 描画関数=========
	// Floorと4面WallをOpaque Passで描画する。
	void Draw( BasicMeshRenderer& basicMeshRenderer, GraphicsSystem& graphicsSystem, const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix ) const;
private:
	//========= Field Transform=========
	// FloorのTransform。
	Transform m_FloorTransform{};
	// Left WallのTransform。
	Transform m_LeftWallTransform{};
	// Right WallのTransform。
	Transform m_RightWallTransform{};
	// Near WallのTransform。
	Transform m_NearWallTransform{};
	// Far WallのTransform。
	Transform m_FarWallTransform{};
};