#include "../H/BasicMeshRenderer.h"

//========= C++標準ライブラリ インクルード=========
#include <string>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/DirectX/H/TextureLoader.h"
#include "Framework/Etc/H/Logger.h"

namespace
{
	//========= Cubeメッシュ定数=========
	// Cubeの頂点数とIndex数。
	constexpr unsigned int CUBE_VERTEX_COUNT = 24;
	constexpr unsigned int CUBE_INDEX_COUNT = 36;

	// Cubeの各軸における最小座標と最大座標。
	constexpr float CUBE_MIN_COORDINATE = -0.5f;
	constexpr float CUBE_MAX_COORDINATE = 0.5f;

	//========= Shaderファイルパス定数=========
	// BasicColor描画に使用するShaderのCSOファイルパス。
	constexpr char BASIC_COLOR_VERTEX_SHADER_CSO_PATH[] = "Shaders/BasicColorVS.cso";
	constexpr char BASIC_COLOR_PIXEL_SHADER_CSO_PATH[] = "Shaders/BasicColorPS.cso";

	//========= Textureファイルパス定数=========
	// Cube描画に使用するテクスチャファイルパス。
	constexpr wchar_t FLOOR_TEXTURE_PATH[] = L"Assets/Textures/Floor.png";
	constexpr wchar_t WALL_TEXTURE_PATH[] = L"Assets/Textures/Wall.png";
	constexpr wchar_t OBJECT_TEXTURE_PATH[] = L"Assets/Textures/Object.png";

	//========= Input Layout定数=========
	// Vertex Shader入力に使用する頂点属性のセマンティック名。
	constexpr char POSITION_SEMANTIC_NAME[] = "POSITION";
	constexpr char TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";

	//========= 補助関数=========
	// GraphicsカテゴリでBasicMesh描画Resource生成失敗を出力する。
	void WriteBasicMeshGraphicsError( const wchar_t* functionName, HRESULT result )
	{
		std::wstring message{ functionName };
		message += L" に失敗しました。HRESULT: 0x";

		constexpr wchar_t hexDigits[]{ L"0123456789ABCDEF" };
		const unsigned long resultValue = static_cast<unsigned long>( result );

		for ( int digitIndex = 7; digitIndex >= 0; --digitIndex )
		{
			const unsigned long digit = ( resultValue >> digitIndex * 4 ) & 0x0f;
			message += hexDigits[ digit ];
		}

		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_GRAPHICS, message );
	}
}

