#include "../H/StageField.h"

// 床と4面の壁の固定配置を設定し、既存の色・UV倍率・Texture種別と順序で描画する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

// 床と壁の配置・拡大縮小に使う固定値を、この実装ファイル内にまとめる。
namespace
{
	//========= Floor配置・寸法定数=========
	// 床のX軸方向の倍率。
	constexpr float ns_FLOOR_SCALE_X = 20.0f;
	// 床の厚みを表すY軸倍率。
	constexpr float ns_FLOOR_SCALE_Y = 0.2f;
	// 床のZ軸方向の倍率。
	constexpr float ns_FLOOR_SCALE_Z = 20.0f;
	// 床中心のX座標。
	constexpr float ns_FLOOR_POSITION_X = 0.0f;
	// 床中心のY座標。
	constexpr float ns_FLOOR_POSITION_Y = -0.6f;
	// 床中心のZ座標。
	constexpr float ns_FLOOR_POSITION_Z = 8.0f;

	//========= Wall配置・寸法定数=========
	// 壁の薄い軸に適用する倍率。
	constexpr float ns_WALL_THICKNESS = 0.2f;
	// 各壁のY軸倍率。
	constexpr float ns_WALL_HEIGHT = 3.2f;
	// 各壁の長辺に適用する倍率。
	constexpr float ns_WALL_LENGTH = 20.0f;
	// 全壁で共通の中心Y座標。
	constexpr float ns_WALL_CENTER_Y = 1.1f;
	// 左壁の中心X座標。
	constexpr float ns_LEFT_WALL_POSITION_X = -10.0f;
	// 右壁の中心X座標。
	constexpr float ns_RIGHT_WALL_POSITION_X = 10.0f;
	// 左右壁で共通の中心Z座標。
	constexpr float ns_SIDE_WALL_POSITION_Z = 8.0f;
	// 手前壁の中心X座標。
	constexpr float ns_FRONT_WALL_POSITION_X = 0.0f;
	// 奥壁の中心X座標。
	constexpr float ns_BACK_WALL_POSITION_X = 0.0f;
	// 手前壁の中心Z座標。
	constexpr float ns_NEAR_WALL_POSITION_Z = -2.0f;
	// 奥壁の中心Z座標。
	constexpr float ns_FAR_WALL_POSITION_Z = 18.0f;
}

//========= 初期化関数=========
// 床と各壁へ、回転なしの固定配置と各軸倍率を設定する。
void StageField::Initialize()
{
	// 床の中心位置を設定し、薄いCubeを床として広げる。
	m_floorTransform.SetPosition(XMFLOAT3{ns_FLOOR_POSITION_X,ns_FLOOR_POSITION_Y,ns_FLOOR_POSITION_Z});
	m_floorTransform.SetRotation(XMFLOAT3{});
	m_floorTransform.SetScale(XMFLOAT3{ns_FLOOR_SCALE_X,ns_FLOOR_SCALE_Y,ns_FLOOR_SCALE_Z});

	// 左壁を-X側へ置き、X方向を厚み、Z方向を長辺にする。
	m_leftWallTransform.SetPosition(XMFLOAT3{ns_LEFT_WALL_POSITION_X,ns_WALL_CENTER_Y,ns_SIDE_WALL_POSITION_Z});
	m_leftWallTransform.SetRotation(XMFLOAT3{});
	m_leftWallTransform.SetScale(XMFLOAT3{ns_WALL_THICKNESS,ns_WALL_HEIGHT,ns_WALL_LENGTH});

	// 右壁を+X側へ置き、左壁と同じ各軸倍率を適用する。
	m_rightWallTransform.SetPosition(XMFLOAT3{ns_RIGHT_WALL_POSITION_X,ns_WALL_CENTER_Y,ns_SIDE_WALL_POSITION_Z});
	m_rightWallTransform.SetRotation(XMFLOAT3{});
	m_rightWallTransform.SetScale(XMFLOAT3{ns_WALL_THICKNESS,ns_WALL_HEIGHT,ns_WALL_LENGTH});

	// 手前壁を-Z側へ置き、Z方向を厚み、X方向を長辺にする。
	m_nearWallTransform.SetPosition(XMFLOAT3{ns_FRONT_WALL_POSITION_X,ns_WALL_CENTER_Y,ns_NEAR_WALL_POSITION_Z});
	m_nearWallTransform.SetRotation(XMFLOAT3{});
	m_nearWallTransform.SetScale(XMFLOAT3{ns_WALL_LENGTH,ns_WALL_HEIGHT,ns_WALL_THICKNESS});

	// 奥壁を+Z側へ置き、手前壁と同じ各軸倍率を適用する。
	m_farWallTransform.SetPosition(XMFLOAT3{ns_BACK_WALL_POSITION_X,ns_WALL_CENTER_Y,ns_FAR_WALL_POSITION_Z});
	m_farWallTransform.SetRotation(XMFLOAT3{});
	m_farWallTransform.SetScale(XMFLOAT3{ns_WALL_LENGTH,ns_WALL_HEIGHT,ns_WALL_THICKNESS});
}

//========= 描画関数=========
// 床と各壁をCubeとして描画する。GPU側の更新とBindはRendererへ委譲する。
void StageField::Draw(BasicMeshRenderer& basicMeshRenderer,GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
	// 壁Textureへ掛ける共通色を不透明な白とし、Texture本来の色を維持する。
	const XMFLOAT4 wall_color{1.0f,1.0f,1.0f,1.0f};

	// 床を白色・UV10倍のFloor Textureで描く。
	basicMeshRenderer.DrawCube(graphicsSystem,m_floorTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,
														 XMFLOAT4{1.0f,1.0f,1.0f,1.0f},XMFLOAT2{10.0f,10.0f},BasicMeshRenderer::TextureType::Floor);

	// 左壁を白色・UV1倍のWall Textureで描く。
	basicMeshRenderer.DrawCube(graphicsSystem,m_leftWallTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,
														 wall_color,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Wall);

	// 右壁を左壁の後に同じ色・UV倍率・Texture種別で描く。
	basicMeshRenderer.DrawCube(graphicsSystem,m_rightWallTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,
														 wall_color,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Wall);

	// 手前壁を左右壁の後に描く。
	basicMeshRenderer.DrawCube(graphicsSystem,m_nearWallTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,
														 wall_color,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Wall);

	// 奥壁を最後に描く。
	basicMeshRenderer.DrawCube(graphicsSystem,m_farWallTransform.GetWorldMatrix(),viewMatrix,projectionMatrix,
														 wall_color,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Wall);
}