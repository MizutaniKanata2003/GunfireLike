// スキニング付きモデル用のPixel Shader。拡散色の画像（なければObject色）に、平行光源とアンビエントの光を掛けて、不透明な色を返す。
// 光は、Half Lambertで、影の側も真っ黒にならないようにする。画像のAlphaが低い画素は、描かずに捨てる（切り抜き）。
// Object色のAlphaが1未満の時は、Alphaが低いほど、画面上の画素を、市松状（4x4のBayer配列）に、多く捨てる（ディザ透明）。徐々に消える死体に使う。
// 法線の画像・鏡面・影は、まだ扱わない。

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

//========= ディザ透明の設定=========
// 4x4のBayer配列。値は、0〜15の順位で、(順位 + 0.5) / 16 を閾値にする。Alphaが閾値以下の画素を捨てる。Alphaが0.5の時は、市松状に半分の画素が残る。
static const float ditherMatrix[16] =
{
   0.0f,  8.0f,  2.0f, 10.0f,
  12.0f,  4.0f, 14.0f,  6.0f,
   3.0f, 11.0f,  1.0f,  9.0f,
  15.0f,  7.0f, 13.0f,  5.0f
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
  // ラスタライズ後の画面座標。ディザ透明の、画素の位置に使う。
  float4 position: SV_POSITION;
  // ObjectのRGBA色。Alphaは、ディザ透明の濃さに使う。
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
  // ディザ透明: 画面上の画素の位置（4x4の周期）から閾値を引き、Object色のAlphaが閾値以下なら、描かずに捨てる。Alphaが1なら、全ての画素が残る。
  const uint2 pixelPosition = uint2(input.position.xy) % 4;
  const float ditherThreshold = (ditherMatrix[pixelPosition.y * 4 + pixelPosition.x] + 0.5f) / 16.0f;
  clip(input.color.a - ditherThreshold);

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
