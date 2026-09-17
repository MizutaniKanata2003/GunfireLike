#pragma once

//========= C++標準ライブラリ インクルード=========
#include <string>
#include <vector>

//========= DirectX インクルード=========
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

//========= 前方宣言=========
class GraphicsSystem;

// OBJモデルを読み込み、テクスチャまたは単色で描画する3Dモデルレンダラー。
class ObjModelRenderer final
{
public:
	//========= 初期化・終了関数=========
	// OBJ、Shader、Buffer、必要なTexture、Samplerを初期化する。
	bool Initialize(
		GraphicsSystem& graphicsSystem,
		const std::wstring& objFilePath,
		const std::wstring& textureFilePath );
	// OBJ描画で使用したDirect3Dリソースを解放する。
	void Uninit();

	//========= 描画関数=========
	// 指定したWorld、View、Projection行列と色でOBJモデルを描画する。
	void Draw(
		GraphicsSystem& graphicsSystem,
		const DirectX::XMMATRIX& worldMatrix,
		const DirectX::XMMATRIX& viewMatrix,
		const DirectX::XMMATRIX& projectionMatrix,
		const DirectX::XMFLOAT4& color = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f } );

private:
	//========= 構造体=========
	// OBJモデルの頂点座標とUV座標。
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

	//========= 補助関数=========
	// OBJファイルを読み込み、頂点配列とIndex配列を生成する。
	bool LoadObjFile(
		const std::wstring& objFilePath,
		std::vector<Vertex>& vertices,
		std::vector<unsigned int>& indices );
	// テクスチャファイルを読み込み、Shader Resource Viewを生成する。
	bool LoadTextureFromFile( ID3D11Device* device, const std::wstring& textureFilePath );

	//========= Shader関連=========
	// OBJ描画に使用するVertex Shader。
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VertexShader{};
	// OBJ描画に使用するPixel Shader。
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PixelShader{};
	// OBJ頂点形式とVertex Shader入力を対応付けるInput Layout。
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout{};

	//========= Buffer関連=========
	// OBJ頂点情報を保持するVertex Buffer。
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_VertexBuffer{};
	// OBJ三角形のIndex情報を保持するIndex Buffer。
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndexBuffer{};
	// World、View、Projection、色、Texture使用有無をShaderへ渡す定数バッファ。
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_TransformBuffer{};

	//========= Texture関連=========
	// OBJに貼り付けるテクスチャ。
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_TextureView{};
	// テクスチャの拡大縮小とアドレス指定を行うSampler。
	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_TextureSampler{};

	//========= 描画情報=========
	// DrawIndexedに渡すIndex数。
	unsigned int m_IndexCount{};
};