// スキニング付きモデル用のVertex Shader。頂点と法線を、最大4本のボーンの行列で変形してから、Clip空間へ変換する。
// 出力は、Pixel Shader（SkinnedPS）が、光の計算に使う、ワールド空間の法線を含む。

//========= 共通Shaderインクルード=========
#include "ShaderCommon.hlsl"

//========= ボーン定数バッファ=========
// CPU側のBONE_CONSTANTSに対応する。行列は、転送前にRenderer側が転置する。b3・b4は、LightとPostProcessの割当のため、b5を使う。
cbuffer BoneConstants: register(b5)
{
  // ボーンごとの最終の行列。添字は、頂点のボーン番号。使わない分は単位行列。
  matrix boneMatrices[128];
};

//========= Vertex Shader入力=========
// SKINNED_VERTEXに対応する、スキニング付きの頂点入力。
struct SkinnedVSInput
{
  // Local空間の3成分頂点座標。
  float3 position: POSITION;
  // Local空間の法線。単位ベクトル。
  float3 normal: NORMAL;
  // 頂点の2成分Texture座標。
  float2 uv: TEXCOORD0;
  // 影響を与える4本のボーンの番号。
  uint4 boneIndices: BLENDINDICES;
  // 4本のボーンの重み。合計が1。全て0なら、ボーンの影響を受けない。
  float4 boneWeights: BLENDWEIGHT;
};

//========= Vertex Shader出力=========
// Pixel Shaderへ渡す、座標・色・UV・ワールド空間の法線。
struct SkinnedVSOutput
{
  // Clip空間の同次座標。
  float4 position: SV_POSITION;
  // ObjectのRGBA色。
  float4 color: COLOR0;
  // タイリング倍率を反映したUV。
  float2 uv: TEXCOORD0;
  // ワールド空間の法線。補間されるため、Pixel Shaderで、正規化し直す。
  float3 worldNormal: TEXCOORD1;
};

//========= Vertex Shaderエントリーポイント=========
// Local頂点と法線を、ボーンで変形し、World、Clipの順に変換する。色とタイリング済みUVを返す。
// input: スキニング付きの頂点情報。
// 戻り値: ラスタライズに使う座標と、Pixel Shaderへ渡す色・UV・法線。
SkinnedVSOutput main(SkinnedVSInput input)
{
  SkinnedVSOutput output;

  // 4本のボーンの変形を、重みで混ぜる。重みが全て0の頂点は、変形せず、そのまま使う。
  const float4 localPosition = float4(input.position,1.0f);
  const float4 localNormal = float4(input.normal,0.0f);
  const float totalWeight = input.boneWeights.x + input.boneWeights.y + input.boneWeights.z + input.boneWeights.w;
  float4 skinnedPosition = localPosition;
  float4 skinnedNormal = localNormal;
  if(totalWeight > 0.0f)
  {
    skinnedPosition = float4(0.0f,0.0f,0.0f,0.0f);
    skinnedNormal = float4(0.0f,0.0f,0.0f,0.0f);
    [unroll]
    for(int slot = 0; slot < 4; ++slot)
    {
      const matrix boneMatrix = boneMatrices[input.boneIndices[slot]];
      skinnedPosition += mul(localPosition,boneMatrix) * input.boneWeights[slot];
      skinnedNormal += mul(localNormal,boneMatrix) * input.boneWeights[slot];
    }
  }

  // 行ベクトル形式の既存mul順を保ち、変形後の座標をWorld座標、Clip座標へ変換する。法線は、移動を含めず、Worldの回転だけを掛ける（拡大は、全軸同じ前提）。
  const float4 worldPosition = mul(skinnedPosition,worldMatrix);
  output.position = mul(worldPosition,viewProjectionMatrix);
  output.worldNormal = normalize(mul(float4(skinnedNormal.xyz,0.0f),worldMatrix).xyz);

  // Object色はそのまま渡し、UVだけMaterialのタイリング倍率を反映する。
  output.color = color;
  output.uv = input.uv * uvTiling;
  return output;
}
