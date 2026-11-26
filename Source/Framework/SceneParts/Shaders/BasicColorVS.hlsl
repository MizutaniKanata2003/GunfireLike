// BasicColor描画用のVertex Shader。頂点をClip空間へ変換し、Object色とUVを出力する。
// 行列の乗算順と、入力・出力semanticは既存のまま維持する。

//========= 共通Shaderインクルード=========
#include "ShaderCommon.hlsl"

//========= Vertex Shaderエントリーポイント=========
// Local頂点をWorld、Clipの順に変換し、色とタイリング済みUVを返す。
// input: POSITIONとTEXCOORD0から受け取る頂点情報。
// 戻り値: ラスタライズに使う座標と、Pixel Shaderへ渡す色・UV。
VSOutput main(VSInput input)
{
  // 後続処理で全フィールドを設定する出力を用意する。初期化方式は変更しない。
  VSOutput output;

  // 行ベクトル形式の既存mul順を保ち、Local座標をWorld座標、Clip座標へ変換する。
  const float4 worldPosition = mul(float4(input.position,1.0f),worldMatrix);
  output.position = mul(worldPosition,viewProjectionMatrix);

  // Object色はそのまま渡し、UVだけMaterialのタイリング倍率を反映する。
  output.color = color;
  output.uv = input.uv * uvTiling;
  return output;
}
