#include "../H/GunViewModel.h"

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

namespace
{
	//========= Gun定数=========
	// GunのCamera空間における本体位置。
	constexpr float GUN_POSITION_X = 0.55f;
	constexpr float GUN_POSITION_Y = -0.38f;
	constexpr float GUN_POSITION_Z = 1.20f;

	// Gun本体のScale。
	constexpr float GUN_BODY_SCALE_X = 0.24f;
	constexpr float GUN_BODY_SCALE_Y = 0.16f;
	constexpr float GUN_BODY_SCALE_Z = 0.65f;

	// Gun BarrelのScaleと本体からの位置補正。
	constexpr float GUN_BARREL_SCALE_X = 0.09f;
	constexpr float GUN_BARREL_SCALE_Y = 0.09f;
	constexpr float GUN_BARREL_SCALE_Z = 0.55f;
	constexpr float GUN_BARREL_OFFSET_Y = 0.04f;
	constexpr float GUN_BARREL_OFFSET_Z = 0.55f;

	// Muzzle FlashのScale、位置補正、表示時間。
	constexpr float MUZZLE_FLASH_SCALE = 0.20f;
	constexpr float MUZZLE_FLASH_OFFSET_Z = 1.10f;
	constexpr float MUZZLE_FLASH_DURATION = 0.08f;
}

// Gun View ModelのMuzzle Flash状態を初期化する。
void GunViewModel::Initialize()
{
	m_BodyPosition = DirectX::XMFLOAT3{ GUN_POSITION_X,GUN_POSITION_Y,GUN_POSITION_Z };
	m_BodyScale = DirectX::XMFLOAT3{ GUN_BODY_SCALE_X,GUN_BODY_SCALE_Y,GUN_BODY_SCALE_Z };
	m_BarrelPosition = DirectX::XMFLOAT3{ GUN_POSITION_X,GUN_POSITION_Y + GUN_BARREL_OFFSET_Y,GUN_POSITION_Z + GUN_BARREL_OFFSET_Z };
	m_BarrelScale = DirectX::XMFLOAT3{ GUN_BARREL_SCALE_X,GUN_BARREL_SCALE_Y,GUN_BARREL_SCALE_Z };
	m_MuzzleFlashPosition = DirectX::XMFLOAT3{ GUN_POSITION_X,GUN_POSITION_Y + GUN_BARREL_OFFSET_Y,GUN_POSITION_Z + MUZZLE_FLASH_OFFSET_Z };
	m_MuzzleFlashScale = MUZZLE_FLASH_SCALE;
	m_MuzzleFlashDuration = MUZZLE_FLASH_DURATION;
	m_MuzzleFlashTimer = {};
}

// Gun本体とGun BarrelをOpaque Passで描画する。
void GunViewModel::DrawOpaque( BasicMeshRenderer& basicMeshRenderer, GraphicsSystem& graphicsSystem,
							   const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix ) const
{
	const DirectX::XMMATRIX inverseViewMatrix = DirectX::XMMatrixInverse( nullptr, viewMatrix );

	const DirectX::XMMATRIX bodyWorldMatrix =
		DirectX::XMMatrixScaling( m_BodyScale.x, m_BodyScale.y, m_BodyScale.z ) *
		DirectX::XMMatrixTranslation( m_BodyPosition.x, m_BodyPosition.y, m_BodyPosition.z ) *
		inverseViewMatrix;

	const DirectX::XMMATRIX barrelWorldMatrix =
		DirectX::XMMatrixScaling( m_BarrelScale.x, m_BarrelScale.y, m_BarrelScale.z ) *
		DirectX::XMMatrixTranslation( m_BarrelPosition.x, m_BarrelPosition.y, m_BarrelPosition.z ) *
		inverseViewMatrix;

	basicMeshRenderer.DrawCube( graphicsSystem, bodyWorldMatrix, viewMatrix, projectionMatrix,
								DirectX::XMFLOAT4{ 0.12f, 0.12f, 0.14f, 1.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f },
								BasicMeshRenderer::TextureType::Color );

	basicMeshRenderer.DrawCube( graphicsSystem, barrelWorldMatrix, viewMatrix, projectionMatrix,
								DirectX::XMFLOAT4{ 0.30f, 0.32f, 0.36f, 1.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f },
								BasicMeshRenderer::TextureType::Color );
}

// Muzzle FlashをTransparent Passで描画する。
void GunViewModel::DrawTransparent( BasicMeshRenderer& basicMeshRenderer, GraphicsSystem& graphicsSystem,
									const DirectX::XMMATRIX& viewMatrix, const DirectX::XMMATRIX& projectionMatrix ) const
{
	if ( m_MuzzleFlashTimer <= 0.0f ) return;

	const float muzzleFlashAlpha = m_MuzzleFlashTimer / m_MuzzleFlashDuration;
	const DirectX::XMMATRIX inverseViewMatrix = DirectX::XMMatrixInverse( nullptr, viewMatrix );

	const DirectX::XMMATRIX muzzleFlashWorldMatrix =
		DirectX::XMMatrixScaling( m_MuzzleFlashScale, m_MuzzleFlashScale, m_MuzzleFlashScale ) *
		DirectX::XMMatrixTranslation( m_MuzzleFlashPosition.x, m_MuzzleFlashPosition.y, m_MuzzleFlashPosition.z ) *
		inverseViewMatrix;

	graphicsSystem.SetRenderPass( e_RenderPass::e_TRANSPARENT );

	basicMeshRenderer.DrawCube( graphicsSystem, muzzleFlashWorldMatrix, viewMatrix, projectionMatrix,
								DirectX::XMFLOAT4{ 1.0f, 0.65f, 0.05f, muzzleFlashAlpha }, DirectX::XMFLOAT2{ 1.0f, 1.0f },
								BasicMeshRenderer::TextureType::Color );
}