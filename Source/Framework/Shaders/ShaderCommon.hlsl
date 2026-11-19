// BasicColorのVertex/Pixel Shaderが共有する定数バッファと頂点入出力を定義する。
// CPU側の転送型と合わせ、フィールド順・型・register・semanticは既存のまま維持する。

#ifndef SHADER_COMMON_HLSLI
#define SHADER_COMMON_HLSLI

//========= Constant Buffer Slot規約=========
// b0: Camera Constants。
// b1: Object Constants。
// b2: Material Constants。
// b3: Light Constants用の割当。このファイルでは宣言・照明処理を実装しない。
// b4: Post Process Constants用の割当。このファイルでは宣言・効果処理を実装しない。

//========= Camera定数バッファ=========
// CPU側のCAMERA_CONSTANTSに対応する。行列の転送前transposeはRenderer側が担当する。
cbuffer CameraConstants: register(b0)
{
  // World空間からClip空間へのView・Projection合成行列。
  matrix viewProjectionMatrix;
};

//========= Object定数バッファ=========
// CPU側のOBJECT_CONSTANTSに対応する。行列と色の配置を維持する。
cbuffer ObjectConstants: register(b1)
{
  // Local空間からWorld空間へ変換する行列。
  matrix worldMatrix;
  // Objectに適用し、Vertex ShaderからPixel Shaderへ渡すRGBA色。
  float4 color;
};

//========= Material定数バッファ=========
// CPU側のMATERIAL_CONSTANTSに対応する。未使用フィールドも転送配置のため残す。
cbuffer MaterialConstants: register(b2)
{
  // Material基準色。現行のBasicColorでは参照せず、Object側のcolorを使用する。
  float4 baseColor;
  // Vertex ShaderでUVへ掛けるタイリング倍率。
  float2 uvTiling;
  // Pixel ShaderのTexture使用判定値。0.5未満ならTextureを参照しない。
  float useTexture;
  // CPU側との配置を維持する余白。描画計算には使用しない。
  float padding;
};

//========= Vertex Shader入力=========
// BasicMeshRendererとObjModelRendererのe_POSITION_TEXTUREに対応する頂点入力。
struct VSInput
{
  // Local空間の3成分頂点座標。
  float3 position: POSITION;
  // 頂点の2成分Texture座標。
  float2 uv: TEXCOORD0;
};

//========= Vertex Shader出力=========
// 変換後の座標と、ラスタライズ後にPixel Shaderへ渡す色・UVをまとめる。
struct VSOutput
{
  // Clip空間の同次座標。
  float4 position: SV_POSITION;
  // ObjectのRGBA色。
  float4 color: COLOR0;
  // タイリング倍率を反映したUV。
  float2 uv: TEXCOORD0;
};

#endif
