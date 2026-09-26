#pragma once

//========= Framework インクルード=========
#include "Framework/3D/H/ObjModelRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/H/Transform.h"

//========= 前方宣言=========
class GraphicsSystem;

// Cameraに追従するSky DomeのTransformとSky Pass描画を管理する。
class SkyDome final
{
public:
	//========= 初期化関数=========
	// Sky DomeのScale、Transformを初期化する。
	void Initialize();

	//========= 描画関数=========
	// Camera位置に追従するSky DomeをSky Passで描画する。
	void Draw(
	ObjModelRenderer& skyDomeRenderer,
	GraphicsSystem& graphicsSystem,
	const DirectX::XMMATRIX& viewMatrix,
	const DirectX::XMMATRIX& projectionMatrix,
	const DirectX::XMFLOAT3& cameraPosition );

private:
	//========= Sky Dome状態=========
	// Sky Domeの位置、回転、Scale、World行列を管理する。
	Transform m_Transform{};
};