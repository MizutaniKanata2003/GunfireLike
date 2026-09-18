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

//========= Vertex Shader=========
VSOutput VSMain(VSInput input)
{
    VSOutput output;

    // 頂点座標へWorld、View、Projectionを反映してクリップ座標へ変換する。
    output.position = mul(float4(input.position, 1.0f), worldViewProjection);

    // Pixel Shaderで使用する色とUVタイリング後のUVを設定する。
    output.color = color;
    output.uv = input.uv * uvTiling;

    return output;
}