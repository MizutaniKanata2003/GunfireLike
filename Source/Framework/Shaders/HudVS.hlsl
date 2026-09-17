cbuffer HudBuffer : register( b0 )
{
    float4 g_Rectangle;
    float4 g_Color;
};

struct VertexInput
{
    float2 position : POSITION;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

PixelInput main( VertexInput input )
{
    PixelInput output;

    float2 pixelPosition =
        g_Rectangle.xy +
        input.position *
        g_Rectangle.zw;

    float2 normalizedPosition =
        pixelPosition /
        float2( 1280.0f, 720.0f );

    output.position = float4(
        normalizedPosition.x * 2.0f - 1.0f,
        1.0f - normalizedPosition.y * 2.0f,
        0.0f,
        1.0f );

    output.color = g_Color;

    return output;
}
