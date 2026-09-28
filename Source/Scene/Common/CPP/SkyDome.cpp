#include "../H/SkyDome.h"

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

namespace
{
	//========= Sky Dome定数=========
	// Sky DomeのScaleとCamera追従時のY座標補正。
	constexpr float SKY_DOME_SCALE = 150.0f;
	constexpr float SKY_DOME_Y_OFFSET = 0.0f;
}

// Sky DomeのScale、Transformを初期化する。
void SkyDome::Initialize()
{
	m_Transform.SetPosition( DirectX::XMFLOAT3{} );
	m_Transform.SetRotation( DirectX::XMFLOAT3{} );
	m_Transform.SetScale( DirectX::XMFLOAT3{ SKY_DOME_SCALE,SKY_DOME_SCALE,SKY_DOME_SCALE } );
}

// Camera位置に追従するSky DomeをSky Passで描画する。
void SkyDome::Draw( ObjModelRenderer& skyDomeRenderer, GraphicsSystem& graphicsSystem, const DirectX::XMMATRIX& viewMatrix,
					const DirectX::XMMATRIX& projectionMatrix, const DirectX::XMFLOAT3& cameraPosition )
{
	m_Transform.SetPosition( DirectX::XMFLOAT3{ cameraPosition.x,cameraPosition.y + SKY_DOME_Y_OFFSET,cameraPosition.z } );

	graphicsSystem.SetRenderPass( e_RenderPass::e_SKY );

	skyDomeRenderer.Draw( graphicsSystem, m_Transform.GetWorldMatrix(), viewMatrix, projectionMatrix, DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f } );

	graphicsSystem.SetRenderPass( e_RenderPass::e_OPAQUE );
}