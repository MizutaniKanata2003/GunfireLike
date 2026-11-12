#include "../H/BasicMeshRenderer.h"

// 面ごとのUVを持つCubeとShader・Texture・Bufferを生成し、外観設定に応じた描画と解放を実装する。

//========= C++標準ライブラリ インクルード=========
#include <cstddef>
#include <string>
#include <vector>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/DirectX/H/TextureLoader.h"
#include "Framework/Etc/H/Logger.h"

// Cube定義、Assetパス、Input Layoutの定数と、内部用の失敗ログをまとめる。
namespace
{
	//========= Cubeメッシュ定数=========
	// 6面に独立した4頂点ずつを割り当てるための頂点数。
	constexpr unsigned int ns_CUBE_VERTEX_COUNT = 24;
	// 各面2三角形、各三角形3Indexで構成するIndex数。
	constexpr unsigned int ns_CUBE_INDEX_COUNT = 36;

	// Cubeの各軸における最小座標と最大座標。
	constexpr float ns_CUBE_MIN_COORDINATE = -0.5f;
	constexpr float ns_CUBE_MAX_COORDINATE = 0.5f;

	//========= Shaderファイルパス定数=========
	// BasicColor描画に使用するShaderのCSOファイルパス。
	constexpr char ns_BASIC_COLOR_VERTEX_SHADER_CSO_PATH[] = "Shaders/BasicColorVS.cso";
	constexpr char ns_BASIC_COLOR_PIXEL_SHADER_CSO_PATH[] = "Shaders/BasicColorPS.cso";

	//========= Textureファイルパス定数=========
	// Cube描画に使用するテクスチャファイルパス。
	constexpr wchar_t ns_FLOOR_TEXTURE_PATH[] = L"Assets/Textures/Floor.png";
	constexpr wchar_t ns_WALL_TEXTURE_PATH[] = L"Assets/Textures/Wall.png";
	constexpr wchar_t ns_OBJECT_TEXTURE_PATH[] = L"Assets/Textures/Object.png";

	//========= Input Layout定数=========
	// Vertex Shader入力に使用する頂点属性のセマンティック名。
	constexpr char ns_POSITION_SEMANTIC_NAME[] = "POSITION";
	constexpr char ns_TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";

	//========= 補助関数=========
	// GraphicsカテゴリへResource生成失敗を出力する。
	// functionName: 失敗した処理を示す終端付きワイド文字列。nullptr不可。
	// result: 16進数8桁で出力するHRESULT。
	void WriteBasicMeshGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		// 処理名へHRESULTを連結するためのログ文字列を用意する。
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: 0x";

		// HRESULTを4bitずつ上位から取り出し、符号付き表示ではなく16進数として記録する。
		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);

		for(int digit_index = 7; digit_index >= 0; --digit_index){
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			message += HEX_DIGITS[digit];
		}

		// 組み立てた一つのメッセージをGraphicsカテゴリへ出力する。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}
}

