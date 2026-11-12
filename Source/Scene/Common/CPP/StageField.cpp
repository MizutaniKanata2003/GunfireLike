#include "../H/StageField.h"

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

// フィールドの配置・寸法に使用する定数を実装内に限定する。
namespace
{
	//========= Floor配置・寸法定数=========
	constexpr float ns_FLOOR_SCALE_X = 20.0f;
	constexpr float ns_FLOOR_SCALE_Y = 0.2f;
	constexpr float ns_FLOOR_SCALE_Z = 20.0f;
	constexpr float ns_FLOOR_POSITION_X = 0.0f;
	constexpr float ns_FLOOR_POSITION_Y = -0.6f;
	constexpr float ns_FLOOR_POSITION_Z = 8.0f;

	//========= Wall配置・寸法定数=========
	constexpr float ns_WALL_THICKNESS = 0.2f;
	constexpr float ns_WALL_HEIGHT = 3.2f;
	constexpr float ns_WALL_LENGTH = 20.0f;
	constexpr float ns_WALL_CENTER_Y = 1.1f;
	constexpr float ns_LEFT_WALL_POSITION_X = -10.0f;
	constexpr float ns_RIGHT_WALL_POSITION_X = 10.0f;
	constexpr float ns_SIDE_WALL_POSITION_Z = 8.0f;
	constexpr float ns_FRONT_WALL_POSITION_X = 0.0f;
	constexpr float ns_BACK_WALL_POSITION_X = 0.0f;
	constexpr float ns_NEAR_WALL_POSITION_Z = -2.0f;
	constexpr float ns_FAR_WALL_POSITION_Z = 18.0f;
}

void StageField::Initialize()
{
	m_floorTransform.SetPosition(XMFLOAT3{ns_FLOOR_POSITION_X,ns_FLOOR_POSITION_Y,ns_FLOOR_POSITION_Z});
	m_floorTransform.SetRotation(XMFLOAT3{});
	m_floorTransform.SetScale(XMFLOAT3{ns_FLOOR_SCALE_X,ns_FLOOR_SCALE_Y,ns_FLOOR_SCALE_Z});

	m_leftWallTransform.SetPosition(XMFLOAT3{ns_LEFT_WALL_POSITION_X,ns_WALL_CENTER_Y,ns_SIDE_WALL_POSITION_Z});
	m_leftWallTransform.SetRotation(XMFLOAT3{});
	m_leftWallTransform.SetScale(XMFLOAT3{ns_WALL_THICKNESS,ns_WALL_HEIGHT,ns_WALL_LENGTH});

	m_rightWallTransform.SetPosition(XMFLOAT3{ns_RIGHT_WALL_POSITION_X,ns_WALL_CENTER_Y,ns_SIDE_WALL_POSITION_Z});
	m_rightWallTransform.SetRotation(XMFLOAT3{});
	m_rightWallTransform.SetScale(XMFLOAT3{ns_WALL_THICKNESS,ns_WALL_HEIGHT,ns_WALL_LENGTH});

	m_nearWallTransform.SetPosition(XMFLOAT3{ns_FRONT_WALL_POSITION_X,ns_WALL_CENTER_Y,ns_NEAR_WALL_POSITION_Z});
	m_nearWallTransform.SetRotation(XMFLOAT3{});
	m_nearWallTransform.SetScale(XMFLOAT3{ns_WALL_LENGTH,ns_WALL_HEIGHT,ns_WALL_THICKNESS});

	m_farWallTransform.SetPosition(XMFLOAT3{ns_BACK_WALL_POSITION_X,ns_WALL_CENTER_Y,ns_FAR_WALL_POSITION_Z});
	m_farWallTransform.SetRotation(XMFLOAT3{});
	m_farWallTransform.SetScale(XMFLOAT3{ns_WALL_LENGTH,ns_WALL_HEIGHT,ns_WALL_THICKNESS});
}

void StageField::Draw(BasicMeshRenderer& basicMeshRenderer,GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
	const XMFLOAT4 wall_color{1.0f,1.0f,1.0f,1.0f};

	basicMeshRenderer.DrawCube(graphicsSystem,m_floorTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,
														 XMFLOAT4{1.0f,1.0f,1.0f,1.0f},XMFLOAT2{10.0f,10.0f},BasicMeshRenderer::TextureType::Floor);

	basicMeshRenderer.DrawCube(graphicsSystem,m_leftWallTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,
														 wall_color,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Wall);

	basicMeshRenderer.DrawCube(graphicsSystem,m_rightWallTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,
														 wall_color,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Wall);

	basicMeshRenderer.DrawCube(graphicsSystem,m_nearWallTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,
														 wall_color,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Wall);

	basicMeshRenderer.DrawCube(graphicsSystem,m_farWallTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,
														 wall_color,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Wall);
}