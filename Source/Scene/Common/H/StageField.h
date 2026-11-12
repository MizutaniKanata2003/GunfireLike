#pragma once

// 静的な床と4面の壁のTransformを所有し、外部RendererへCube描画を要求する。
// GPU Resourceの所有、プレイヤーの移動制限、壁との衝突判定は担当しない。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/H/Transform.h"

//========= 前方宣言=========
class GraphicsSystem;

// 床・左壁・右壁・手前壁・奥壁の固定配置と描画順をまとめる。
class StageField final
{
public:
	//========= 初期化関数=========
	// 5つのTransformへ位置・回転・倍率を設定する。Resource生成や読込は行わない。
	// 再呼出時も各Transformの設定を固定値で上書きする。
	void Initialize();

	//========= 描画関数=========
	// 床、左壁、右壁、手前壁、奥壁の順に描画する。呼出側でOPAQUE Passを設定する。
	// basicMeshRenderer: 初期化済みのCube描画基盤への非所有参照。DrawCubeで描画状態を変更する。
	// graphicsSystem: RendererがContextなどを取得する描画基盤への非所有参照。
	// viewMatrix: World空間からView空間への変換行列。参照は保持しない。
	// projectionMatrix: View空間からClip空間への変換行列。参照は保持しない。
	// FieldのTransformは変更しない。行列転送とBindはRendererへ任せる。
	void Draw(BasicMeshRenderer& basicMeshRenderer,GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

private:
	//========= Field Transform=========
	// 床の位置・回転・厚みと広さを保持する。
	Transform m_floorTransform{};
	// -X側の壁の位置・回転・寸法を保持する。
	Transform m_leftWallTransform{};
	// +X側の壁の位置・回転・寸法を保持する。
	Transform m_rightWallTransform{};
	// 手前の-Z側の壁の位置・回転・寸法を保持する。
	Transform m_nearWallTransform{};
	// 奥の+Z側の壁の位置・回転・寸法を保持する。
	Transform m_farWallTransform{};
};