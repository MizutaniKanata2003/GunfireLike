#pragma once

// Rendererが扱う頂点属性の組み合わせを識別する列挙型を定義する。
// 頂点構造体、GPU Buffer、Input Layout、Shaderの生成・Bindは担当しない。
// C++の列挙型だけを定義するため、標準・Windows・DirectXのHeaderはここではincludeしない。

//========= 頂点形式識別型=========
// 属性の分類を示す識別値であり、成分数・Format・Offset・Strideを自動決定する値ではない。
// 各Rendererは、CPU頂点構造体・Input Layout・Vertex Shader入力を別途対応させる。
// 列挙名・列挙順・暗黙値は維持する。下記は0、1、2、3であり、bitの組合せを表すFlagとしては定義していない。
enum class VERTEX_FORMAT
{
	// Positionだけを持つ形式。HudRendererが返す識別値で、実際の入力は2成分のPOSITION。
	// この識別値だけから3Dのfloat3位置やWorld座標を想定せず、HUD専用Layoutを参照する。
	e_POSITION,
	// Positionと頂点Colorを持つ形式の識別値。Colorの成分数や配置はこの列挙子では指定しない。
	// 定義の存在だけで、この形式に対応するRendererやShaderが実装済みとは扱わない。
	e_POSITION_COLOR,
	// PositionとTexture UVを持つ形式。BasicMeshRendererとObjModelRendererが返す識別値。
	// 現行の両Rendererはfloat3位置とfloat2 UVを使う。Texture画像・Samplerの所有やBindは別の処理。
	e_POSITION_TEXTURE,
	// Position・Normal・Texture UVを持つ形式の識別値。法線の座標系・正規化・転送配置は利用側が決める。
	// この値の定義は、OBJの法線値読込やLighting処理が実装済みであることを意味しない。
	e_POSITION_NORMAL_TEXTURE
};