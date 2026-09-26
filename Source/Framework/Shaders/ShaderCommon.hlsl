#ifndef SHADER_COMMON_HLSLI
#define SHADER_COMMON_HLSLI

//========= Constant Buffer Slot規約=========
// b0: Camera Constants。
// b1: Object Constants。
// b2: Material Constants。
// b3: Light Constants。
// b4: Post Process Constants。

cbuffer CameraConstants : register(b0)
{
    matrix viewProjectionMatrix;
};

cbuffer ObjectConstants : register(b1)
{
    matrix worldMatrix;
    float4 color;
};

cbuffer MaterialConstants : register(b2)
{
    float4 baseColor;
    float2 uvTiling;
    float useTexture;
    float padding;
};

//========= Vertex Shader入力=========
// BasicMeshRendererとObjModelRendererのe_POSITION_TEXTUREに対応する。
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

#endif