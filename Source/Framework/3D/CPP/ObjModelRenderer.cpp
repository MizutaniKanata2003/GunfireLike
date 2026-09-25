#include "../H/ObjModelRenderer.h"

//========= C++標準ライブラリ インクルード=========
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/DirectX/H/TextureLoader.h"
#include "Framework/Etc/H/Logger.h"

namespace
{
	//========= Shaderファイルパス定数=========
	constexpr char MODEL_VERTEX_SHADER_CSO_PATH[] = "Shaders/BasicColorVS.cso";
	constexpr char MODEL_PIXEL_SHADER_CSO_PATH[] = "Shaders/BasicColorPS.cso";

	//========= OBJ形式定数=========
	// OBJ頂点形式で使用する位置座標とUV座標のセマンティック名。
	constexpr char POSITION_SEMANTIC_NAME[] = "POSITION";
	constexpr char TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";

	//========= OBJ読込用構造体=========
	// OBJのv/vt/vn形式から取得した位置、UV、法線のIndex。
	struct ObjIndex
	{
		int positionIndex{};
		int uvIndex{};
		int normalIndex{};
	};

	//========= 補助関数=========
	// AssetカテゴリでOBJまたはTextureの読込失敗を出力する。
	void WriteObjModelAssetError( const wchar_t* errorText, const std::wstring& filePath )
	{
		std::wstring message{ errorText };
		message += L": ";
		message += filePath;

		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_ASSET, message );
	}

	// GraphicsカテゴリでOBJ描画Resource生成失敗を出力する。
	void WriteObjModelGraphicsError( const wchar_t* functionName, HRESULT result )
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

	// OBJのv/vt/vn形式を位置、UV、法線のIndexへ分解する。
	bool ParseObjIndex( const std::string& token, ObjIndex& result )
	{
		std::stringstream stream( token );
		std::string value{};

		if ( !std::getline( stream, value, '/' ) ) return false;
		if ( !value.empty() ) result.positionIndex = std::stoi( value );
		if ( std::getline( stream, value, '/' ) && !value.empty() ) result.uvIndex = std::stoi( value );
		if ( std::getline( stream, value, '/' ) && !value.empty() ) result.normalIndex = std::stoi( value );

		return result.positionIndex != 0;
	}

	// 同じ位置、UV、法線の組み合わせを共有するための文字列Keyを作る。
	std::string MakeVertexKey( const ObjIndex& index )
	{
		return std::to_string( index.positionIndex ) + "/" +
			std::to_string( index.uvIndex ) + "/" +
			std::to_string( index.normalIndex );
	}
}

