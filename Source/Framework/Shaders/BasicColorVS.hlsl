#include "ShaderCommon.hlsl"

VSOutput main(VSInput input)
{
    VSOutput output;

    const float4 worldPosition = mul(float4(input.position, 1.0f), worldMatrix);
    output.position = mul(worldPosition, viewProjectionMatrix);

    output.color = color;
    output.uv = input.uv * uvTiling;

    return output;
}