// 単色HUD QuadのPixel Shader。Vertex Shaderから受け取ったRGBA色をそのまま出力する。
// Texture参照や定数バッファの参照は行わず、色の合成は描画側のBlend設定へ任せる。

//========= Pixel Shader入力=========
// HudVSの出力とsemanticを一致させ、ラスタライズ後の位置と色を受け取る。
struct PixelInput
{
  // ラスタライズ後の画面座標。このShaderでは色計算に使用しない。
  float4 position: SV_POSITION;
  // QuadのRGBA色。Alphaも変更せずに出力する。
  float4 color: COLOR0;
};

//========= Pixel Shaderエントリーポイント=========
// 入力色をそのままRender Targetへの出力値として返す。
// input: Vertex Shaderから渡された色と、ラスタライズ後の座標。
// 戻り値: 入力と同じRGBA色。
float4 main(PixelInput input): SV_TARGET
{
  // 不透明度を含めて入力色を維持し、追加の色補正やAlpha加工を行わない。
  return input.color;
}
