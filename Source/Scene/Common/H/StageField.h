#pragma once

// 静的な床と4面の壁のTransformおよび描画を管理する。衝突判定は担当しない。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/H/Transform.h"

//========= 前方宣言=========
class GraphicsSystem;

class StageField final
{
public:
	//========= 初期化関数=========
	// 床と4面の壁のTransformを初期化する。
	void Initialize();

	//========= 描画関数=========
	// 床と4面の壁を描画する。呼出側でOpaque Passを設定する。
	// basicMeshRenderer: Cubeの描画Resourceを所有する非所有参照。
	// graphicsSystem: 描画Device Contextを取得する非所有参照。
	// viewMatrix: ワールド空間からView空間への変換行列。
	// projectionMatrix: View空間からClip空間への変換行列。
	void Draw(BasicMeshRenderer& basicMeshRenderer,GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

private:
	//========= Field Transform=========
	// 床の配置と大きさ。
	Transform m_floorTransform{};
	// 左壁の配置と大きさ。
	Transform m_leftWallTransform{};
	// 右壁の配置と大きさ。
	Transform m_rightWallTransform{};
	// 手前の壁の配置と大きさ。
	Transform m_nearWallTransform{};
	// 奥の壁の配置と大きさ。
	Transform m_farWallTransform{};
};