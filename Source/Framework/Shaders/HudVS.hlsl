// 単色HUD QuadのVertex Shader。Render Target上の矩形情報からClip座標を生成する。
// Virtual CanvasからRender Target座標への変換はCPU側のHudRendererが担当する。

//========= 定数バッファ=========
// HudRenderer::HUD_BUFFERと対応する48バイトの配置を維持する。Vertex Shaderのb0から参照する。
cbuffer HudBuffer: register(b0)
{
  // Render Target上の左上X・左上Y・幅・高さ。単位はpixel。
  float4 g_Rectangle;
  // Quad全体に適用するRGBA色。Pixel Shaderへ頂点出力として渡す。
  float4 g_Color;
  // 描画先の幅と高さ。単位はpixel。CPU側で両成分が0でないことを確認してから描画する。
  float2 g_RenderSize;
  // CPU側との転送配置を維持する余白。座標計算には使用しない。
  float2 g_Padding;
};

//========= Vertex Shader入力=========
// HudRendererのe_POSITIONに対応する2成分入力。3DのPOSITION入力とは配置が異なる。
struct VertexInput
{
  // 0〜1で表すQuad内の頂点座標。左上が(0,0)、右下が(1,1)。
  float2 position: POSITION;
};

//========= Vertex Shader出力・Pixel Shader入力=========
// HudPSのPixelInputとsemanticを一致させ、位置と色を受け渡す。
struct PixelInput
{
  // Clip空間の同次座標。出力時はZを0、Wを1に固定する。
  float4 position: SV_POSITION;
  // QuadのRGBA色。ラスタライズ後にPixel Shaderへ渡される。
  float4 color: COLOR0;
};

//========= Vertex Shaderエントリーポイント=========
// 単位Quadを矩形のpixel座標へ拡縮・移動し、Clip座標と色を返す。
// input: Quad内の2成分頂点座標。
// 戻り値: ラスタライズ用の座標と、Pixel Shaderへ渡すRGBA色。
PixelInput main(VertexInput input)
{
  // 後続処理で全フィールドを設定する出力を用意する。初期化方式は変更しない。
  PixelInput output;

  // 矩形の幅・高さを掛けて左上位置を加え、単位QuadをRender Target上のpixel座標へ変換する。
  const float2 pixelPosition = g_Rectangle.xy + input.position * g_Rectangle.zw;

  // 描画先サイズで割り、画面左上を(0,0)、右下を(1,1)とする座標へ正規化する。
  const float2 normalizedPosition = pixelPosition / g_RenderSize;

  // Xを-1〜1へ変換し、下向きYを反転してClip座標の上向きYへ合わせる。
  output.position = float4(normalizedPosition.x * 2.0f - 1.0f,1.0f - normalizedPosition.y * 2.0f,0.0f,1.0f);

  // 色とAlphaは加工せず渡す。後段のBlend設定は呼出側で行う。
  output.color = g_Color;
  return output;
}
