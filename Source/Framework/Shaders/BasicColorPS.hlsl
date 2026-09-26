#include "ShaderCommon.hlsl"

Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
    if (useTexture < 0.5f)
        return input.color;

    const float4 textureColor = diffuseTexture.Sample(textureSampler, input.uv);

    return textureColor * input.color;
}