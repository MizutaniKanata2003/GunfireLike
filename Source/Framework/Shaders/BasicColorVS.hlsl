cbuffer TransformBuffer : register(b0)
{
    matrix worldViewProjection;
    float4 color;

    float2 uvTiling;
    float useTexture;
    float padding;
};

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(
    VSInput input)
{
    VSOutput output;

    output.position =
        mul(
            float4(
                input.position,
                1.0f),
            worldViewProjection);

    output.color = color;

    output.uv =
        input.uv *
        uvTiling;

    return output;
}