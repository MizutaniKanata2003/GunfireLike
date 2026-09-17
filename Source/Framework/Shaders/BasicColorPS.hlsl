Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);

cbuffer TransformBuffer : register(b0)
{
    matrix worldViewProjection;
    float4 color;

    float2 uvTiling;
    float useTexture;
    float padding;
};

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float4 main(
    PixelShaderInput input) : SV_TARGET
{
    // Color指定のCubeや、テクスチャ無しの敵OBJ。
    if (useTexture < 0.5f)
    {
        return input.color;
    }

    const float4 textureColor =
        diffuseTexture.Sample(
            textureSampler,
            input.uv);

    return textureColor *
        input.color;
}