#include "../H/BasicMeshRenderer.h"

// CubeのCPU配列をGPUへ生成し、描画ごとに定数転送・Bind・DrawIndexedを実行する。
// 原本の頂点・Index・UV・API設定と失敗経路を維持し、描画Stateの復元や新機能は追加しない。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // offsetofでCPU頂点の属性OffsetをInput Layoutへ指定する。
#include <string> // wstringでAPI名とHRESULTを含む失敗ログを組み立てる。
#include <vector> // vector<char>でCSOを連続したバイト列として保持する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/DirectX/H/TextureLoader.h"
#include "Framework/Etc/H/Logger.h"

// Cube形状・Assetパス・入力属性名と、内部のGPU生成失敗ログをまとめる。
namespace
{
	//========= Cubeメッシュ定数=========
	// Cubeの頂点数とIndex数。
	// 6面それぞれ4頂点を持ち、UV境界を共有しないため24頂点。Indexは6面×2三角形×3で36個。
	constexpr unsigned int ns_CUBE_VERTEX_COUNT = 24;
	constexpr unsigned int ns_CUBE_INDEX_COUNT = 36;

	// Cubeの各軸における最小座標と最大座標。
	// 原点を中心に各軸-0.5〜+0.5のLocal座標を使う。一辺1の既存寸法で、Worldでの寸法は行列に従う。
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
	// VERTEX Shader入力に使用する頂点属性のセマンティック名。
	constexpr char ns_POSITION_SEMANTIC_NAME[] = "POSITION";
	constexpr char ns_TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";

	//========= 補助関数=========
	// GraphicsカテゴリでBasicMesh描画Resource生成失敗を出力する。
	// functionName: 失敗APIを示す有効な終端付き文字列。nullptr不可。
	// result: 16進数8桁で記録するHRESULT。後始末は呼出元が担当する。
	void WriteBasicMeshGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		// API名と失敗値の接頭辞を連結する。ログ用メモリ確保の例外は捕捉しない。
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: 0x";

		// 4bitの値0〜15を数字とA〜Fへ対応させる表と、unsigned表現を用意する。
		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);

		// 桁番号7から0へ逆順走査し、上位から8桁を0埋め相当で出力する。
		for(int digit_index = 7; digit_index >= 0; --digit_index){
			// 1桁4bitなので対象桁を最下位へ移動し、0x0fでその桁だけを抽出する。
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			message += HEX_DIGITS[digit];
		}

		// 完成したログをGraphics分類へ送る。Resourceの解放や復旧はここでは行わない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}
}

