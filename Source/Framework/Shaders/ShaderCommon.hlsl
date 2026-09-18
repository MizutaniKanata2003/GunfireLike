#ifndef SHADER_COMMON_HLSLI
#define SHADER_COMMON_HLSLI

//========= 定数バッファ=========
cbuffer TransformBuffer : register(b0)
{
    matrix worldViewProjection;
    float4 color;
    float2 uvTiling;
    float useTexture;
    float padding;
};

//========= Vertex Shader入力=========
struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

//========= Vertex Shader出力=========
struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

#endif