// OBJ、Shader、Buffer、必要なTexture、Samplerを初期化する。
bool ObjModelRenderer::Initialize(
	GraphicsSystem& graphicsSystem,
	const std::wstring& objFilePath,
	const std::wstring& textureFilePath )
{
	Uninit();

	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	if ( device == nullptr ) return false;

	// OBJファイルからGPU Buffer生成に使用する頂点・Index情報を読み込む。
	std::vector<Vertex> vertices{};
	std::vector<unsigned int> indices{};

	if ( !LoadObjFile( objFilePath, vertices, indices ) )
	{
		WriteObjModelAssetError( L"OBJ読込失敗", objFilePath );
		return false;
	}

	// OBJモデルのVertex Bufferを生成する。
	D3D11_BUFFER_DESC vertexBufferDesc{};
	vertexBufferDesc.ByteWidth = static_cast<UINT>( sizeof( Vertex ) * vertices.size() );
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexData{};
	vertexData.pSysMem = vertices.data();

	const HRESULT vertexBufferResult = device->CreateBuffer(
	&vertexBufferDesc,
	&vertexData,
	m_VertexBuffer.GetAddressOf() );

	if ( FAILED( vertexBufferResult ) )
	{
		WriteObjModelGraphicsError( L"ID3D11Device::CreateBuffer(VertexBuffer)", vertexBufferResult );
		Uninit();
		return false;
	}

	// OBJモデルのIndex Bufferを生成する。
	D3D11_BUFFER_DESC indexBufferDesc{};
	indexBufferDesc.ByteWidth = static_cast<UINT>( sizeof( unsigned int ) * indices.size() );
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices.data();

	const HRESULT indexBufferResult = device->CreateBuffer(
	&indexBufferDesc,
	&indexData,
	m_IndexBuffer.GetAddressOf() );

	if ( FAILED( indexBufferResult ) )
	{
		WriteObjModelGraphicsError( L"ID3D11Device::CreateBuffer(IndexBuffer)", indexBufferResult );
		Uninit();
		return false;
	}

	// Vertex ShaderのCSO読込、Shader生成、Input Layout生成を行う。
	std::vector<char> vertexShaderBinary{};

	if ( !ShaderBinaryLoader::Load( MODEL_VERTEX_SHADER_CSO_PATH, vertexShaderBinary ) )
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
		WriteObjModelGraphicsError( L"ID3D11Device::CreateVertexShader", vertexShaderResult );
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
		WriteObjModelGraphicsError( L"ID3D11Device::CreateInputLayout", inputLayoutResult );
		Uninit();
		return false;
	}

	// Pixel Shaderを読み込み、OBJ描画用のPixel Shaderを生成する。
	std::vector<char> pixelShaderBinary{};

	if ( !ShaderBinaryLoader::Load( MODEL_PIXEL_SHADER_CSO_PATH, pixelShaderBinary ) )
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
		WriteObjModelGraphicsError( L"ID3D11Device::CreatePixelShader", pixelShaderResult );
		Uninit();
		return false;
	}

	// World、View、Projection、色、Texture使用有無を渡す定数バッファを生成する。
	D3D11_BUFFER_DESC transformBufferDesc{};
	transformBufferDesc.ByteWidth = static_cast<UINT>( sizeof( TransformBuffer ) );
	transformBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	transformBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	const HRESULT transformBufferResult = device->CreateBuffer(
	&transformBufferDesc,
	nullptr,
	m_TransformBuffer.GetAddressOf() );

	if ( FAILED( transformBufferResult ) )
	{
		WriteObjModelGraphicsError( L"ID3D11Device::CreateBuffer(TransformBuffer)", transformBufferResult );
		Uninit();
		return false;
	}

	// 空パスの場合はTextureを読み込まず、単色描画として扱う。
	if ( !textureFilePath.empty() && !LoadTextureFromFile( device, textureFilePath ) )
	{
		WriteObjModelAssetError( L"Texture読込失敗", textureFilePath );
		Uninit();
		return false;
	}

	// Texture参照時のフィルタリングとアドレス指定を行うSamplerを生成する。
	D3D11_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	const HRESULT samplerStateResult = device->CreateSamplerState(
	&samplerDesc,
	m_TextureSampler.GetAddressOf() );

	if ( FAILED( samplerStateResult ) )
	{
		WriteObjModelGraphicsError( L"ID3D11Device::CreateSamplerState", samplerStateResult );
		Uninit();
		return false;
	}

	m_IndexCount = static_cast<unsigned int>( indices.size() );

	return true;
}

