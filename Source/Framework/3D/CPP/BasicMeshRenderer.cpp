#include "../H/BasicMeshRenderer.h"

// CPUメッシュをGPU Resourceへ変換し、定数転送・Bind・DrawIndexed・所有参照の解放を実装する。
// API設定と既存の失敗経路を維持する。以下の説明は未実装の安全策を保証するものではない。

//========= C++標準ライブラリ インクルード=========
#include <cstddef>
#include <string>
#include <vector>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/DirectX/H/TextureLoader.h"
#include "Framework/Etc/H/Logger.h"

// Shader/Textureパス、頂点属性とメッシュの内部定義、および失敗ログをまとめる。
namespace
{
	//========= Cubeメッシュ定数=========
	// Cubeの頂点数とIndex数。
	constexpr unsigned int ns_CUBE_VERTEX_COUNT = 24;
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
	// GraphicsカテゴリでBasicMesh描画Resource生成失敗を出力する。
	// functionName: 失敗したAPIを識別する有効な終端付きワイド文字列。nullptr不可。
	// result: 32bitのHRESULT値。上位4bitから8桁の16進数へ変換して記録する。
	void WriteBasicMeshGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		// API名とHRESULTを一つのメッセージへ組み立てる。呼出元ごとにAssetとGPU生成の失敗を区別する。
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: 0x";

		// unsignedへ変換し、右シフトと0x0fのマスクで各桁を取り出す。元の演算順は変更しない。
		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);

		for(int digit_index = 7; digit_index >= 0; --digit_index){
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			message += HEX_DIGITS[digit];
		}

		// 組立済み文字列をデバッグ出力へ渡す。ログ生成自体の例外捕捉は追加しない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}
}

