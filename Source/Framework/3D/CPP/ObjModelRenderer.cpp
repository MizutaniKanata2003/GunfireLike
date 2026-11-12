#include "../H/ObjModelRenderer.h"

// OBJの解析、GPU Resourceの生成・解放、既存Shaderによる描画を実装する。

//========= C++標準ライブラリ インクルード=========
#include <cstddef>
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

// OBJ解析、Shaderの読込、失敗ログに使う内部定義をまとめる。
namespace
{
	//========= Shaderファイルパス定数=========
	// OBJ描画のVertex Shaderバイナリ。
	constexpr char ns_MODEL_VERTEX_SHADER_CSO_PATH[] = "Shaders/BasicColorVS.cso";
	// OBJ描画のPixel Shaderバイナリ。
	constexpr char ns_MODEL_PIXEL_SHADER_CSO_PATH[] = "Shaders/BasicColorPS.cso";

	//========= OBJ形式定数=========
	// OBJ頂点形式で使用する位置座標とUV座標のセマンティック名。
	constexpr char ns_POSITION_SEMANTIC_NAME[] = "POSITION";
	constexpr char ns_TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";

	//========= OBJ読込用構造体=========
	// OBJのv/vt/vn形式から取得した位置、UV、法線のIndex。
	struct OBJ_INDEX
	{
		// 位置のOBJ Index。正数は1始まり。
		int positionIndex{};
		// UVのOBJ Index。0は未指定。
		int UVIndex{};
		// 法線のOBJ Index。頂点共有Keyには使用するが、法線値の読込は行わない。
		int normalIndex{};
	};

	//========= 補助関数=========
	// Assetカテゴリへ読込失敗を出力する。
	// errorText: 失敗内容を示す有効な文字列。nullptr不可。
	// filePath: 失敗した対象ファイルのパス。
	void WriteOBJModelAssetError(const wchar_t* errorText,const std::wstring& filePath)
	{
		std::wstring message{errorText};
		message += L": ";
		message += filePath;

		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,message);
	}

	// GraphicsカテゴリへResource生成失敗を出力する。
	// functionName: 失敗した処理を示す有効な文字列。nullptr不可。
	// result: 16進数8桁で出力するHRESULT。
	void WriteOBJModelGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: 0x";

		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);

		for(int digit_index = 7; digit_index >= 0; --digit_index){
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			message += HEX_DIGITS[digit];
		}

		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}

	// OBJのv/vt/vn形式をIndexへ分解する。
	// token: 面を構成する1頂点分のトークン。
	// result: 解析結果の出力先。未指定成分のため0初期化済みの値を渡す。
	// 戻り値: 位置Indexが0以外ならtrue。数値変換例外は捕捉しない。
	bool ParseOBJIndex(const std::string& token,OBJ_INDEX& result)
	{
		std::stringstream stream(token);
		std::string value{};

		if(!std::getline(stream,value,'/'))return false;
		if(!value.empty()) result.positionIndex = std::stoi(value);
		if(std::getline(stream,value,'/') && !value.empty()) result.UVIndex = std::stoi(value);
		if(std::getline(stream,value,'/') && !value.empty()) result.normalIndex = std::stoi(value);

		return result.positionIndex != 0;
	}

	// 同じ位置・UV・法線Indexの頂点を共有するためのKeyを返す。
	// index: Keyへ変換するOBJ Indexの組み合わせ。
	std::string MakeVertexKey(const OBJ_INDEX& index)
	{
		return std::to_string(index.positionIndex) + "/" + std::to_string(index.UVIndex) + "/" + std::to_string(index.normalIndex);
	}
}

