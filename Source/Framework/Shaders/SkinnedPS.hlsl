// スキニング付きモデル用のPixel Shader。拡散色の画像（なければObject色）に、平行光源とアンビエントの光を掛けて、不透明な色を返す。
// 光は、Half Lambertで、影の側も真っ黒にならないようにする。画像のAlphaは使わない。法線の画像・鏡面・影は、まだ扱わない。

//========= 共通Shaderインクルード=========
#include "ShaderCommon.hlsl"

//========= Light定数バッファ=========
// CPU側のLIGHT_CONSTANTSに対応する。b3は、ShaderCommon.hlslの、Light用の割当。
cbuffer LightConstants: register(b3)
{
  // 光の進む向き。正規化済み。
  float3 lightDirection;
  // 平行光源の強さ。
  float lightIntensity;
  // 平行光源のRGB色。
  float3 lightColor;
  // 影の側にも加える、環境光の強さ。
  float ambientIntensity;
  //========= 切り抜きの設定=========
  // 画像のAlphaが、この値より低い画素は、描かずに捨てる。服の破れなどの、透明な部分を、描かないための値。見た目を確認して、調整する仮値。
  static const float alphaCutoff = 0.5f;
};

//========= Texture・Sampler=========
// Rendererがt0へ設定する、拡散色の2D Texture。
Texture2D diffuseTexture: register(t0);
// Rendererがs0へ設定する、Texture参照用のSampler。
SamplerState textureSampler: register(s0);

//========= Pixel Shader入力=========
// SkinnedVSの出力と対応する。
struct SkinnedPSInput
{
  // ラスタライズ後の画面座標。このShaderの色計算では参照しない。
  float4 position: SV_POSITION;
  // ObjectのRGBA色。
  float4 color: COLOR0;
  // タイリング倍率が反映されたUV。
  float2 uv: TEXCOORD0;
  // ワールド空間の法線。
  float3 worldNormal: TEXCOORD1;
};

//========= Pixel Shaderエントリーポイント=========
// 拡散色に、光を掛けた色を返す。
// input: ラスタライズにより補間された色・UV・法線。
// 戻り値: Render Targetへ出力する、不透明なRGBA色。
float4 main(SkinnedPSInput input): SV_TARGET
{
  // 画像を使う場合は、画像のRGBにObject色を掛ける。Alphaが低い画素は、描かずに捨て、穴の奥（肌など）を見せる。
  float3 albedo = input.color.rgb;
  if(useTexture >= 0.5f)
  {
    const float4 textureColor = diffuseTexture.Sample(textureSampler,input.uv);
    clip(textureColor.a - alphaCutoff);
    albedo *= textureColor.rgb;
  }

  // Half Lambert: 光に背を向けた面も、0.25まで下げるだけにして、2乗でなだらかにする。
  const float3 normal = normalize(input.worldNormal);
  const float halfLambert = dot(normal,-normalize(lightDirection)) * 0.5f + 0.5f;
  const float diffuse = halfLambert * halfLambert;

  // 環境光と、平行光源の光を足し、拡散色に掛ける。
  const float3 lighting = ambientIntensity + lightColor * lightIntensity * diffuse;
  return float4(saturate(albedo * lighting),1.0f);
}
