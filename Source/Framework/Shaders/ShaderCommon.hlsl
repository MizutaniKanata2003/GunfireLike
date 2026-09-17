#ifndef SHADER_COMMON_HLSLI
#define SHADER_COMMON_HLSLI

cbuffer TransformBuffer : register(b0)
{
    matrix worldViewProjection;
    float4 color;
};

struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

#endif