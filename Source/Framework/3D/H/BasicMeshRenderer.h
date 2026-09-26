#pragma once

//========= DirectX インクルード=========
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

//========= Framework インクルード=========
#include "Framework/DirectX/H/ConstantBuffer.h"

//========= 前方宣言=========
class GraphicsSystem;

// Cubeを使い、床、壁、単色オブジェクトを描画する基本3Dメッシュレンダラー。
class BasicMeshRenderer final
{
public:
	//========= 列挙型=========
	// Cube描画に使用するテクスチャの種類。
	enum class TextureType
	{
		// Floor.pngを貼り付ける。
		Floor,
		// Wall.pngを貼り付ける。
		Wall,
		// Object.pngを貼り付ける。
		Object,
		// テクスチャを使わず、指定した色だけで描画する。
		Color
	};

	//========= ライフサイクル・描画関数=========
	// Cubeメッシュ、Shader、Texture、Sampler、定数バッファを初期化する。
	bool Initialize( GraphicsSystem& graphicsSystem );

	// 指定した行列、色、UVタイリング、テクスチャ種別でCubeを描画する。
	void DrawCube(
	GraphicsSystem& graphicsSystem,
	const DirectX::XMMATRIX& worldMatrix,
	const DirectX::XMMATRIX& viewMatrix,
	const DirectX::XMMATRIX& projectionMatrix,
	const DirectX::XMFLOAT4& color,
	const DirectX::XMFLOAT2& uvTiling,
	TextureType textureType );

	// 描画に使用したDirect3Dリソースを解放する。
	void Uninit();

private:
	//========= 構造体=========
	// Cubeの頂点座標とUV座標。
	struct Vertex
	{
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT2 uv{};
	};

	// BasicColorVS.hlslとBasicColorPS.hlslの定数バッファ定義に合わせる。
	struct TransformBuffer
	{
		DirectX::XMMATRIX worldViewProjection{};
		DirectX::XMFLOAT4 color{};
		DirectX::XMFLOAT2 uvTiling{};
		float useTexture{};
		float padding{};
	};

	//========= Shader関連=========
	// Cube描画に使用するVertex Shader。
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VertexShader{};
	// Cube描画に使用するPixel Shader。
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PixelShader{};
	// Vertex構造とVertex Shader入力を対応付けるInput Layout。
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout{};

	//========= Texture関連=========
	// テクスチャ参照時のフィルタリングとアドレス指定を行うSampler。
	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_TextureSampler{};
	// 床描画に使用するテクスチャ。
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_FloorTextureView{};
	// 壁描画に使用するテクスチャ。
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_WallTextureView{};
	// 画像付きオブジェクト描画に使用するテクスチャ。
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_ObjectTextureView{};

	//========= Buffer関連=========
	// Cubeの頂点情報を保持するVertex Buffer。
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_VertexBuffer{};
	// Cubeの三角形順序を保持するIndex Buffer。
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndexBuffer{};
	// World、View、Projection、色、UV情報をShaderへ渡す定数バッファ。
	ConstantBuffer<TransformBuffer> m_TransformBuffer{};

	//========= 描画情報=========
	// DrawIndexedに渡すCubeのIndex数。
	unsigned int m_IndexCount{};
};