// Cubeメッシュ、Shader、Texture、Sampler、定数バッファを初期化する。
bool BasicMeshRenderer::Initialize( GraphicsSystem& graphicsSystem )
{
	Uninit();

	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	if ( device == nullptr ) return false;

	// Cubeの各面で独立したUVを持つ頂点配列を作成する。
	const Vertex vertices[ CUBE_VERTEX_COUNT ]
	{
		// 前面: Z = -0.5
		{ { CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE }, { 0.0f, 1.0f } },
		{ { CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE }, { 0.0f, 0.0f } },
		{ { CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE }, { 1.0f, 0.0f } },
		{ { CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE }, { 1.0f, 1.0f } },

		// 背面: Z = +0.5
		{ { CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE }, { 0.0f, 1.0f } },
		{ { CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE }, { 0.0f, 0.0f } },
		{ { CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE }, { 1.0f, 0.0f } },
		{ { CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE }, { 1.0f, 1.0f } },

		// 左面: X = -0.5
		{ { CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE }, { 0.0f, 1.0f } },
		{ { CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE }, { 0.0f, 0.0f } },
		{ { CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE }, { 1.0f, 0.0f } },
		{ { CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE }, { 1.0f, 1.0f } },

		// 右面: X = +0.5
		{ { CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE }, { 0.0f, 1.0f } },
		{ { CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE }, { 0.0f, 0.0f } },
		{ { CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE }, { 1.0f, 0.0f } },
		{ { CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE }, { 1.0f, 1.0f } },

		// 上面: Y = +0.5
		{ { CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE }, { 0.0f, 1.0f } },
		{ { CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE }, { 0.0f, 0.0f } },
		{ { CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE }, { 1.0f, 0.0f } },
		{ { CUBE_MAX_COORDINATE, CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE }, { 1.0f, 1.0f } },

		// 下面: Y = -0.5
		{ { CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE }, { 0.0f, 1.0f } },
		{ { CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE }, { 0.0f, 0.0f } },
		{ { CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MIN_COORDINATE }, { 1.0f, 0.0f } },
		{ { CUBE_MAX_COORDINATE, CUBE_MIN_COORDINATE, CUBE_MAX_COORDINATE }, { 1.0f, 1.0f } }
	};

	// Cubeを構成する12枚の三角形のIndex配列を作成する。
	const unsigned short indices[ CUBE_INDEX_COUNT ]
	{
		// 前面
		0, 1, 2, 0, 2, 3,
		// 背面
		4, 5, 6, 4, 6, 7,
		// 左面
		8, 9, 10, 8, 10, 11,
		// 右面
		12, 13, 14, 12, 14, 15,
		// 上面
		16, 17, 18, 16, 18, 19,
		// 下面
		20, 21, 22, 20, 22, 23
	};

	// CubeのVertex Bufferを生成する。
	D3D11_BUFFER_DESC vertexBufferDesc{};
	vertexBufferDesc.ByteWidth = static_cast<UINT>( sizeof( vertices ) );
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexData{};
	vertexData.pSysMem = vertices;

	const HRESULT vertexBufferResult = device->CreateBuffer( &vertexBufferDesc, &vertexData, m_VertexBuffer.GetAddressOf() );

	if ( FAILED( vertexBufferResult ) )
	{
		WriteBasicMeshGraphicsError( L"ID3D11Device::CreateBuffer(VertexBuffer)", vertexBufferResult );
		Uninit();
		return false;
	}

	// CubeのIndex Bufferを生成する。
	D3D11_BUFFER_DESC indexBufferDesc{};
	indexBufferDesc.ByteWidth = static_cast<UINT>( sizeof( indices ) );
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices;

	const HRESULT indexBufferResult = device->CreateBuffer( &indexBufferDesc, &indexData, m_IndexBuffer.GetAddressOf() );

	if ( FAILED( indexBufferResult ) )
	{
		WriteBasicMeshGraphicsError( L"ID3D11Device::CreateBuffer(IndexBuffer)", indexBufferResult );
		Uninit();
		return false;
	}

	// Vertex ShaderのCSO読込、Shader生成、Input Layout生成を行う。
	std::vector<char> vertexShaderBinary{};

	if ( !ShaderBinaryLoader::Load( BASIC_COLOR_VERTEX_SHADER_CSO_PATH, vertexShaderBinary ) )
	{
		Uninit();
		return false;
	}

	const HRESULT vertexShaderResult = device->CreateVertexShader(
	vertexShaderBinary.data(),
	vertexShaderBinary.size(),
	nullptr,
	m_VertexShader.GetAddressOf() );

	if ( FAILED( vertexShaderResult ) )
	{
		WriteBasicMeshGraphicsError( L"ID3D11Device::CreateVertexShader", vertexShaderResult );
		Uninit();
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC inputElements[]
	{
	{
	POSITION_SEMANTIC_NAME,
	0,
	DXGI_FORMAT_R32G32B32_FLOAT,
	0,
	offsetof( Vertex, position ),
	D3D11_INPUT_PER_VERTEX_DATA,
	0
	},
	{
	TEXCOORD_SEMANTIC_NAME,
	0,
	DXGI_FORMAT_R32G32_FLOAT,
	0,
	offsetof( Vertex, uv ),
	D3D11_INPUT_PER_VERTEX_DATA,
	0
	}
	};

	const HRESULT inputLayoutResult = device->CreateInputLayout(
	inputElements,
	ARRAYSIZE( inputElements ),
	vertexShaderBinary.data(),
	vertexShaderBinary.size(),
	m_InputLayout.GetAddressOf() );

	if ( FAILED( inputLayoutResult ) )
	{
		WriteBasicMeshGraphicsError( L"ID3D11Device::CreateInputLayout", inputLayoutResult );
		Uninit();
		return false;
	}

	// Pixel Shaderを読み込み、Cube描画用のPixel Shaderを生成する。
	std::vector<char> pixelShaderBinary{};

	if ( !ShaderBinaryLoader::Load( BASIC_COLOR_PIXEL_SHADER_CSO_PATH, pixelShaderBinary ) )
	{
		Uninit();
		return false;
	}

	const HRESULT pixelShaderResult = device->CreatePixelShader(
	pixelShaderBinary.data(),
	pixelShaderBinary.size(),
	nullptr,
	m_PixelShader.GetAddressOf() );

	if ( FAILED( pixelShaderResult ) )
	{
		WriteBasicMeshGraphicsError( L"ID3D11Device::CreatePixelShader", pixelShaderResult );
		Uninit();
		return false;
	}

	// Camera、Object、Material用の定数バッファを生成する。
	HRESULT cameraBufferResult{};

	if ( !m_CameraBuffer.Init( device, cameraBufferResult ) )
	{
		WriteBasicMeshGraphicsError( L"ID3D11Device::CreateBuffer(CameraConstants)", cameraBufferResult );
		Uninit();
		return false;
	}

	HRESULT objectBufferResult{};

	if ( !m_ObjectBuffer.Init( device, objectBufferResult ) )
	{
		WriteBasicMeshGraphicsError( L"ID3D11Device::CreateBuffer(ObjectConstants)", objectBufferResult );
		Uninit();
		return false;
	}

	HRESULT materialBufferResult{};

	if ( !m_MaterialBuffer.Init( device, materialBufferResult ) )
	{
		WriteBasicMeshGraphicsError( L"ID3D11Device::CreateBuffer(MaterialConstants)", materialBufferResult );
		Uninit();
		return false;
	}

	// 床、壁、画像付きオブジェクトに使用するテクスチャを読み込む。
	if ( !TextureLoader::LoadWicTexture( device, FLOOR_TEXTURE_PATH, m_FloorTextureView ) ||
	 !TextureLoader::LoadWicTexture( device, WALL_TEXTURE_PATH, m_WallTextureView ) ||
	 !TextureLoader::LoadWicTexture( device, OBJECT_TEXTURE_PATH, m_ObjectTextureView ) )
	{
		Uninit();
		return false;
	}

	// Texture参照時のフィルタリングとアドレス指定を行うSamplerを生成する。
	D3D11_SAMPLER_DESC samplerDescription{};
	samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDescription.MinLOD = 0.0f;
	samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;

	const HRESULT samplerStateResult = device->CreateSamplerState( &samplerDescription, m_TextureSampler.GetAddressOf() );

	if ( FAILED( samplerStateResult ) )
	{
		WriteBasicMeshGraphicsError( L"ID3D11Device::CreateSamplerState", samplerStateResult );
		Uninit();
		return false;
	}

	m_IndexCount = CUBE_INDEX_COUNT;

	return true;
}

// 指定した行列、色、UVタイリング、テクスチャ種別でCubeを描画する。
void BasicMeshRenderer::DrawCube(
GraphicsSystem& graphicsSystem,
const DirectX::XMMATRIX& worldMatrix,
const DirectX::XMMATRIX& viewMatrix,
const DirectX::XMMATRIX& projectionMatrix,
const DirectX::XMFLOAT4& color,
const DirectX::XMFLOAT2& uvTiling,
TextureType textureType )
{
	if ( !m_VertexBuffer || !m_IndexBuffer || !m_CameraBuffer.IsValid() ||
	!m_ObjectBuffer.IsValid() || !m_MaterialBuffer.IsValid() || !m_VertexShader ||
	!m_PixelShader || !m_InputLayout || !m_TextureSampler || m_IndexCount == 0 ) return;

	// 描画に使用するDirect3D Contextを取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	if ( context == nullptr ) return;

	// テクスチャ種別に応じてShaderへ渡すTextureと使用フラグを決定する。
	ID3D11ShaderResourceView* textureView{};
	float useTexture{};

	switch ( textureType )
	{
		case TextureType::Floor:
		textureView = m_FloorTextureView.Get();
		useTexture = 1.0f;
		break;

		case TextureType::Wall:
		textureView = m_WallTextureView.Get();
		useTexture = 1.0f;
		break;

		case TextureType::Object:
		textureView = m_ObjectTextureView.Get();
		useTexture = 1.0f;
		break;

		case TextureType::Color:
		default:
		break;
	}

	// Shaderへ渡すCamera、Object、Material定数を作成する。
	const DirectX::XMMATRIX viewProjectionMatrix = DirectX::XMMatrixTranspose( viewMatrix * projectionMatrix );
	const DirectX::XMMATRIX transposedWorldMatrix = DirectX::XMMatrixTranspose( worldMatrix );

	CameraConstants cameraConstants{};
	DirectX::XMStoreFloat4x4( &cameraConstants.viewProjectionMatrix, viewProjectionMatrix );

	ObjectConstants objectConstants{};
	DirectX::XMStoreFloat4x4( &objectConstants.worldMatrix, transposedWorldMatrix );
	objectConstants.color = color;

	MaterialConstants materialConstants{};
	materialConstants.uvTiling = uvTiling;
	materialConstants.useTexture = useTexture;

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	const UINT vertexStride = sizeof( Vertex );
	const UINT vertexOffset{};
	ID3D11Buffer* vertexBuffers[]{ m_VertexBuffer.Get() };

	// Shaderへ設定する定数Buffer、Texture、Samplerをまとめる。
	ID3D11Buffer* cameraBuffers[]{ m_CameraBuffer.Get() };
	ID3D11Buffer* objectBuffers[]{ m_ObjectBuffer.Get() };
	ID3D11Buffer* materialBuffers[]{ m_MaterialBuffer.Get() };
	ID3D11SamplerState* samplers[]{ m_TextureSampler.Get() };

	m_CameraBuffer.Update( context, cameraConstants );
	m_ObjectBuffer.Update( context, objectConstants );
	m_MaterialBuffer.Update( context, materialConstants );

	context->IASetVertexBuffers( 0, 1, vertexBuffers, &vertexStride, &vertexOffset );
	context->IASetIndexBuffer( m_IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0 );
	context->IASetInputLayout( m_InputLayout.Get() );
	context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	context->VSSetShader( m_VertexShader.Get(), nullptr, 0 );
	context->PSSetShader( m_PixelShader.Get(), nullptr, 0 );
	context->VSSetConstantBuffers( 0, 1, cameraBuffers );
	context->VSSetConstantBuffers( 1, 1, objectBuffers );
	context->VSSetConstantBuffers( 2, 1, materialBuffers );
	context->PSSetConstantBuffers( 1, 1, objectBuffers );
	context->PSSetConstantBuffers( 2, 1, materialBuffers );

	// 単色描画時もnullptrを設定し、直前のテクスチャ参照を残さない。
	context->PSSetShaderResources( 0, 1, &textureView );
	context->PSSetSamplers( 0, 1, samplers );
	context->DrawIndexed( m_IndexCount, 0, 0 );
}

// 描画に使用したDirect3Dリソースを解放する。
void BasicMeshRenderer::Uninit()
{
	// SamplerとTextureを解放する。
	m_TextureSampler.Reset();
	m_ObjectTextureView.Reset();
	m_WallTextureView.Reset();
	m_FloorTextureView.Reset();

	// 定数バッファとメッシュBufferを解放する。
	m_MaterialBuffer.Uninit();
	m_ObjectBuffer.Uninit();
	m_CameraBuffer.Uninit();
	m_IndexBuffer.Reset();
	m_VertexBuffer.Reset();

	// ShaderとInput Layoutを解放する。
	m_InputLayout.Reset();
	m_PixelShader.Reset();
	m_VertexShader.Reset();

	m_IndexCount = {};
}