//========= ライフサイクル・描画関数=========
// Cubeメッシュ、Shader、Texture、Sampler、定数バッファを初期化する。
bool BasicMeshRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
	// 先に旧所有参照を解放し、GetAddressOfへ渡す出力先を空にする。失敗しても旧モデルへは復帰しない。
	Uninit();

	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	// Device未取得なら何も生成しない。GraphicsSystemは借用し、このRendererでは保存しない。
	if(device == nullptr)return false;

	// Cubeの各面で独立したUVを持つ頂点配列を作成する。
	// 同じ角の位置も面ごとに別頂点とし、各面のUVを独立に割り当てる。配列順とUVの向きは変更しない。
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
	// ByteWidthは頂点数ではなく総バイト数。Usage=DEFAULTはGPU側のResource使用法を指定する。
	// BindFlagsはIAの頂点Buffer用途。ゼロ初期化によりCPUAccessFlags・MiscFlags・StructureByteStrideは0を維持する。
	// CPUから直接Mapする構成ではなく、初期データをCreateBufferへ渡す。Usage選択の設計意図は推測しない。
	D3D11_BUFFER_DESC vertex_buffer_desc{};
	vertex_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(vertices));
	vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	// pSysMemはCPU側の連続頂点配列。CreateBuffer呼出中に有効である必要があり、配列の所有権は渡さない。
	// 画像ではないBufferの初期データなので、SysMemPitch・SysMemSlicePitchはゼロ初期化のままにする。
	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices;

	const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_desc,&vertex_data,m_vertexBuffer.GetAddressOf());
	if(FAILED(vertex_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(VertexBuffer)",vertex_buffer_result);
		Uninit();
		return false;
	}

	// CubeのIndex Bufferを生成する。
	// Indexの総バイト数を指定する。BindFlagsをIndex用途にし、Draw時のFormatと要素型を対応させる。
	// BasicMeshはunsigned short/R16_UINT、OBJはunsigned int/R32_UINTという既存の違いを維持する。
	D3D11_BUFFER_DESC index_buffer_desc{};
	index_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(indices));
	index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	// 三角形の頂点参照順を初期データとして渡す。Vertexと同様、CPU配列の所有をDeviceへ移さない。
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

	// CreateVertexShaderにはHLSL本文ではなくCSOの先頭とバイト長を渡す。ここではShaderのコンパイルを行わない。
	// ClassLinkageはnullptrで、動的リンク用の設定は渡さない。失敗時は作成済みBufferもUninitで解放する。
	const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_binary.data(),vertex_shader_binary.size(),
																																	nullptr,m_vertexShader.GetAddressOf());

	if(FAILED(vertex_shader_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
		Uninit();
		return false;
	}

	// POSITION0=float3とTEXCOORD0=float2を、同一Input Slot 0の頂点ごとのデータとして読む。
	// offsetofはCPUのVERTEX内のバイト位置。PER_VERTEX_DATAとStepRate 0で、Instanceごとの属性にはしない。
	// 名前・番号・FormatはBasicColorVSの入力semanticへ対応する。法線や色の頂点入力は追加しない。
	const D3D11_INPUT_ELEMENT_DESC input_elements[]
	{
		{ns_POSITION_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VERTEX,position),D3D11_INPUT_PER_VERTEX_DATA,0},
		{ns_TEXCOORD_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(VERTEX,UV),D3D11_INPUT_PER_VERTEX_DATA,0}
	};

	// VSバイナリの入力signatureと上の記述を対応付けてLayoutを生成する。頂点のStrideはDraw時に別途指定する。
	// CSO配列はLayout生成が終わるまで有効なローカルデータとして保持する。
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

	// BasicColorPSのCSOからPSを生成する。Texture・Sampler・定数Bufferの接続は生成時ではなくDrawで行う。
	const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_binary.data(),pixel_shader_binary.size(),
																																nullptr,m_pixelShader.GetAddressOf());

	if(FAILED(pixel_shader_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
		Uninit();
		return false;
	}

	// Camera、Object、Material用の定数バッファを生成する。
	// CameraはVS b0に対応し、ViewProjectionの保存型を転送する。ConstantBufferのInitはデータの初期値をGPUへ渡さない。
	// 失敗したBufferだけでなく、それまで生成したResourceも終了する。初回Draw前にUpdateで内容を設定する。
	HRESULT camera_buffer_result{};
	if(!m_cameraBuffer.Init(device,camera_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(CAMERA_CONSTANTS)",camera_buffer_result);
		Uninit();
		return false;
	}

	// ObjectはVS/PS b1に対応し、World行列とRGBA色を転送する。ConstantBufferのInitはデータの初期値をGPUへ渡さない。
	// 失敗したBufferだけでなく、それまで生成したResourceも終了する。初回Draw前にUpdateで内容を設定する。
	HRESULT object_buffer_result{};
	if(!m_objectBuffer.Init(device,object_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(OBJECT_CONSTANTS)",object_buffer_result);
		Uninit();
		return false;
	}

	// MaterialはVS/PS b2に対応し、UV倍率とTexture使用フラグを転送する。ConstantBufferのInitはデータの初期値をGPUへ渡さない。
	// 失敗したBufferだけでなく、それまで生成したResourceも終了する。初回Draw前にUpdateで内容を設定する。
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
	// MIN_MAG_MIP_LINEARは縮小・拡大・Mip間の参照をLinearにする設定。画像にMipを追加生成する機能ではない。
	// AddressU/V/W=WRAPは範囲外のTexture座標を繰り返す設定。UV倍率によるタイル表示に対応する。
	// MinLOD=0からMaxLOD=FLOAT32_MAXを許可し、実際に使えるMipは画像Resource側に従う。
	// その他のフィールドはゼロ初期化を維持する。2D参照では未使用のAddressWも既存値のまま設定する。
	D3D11_SAMPLER_DESC sampler_description{};
	sampler_description.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_description.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_description.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_description.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_description.MinLOD = 0.0f;
	sampler_description.MaxLOD = D3D11_FLOAT32_MAX;

	const HRESULT sampler_state_result = device->CreateSamplerState(&sampler_description,m_textureSampler.GetAddressOf());
	if(FAILED(sampler_state_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateSamplerState",sampler_state_result);
		Uninit();
		return false;
	}

	// 全Resourceが揃った後にだけIndex数を確定する。この後、初期データ用CPU配列は関数終了で破棄できる。
	m_indexCount = ns_CUBE_INDEX_COUNT;

	return true;
}

// 指定した行列、色、UVタイリング、テクスチャ種別でCubeを描画する。
void BasicMeshRenderer::DrawCube(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
																 const XMMATRIX& projectionMatrix,const XMFLOAT4& color,const XMFLOAT2& UVTiling,TextureType textureType)
{
	// 必須のメッシュ・Shader・定数Buffer・Samplerと描画数を確認する。Blendや描画先の検証は呼出側の責務。
	if(!m_vertexBuffer || !m_indexBuffer || !m_cameraBuffer.IsValid() ||
	!m_objectBuffer.IsValid() || !m_materialBuffer.IsValid() || !m_vertexShader ||
	!m_pixelShader || !m_inputLayout || !m_textureSampler || m_indexCount == 0)return;

	// 描画に使用するDirect3D Contextを取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	if(context == nullptr)return;

	// テクスチャ種別に応じてShaderへ渡すTextureと使用フラグを決定する。
	ID3D11ShaderResourceView* texture_view{};
	float use_texture{};

	// Colorと未定義値はnullptr・使用フラグ0を保つ。各Texture選択では個別SRVのnull確認を追加しない。
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
	// BasicColorVSはmul(position,world)の後にViewProjectionを掛ける。CPUではView*Projectionをこの順で合成する。
	// 転送時の行列格納契約に合わせて各行列をtransposeし、XMFLOAT4X4へ保存する。座標系変更ではない。
	const XMMATRIX view_projection_matrix = XMMatrixTranspose(viewMatrix * projectionMatrix);
	const XMMATRIX transposed_world_matrix = XMMatrixTranspose(worldMatrix);

	// b0のCameraConstants.viewProjectionMatrixへ対応するCPUデータを作る。
	CAMERA_CONSTANTS camera_constants{};
	XMStoreFloat4x4(&camera_constants.viewProjectionMatrix,view_projection_matrix);

	// b1のObjectConstantsへWorldとObject色を格納する。Texture色との合成はPSの計算に任せる。
	OBJECT_CONSTANTS object_constants{};
	XMStoreFloat4x4(&object_constants.worldMatrix,transposed_world_matrix);
	object_constants.color = color;

	// CPU/HLSLでbaseColor・uvTiling・useTexture・paddingの順を一致させる。
	// baseColorは現行BasicColorでは未参照だが、転送配置を保つため既定値のまま残す。
	MATERIAL_CONSTANTS material_constants{};
	material_constants.uvTiling = UVTiling;
	material_constants.useTexture = use_texture;

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	// Strideは次の頂点へ進むバイト数、Offset 0はBufferの先頭から読む指定。
	// 下の配列はBind呼出へ渡す借用ポインターの束であり、Resource所有者はメンバーのComPtrのまま。
	const UINT vertex_stride = sizeof(VERTEX);
	const UINT vertex_offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};

	// Shaderへ設定する定数Buffer、Texture、Samplerをまとめる。
	ID3D11Buffer* camera_buffers[]{m_cameraBuffer.Get()};
	ID3D11Buffer* object_buffers[]{m_objectBuffer.Get()};
	ID3D11Buffer* material_buffers[]{m_materialBuffer.Get()};
	ID3D11SamplerState* samplers[]{m_textureSampler.Get()};

	// UpdateSubresourceを使って3つのBuffer全体を更新する。Shader slotへのBindはこの更新だけでは行われない。
	// Buffer更新APIはここでHRESULTを返さない。成否通知や新しい失敗分岐は追加せず、既存の順序を維持する。
	m_cameraBuffer.Update(context,camera_constants);
	m_objectBuffer.Update(context,object_constants);
	m_materialBuffer.Update(context,material_constants);

	// IAにはSlot 0から1本の頂点Bufferを設定し、Index FormatはCPU配列の要素幅と合わせる。
	// Triangle Listでは3個のIndexで1三角形を指定する。Layoutは各要素の解釈、Strideは頂点間隔を定義する。
	context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R16_UINT,0);
	context->IASetInputLayout(m_inputLayout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// VS/PSを今回のメッシュ用へ切り替える。nullptrのClassInstance配列と個数0で動的リンク指定は行わない。
	// 定数BufferのslotはStageごとに独立。VS b0=Camera、VS/PS b1=Object、VS/PS b2=Materialへ接続する。
	// PS b0はこの関数では変更しない。Bindされた状態はDraw後も残り、以前のShaderへは自動復元しない。
	context->VSSetShader(m_vertexShader.Get(),nullptr,0);
	context->PSSetShader(m_pixelShader.Get(),nullptr,0);
	context->VSSetConstantBuffers(0,1,camera_buffers);
	context->VSSetConstantBuffers(1,1,object_buffers);
	context->VSSetConstantBuffers(2,1,material_buffers);
	context->PSSetConstantBuffers(1,1,object_buffers);
	context->PSSetConstantBuffers(2,1,material_buffers);

	// 単色描画時もnullptrを設定し、直前のテクスチャ参照を残さない。
	// PS t0へSRVを設定する。画像なしでもnullptrを明示し、以前のSRVを誤って参照し続けることを防ぐ。
	// SamplerはPS s0へ別にBindする。画像データと、座標の繰返し・補間方法の設定を混同しない。
	context->PSSetShaderResources(0,1,&texture_view);
	context->PSSetSamplers(0,1,samplers);
	// DrawIndexed(Index数,先頭Index,頂点Base)を(保持数,0,0)で発行する。CPU配列へのアクセスではなく、Bind済みGPU Bufferを参照する。
	// 戻り値で描画成否は通知されない。Render TargetやBlend・Depth等のStateは呼出側が設定したものを使用する。
	context->DrawIndexed(m_indexCount,0,0);
}

// 描画に使用したDirect3Dリソースを解放する。
void BasicMeshRenderer::Uninit()
{
	// SamplerとTextureを解放する。
	// ここで解放するのはメンバーが持つCOM参照。ContextへBind中なら、Context側の参照も別途残り得る。
	// Shader ResourceのUnbind・ClearState・GPU完了待機は追加せず、他の描画への影響を変えない。
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

	// 次の描画で未準備状態を識別できるよう、GPU所有参照とともにIndex数を0へ戻す。
	m_indexCount = {};
}