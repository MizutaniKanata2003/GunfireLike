//========= 定数バッファ=========
cbuffer HudBuffer : register(b0)
{
    float4 g_Rectangle;
    float4 g_Color;
};

//========= Vertex Shader入力=========
struct VertexInput
{
    float2 position : POSITION;
};

//========= Pixel Shader入力=========
struct PixelInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

//========= Vertex Shader=========
PixelInput main(VertexInput input)
{
    PixelInput output;

    // 0.0から1.0のQuad座標をHUDのピクセル座標へ変換する。
    const float2 pixelPosition = g_Rectangle.xy + input.position * g_Rectangle.zw;

    // 基準画面サイズを使ってピクセル座標を0.0から1.0へ正規化する。
    const float2 normalizedPosition = pixelPosition / float2(1280.0f, 720.0f);

    // 正規化座標をDirectXのクリップ座標へ変換する。
    output.position = float4(
        normalizedPosition.x * 2.0f - 1.0f,
        1.0f - normalizedPosition.y * 2.0f,
        0.0f,
        1.0f);

    output.color = g_Color;

    return output;
}