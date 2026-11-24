#include "../H/BasicMeshRenderer.h"

// CubeのCPU配列をGPUへ生成し、描画ごとに定数転送・Bind・DrawIndexedを実行する。
// 原本の頂点・Index・UV・API設定と失敗経路を維持し、描画Stateの復元や新機能は追加しない。BasicColorのShader・定数Bufferは、BasicColorPipelineを使う。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsErrorLog.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/MeshBuffers.h"
#include "Framework/DirectX/H/TextureLoader.h"

// Cube形状・Assetパスと、Cubeの頂点・Indexの配列をまとめる。
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

	//========= Textureファイルパス定数=========
	// Cube描画に使用するテクスチャファイルパス。
	constexpr wchar_t ns_FLOOR_TEXTURE_PATH[] = L"Assets/Textures/Floor.png";
	constexpr wchar_t ns_WALL_TEXTURE_PATH[] = L"Assets/Textures/Wall.png";
	constexpr wchar_t ns_OBJECT_TEXTURE_PATH[] = L"Assets/Textures/Object.png";

	//========= Cubeの頂点・Index配列=========
	// Cubeの各面で独立したUVを持つ頂点配列。
	// 位置は各面で重複しても独立頂点とし、各面へUV(0〜1)を割り当てる。配列順・面の向きは維持する。
	const BASIC_COLOR_VERTEX ns_CUBE_VERTICES[ns_CUBE_VERTEX_COUNT]
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

	// Cubeを構成する12枚の三角形のIndex配列。16bit Indexで、描画時のR16_UINTと要素幅を対応させる。
	const unsigned short ns_CUBE_INDICES[ns_CUBE_INDEX_COUNT]
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
}

//========= ライフサイクル・描画関数=========
// Cubeメッシュ、BasicColorPipeline、Texture、Samplerを初期化する。
bool BasicMeshRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
	// 旧所有参照を先に解放する。以後の生成失敗でも旧描画状態へは復帰しない。
	Uninit();

	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	// Device未取得なら生成せずfalseを返す。先頭のUninitにより旧Resourceは既に解放済み。
	if(device == nullptr)return false;

	// Cube Buffer→Shader・定数Buffer→画像→Samplerの順に生成する。失敗のログは、各生成関数が記録する。
	// 最初に失敗した時点で、生成済みのResourceを全て解放してfalseを返す。LoaderのCOM初期化前提は呼出側で守る。
	if(!CreateCubeBuffers(device) || !m_pipeline.Initialize(device) || !LoadTextures(device) || !CreateSampler(device)){
		Uninit();
		return false;
	}

	// 全生成後にだけ36Indexを確定する。失敗経路ではUninitにより0へ戻る。
	m_indexCount = ns_CUBE_INDEX_COUNT;
	return true;
}

// 指定した行列と外観でCubeを描画する。
void BasicMeshRenderer::DrawCube(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const CAMERA_MATRICES& camera,const CUBE_APPEARANCE& appearance)
{
	// 必須のBuffer・Shader・Layout・SamplerとIndex数が揃わなければ転送・Bind・描画を省く。
	if(!m_vertexBuffer || !m_indexBuffer || !m_pipeline.IsValid() || !m_textureSampler || m_indexCount == 0)return;

	// 描画に使用するDirect3D Contextを取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	// Contextがなければ描画しない。GraphicsSystemは借用し、このRendererに参照を登録しない。
	if(context == nullptr)return;

	// Shaderへ渡すCamera、Object、Material定数を作成する。
	// View*Projectionの順で合成して転置し、Worldも転置する。HLSLの行列格納契約へ合わせ、座標系変更はしない。
	const XMMATRIX view_projection_matrix = XMMatrixTranspose(camera.view * camera.projection);
	const XMMATRIX transposed_world_matrix = XMMatrixTranspose(worldMatrix);

	// 転置済みのWorld→Clip行列をVS b0の配置へ保存する。
	CAMERA_CONSTANTS camera_constants{};
	XMStoreFloat4x4(&camera_constants.viewProjectionMatrix,view_projection_matrix);

	// Local→World行列とRGBA色をb1の配置へ保存する。色のclampやAlpha操作は追加しない。
	OBJECT_CONSTANTS object_constants{};
	XMStoreFloat4x4(&object_constants.worldMatrix,transposed_world_matrix);
	object_constants.color = appearance.color;

	// Floor・Wall・Objectは使用値1で画像参照を選ぶ。Colorと未定義値は使用値0のまま単色経路とする。個別SRVのnull判定は追加しない。
	// baseColor・uvTiling・useTexture・paddingの配置を保つ。未参照baseColorと余白も既定値で残す。
	const TextureType texture_type = appearance.textureType;
	const bool uses_texture = texture_type == TextureType::Floor || texture_type == TextureType::Wall || texture_type == TextureType::Object;
	MATERIAL_CONSTANTS material_constants{};
	material_constants.uvTiling = appearance.UVTiling;
	material_constants.useTexture = uses_texture ? 1.0f : 0.0f;

	// 3つの定数Bufferへ転送し、Cubeと画像を接続して描画する。更新だけではBindされず、成否の戻り値もない。
	m_pipeline.UpdateConstants(context,camera_constants,object_constants,material_constants);
	BindAndDraw(context,SelectTextureView(texture_type));
}

// 移行中の旧形式。引数を構造体へまとめ、新しい形式へ渡す。呼出側の移行後に削除する。
void BasicMeshRenderer::DrawCube(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
																 const XMMATRIX& projectionMatrix,const XMFLOAT4& color,const XMFLOAT2& UVTiling,TextureType textureType)
{
	// View・Projectionの2行列と、色・UV倍率・画像選択を、それぞれ1つの構造体へまとめる。値は変更しない。
	const CAMERA_MATRICES camera{viewMatrix,projectionMatrix};
	const CUBE_APPEARANCE appearance{color,UVTiling,textureType};
	DrawCube(graphicsSystem,worldMatrix,camera,appearance);
}

