#pragma once

// 3D Objectの生成設定を表す型を定義する。設定表の各行から、SceneObjectFactoryが共有Rendererと組み合わせてObjectを生成する。
// Resourceの読込・生成・描画は担当しない。設定が指す文字列は所有しないため、静的な文字列定数だけを指定する。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tでCollisionレイヤーのbitを保持する。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/Etc/H/MathTypes.h"

// 3D Objectの描画に使う形状の種別。
enum class OBJECT_MESH_KIND
{
	e_CUBE, // BasicMeshRendererの1辺1のCubeを使う。画像はBasicMeshRenderer::TextureTypeから選ぶ。
	e_OBJ   // ObjModelRendererの共有モデルを使う。OBJパスと画像パスの組で共有Rendererを選ぶ。
};

// 3D Object1個分の生成設定。設定表の1行として並べ、追加する物が増えてもFrameworkを編集しない。
struct OBJECT_CONFIG
{
	//========= 描画する形状=========
	// 形状の種別。既定はCube。
	OBJECT_MESH_KIND meshKind{OBJECT_MESH_KIND::e_CUBE};
	// Cubeの画像選択。meshKindがe_CUBEの場合だけ使用し、既定は画像なしの単色。
	BasicMeshRenderer::TextureType cubeTexture{BasicMeshRenderer::TextureType::Color};
	// OBJファイルのパス。作業ディレクトリ基準。meshKindがe_OBJの場合は必須で、Cubeではnullptr。文字列は所有しない。
	const wchar_t* modelPath{};
	// OBJに貼る画像のパス。nullptrまたは空文字列なら画像を読み込まず単色。meshKindがe_OBJの場合だけ使用する。
	const wchar_t* texturePath{};

	//========= 初期Transform=========
	// 初期位置。ワールド座標のX・Y・Z。
	XMFLOAT3 position{};
	// 初期回転。X=Pitch、Y=Yaw、Z=Rollで、単位はラジアン。
	XMFLOAT3 rotation{};
	// 各軸の倍率。1辺1のCubeではそのまま各軸の寸法になる。既定は等倍。
	XMFLOAT3 scale{1.0f,1.0f,1.0f};

	//========= 見た目=========
	// 乗算するRGBA色。既定は不透明な白で、画像本来の色を維持する。
	XMFLOAT4 color{1.0f,1.0f,1.0f,1.0f};
	// UVのU・Vへ掛ける倍率。Cubeだけに反映する。OBJは現行ObjModelRendererの固定値(1,1)を使い、この値を使用しない。
	XMFLOAT2 uvTiling{1.0f,1.0f};

	//========= Collision=========
	// 属するCollisionレイヤーのbit。0ならColliderを登録しない。0以外ならCollisionWorldへAABBを登録する。
	// AABBはposition±scale/2から作り、rotationは反映しない。meshKindがe_CUBEの場合だけ使用でき、e_OBJでは生成に失敗する。
	std::uint32_t collisionLayer{};
};