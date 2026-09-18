//========= Texture・Sampler=========
Texture2D diffuseTexture : register(t0);
SamplerState textureSampler : register(s0);

//========= 定数バッファ=========
cbuffer TransformBuffer : register(b0)
{
    matrix worldViewProjection;
    float4 color;
    float2 uvTiling;
    float useTexture;
    float padding;
};

//========= Pixel Shader入力=========
struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

//========= Pixel Shader=========
float4 main(PixelShaderInput input) : SV_TARGET
{
    // 単色指定のCubeやテクスチャを持たないOBJは、頂点色だけで描画する。
    if (useTexture < 0.5f)
        return input.color;

    // テクスチャ色と頂点色を乗算して最終色を作る。
    const float4 textureColor = diffuseTexture.Sample(textureSampler, input.uv);

    return textureColor * input.color;
}