// OBJ描画で使用したDirect3Dリソースを解放する。
void ObjModelRenderer::Uninit()
{
	// TextureとSamplerを解放する。
	m_TextureSampler.Reset();
	m_TextureView.Reset();

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

// 指定したWorld、View、Projection行列と色でOBJモデルを描画する。
void ObjModelRenderer::Draw(
	GraphicsSystem& graphicsSystem,
	const DirectX::XMMATRIX& worldMatrix,
	const DirectX::XMMATRIX& viewMatrix,
	const DirectX::XMMATRIX& projectionMatrix,
	const DirectX::XMFLOAT4& color )
{
	if ( !m_VertexBuffer || !m_IndexBuffer || !m_TransformBuffer || !m_VertexShader ||
		!m_PixelShader || !m_InputLayout || !m_TextureSampler || m_IndexCount == 0 ) return;

	// 描画に使用するDirect3D Contextを取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	if ( context == nullptr ) return;

	// Shaderへ渡すWorld、View、Projection、色、Texture使用有無をまとめる。
	const DirectX::XMMATRIX worldViewProjection =
		DirectX::XMMatrixTranspose( worldMatrix * viewMatrix * projectionMatrix );
	const TransformBuffer transformBuffer
	{
		worldViewProjection,
		color,
		DirectX::XMFLOAT2{ 1.0f, 1.0f },
		m_TextureView ? 1.0f : 0.0f,
		0.0f
	};

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	const UINT vertexStride = sizeof( Vertex );
	const UINT vertexOffset{};
	ID3D11Buffer* vertexBuffers[]{ m_VertexBuffer.Get() };

	// Shaderへ設定する定数Buffer、Texture、Samplerをまとめる。
	ID3D11Buffer* transformBuffers[]{ m_TransformBuffer.Get() };
	ID3D11ShaderResourceView* textureViews[]{ m_TextureView.Get() };
	ID3D11SamplerState* samplers[]{ m_TextureSampler.Get() };

	context->UpdateSubresource( m_TransformBuffer.Get(), 0, nullptr, &transformBuffer, 0, 0 );

	context->IASetVertexBuffers( 0, 1, vertexBuffers, &vertexStride, &vertexOffset );
	context->IASetIndexBuffer( m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0 );
	context->IASetInputLayout( m_InputLayout.Get() );
	context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	context->VSSetShader( m_VertexShader.Get(), nullptr, 0 );
	context->PSSetShader( m_PixelShader.Get(), nullptr, 0 );
	context->VSSetConstantBuffers( 0, 1, transformBuffers );
	context->PSSetConstantBuffers( 0, 1, transformBuffers );

	// Textureなしの場合もnullptrを設定し、直前のSRV参照を残さない。
	context->PSSetShaderResources( 0, 1, textureViews );
	context->PSSetSamplers( 0, 1, samplers );
	context->DrawIndexed( m_IndexCount, 0, 0 );
}

// OBJファイルを読み込み、頂点配列とIndex配列を生成する。
bool ObjModelRenderer::LoadObjFile(
	const std::wstring& objFilePath,
	std::vector<Vertex>& vertices,
	std::vector<unsigned int>& indices )
{
	std::ifstream file( objFilePath );
	if ( !file ) return false;

	// OBJから読み込む位置、UV、重複頂点の対応表を保持する。
	std::vector<DirectX::XMFLOAT3> positions{};
	std::vector<DirectX::XMFLOAT2> uvs{};
	std::unordered_map<std::string, unsigned int> vertexMap{};

	std::string line{};

	while ( std::getline( file, line ) )
	{
		std::stringstream lineStream( line );
		std::string type{};

		lineStream >> type;

		if ( type == "v" )
		{
			DirectX::XMFLOAT3 position{};
			lineStream >> position.x >> position.y >> position.z;
			positions.push_back( position );
			continue;
		}

		if ( type == "vt" )
		{
			DirectX::XMFLOAT2 uv{};
			lineStream >> uv.x >> uv.y;

			// OBJのUV座標をDirect3Dの上下反転したUV座標へ変換する。
			uv.y = 1.0f - uv.y;
			uvs.push_back( uv );
			continue;
		}

		if ( type != "f" ) continue;

		// 面を構成するOBJ Indexを読み込む。
		std::vector<ObjIndex> faceIndices{};
		std::string token{};

		while ( lineStream >> token )
		{
			ObjIndex objIndex{};
			if ( ParseObjIndex( token, objIndex ) ) faceIndices.push_back( objIndex );
		}

		if ( faceIndices.size() < 3 ) continue;

		// 四角形以上の面も三角形の扇形分割としてIndex化する。
		for ( size_t index = 1; index + 1 < faceIndices.size(); ++index )
		{
			const ObjIndex triangle[]
			{
				faceIndices[ 0 ],
				faceIndices[ index ],
				faceIndices[ index + 1 ]
			};

			for ( const ObjIndex& objIndex : triangle )
			{
				const std::string key = MakeVertexKey( objIndex );
				const auto found = vertexMap.find( key );

				if ( found != vertexMap.end() )
				{
					indices.push_back( found->second );
					continue;
				}

				const int positionIndex = objIndex.positionIndex - 1;
				if ( positionIndex < 0 || positionIndex >= static_cast<int>( positions.size() ) ) return false;

				DirectX::XMFLOAT2 uv{};

				if ( objIndex.uvIndex > 0 )
				{
					const int uvIndex = objIndex.uvIndex - 1;
					if ( uvIndex < 0 || uvIndex >= static_cast<int>( uvs.size() ) ) return false;

					uv = uvs[ uvIndex ];
				}

				const Vertex vertex{ positions[ positionIndex ], uv };
				const unsigned int newIndex = static_cast<unsigned int>( vertices.size() );

				vertices.push_back( vertex );
				vertexMap.emplace( key, newIndex );
				indices.push_back( newIndex );
			}
		}
	}

	return !vertices.empty() && !indices.empty();
}

// テクスチャファイルを読み込み、Shader Resource Viewを生成する。
bool ObjModelRenderer::LoadTextureFromFile( ID3D11Device* device, const std::wstring& textureFilePath )
{
	return TextureLoader::LoadWicTexture( device, textureFilePath, m_TextureView );
}