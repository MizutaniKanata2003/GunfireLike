#include "../H/EnemyHealthBar.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

namespace
{
	//========= HP Bar定数=========
	// Enemy頭上に描画するHPバーのサイズと配置。
	constexpr float HP_BAR_WIDTH = 2.0f;
	constexpr float HP_BAR_HEIGHT = 0.18f;

	// HPバーの厚さ、Camera方向への補正、表示判定に使用する値。
	constexpr float HP_BAR_BACKGROUND_DEPTH = 0.02f;
	constexpr float HP_BAR_FOREGROUND_DEPTH = 0.02f;
	constexpr float HP_BAR_FOREGROUND_CAMERA_OFFSET = 0.08f;
	constexpr float HP_BAR_VISIBLE_RATIO_THRESHOLD = 0.001f;
	constexpr float BILLBOARD_MIN_CAMERA_DISTANCE_SQUARED = 0.0001f;
}

// Enemyの現在HPをCamera方向へ向けたBillboard HPバーとして描画する。
void EnemyHealthBar::Draw( BasicMeshRenderer& basicMeshRenderer, GraphicsSystem& graphicsSystem, const DirectX::XMMATRIX& viewMatrix,
						   const DirectX::XMMATRIX& projectionMatrix, const DirectX::XMFLOAT3& cameraPosition, const DirectX::XMFLOAT3& healthBarPosition,
						   float currentHp, float maxHp, bool isEnemyDead ) const
{
	if ( maxHp <= 0.0f || isEnemyDead ) return;

	const float enemyHpRatio = std::clamp( currentHp / maxHp, 0.0f, 1.0f );

	if ( enemyHpRatio <= HP_BAR_VISIBLE_RATIO_THRESHOLD ) return;

	DirectX::XMVECTOR toCameraVector =
		DirectX::XMVectorSubtract( DirectX::XMLoadFloat3( &cameraPosition ), DirectX::XMLoadFloat3( &healthBarPosition ) );
	toCameraVector = DirectX::XMVectorSetY( toCameraVector, 0.0f );

	const float toCameraLengthSquared = DirectX::XMVectorGetX( DirectX::XMVector3LengthSq( toCameraVector ) );

	if ( toCameraLengthSquared <= BILLBOARD_MIN_CAMERA_DISTANCE_SQUARED ) return;

	toCameraVector = DirectX::XMVector3Normalize( toCameraVector );

	const DirectX::XMVECTOR worldUpVector = DirectX::XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
	const DirectX::XMVECTOR billboardRightVector = DirectX::XMVector3Normalize( DirectX::XMVector3Cross( worldUpVector, toCameraVector ) );
	const DirectX::XMVECTOR billboardForwardVector = DirectX::XMVector3Normalize( DirectX::XMVector3Cross( billboardRightVector, worldUpVector ) );

	DirectX::XMMATRIX billboardRotationMatrix = DirectX::XMMatrixIdentity();
	billboardRotationMatrix.r[0] = DirectX::XMVectorSetW( billboardRightVector, 0.0f );
	billboardRotationMatrix.r[1] = DirectX::XMVectorSetW( worldUpVector, 0.0f );
	billboardRotationMatrix.r[2] = DirectX::XMVectorSetW( billboardForwardVector, 0.0f );

	const float hpBarForegroundWidth = HP_BAR_WIDTH * enemyHpRatio;
	const float hpBarForegroundXOffset = -( HP_BAR_WIDTH - hpBarForegroundWidth ) * 0.5f;

	const DirectX::XMMATRIX hpBarBackgroundWorldMatrix = DirectX::XMMatrixScaling( HP_BAR_WIDTH, HP_BAR_HEIGHT, HP_BAR_BACKGROUND_DEPTH ) *
		billboardRotationMatrix * DirectX::XMMatrixTranslation( healthBarPosition.x, healthBarPosition.y, healthBarPosition.z );

	const DirectX::XMVECTOR foregroundPositionVector = DirectX::XMVectorAdd(
	DirectX::XMVectorSet( healthBarPosition.x + hpBarForegroundXOffset, healthBarPosition.y, healthBarPosition.z, 1.0f ),
	DirectX::XMVectorScale( toCameraVector, HP_BAR_FOREGROUND_CAMERA_OFFSET ) );

	DirectX::XMFLOAT3 foregroundPosition {};
	DirectX::XMStoreFloat3( &foregroundPosition, foregroundPositionVector );

	const DirectX::XMMATRIX hpBarForegroundWorldMatrix = DirectX::XMMatrixScaling( hpBarForegroundWidth, HP_BAR_HEIGHT, HP_BAR_FOREGROUND_DEPTH ) *
		billboardRotationMatrix * DirectX::XMMatrixTranslation( foregroundPosition.x, foregroundPosition.y, foregroundPosition.z );

	graphicsSystem.SetRenderPass( e_RenderPass::e_TRANSPARENT );

	basicMeshRenderer.DrawCube( graphicsSystem, hpBarBackgroundWorldMatrix, viewMatrix, projectionMatrix,
	DirectX::XMFLOAT4 { 0.0f,0.0f,0.0f,0.70f }, DirectX::XMFLOAT2 { 1.0f, 1.0f }, BasicMeshRenderer::TextureType::Color );

	basicMeshRenderer.DrawCube( graphicsSystem, hpBarForegroundWorldMatrix, viewMatrix, projectionMatrix,
	DirectX::XMFLOAT4 { 0.10f,1.0f,0.20f,0.95f }, DirectX::XMFLOAT2 { 1.0f, 1.0f }, BasicMeshRenderer::TextureType::Color );
}