//========= ライフサイクル・描画関数=========
// Cubeメッシュ、Shader、Texture、Sampler、定数バッファを初期化する。
bool BasicMeshRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
	// 再初期化時に古いResourceを残さず、以後の失敗経路でも同じUninitで部分解放できる状態にする。
	Uninit();

	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	if(device == nullptr)return false;

	// Cubeの各面で独立したUVを持つ頂点配列を作成する。
	const VERTEX vertices[ns_CUBE_VERTEX_COUNT]
	{
		// 前面: Z = -0.5
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE},{0.0f,1.0f}},
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE},{0.0f,0.0f}},
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE},{1.0f,0.0f}},
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE},{1.0f,1.0f}},

		// 背面: Z = +0.5
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE},{0.0f,1.0f}},
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE},{0.0f,0.0f}},
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE},{1.0f,0.0f}},
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE},{1.0f,1.0f}},

		// 左面: X = -0.5
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE},{0.0f,1.0f}},
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE},{0.0f,0.0f}},
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE},{1.0f,0.0f}},
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE},{1.0f,1.0f}},

		// 右面: X = +0.5
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE},{0.0f,1.0f}},
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE},{0.0f,0.0f}},
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE},{1.0f,0.0f}},
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE},{1.0f,1.0f}},

		// 上面: Y = +0.5
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE},{0.0f,1.0f}},
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE},{0.0f,0.0f}},
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE},{1.0f,0.0f}},
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE},{1.0f,1.0f}},

		// 下面: Y = -0.5
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE},{0.0f,1.0f}},
		{{ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE},{0.0f,0.0f}},
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MIN_COORDINATE},{1.0f,0.0f}},
		{{ns_CUBE_MAX_COORDINATE,ns_CUBE_MIN_COORDINATE,ns_CUBE_MAX_COORDINATE},{1.0f,1.0f}}
	};

	// Cubeを構成する12枚の三角形のIndex配列を作成する。
	const unsigned short indices[ns_CUBE_INDEX_COUNT]
	{
		// 前面
		0,1,2,0,2,3,
		// 背面
		4,5,6,4,6,7,
		// 左面
		8,9,10,8,10,11,
		// 右面
		12,13,14,12,14,15,
		// 上面
		16,17,18,16,18,19,
		// 下面
		20,21,22,20,22,23
	};

	// CubeのVertex Bufferを生成する。
	D3D11_BUFFER_DESC vertex_buffer_desc{};
	vertex_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(vertices));
	vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices;
	const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_desc,&vertex_data,m_vertexBuffer.GetAddressOf());

	if(FAILED(vertex_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(VertexBuffer)",vertex_buffer_result);
		Uninit();
		return false;
	}

	// CubeのIndex Bufferを生成する。
	D3D11_BUFFER_DESC index_buffer_desc{};
	index_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(indices));
	index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA index_data{};
	index_data.pSysMem = indices;
	const HRESULT index_buffer_result = device->CreateBuffer(&index_buffer_desc,&index_data,m_indexBuffer.GetAddressOf());

	if(FAILED(index_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(IndexBuffer)",index_buffer_result);
		Uninit();
		return false;
	}

	// Vertex ShaderのCSO読込、Shader生成、Input Layout生成を行う。
	std::vector<char> vertex_shader_binary{};

	if(!ShaderBinaryLoader::Load(ns_BASIC_COLOR_VERTEX_SHADER_CSO_PATH,vertex_shader_binary)){
		Uninit();
		return false;
	}

	// 読み込んだバイナリからVertex Shaderを生成する。失敗時は全所有Resourceを終了する。
	const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_binary.data(),vertex_shader_binary.size(),
																																	nullptr,m_vertexShader.GetAddressOf());

	if(FAILED(vertex_shader_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
		Uninit();
		return false;
	}

	// 3成分Positionと2成分UVを、VERTEX内の実際のOffsetでShader入力へ対応付ける。
	const D3D11_INPUT_ELEMENT_DESC input_elements[]
	{
		{ns_POSITION_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VERTEX,position),D3D11_INPUT_PER_VERTEX_DATA,0},
		{ns_TEXCOORD_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(VERTEX,UV),D3D11_INPUT_PER_VERTEX_DATA,0}
	};

	const HRESULT input_layout_result = device->CreateInputLayout(input_elements,ARRAYSIZE(input_elements),
																																vertex_shader_binary.data(),vertex_shader_binary.size(),m_inputLayout.GetAddressOf());

	if(FAILED(input_layout_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateInputLayout",input_layout_result);
		Uninit();
		return false;
	}

	// Pixel Shaderを読み込み、Cube描画用のPixel Shaderを生成する。
	std::vector<char> pixel_shader_binary{};

	if(!ShaderBinaryLoader::Load(ns_BASIC_COLOR_PIXEL_SHADER_CSO_PATH,pixel_shader_binary)){
		Uninit();
		return false;
	}

	// 読み込んだPixel ShaderバイナリからGPU Resourceを生成する。
	const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_binary.data(),pixel_shader_binary.size(),
																																nullptr,m_pixelShader.GetAddressOf());

	if(FAILED(pixel_shader_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
		Uninit();
		return false;
	}

	// Camera、Object、Material用の定数バッファを生成する。
	HRESULT camera_buffer_result{};

	if(!m_cameraBuffer.Init(device,camera_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(CAMERA_CONSTANTS)",camera_buffer_result);
		Uninit();
		return false;
	}

	// ObjectのWorld行列と色を転送するBufferを生成する。
	HRESULT object_buffer_result{};

	if(!m_objectBuffer.Init(device,object_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(OBJECT_CONSTANTS)",object_buffer_result);
		Uninit();
		return false;
	}

	// UV倍率とTexture使用フラグを転送するBufferを生成する。
	HRESULT material_buffer_result{};

	if(!m_materialBuffer.Init(device,material_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(MATERIAL_CONSTANTS)",material_buffer_result);
		Uninit();
		return false;
	}

	// 床、壁、画像付きオブジェクトに使用するテクスチャを読み込む。
	if(!TextureLoader::LoadWicTexture(device,ns_FLOOR_TEXTURE_PATH,m_floorTextureView) ||
	 !TextureLoader::LoadWicTexture(device,ns_WALL_TEXTURE_PATH,m_wallTextureView) ||
	 !TextureLoader::LoadWicTexture(device,ns_OBJECT_TEXTURE_PATH,m_objectTextureView)){
		Uninit();
		return false;
	}

	// Texture参照時のフィルタリングとアドレス指定を行うSamplerを生成する。
	D3D11_SAMPLER_DESC sampler_description{};
	sampler_description.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_description.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_description.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_description.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_description.MinLOD = 0.0f;
	sampler_description.MaxLOD = D3D11_FLOAT32_MAX;

	// 設定をGPUのSampler Stateとして生成し、失敗時は部分Resourceを終了する。
	const HRESULT sampler_state_result = device->CreateSamplerState(&sampler_description,m_textureSampler.GetAddressOf());

	if(FAILED(sampler_state_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateSamplerState",sampler_state_result);
		Uninit();
		return false;
	}

	// 全Resource生成の成功後にだけ、描画へ使用するIndex数を確定する。
	m_indexCount = ns_CUBE_INDEX_COUNT;

	return true;
}

// 指定した行列、色、UVタイリング、テクスチャ種別でCubeを描画する。
void BasicMeshRenderer::DrawCube(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
																 const XMMATRIX& projectionMatrix,const XMFLOAT4& color,const XMFLOAT2& UVTiling,TextureType textureType)
{
	// 描画に必要なResourceとIndex数が揃わない場合は、Context取得やBindを行わない。
	if(!m_vertexBuffer || !m_indexBuffer || !m_cameraBuffer.IsValid() ||
	!m_objectBuffer.IsValid() || !m_materialBuffer.IsValid() || !m_vertexShader ||
	!m_pixelShader || !m_inputLayout || !m_textureSampler || m_indexCount == 0)return;

	// 描画に使用するDirect3D Contextを取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	if(context == nullptr)return;

	// テクスチャ種別に応じてShaderへ渡すTextureと使用フラグを決定する。
	ID3D11ShaderResourceView* texture_view{};
	float use_texture{};

	// Colorと未定義値では既定のnullptr・使用フラグ0を保つ。各Textureのnull確認は追加しない。
	switch(textureType){
	case TextureType::Floor:
	texture_view = m_floorTextureView.Get();
	use_texture = 1.0f;
	break;

	case TextureType::Wall:
	texture_view = m_wallTextureView.Get();
	use_texture = 1.0f;
	break;

	case TextureType::Object:
	texture_view = m_objectTextureView.Get();
	use_texture = 1.0f;
	break;

	case TextureType::Color:
	default:
	break;
	}

	// Shaderへ渡すCamera、Object、Material定数を作成する。
	const XMMATRIX view_projection_matrix = XMMatrixTranspose(viewMatrix * projectionMatrix);
	const XMMATRIX transposed_world_matrix = XMMatrixTranspose(worldMatrix);

	// 転置済みViewProjectionをCamera用の保存型へ格納する。
	CAMERA_CONSTANTS camera_constants{};
	XMStoreFloat4x4(&camera_constants.viewProjectionMatrix,view_projection_matrix);

	// 転置済みWorld行列と指定RGBA色をObject用データへ格納する。
	OBJECT_CONSTANTS object_constants{};
	XMStoreFloat4x4(&object_constants.worldMatrix,transposed_world_matrix);
	object_constants.color = color;

	// 既定のbaseColor・paddingを維持し、UV倍率と使用フラグだけを設定する。
	MATERIAL_CONSTANTS material_constants{};
	material_constants.uvTiling = UVTiling;
	material_constants.useTexture = use_texture;

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	const UINT vertex_stride = sizeof(VERTEX);
	const UINT vertex_offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};

	// Shaderへ設定する定数Buffer、Texture、Samplerをまとめる。
	ID3D11Buffer* camera_buffers[]{m_cameraBuffer.Get()};
	ID3D11Buffer* object_buffers[]{m_objectBuffer.Get()};
	ID3D11Buffer* material_buffers[]{m_materialBuffer.Get()};
	ID3D11SamplerState* samplers[]{m_textureSampler.Get()};

	// Bind前に3種類の定数Bufferを更新し、今回のCubeの変換・外観を反映する。
	m_cameraBuffer.Update(context,camera_constants);
	m_objectBuffer.Update(context,object_constants);
	m_materialBuffer.Update(context,material_constants);

	// 頂点・Index・Input Layout・Topologyを今回のCube描画用に設定する。
	context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R16_UINT,0);
	context->IASetInputLayout(m_inputLayout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Shaderを設定し、VSのb0/b1/b2とPSのb1/b2へ既存どおりBufferをBindする。
	context->VSSetShader(m_vertexShader.Get(),nullptr,0);
	context->PSSetShader(m_pixelShader.Get(),nullptr,0);
	context->VSSetConstantBuffers(0,1,camera_buffers);
	context->VSSetConstantBuffers(1,1,object_buffers);
	context->VSSetConstantBuffers(2,1,material_buffers);
	context->PSSetConstantBuffers(1,1,object_buffers);
	context->PSSetConstantBuffers(2,1,material_buffers);

	// 単色描画時もnullptrを設定し、直前のテクスチャ参照を残さない。
	context->PSSetShaderResources(0,1,&texture_view);
	context->PSSetSamplers(0,1,samplers);
	context->DrawIndexed(m_indexCount,0,0);
}

// 描画に使用したDirect3Dリソースを解放する。
void BasicMeshRenderer::Uninit()
{
	// SamplerとTextureを解放する。
	m_textureSampler.Reset();
	m_objectTextureView.Reset();
	m_wallTextureView.Reset();
	m_floorTextureView.Reset();

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

	// 解放後にIndex数だけが残らないよう、描画用カウントも0へ戻す。
	m_indexCount = {};
}