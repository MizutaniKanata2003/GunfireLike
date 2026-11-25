// スキニング付きモデル用のVertex Shader。頂点を、最大4本のボーンの行列で変形してから、BasicColorと同じ流れでClip空間へ変換する。
// Pixel Shaderは、BasicColorPSを共用するため、出力の形（VSOutput）は、BasicColorVSと同じにする。

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
  // 法線。現在は、照明を使わないため、参照しない。
  float3 normal: NORMAL;
  // 頂点の2成分Texture座標。
  float2 uv: TEXCOORD0;
  // 影響を与える4本のボーンの番号。
  uint4 boneIndices: BLENDINDICES;
  // 4本のボーンの重み。合計が1。全て0なら、ボーンの影響を受けない。
  float4 boneWeights: BLENDWEIGHT;
};

//========= Vertex Shaderエントリーポイント=========
// Local頂点を、ボーンで変形し、World、Clipの順に変換する。色とタイリング済みUVを返す。
// input: スキニング付きの頂点情報。
// 戻り値: ラスタライズに使う座標と、Pixel Shaderへ渡す色・UV。
VSOutput main(SkinnedVSInput input)
{
  VSOutput output;

  // 4本のボーンの変形を、重みで混ぜる。重みが全て0の頂点は、変形せず、そのまま使う。
  const float4 localPosition = float4(input.position,1.0f);
  const float totalWeight = input.boneWeights.x + input.boneWeights.y + input.boneWeights.z + input.boneWeights.w;
  float4 skinnedPosition = localPosition;
  if(totalWeight > 0.0f){
    skinnedPosition = mul(localPosition,boneMatrices[input.boneIndices.x]) * input.boneWeights.x
                    + mul(localPosition,boneMatrices[input.boneIndices.y]) * input.boneWeights.y
                    + mul(localPosition,boneMatrices[input.boneIndices.z]) * input.boneWeights.z
                    + mul(localPosition,boneMatrices[input.boneIndices.w]) * input.boneWeights.w;
  }

  // 行ベクトル形式の既存mul順を保ち、変形後の座標をWorld座標、Clip座標へ変換する。
  const float4 worldPosition = mul(skinnedPosition,worldMatrix);
  output.position = mul(worldPosition,viewProjectionMatrix);

  // Object色はそのまま渡し、UVだけMaterialのタイリング倍率を反映する。
  output.color = color;
  output.uv = input.uv * uvTiling;
  return output;
}
