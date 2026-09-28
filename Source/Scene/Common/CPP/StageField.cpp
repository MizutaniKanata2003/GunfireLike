#include "../H/StageField.h"

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

namespace
{
	//========= Floor定数=========
	// FloorのScaleとWorld座標。
	constexpr float FLOOR_SCALE_X = 20.0f;
	constexpr float FLOOR_SCALE_Y = 0.2f;
	constexpr float FLOOR_SCALE_Z = 20.0f;
	constexpr float FLOOR_POSITION_X = 0.0f;
	constexpr float FLOOR_POSITION_Y = -0.6f;
	constexpr float FLOOR_POSITION_Z = 8.0f;

	//========= Wall定数=========
	// 4面WallのScaleとWorld座標。
	constexpr float WALL_THICKNESS = 0.2f;
	constexpr float WALL_HEIGHT = 3.2f;
	constexpr float WALL_LENGTH = 20.0f;
	constexpr float WALL_CENTER_Y = 1.1f;

	constexpr float LEFT_WALL_POSITION_X = -10.0f;
	constexpr float RIGHT_WALL_POSITION_X = 10.0f;
	constexpr float SIDE_WALL_POSITION_Z = 8.0f;

	constexpr float FRONT_WALL_POSITION_X = 0.0f;
	constexpr float BACK_WALL_POSITION_X = 0.0f;
	constexpr float NEAR_WALL_POSITION_Z = -2.0f;
	constexpr float FAR_WALL_POSITION_Z = 18.0f;
}

// Floorと4面WallのTransformを初期化する。
void StageField::Initialize()
{
	m_FloorTransform.SetPosition( DirectX::XMFLOAT3{ FLOOR_POSITION_X,FLOOR_POSITION_Y,FLOOR_POSITION_Z } );
	m_FloorTransform.SetRotation( DirectX::XMFLOAT3{} );
	m_FloorTransform.SetScale( DirectX::XMFLOAT3{ FLOOR_SCALE_X,FLOOR_SCALE_Y,FLOOR_SCALE_Z } );

	m_LeftWallTransform.SetPosition( DirectX::XMFLOAT3{ LEFT_WALL_POSITION_X,WALL_CENTER_Y,SIDE_WALL_POSITION_Z } );
	m_LeftWallTransform.SetRotation( DirectX::XMFLOAT3{} );
	m_LeftWallTransform.SetScale( DirectX::XMFLOAT3{ WALL_THICKNESS,WALL_HEIGHT,WALL_LENGTH } );

	m_RightWallTransform.SetPosition( DirectX::XMFLOAT3{ RIGHT_WALL_POSITION_X,WALL_CENTER_Y,SIDE_WALL_POSITION_Z } );
	m_RightWallTransform.SetRotation( DirectX::XMFLOAT3{} );
	m_RightWallTransform.SetScale( DirectX::XMFLOAT3{ WALL_THICKNESS,WALL_HEIGHT,WALL_LENGTH } );

	m_NearWallTransform.SetPosition( DirectX::XMFLOAT3{ FRONT_WALL_POSITION_X,WALL_CENTER_Y,NEAR_WALL_POSITION_Z } );
	m_NearWallTransform.SetRotation( DirectX::XMFLOAT3{} );
	m_NearWallTransform.SetScale( DirectX::XMFLOAT3{ WALL_LENGTH,WALL_HEIGHT,WALL_THICKNESS } );

	m_FarWallTransform.SetPosition( DirectX::XMFLOAT3{ BACK_WALL_POSITION_X,WALL_CENTER_Y,FAR_WALL_POSITION_Z } );
	m_FarWallTransform.SetRotation( DirectX::XMFLOAT3{} );
	m_FarWallTransform.SetScale( DirectX::XMFLOAT3{ WALL_LENGTH,WALL_HEIGHT,WALL_THICKNESS } );
}

// Floorと4面WallをOpaque Passで描画する。
void StageField::Draw( BasicMeshRenderer& basicMeshRenderer, GraphicsSystem& graphicsSystem,
					   const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix ) const
{
	const DirectX::XMFLOAT4 wallColor{ 1.0f, 1.0f, 1.0f, 1.0f };

	basicMeshRenderer.DrawCube( graphicsSystem, m_FloorTransform.GetWorldMatrix(), viewMatrix, projectionMatrix,
								DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f }, DirectX::XMFLOAT2{ 10.0f, 10.0f }, BasicMeshRenderer::TextureType::Floor );

	basicMeshRenderer.DrawCube( graphicsSystem, m_LeftWallTransform.GetWorldMatrix(), viewMatrix, projectionMatrix,
								wallColor, DirectX::XMFLOAT2{ 1.0f, 1.0f }, BasicMeshRenderer::TextureType::Wall );

	basicMeshRenderer.DrawCube( graphicsSystem, m_RightWallTransform.GetWorldMatrix(), viewMatrix, projectionMatrix, wallColor,
								DirectX::XMFLOAT2{ 1.0f, 1.0f }, BasicMeshRenderer::TextureType::Wall );

	basicMeshRenderer.DrawCube( graphicsSystem, m_NearWallTransform.GetWorldMatrix(), viewMatrix, projectionMatrix, wallColor,
								DirectX::XMFLOAT2{ 1.0f, 1.0f }, BasicMeshRenderer::TextureType::Wall );

	basicMeshRenderer.DrawCube( graphicsSystem, m_FarWallTransform.GetWorldMatrix(), viewMatrix, projectionMatrix, wallColor,
								DirectX::XMFLOAT2{ 1.0f, 1.0f }, BasicMeshRenderer::TextureType::Wall );
}