//========= ライフサイクル・描画関数=========
// Cubeメッシュ、Shader、Texture、Sampler、定数バッファを初期化する。
bool BasicMeshRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
	// 旧所有参照を先に解放する。以後の生成失敗でも旧描画状態へは復帰しない。
	Uninit();

	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	// Device未取得なら生成せずfalseを返す。先頭のUninitにより旧Resourceは既に解放済み。
	if(device == nullptr)return false;

	// Cubeの各面で独立したUVを持つ頂点配列を作成する。
	// 位置は各面で重複しても独立頂点とし、各面へUV(0〜1)を割り当てる。配列順・面の向きは維持する。
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
	// ByteWidthは配列の総バイト数。DEFAULT・頂点用途、CPUAccessFlags等0の既存設定を維持する。
	D3D11_BUFFER_DESC vertex_buffer_desc{};
	vertex_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(vertices));
	vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	// CPU配列を読取元として渡し所有は移さない。BufferなのでPitch類はゼロのまま。
	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices;

	// 生成出力を空のメンバーComPtrで受け取る。初期データ配列は呼出中に有効に保つ。
	const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_desc,&vertex_data,m_vertexBuffer.GetAddressOf());
	// Vertex Buffer生成失敗ならAPI名・結果を記録し、部分ResourceをUninitして戻る。
	if(FAILED(vertex_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(VertexBuffer)",vertex_buffer_result);
		Uninit();
		return false;
	}

	// CubeのIndex Bufferを生成する。
	// unsigned shortの総バイト数を指定する。描画時のR16_UINTと要素幅を対応させる。
	D3D11_BUFFER_DESC index_buffer_desc{};
	index_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(indices));
	index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	// 12枚の三角形の参照順を初期データに渡す。Index配列の所有権は移さない。
	D3D11_SUBRESOURCE_DATA index_data{};
	index_data.pSysMem = indices;

	// DEFAULT・Index用途のBufferを生成し、メンバーへ所有参照を受け取る。
	const HRESULT index_buffer_result = device->CreateBuffer(&index_buffer_desc,&index_data,m_indexBuffer.GetAddressOf());
	// Index Buffer生成失敗なら先に生成したVertex BufferもUninitで解放する。
	if(FAILED(index_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(IndexBuffer)",index_buffer_result);
		Uninit();
		return false;
	}

	// VERTEX ShaderのCSO読込、Shader生成、Input Layout生成を行う。
	std::vector<char> vertex_shader_binary{};
	// VSのCSO読込失敗では、Cubeの生成済みBufferを含めて部分解放しfalseを返す。
	if(!ShaderBinaryLoader::Load(ns_BASIC_COLOR_VERTEX_SHADER_CSO_PATH,vertex_shader_binary)){
		Uninit();
		return false;
	}

	// CSOの先頭とバイト長を使う。コンパイルではなくVS生成で、ClassLinkageは指定しない。
	const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_binary.data(),vertex_shader_binary.size(),
																																	nullptr,m_vertexShader.GetAddressOf());

	// VS生成失敗ならAPI名・結果を記録し、先行するCube Bufferも解放する。
	if(FAILED(vertex_shader_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
		Uninit();
		return false;
	}

	// POSITION0=float3、TEXCOORD0=float2をSlot0の頂点単位データとして読む。offsetofはバイト位置。
	// StepRate0でInstanceごとの属性にはせず、StrideはDraw時に別途指定する。
	const D3D11_INPUT_ELEMENT_DESC input_elements[]
	{
		{ns_POSITION_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VERTEX,position),D3D11_INPUT_PER_VERTEX_DATA,0},
		{ns_TEXCOORD_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(VERTEX,UV),D3D11_INPUT_PER_VERTEX_DATA,0}
	};

	// CPU属性の解釈とVSの入力signatureを対応付ける。CSOはこの生成終了まで有効に保持する。
	const HRESULT input_layout_result = device->CreateInputLayout(input_elements,ARRAYSIZE(input_elements),
																																vertex_shader_binary.data(),vertex_shader_binary.size(),m_inputLayout.GetAddressOf());

	// Layout生成失敗ならVSとCube Bufferを含め所有Resourceを部分解放する。
	if(FAILED(input_layout_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateInputLayout",input_layout_result);
		Uninit();
		return false;
	}

	// Pixel Shaderを読み込み、Cube描画用のPixel Shaderを生成する。
	std::vector<char> pixel_shader_binary{};
	// PSのCSO読込失敗でも部分ResourceをUninitする。CPU配列の寿命はローカルで管理する。
	if(!ShaderBinaryLoader::Load(ns_BASIC_COLOR_PIXEL_SHADER_CSO_PATH,pixel_shader_binary)){
		Uninit();
		return false;
	}

	// BasicColorPSのCSOからPSを生成する。Texture・Samplerや定数のBindは描画時に行う。
	const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_binary.data(),pixel_shader_binary.size(),
																																nullptr,m_pixelShader.GetAddressOf());

	// PS生成失敗ならログ後に先行Resourceをまとめて解放し、falseを返す。
	if(FAILED(pixel_shader_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
		Uninit();
		return false;
	}

	// Camera、Object、Material用の定数バッファを生成する。
	// Cameraは64バイトのVS b0用。生成時に初期値は転送せず、初回描画前にUpdateする。
	HRESULT camera_buffer_result{};
	// Camera Buffer生成失敗なら結果を記録し、Shader・メッシュもUninitする。
	if(!m_cameraBuffer.Init(device,camera_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(CAMERA_CONSTANTS)",camera_buffer_result);
		Uninit();
		return false;
	}

	// Objectは80バイトのVS/PS b1用。World行列とRGBA色を描画ごとに転送する。
	HRESULT object_buffer_result{};
	// Object Buffer生成失敗なら先行Camera Bufferも含め部分解放する。
	if(!m_objectBuffer.Init(device,object_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(OBJECT_CONSTANTS)",object_buffer_result);
		Uninit();
		return false;
	}

	// Materialは32バイトのVS/PS b2用。UV倍率・Texture使用値と既存余白の配置を維持する。
	HRESULT material_buffer_result{};
	// Material Buffer生成失敗なら生成済みResourceを全てUninitして戻る。
	if(!m_materialBuffer.Init(device,material_buffer_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateBuffer(MATERIAL_CONSTANTS)",material_buffer_result);
		Uninit();
		return false;
	}

	// 床、壁、画像付きオブジェクトに使用するテクスチャを読み込む。
	// 床→壁→Objectの順に試し、最初の失敗で短絡して後続読込を省く。
	// どれかが失敗なら既に生成したSRV・Buffer・ShaderもUninitする。LoaderのCOM初期化前提は呼出側で守る。
	if(!TextureLoader::LoadWicTexture(device,ns_FLOOR_TEXTURE_PATH,m_floorTextureView) ||
	 !TextureLoader::LoadWicTexture(device,ns_WALL_TEXTURE_PATH,m_wallTextureView) ||
	 !TextureLoader::LoadWicTexture(device,ns_OBJECT_TEXTURE_PATH,m_objectTextureView)){
		Uninit();
		return false;
	}

	// Texture参照時のフィルタリングとアドレス指定を行うSamplerを生成する。
	// 縮小・拡大・Mip間をLinear参照し、U/V/Wは範囲外をWRAPで繰り返す。自動Mip生成の指定ではない。
	// LODは0〜FLOAT32_MAXを許可するが、実際のMipは画像Resourceに従う。その他はゼロ初期値を維持する。
	D3D11_SAMPLER_DESC sampler_description{};
	sampler_description.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_description.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_description.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_description.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_description.MinLOD = 0.0f;
	sampler_description.MaxLOD = D3D11_FLOAT32_MAX;

	// Texture本体とは別に、画像の補間・繰り返し方法を持つSampler Stateを生成する。
	const HRESULT sampler_state_result = device->CreateSamplerState(&sampler_description,m_textureSampler.GetAddressOf());
	// Sampler生成失敗なら画像を含む所有Resourceを部分解放し、Index数も0へ戻す。
	if(FAILED(sampler_state_result)){
		WriteBasicMeshGraphicsError(L"ID3D11Device::CreateSamplerState",sampler_state_result);
		Uninit();
		return false;
	}

	// 全生成後にだけ36Indexを確定する。CPU配列とCSOは関数終了後に保持し続ける必要はない。
	m_indexCount = ns_CUBE_INDEX_COUNT;
	return true;
}

// 指定した行列、色、UVタイリング、テクスチャ種別でCubeを描画する。
void BasicMeshRenderer::DrawCube(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
																 const XMMATRIX& projectionMatrix,const XMFLOAT4& color,const XMFLOAT2& UVTiling,TextureType textureType)
{
	// 必須のBuffer・Shader・Layout・SamplerとIndex数が揃わなければ転送・Bind・描画を省く。
	if(!m_vertexBuffer || !m_indexBuffer || !m_cameraBuffer.IsValid() ||
	!m_objectBuffer.IsValid() || !m_materialBuffer.IsValid() || !m_vertexShader ||
	!m_pixelShader || !m_inputLayout || !m_textureSampler || m_indexCount == 0)return;

	// 描画に使用するDirect3D Contextを取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	// Contextがなければ描画しない。GraphicsSystemは借用し、このRendererに参照を登録しない。
	if(context == nullptr)return;

	// テクスチャ種別に応じてShaderへ渡すTextureと使用フラグを決定する。
	ID3D11ShaderResourceView* texture_view{};
	float use_texture{};

	// Texture選択でSRVと使用値を揃える。個別SRVのnull判定は追加せず、Color・未定義値は初期値を維持する。
	switch(textureType){
		// Floor指定なら床用SRVを使い、使用値1で画像参照を選ぶ。
	case TextureType::Floor:
	texture_view = m_floorTextureView.Get();
	use_texture = 1.0f;
	break;

	// Wall指定なら壁用SRVを使い、使用値1を転送する。
	case TextureType::Wall:
	texture_view = m_wallTextureView.Get();
	use_texture = 1.0f;
	break;

	// Object指定ならObject用SRVを使い、使用値1を転送する。
	case TextureType::Object:
	texture_view = m_objectTextureView.Get();
	use_texture = 1.0f;
	break;

	// Colorと未定義値はSRV=nullptr・使用値0のまま単色経路とする。
	case TextureType::Color:
	default:
	break;
	}

	// Shaderへ渡すCamera、Object、Material定数を作成する。
	// View*Projectionの順で合成して転置し、Worldも転置する。HLSLの行列格納契約へ合わせ、座標系変更はしない。
	const XMMATRIX view_projection_matrix = XMMatrixTranspose(viewMatrix * projectionMatrix);
	const XMMATRIX transposed_world_matrix = XMMatrixTranspose(worldMatrix);

	// 転置済みのWorld→Clip行列をVS b0の配置へ保存する。
	CAMERA_CONSTANTS camera_constants{};
	XMStoreFloat4x4(&camera_constants.viewProjectionMatrix,view_projection_matrix);

	// Local→World行列とRGBA色をb1の配置へ保存する。色のclampやAlpha操作は追加しない。
	OBJECT_CONSTANTS object_constants{};
	XMStoreFloat4x4(&object_constants.worldMatrix,transposed_world_matrix);
	object_constants.color = color;

	// baseColor・uvTiling・useTexture・paddingの配置を保つ。未参照baseColorと余白も既定値で残す。
	MATERIAL_CONSTANTS material_constants{};
	material_constants.uvTiling = UVTiling;
	material_constants.useTexture = use_texture;

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	// Strideは頂点1個のバイト間隔、Offset0はBuffer先頭。配列は借用ポインターで所有はメンバーに残る。
	const UINT vertex_stride = sizeof(VERTEX);
	const UINT vertex_offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};

	// Shaderへ設定する定数Buffer、Texture、Samplerをまとめる。
	// 各Stageのslotへ渡す定数Buffer配列と、PS s0のSampler配列を1要素ずつ用意する。
	ID3D11Buffer* camera_buffers[]{m_cameraBuffer.Get()};
	ID3D11Buffer* object_buffers[]{m_objectBuffer.Get()};
	ID3D11Buffer* material_buffers[]{m_materialBuffer.Get()};
	ID3D11SamplerState* samplers[]{m_textureSampler.Get()};

	// Camera→Object→Materialの順でBuffer全体を転送する。更新だけではBindされず、成否の戻り値もない。
	m_cameraBuffer.Update(context,camera_constants);
	m_objectBuffer.Update(context,object_constants);
	m_materialBuffer.Update(context,material_constants);

	// Slot0へ頂点1本、16bit Index、Layout、Triangle Listを接続する。3Indexで1三角形となる。
	context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R16_UINT,0);
	context->IASetInputLayout(m_inputLayout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// VS/PSと定数Bufferを上書きする。VS b0=Camera、VS/PS b1=Object、VS/PS b2=Material。
	// PS b0やRender Target・Blend・Depth・Cullはこの描画で設定せず、呼出側に任せる。
	context->VSSetShader(m_vertexShader.Get(),nullptr,0);
	context->PSSetShader(m_pixelShader.Get(),nullptr,0);
	context->VSSetConstantBuffers(0,1,camera_buffers);
	context->VSSetConstantBuffers(1,1,object_buffers);
	context->VSSetConstantBuffers(2,1,material_buffers);
	context->PSSetConstantBuffers(1,1,object_buffers);
	context->PSSetConstantBuffers(2,1,material_buffers);

	// 単色描画時もnullptrを設定し、直前のテクスチャ参照を残さない。
	// PS t0は単色時もnullptrを明示し、直前の画像参照を残さない。PS s0のSamplerは別にBindする。
	context->PSSetShaderResources(0,1,&texture_view);
	context->PSSetSamplers(0,1,samplers);
	// 36Indexを先頭Index0・BaseVertex0で描く。IA・Shader・定数・SRV・Samplerを元へ戻さない。
	context->DrawIndexed(m_indexCount,0,0);
}

// 描画に使用したDirect3Dリソースを解放する。
void BasicMeshRenderer::Uninit()
{
	// SamplerとTextureを解放する。
	// Sampler→各SRVの所有参照を解放する。Contextの別参照は残り得るためUnbindやGPU完了待機とは区別する。
	m_textureSampler.Reset();
	m_objectTextureView.Reset();
	m_wallTextureView.Reset();
	m_floorTextureView.Reset();

	// 定数バッファとメッシュBufferを解放する。
	// Material→Object→Camera、Index→頂点の順でBufferを解放する。
	m_materialBuffer.Uninit();
	m_objectBuffer.Uninit();
	m_cameraBuffer.Uninit();
	m_indexBuffer.Reset();
	m_vertexBuffer.Reset();

	// ShaderとInput Layoutを解放する。
	// Layout→PS→VSの順に所有参照を解放する。
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_vertexShader.Reset();

	// 解放後に描画数だけを残さず、初期状態の0へ戻す。
	m_indexCount = {};
}