// 描画に使用したDirect3Dリソースを解放する。
void BasicMeshRenderer::Uninit()
{
	// Sampler→各SRVの所有参照を解放する。Contextの別参照は残り得るためUnbindやGPU完了待機とは区別する。
	m_textureSampler.Reset();
	m_objectTextureView.Reset();
	m_wallTextureView.Reset();
	m_floorTextureView.Reset();

	// BasicColorのResourceと、Index→頂点の順でBufferを解放する。
	m_pipeline.Uninit();
	m_indexBuffer.Reset();
	m_vertexBuffer.Reset();

	// 解放後に描画数だけを残さず、初期状態の0へ戻す。
	m_indexCount = {};
}

//========= 生成・描画の補助関数=========
// CubeのVertex BufferとIndex Bufferを、この順に生成する。
bool BasicMeshRenderer::CreateCubeBuffers(ID3D11Device* device)
{
	// Vertex Bufferの指定。ByteWidthは配列の総バイト数。DEFAULT・頂点用途、CPUAccessFlags等0の既存設定を維持する。
	const MESH_BUFFER_SOURCE vertex_source{ns_CUBE_VERTICES,sizeof(ns_CUBE_VERTICES),D3D11_USAGE_DEFAULT,D3D11_BIND_VERTEX_BUFFER,
		L"ID3D11Device::CreateBuffer(VertexBuffer)"};

	// Index Bufferの指定。unsigned shortの総バイト数を指定する。描画時のR16_UINTと要素幅を対応させる。
	const MESH_BUFFER_SOURCE index_source{ns_CUBE_INDICES,sizeof(ns_CUBE_INDICES),D3D11_USAGE_DEFAULT,D3D11_BIND_INDEX_BUFFER,
		L"ID3D11Device::CreateBuffer(IndexBuffer)"};

	// Vertex→Indexの順に生成し、最初の失敗で止める。失敗のログは、MeshBuffersが記録する。先に生成したVertex Bufferの解放は、呼出側が行う。
	return MeshBuffers::Create(device,vertex_source,m_vertexBuffer) && MeshBuffers::Create(device,index_source,m_indexBuffer);
}

// 床・壁・Objectの画像を、この順に読み込む。
bool BasicMeshRenderer::LoadTextures(ID3D11Device* device)
{
	// 床→壁→Objectの順に試し、最初の失敗で短絡して後続読込を省く。失敗のログは、TextureLoaderが記録する。
	return TextureLoader::LoadWicTexture(device,ns_FLOOR_TEXTURE_PATH,m_floorTextureView) &&
		TextureLoader::LoadWicTexture(device,ns_WALL_TEXTURE_PATH,m_wallTextureView) &&
		TextureLoader::LoadWicTexture(device,ns_OBJECT_TEXTURE_PATH,m_objectTextureView);
}

// Linear参照・WRAPのSamplerを生成する。
bool BasicMeshRenderer::CreateSampler(ID3D11Device* device)
{
	// Texture参照時のフィルタリングとアドレス指定を行うSamplerを設定する。
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
	// Sampler生成失敗ならAPI名・結果を記録して失敗を返す。生成済みResourceの解放は、呼出側が行う。
	if(FAILED(sampler_state_result)){
		GraphicsErrorLog::Write(L"ID3D11Device::CreateSamplerState",sampler_state_result);
		return false;
	}
	return true;
}

// 画像の種別から、使うSRVを選ぶ。
ID3D11ShaderResourceView* BasicMeshRenderer::SelectTextureView(TextureType textureType) const
{
	// Texture選択でSRVを決める。個別SRVのnull判定は追加せず、Color・未定義値はnullptrの単色経路とする。
	switch(textureType){
	case TextureType::Floor:
	return m_floorTextureView.Get();
	case TextureType::Wall:
	return m_wallTextureView.Get();
	case TextureType::Object:
	return m_objectTextureView.Get();
	case TextureType::Color:
	default:
	return nullptr;
	}
}

// Cube・画像・Samplerを接続して描画する。
void BasicMeshRenderer::BindAndDraw(ID3D11DeviceContext* context,ID3D11ShaderResourceView* textureView) const
{
	// Input Assemblerへ設定する頂点Buffer情報と、Shaderへ設定するSamplerをまとめる。
	// Strideは頂点1個のバイト間隔、Offset0はBuffer先頭。配列は借用ポインターで所有はメンバーに残る。
	const UINT vertex_stride = sizeof(BASIC_COLOR_VERTEX);
	const UINT vertex_offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};
	ID3D11SamplerState* samplers[]{m_textureSampler.Get()};

	// Slot0へ頂点1本、16bit Index、Triangle Listを接続する。3Indexで1三角形となる。
	context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R16_UINT,0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Layout・VS/PS・定数Bufferを接続する。PS b0やRender Target・Blend・Depth・Cullはこの描画で設定せず、呼出側に任せる。
	m_pipeline.Bind(context);

	// PS t0は単色時もnullptrを明示し、直前の画像参照を残さない。PS s0のSamplerは別にBindする。
	context->PSSetShaderResources(0,1,&textureView);
	context->PSSetSamplers(0,1,samplers);

	// 36Indexを先頭Index0・BaseVertex0で描く。IA・Shader・定数・SRV・Samplerを元へ戻さない。
	context->DrawIndexed(m_indexCount,0,0);
}