// OBJ、Shader、Buffer、必要なTexture、Samplerを初期化する。
bool ObjModelRenderer::Initialize(GraphicsSystem& graphicsSystem,const std::wstring& OBJFilePath,const std::wstring& textureFilePath)
{
	Uninit();

	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	if(device == nullptr)return false;

	// OBJファイルからGPU Buffer生成に使用する頂点・Index情報を読み込む。
	std::vector<VERTEX> vertices{};
	std::vector<unsigned int> indices{};

	if(!LoadOBJFile(OBJFilePath,vertices,indices)){
		WriteOBJModelAssetError(L"OBJ読込失敗",OBJFilePath);
		Uninit();
		return false;
	}

	// OBJモデルのVertex Bufferを生成する。
	D3D11_BUFFER_DESC vertex_buffer_desc{};
	vertex_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(VERTEX) * vertices.size());
	vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices.data();

	const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_desc,&vertex_data,m_vertexBuffer.GetAddressOf());

	if(FAILED(vertex_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(VertexBuffer)",vertex_buffer_result);
		Uninit();
		return false;
	}

	// OBJモデルのIndex Bufferを生成する。
	D3D11_BUFFER_DESC index_buffer_desc{};
	index_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * indices.size());
	index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA index_data{};
	index_data.pSysMem = indices.data();

	const HRESULT index_buffer_result = device->CreateBuffer(&index_buffer_desc,&index_data,m_indexBuffer.GetAddressOf());

	if(FAILED(index_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(IndexBuffer)",index_buffer_result);
		Uninit();
		return false;
	}

	// VERTEX ShaderのCSO読込、Shader生成、Input Layout生成を行う。
	std::vector<char> vertex_shader_binary{};

	if(!ShaderBinaryLoader::Load(ns_MODEL_VERTEX_SHADER_CSO_PATH,vertex_shader_binary)){
		Uninit();
		return false;
	}

	const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_binary.data(),vertex_shader_binary.size(),
																																	nullptr,m_vertexShader.GetAddressOf());

	if(FAILED(vertex_shader_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
		Uninit();
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC input_elements[]
	{
		{ns_POSITION_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VERTEX,position),D3D11_INPUT_PER_VERTEX_DATA,0},
		{ns_TEXCOORD_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(VERTEX,UV),D3D11_INPUT_PER_VERTEX_DATA,0}
	};

	const HRESULT input_layout_result = device->CreateInputLayout(input_elements,ARRAYSIZE(input_elements),vertex_shader_binary.data(),
																																vertex_shader_binary.size(),m_inputLayout.GetAddressOf());

	if(FAILED(input_layout_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateInputLayout",input_layout_result);
		Uninit();
		return false;
	}

	// Pixel Shaderを読み込み、OBJ描画用のPixel Shaderを生成する。
	std::vector<char> pixel_shader_binary{};

	if(!ShaderBinaryLoader::Load(ns_MODEL_PIXEL_SHADER_CSO_PATH,pixel_shader_binary)){
		Uninit();
		return false;
	}

	const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_binary.data(),pixel_shader_binary.size(),
																																nullptr,m_pixelShader.GetAddressOf());

	if(FAILED(pixel_shader_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
		Uninit();
		return false;
	}

	HRESULT camera_buffer_result{};
	if(!m_cameraBuffer.Init(device,camera_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(CameraConstants)",camera_buffer_result);
		Uninit();
		return false;
	}

	HRESULT object_buffer_result{};
	if(!m_objectBuffer.Init(device,object_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(ObjectConstants)",object_buffer_result);
		Uninit();
		return false;
	}

	HRESULT material_buffer_result{};
	if(!m_materialBuffer.Init(device,material_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(MaterialConstants)",material_buffer_result);
		Uninit();
		return false;
	}

	// 空パスの場合はTextureを読み込まず、単色描画として扱う。
	if(!textureFilePath.empty() && !LoadTextureFromFile(device,textureFilePath)){
		WriteOBJModelAssetError(L"Texture読込失敗",textureFilePath);
		Uninit();
		return false;
	}

	// Texture参照時のフィルタリングとアドレス指定を行うSamplerを生成する。
	D3D11_SAMPLER_DESC sampler_desc{};
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.MinLOD = 0.0f;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

	const HRESULT sampler_state_result = device->CreateSamplerState(&sampler_desc,m_textureSampler.GetAddressOf());

	if(FAILED(sampler_state_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateSamplerState",sampler_state_result);
		Uninit();
		return false;
	}

	m_indexCount = static_cast<unsigned int>(indices.size());

	return true;
}

// 指定したWorld、View、Projection行列と色でOBJモデルを描画する。
void ObjModelRenderer::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
														const XMMATRIX& projectionMatrix,const XMFLOAT4& color)
{
	if(!m_vertexBuffer || !m_indexBuffer || !m_cameraBuffer.IsValid() ||
	!m_objectBuffer.IsValid() || !m_materialBuffer.IsValid() || !m_vertexShader ||
	!m_pixelShader || !m_inputLayout || !m_textureSampler || m_indexCount == 0)return;

	// 描画に使用するDirect3D Contextを取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	if(context == nullptr)return;

	// Shaderへ渡すCamera、Object、Material定数を作成する。
	const XMMATRIX view_projection_matrix = XMMatrixTranspose(viewMatrix * projectionMatrix);
	const XMMATRIX transposed_world_matrix = XMMatrixTranspose(worldMatrix);

	CAMERA_CONSTANTS camera_constants{};
	XMStoreFloat4x4(&camera_constants.viewProjectionMatrix,view_projection_matrix);

	OBJECT_CONSTANTS object_constants{};
	XMStoreFloat4x4(&object_constants.worldMatrix,transposed_world_matrix);
	object_constants.color = color;

	MATERIAL_CONSTANTS material_constants{};
	material_constants.uvTiling = XMFLOAT2{1.0f,1.0f};
	material_constants.useTexture = m_textureView ? 1.0f : 0.0f;

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	const UINT vertex_stride = sizeof(VERTEX);
	const UINT vertex_offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};

	// Shaderへ設定する定数Buffer、Texture、Samplerをまとめる。
	ID3D11Buffer* camera_buffers[]{m_cameraBuffer.Get()};
	ID3D11Buffer* object_buffers[]{m_objectBuffer.Get()};
	ID3D11Buffer* material_buffers[]{m_materialBuffer.Get()};
	ID3D11ShaderResourceView* texture_views[]{m_textureView.Get()};
	ID3D11SamplerState* samplers[]{m_textureSampler.Get()};

	m_cameraBuffer.Update(context,camera_constants);
	m_objectBuffer.Update(context,object_constants);
	m_materialBuffer.Update(context,material_constants);

	context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R32_UINT,0);
	context->IASetInputLayout(m_inputLayout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->VSSetShader(m_vertexShader.Get(),nullptr,0);
	context->PSSetShader(m_pixelShader.Get(),nullptr,0);
	context->VSSetConstantBuffers(0,1,camera_buffers);
	context->VSSetConstantBuffers(1,1,object_buffers);
	context->VSSetConstantBuffers(2,1,material_buffers);
	context->PSSetConstantBuffers(1,1,object_buffers);
	context->PSSetConstantBuffers(2,1,material_buffers);

	// Textureなしの場合もnullptrを設定し、直前のSRV参照を残さない。
	context->PSSetShaderResources(0,1,texture_views);
	context->PSSetSamplers(0,1,samplers);
	context->DrawIndexed(m_indexCount,0,0);
}

// OBJ描画で使用したDirect3Dリソースを解放する。
void ObjModelRenderer::Uninit()
{
	// TextureとSamplerを解放する。
	m_textureSampler.Reset();
	m_textureView.Reset();

	// 定数バッファとメッシュBufferを解放する。
	m_materialBuffer.Uninit();
	m_objectBuffer.Uninit();
	m_cameraBuffer.Uninit();
	m_indexBuffer.Reset();
	m_vertexBuffer.Reset();

	// ShaderとInput Layoutを解放する。
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_vertexShader.Reset();

	m_indexCount = {};
}

// OBJファイルを読み込み、頂点配列とIndex配列を生成する。
bool ObjModelRenderer::LoadOBJFile(const std::wstring& OBJFilePath,std::vector<VERTEX>& vertices,std::vector<unsigned int>& indices)
{
	std::ifstream file(OBJFilePath);
	if(!file)return false;

	// OBJから読み込む位置、UV、重複頂点の対応表を保持する。
	std::vector<XMFLOAT3> positions{};
	std::vector<XMFLOAT2> UVs{};
	std::unordered_map<std::string,unsigned int> vertex_map{};

	std::string line{};

	while(std::getline(file,line)){
		std::stringstream line_stream(line);
		std::string type{};

		line_stream >> type;

		if(type == "v"){
			XMFLOAT3 position{};
			line_stream >> position.x >> position.y >> position.z;
			positions.push_back(position);
			continue;
		}

		if(type == "vt"){
			XMFLOAT2 UV{};
			line_stream >> UV.x >> UV.y;

			// OBJのUV座標をDirect3Dの上下反転したUV座標へ変換する。
			UV.y = 1.0f - UV.y;
			UVs.push_back(UV);
			continue;
		}

		if(type != "f")continue;

		// 面を構成するOBJ Indexを読み込む。
		std::vector<OBJ_INDEX> face_indices{};
		std::string token{};

		while(line_stream >> token){
			OBJ_INDEX OBJ_index{};
			if(ParseOBJIndex(token,OBJ_index)) face_indices.push_back(OBJ_index);
		}

		if(face_indices.size() < 3)continue;

		// 四角形以上の面も三角形の扇形分割としてIndex化する。
		for(std::size_t index = 1; index + 1 < face_indices.size(); ++index){
			const OBJ_INDEX triangle[]
			{
				face_indices[0],
				face_indices[index],
				face_indices[index + 1]
			};

			for(const OBJ_INDEX& OBJ_index : triangle){
				const std::string key = MakeVertexKey(OBJ_index);
				const auto found = vertex_map.find(key);

				if(found != vertex_map.end()){
					indices.push_back(found->second);
					continue;
				}

				const int position_index = OBJ_index.positionIndex - 1;
				if(position_index < 0 || position_index >= static_cast<int>(positions.size()))return false;

				XMFLOAT2 UV{};

				if(OBJ_index.UVIndex > 0){
					const int UV_index = OBJ_index.UVIndex - 1;
					if(UV_index < 0 || UV_index >= static_cast<int>(UVs.size()))return false;

					UV = UVs[UV_index];
				}

				const VERTEX vertex{positions[position_index],UV};
				const unsigned int new_index = static_cast<unsigned int>(vertices.size());

				vertices.push_back(vertex);
				vertex_map.emplace(key,new_index);
				indices.push_back(new_index);
			}
		}
	}

	return !vertices.empty() && !indices.empty();
}

// テクスチャファイルを読み込み、Shader Resource Viewを生成する。
bool ObjModelRenderer::LoadTextureFromFile(ID3D11Device* device,const std::wstring& textureFilePath)
{
	return TextureLoader::LoadWicTexture(device,textureFilePath,m_textureView);
}