// BasicColor描画用のPixel Shader。使用フラグに応じ、単色またはTextureとObject色の積を出力する。
// Texture・Samplerのregisterと、Alphaを含む色の計算を既存のまま維持する。

//========= 共通Shaderインクルード=========
#include "ShaderCommon.hlsl"

//========= Texture・Sampler=========
// Rendererがt0へ設定する2D Texture。
Texture2D diffuseTexture: register(t0);
// Rendererがs0へ設定するTexture参照用Sampler。
SamplerState textureSampler: register(s0);

//========= Pixel Shader入力=========
// Vertex Shader出力と対応するsemanticで、ラスタライズ後の値を受け取る。
struct PixelShaderInput
{
  // ラスタライズ後の画面座標。このShaderの色計算では参照しない。
  float4 position: SV_POSITION;
  // 頂点から渡されたObjectのRGBA色。
  float4 color: COLOR0;
  // タイリング倍率が反映されたUV。
  float2 uv: TEXCOORD0;
};

//========= Pixel Shaderエントリーポイント=========
// Texture未使用ならObject色、使用時はTexture色との成分ごとの積を返す。
// input: ラスタライズにより補間されたObject色とUV、および画面座標。
// 戻り値: Render Targetへ出力するRGBA色。後段のBlendはGraphicsSystem側の設定に従う。
float4 main(PixelShaderInput input): SV_TARGET
{
  // 単色指定時はTextureを参照せず、Object色をそのまま使用する。
  if(useTexture < 0.5f)
    return input.color;

  // 指定UVで参照したTexture色へObject色を掛ける。Alphaも同じ式で乗算する。
  const float4 textureColor = diffuseTexture.Sample(textureSampler,input.uv);
  return textureColor * input.color;
}
