#include "../H/BasicMeshRenderer.h"

//========= C++標準ライブラリ インクルード=========
#include <filesystem>
#include <fstream>
#include <vector>

//========= Windows インクルード=========
#include <wincodec.h>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

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
	constexpr wchar_t BASIC_COLOR_VERTEX_SHADER_CSO_PATH[] = L"Shaders/BasicColorVS.cso";
	constexpr wchar_t BASIC_COLOR_PIXEL_SHADER_CSO_PATH[] = L"Shaders/BasicColorPS.cso";

	//========= Textureファイルパス定数=========
	// Cube描画に使用するテクスチャファイルパス。
	constexpr wchar_t FLOOR_TEXTURE_PATH[] = L"Assets/Textures/Floor.png";
	constexpr wchar_t WALL_TEXTURE_PATH[] = L"Assets/Textures/Wall.png";
	constexpr wchar_t OBJECT_TEXTURE_PATH[] = L"Assets/Textures/Object.png";

	//========= Input Layout定数=========
	// Vertex Shader入力に使用する頂点属性のセマンティック名。
	constexpr char POSITION_SEMANTIC_NAME[] = "POSITION";
	constexpr char TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";

	//========= WICテクスチャ定数=========
	// BGRA形式の画像データにおける1ピクセルのバイト数。
	constexpr UINT BYTES_PER_PIXEL = 4;

	//========= 補助関数=========
	// 指定パスのバイナリファイルを読み込み、出力配列へ格納する。
	bool LoadBinaryFile( const wchar_t* filePath, std::vector<char>& binaryData )
	{
		std::ifstream fileStream( filePath, std::ios::binary | std::ios::ate );
		if ( !fileStream ) return false;

		const std::streamsize fileSize = fileStream.tellg();
		if ( fileSize <= 0 ) return false;

		binaryData.resize( static_cast<size_t>( fileSize ) );
		fileStream.seekg( 0, std::ios::beg );

		return static_cast<bool>( fileStream.read( binaryData.data(), fileSize ) );
	}

	// WICを使い、画像ファイルからShader Resource Viewを生成する。
	bool LoadTextureFromFile(
		ID3D11Device* device,
		const wchar_t* filePath,
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& textureView )
	{
		if ( device == nullptr || !std::filesystem::exists( filePath ) ) return false;

		// WICを使用して画像をデコードするためのCOMオブジェクト。
		Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory{};
		Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder{};
		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame{};
		Microsoft::WRL::ComPtr<IWICFormatConverter> converter{};

		HRESULT result = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS( wicFactory.GetAddressOf() ) );
		if ( FAILED( result ) ) return false;

		result = wicFactory->CreateDecoderFromFilename(
			filePath,
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnLoad,
			decoder.GetAddressOf() );
		if ( FAILED( result ) ) return false;

		result = decoder->GetFrame( 0, frame.GetAddressOf() );
		if ( FAILED( result ) ) return false;

		// 画像サイズとBGRA変換後のピクセル情報を取得する。
		UINT width{};
		UINT height{};

		result = frame->GetSize( &width, &height );
		if ( FAILED( result ) || width == 0 || height == 0 ) return false;

		result = wicFactory->CreateFormatConverter( converter.GetAddressOf() );
		if ( FAILED( result ) ) return false;

		result = converter->Initialize(
			frame.Get(),
			GUID_WICPixelFormat32bppBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0,
			WICBitmapPaletteTypeCustom );
		if ( FAILED( result ) ) return false;

		const UINT rowPitch = width * BYTES_PER_PIXEL;
		const UINT imageSize = rowPitch * height;
		std::vector<unsigned char> pixels( imageSize );

		result = converter->CopyPixels( nullptr, rowPitch, imageSize, pixels.data() );
		if ( FAILED( result ) ) return false;

		// Direct3DのTextureとShader Resource Viewを生成する。
		D3D11_TEXTURE2D_DESC textureDescription{};
		textureDescription.Width = width;
		textureDescription.Height = height;
		textureDescription.MipLevels = 1;
		textureDescription.ArraySize = 1;
		textureDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		textureDescription.SampleDesc.Count = 1;
		textureDescription.Usage = D3D11_USAGE_DEFAULT;
		textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA textureData{};
		textureData.pSysMem = pixels.data();
		textureData.SysMemPitch = rowPitch;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture{};

		result = device->CreateTexture2D(
			&textureDescription,
			&textureData,
			texture.GetAddressOf() );
		if ( FAILED( result ) ) return false;

		result = device->CreateShaderResourceView(
			texture.Get(),
			nullptr,
			textureView.GetAddressOf() );

		return SUCCEEDED( result );
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

	if ( FAILED( device->CreateBuffer(
		&vertexBufferDesc,
		&vertexData,
		m_VertexBuffer.GetAddressOf() ) ) )
	{
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

	if ( FAILED( device->CreateBuffer(
		&indexBufferDesc,
		&indexData,
		m_IndexBuffer.GetAddressOf() ) ) )
	{
		Uninit();
		return false;
	}

	// Vertex ShaderのCSO読込、Shader生成、Input Layout生成を行う。
	std::vector<char> vertexShaderBinary{};

	if ( !LoadBinaryFile( BASIC_COLOR_VERTEX_SHADER_CSO_PATH, vertexShaderBinary ) )
	{
		Uninit();
		return false;
	}

	if ( FAILED( device->CreateVertexShader(
		vertexShaderBinary.data(),
		vertexShaderBinary.size(),
		nullptr,
		m_VertexShader.GetAddressOf() ) ) )
	{
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

	if ( FAILED( device->CreateInputLayout(
		inputElements,
		ARRAYSIZE( inputElements ),
		vertexShaderBinary.data(),
		vertexShaderBinary.size(),
		m_InputLayout.GetAddressOf() ) ) )
	{
		Uninit();
		return false;
	}

	// Pixel Shaderを読み込み、Cube描画用のPixel Shaderを生成する。
	std::vector<char> pixelShaderBinary{};

	if ( !LoadBinaryFile( BASIC_COLOR_PIXEL_SHADER_CSO_PATH, pixelShaderBinary ) )
	{
		Uninit();
		return false;
	}

	if ( FAILED( device->CreatePixelShader(
		pixelShaderBinary.data(),
		pixelShaderBinary.size(),
		nullptr,
		m_PixelShader.GetAddressOf() ) ) )
	{
		Uninit();
		return false;
	}

	// World、View、Projection、色、UV情報を渡す定数バッファを生成する。
	D3D11_BUFFER_DESC transformBufferDesc{};
	transformBufferDesc.ByteWidth = static_cast<UINT>( sizeof( TransformBuffer ) );
	transformBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	transformBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	if ( FAILED( device->CreateBuffer(
		&transformBufferDesc,
		nullptr,
		m_TransformBuffer.GetAddressOf() ) ) )
	{
		Uninit();
		return false;
	}

	// 床、壁、画像付きオブジェクトに使用するテクスチャを読み込む。
	if ( !LoadTextureFromFile( device, FLOOR_TEXTURE_PATH, m_FloorTextureView ) ||
		!LoadTextureFromFile( device, WALL_TEXTURE_PATH, m_WallTextureView ) ||
		!LoadTextureFromFile( device, OBJECT_TEXTURE_PATH, m_ObjectTextureView ) )
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

	if ( FAILED( device->CreateSamplerState(
		&samplerDescription,
		m_TextureSampler.GetAddressOf() ) ) )
	{
		Uninit();
		return false;
	}

	m_IndexCount = CUBE_INDEX_COUNT;

	return true;
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
	m_TransformBuffer.Reset();
	m_IndexBuffer.Reset();
	m_VertexBuffer.Reset();

	// ShaderとInput Layoutを解放する。
	m_InputLayout.Reset();
	m_PixelShader.Reset();
	m_VertexShader.Reset();

	m_IndexCount = {};
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
	if ( !m_VertexBuffer || !m_IndexBuffer || !m_TransformBuffer || !m_VertexShader ||
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

	// Shaderへ渡すWorld、View、Projection、色、UV情報をまとめる。
	const DirectX::XMMATRIX worldViewProjection =
		DirectX::XMMatrixTranspose( worldMatrix * viewMatrix * projectionMatrix );
	const TransformBuffer transformBuffer
	{
		worldViewProjection,
		color,
		uvTiling,
		useTexture,
		0.0f
	};

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	const UINT vertexStride = sizeof( Vertex );
	const UINT vertexOffset{};
	ID3D11Buffer* vertexBuffers[]{ m_VertexBuffer.Get() };

	// Shaderへ設定する定数Buffer、Texture、Samplerをまとめる。
	ID3D11Buffer* constantBuffers[]{ m_TransformBuffer.Get() };
	ID3D11SamplerState* samplers[]{ m_TextureSampler.Get() };

	context->UpdateSubresource( m_TransformBuffer.Get(), 0, nullptr, &transformBuffer, 0, 0 );

	context->IASetVertexBuffers( 0, 1, vertexBuffers, &vertexStride, &vertexOffset );
	context->IASetIndexBuffer( m_IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0 );
	context->IASetInputLayout( m_InputLayout.Get() );
	context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	context->VSSetShader( m_VertexShader.Get(), nullptr, 0 );
	context->PSSetShader( m_PixelShader.Get(), nullptr, 0 );
	context->VSSetConstantBuffers( 0, 1, constantBuffers );
	context->PSSetConstantBuffers( 0, 1, constantBuffers );

	// 単色描画時もnullptrを設定し、直前のテクスチャ参照を残さない。
	context->PSSetShaderResources( 0, 1, &textureView );
	context->PSSetSamplers( 0, 1, samplers );
	context->DrawIndexed( m_IndexCount, 0, 0 );
}