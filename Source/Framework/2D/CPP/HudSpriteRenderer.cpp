#include "../H/HudSpriteRenderer.h"

// 単位Quad・Samplerの生成と、画像の読込、Canvas座標からpixelへの変換、スプライトの描画を実装する。
// BasicColorのShader・定数Bufferは、BasicColorPipelineを使う。描画Stateの復元や、画像の拡縮以外の加工は行わない。

//========= C++標準ライブラリ インクルード=========
#include <array> // 単位Quadの4頂点と6Indexを、固定要素数の連続配列に保持する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/MeshBuffers.h"
#include "Framework/DirectX/H/SamplerFactory.h"
#include "Framework/DirectX/H/TextureLoader.h"

// 単位Quadの2三角形のIndexを定数としてまとめる。
namespace
{
	//========= 単位Quadの定数=========
	// 4頂点を(0,1,2)と(0,2,3)の2三角形へ分割する、16bit Index。
	constexpr std::array<unsigned short,6> ns_QUAD_INDICES{0,1,2,0,2,3};
}

//========= 初期化・読込関数=========
// 単位Quad、BasicColorPipeline、Samplerを生成する。
bool HudSpriteRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
	// 旧所有参照を先に解放する。以後の生成失敗でも旧描画状態へは復帰しない。
	Uninit();

	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	// Device未取得なら生成せずfalseを返す。先頭のUninitにより旧Resourceは既に解放済み。
	if(device == nullptr)return false;

	// Buffer・Shader・Samplerを、この順に生成する。失敗のログは、各生成関数が記録する。最初に失敗した時点で、部分Resourceを解放してfalseを返す。
	// Samplerは、画像の端を繰り返さないよう、Linear補間・CLAMPで生成する。
	if(!CreateQuadBuffers(device) || !m_pipeline.Initialize(device) || !SamplerFactory::CreateLinear(device,D3D11_TEXTURE_ADDRESS_CLAMP,m_sampler)){
		Uninit();
		return false;
	}

	// 全生成後にだけIndex数を6へ確定する。失敗経路ではUninitにより0へ戻る。
	m_indexCount = static_cast<unsigned int>(ns_QUAD_INDICES.size());
	return true;
}

// 画像を読み込み、SRVを保持して識別値を返す。
HudTextureHandle HudSpriteRenderer::LoadTexture(GraphicsSystem& graphicsSystem,std::wstring_view filePath)
{
	// 初期化前（Index数が0）なら、描画できないため読み込まず無効値を返す。
	if(m_indexCount == 0)return 0;

	// 生成に使うDeviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	// Device未取得なら、画像を生成できないため無効値を返す。
	if(device == nullptr)return 0;

	// 画像を読み込んでSRVを得る。失敗した場合は、TextureLoaderがログを記録し、出力先は空のままになる。
	ComPtr<ID3D11ShaderResourceView> texture_view{};
	// 読込失敗なら、保持せず無効値を返す。
	if(!TextureLoader::LoadWicTexture(device,filePath,texture_view))return 0;

	// 成功したSRVを末尾へ保持し、その位置+1を識別値として返す。0を無効値として使うため、1から始める。
	m_textures.push_back(texture_view);
	return static_cast<HudTextureHandle>(m_textures.size());
}

//========= 描画関数=========
// Canvas座標をpixel座標の矩形へ変換し、画像を貼った単位Quadを描画する。
void HudSpriteRenderer::DrawSprite(GraphicsSystem& graphicsSystem,HudTextureHandle texture,const HUD_RECT& rectangle,const HUD_SPRITE_APPEARANCE& appearance)
{
	// 描く大きさが0以下、または表示割合が0以下なら、何も描かない。識別値が無効値、または範囲外でも、描く画像がないため何も描かない。
	const float visible_width_ratio = appearance.visibleWidthRatio;
	if(rectangle.width <= 0.0f || rectangle.height <= 0.0f || visible_width_ratio <= 0.0f)return;
	if(texture == 0 || texture > m_textures.size())return;

	// 必須のBuffer・Shader・Layout・Sampler・画像が揃わなければ、転送・Bind・描画を省く。
	if(!m_vertexBuffer || !m_indexBuffer || !m_sampler || !m_pipeline.IsValid() || m_indexCount == 0)return;

	// 描画に使用するDirect3D Contextと、現在の描画先のpixel寸法を取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	const unsigned int render_width = graphicsSystem.GetRenderWidth();
	const unsigned int render_height = graphicsSystem.GetRenderHeight();
	// Contextがない、または描画先の幅か高さが0なら、変換の除算も描画も行わず戻る。
	if(context == nullptr || render_width == 0 || render_height == 0)return;

	// Canvas座標の位置と大きさを、描画先のpixel座標へ変換する。位置には倍率と中央寄せのOffset、大きさには倍率だけを適用する。
	const HudCanvas canvas{render_width,render_height};
	const XMFLOAT2 screen_position = canvas.ToScreenPosition(XMFLOAT2{rectangle.x,rectangle.y});
	const XMFLOAT2 screen_size = canvas.ToScreenSize(XMFLOAT2{rectangle.width,rectangle.height});

	// 単位Quadを、表示割合で縮めた幅と高さに拡縮し、左上のpixel座標へ移動するWorld行列を作る。行ベクトル形式で、拡縮→移動の順に合成する。
	const XMMATRIX world_matrix = XMMatrixScaling(screen_size.x * visible_width_ratio,screen_size.y,1.0f) *
		XMMatrixTranslation(screen_position.x,screen_position.y,0.0f);

	// pixel座標をClip座標へ変換する行列を作る。X方向はpixelを幅で割って2倍し-1する。Y方向は上下が逆のため、高さで割った値の符号を反転して+1する。
	// 行ベクトル形式で、拡縮→移動の順に合成する。Zは変えず、Wは1のままにする。
	const XMMATRIX view_projection_matrix = XMMatrixScaling(2.0f / static_cast<float>(render_width),-2.0f / static_cast<float>(render_height),1.0f) *
		XMMatrixTranslation(-1.0f,1.0f,0.0f);

	// HLSLの行列格納契約へ合わせて、BasicMeshRendererと同様に転置して保存する。
	CAMERA_CONSTANTS camera_constants{};
	XMStoreFloat4x4(&camera_constants.viewProjectionMatrix,XMMatrixTranspose(view_projection_matrix));
	OBJECT_CONSTANTS object_constants{};
	XMStoreFloat4x4(&object_constants.worldMatrix,XMMatrixTranspose(world_matrix));
	object_constants.color = appearance.color;

	// UV倍率のXに表示割合を入れ、Quadの横方向のUVを0〜割合へ縮める。縮めた幅と合わせて、画像が伸び縮みせず左側の一部分だけが表示される。
	// 画像使用値を1にして、BasicColorPSに画像の色とObject色の積を出力させる。
	MATERIAL_CONSTANTS material_constants{};
	material_constants.uvTiling = XMFLOAT2{visible_width_ratio,1.0f};
	material_constants.useTexture = 1.0f;

	// 3つの定数Bufferへ転送し、Quadと画像を接続して描画する。更新だけではBindされない。
	m_pipeline.UpdateConstants(context,camera_constants,object_constants,material_constants);
	BindAndDraw(context,m_textures[texture - 1].Get());
}

//========= 終了関数=========
// 画像・Buffer・BasicColorPipeline・Samplerの所有参照を解放する。
void HudSpriteRenderer::Uninit()
{
	// 読み込んだ画像のSRVを全て解放し、識別値を全て無効にする。
	m_textures.clear();

	// Sampler・BasicColorのResource・Index Buffer・Vertex Bufferの所有参照を解放する。Contextの別参照は残り得るためUnbindやGPU完了待機とは区別する。
	m_sampler.Reset();
	m_pipeline.Uninit();
	m_indexBuffer.Reset();
	m_vertexBuffer.Reset();

	// Index数を0へ戻す。
	m_indexCount = {};
}

//========= 生成・描画の補助関数=========
// 固定4頂点のVertex Bufferと、6要素のIndex Bufferを、この順に生成する。
bool HudSpriteRenderer::CreateQuadBuffers(ID3D11Device* device)
{
	// 左上から順に固定4頂点を作る。位置は0〜1の単位Quadで、UVも同じ値。大きさと位置はWorld行列が与える。
	const std::array<BASIC_COLOR_VERTEX,4> vertices
	{
		BASIC_COLOR_VERTEX{XMFLOAT3{0.0f,0.0f,0.0f},XMFLOAT2{0.0f,0.0f}},
		BASIC_COLOR_VERTEX{XMFLOAT3{1.0f,0.0f,0.0f},XMFLOAT2{1.0f,0.0f}},
		BASIC_COLOR_VERTEX{XMFLOAT3{1.0f,1.0f,0.0f},XMFLOAT2{1.0f,1.0f}},
		BASIC_COLOR_VERTEX{XMFLOAT3{0.0f,1.0f,0.0f},XMFLOAT2{0.0f,1.0f}}
	};

	// Vertex Bufferの指定。ByteWidthは総バイト数。IMMUTABLEなので生成時に初期データを渡し、以後は頂点Bufferを更新しない。
	const MESH_BUFFER_SOURCE vertex_source{vertices.data(),sizeof(BASIC_COLOR_VERTEX) * vertices.size(),D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_VERTEX_BUFFER,L"ID3D11Device::CreateBuffer(HudSpriteVertexBuffer)"};

	// Index Bufferの指定。16bit Indexの不変Bufferで、Draw時のR16_UINTと要素型を対応させる。
	const MESH_BUFFER_SOURCE index_source{ns_QUAD_INDICES.data(),sizeof(unsigned short) * ns_QUAD_INDICES.size(),D3D11_USAGE_IMMUTABLE,
		D3D11_BIND_INDEX_BUFFER,L"ID3D11Device::CreateBuffer(HudSpriteIndexBuffer)"};

	// Vertex→Indexの順に生成し、最初の失敗で止める。失敗のログは、MeshBuffersが記録する。先に生成したVertex Bufferの解放は、呼出側が行う。
	return MeshBuffers::Create(device,vertex_source,m_vertexBuffer) && MeshBuffers::Create(device,index_source,m_indexBuffer);
}

// 単位Quadと、画像・Samplerを接続して描画する。
void HudSpriteRenderer::BindAndDraw(ID3D11DeviceContext* context,ID3D11ShaderResourceView* texture) const
{
	// Input Assemblerへ設定する頂点Buffer情報と、Shaderへ設定する画像・Samplerをまとめる。
	const UINT vertex_stride = sizeof(BASIC_COLOR_VERTEX);
	const UINT vertex_offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};
	ID3D11ShaderResourceView* texture_views[]{texture};
	ID3D11SamplerState* samplers[]{m_sampler.Get()};

	// Slot0へ頂点1本、16bit Index、Triangle Listを接続する。
	context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R16_UINT,0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Layout・Shader・定数Bufferを接続し、PS t0=画像、PS s0=Samplerを上書きする。
	m_pipeline.Bind(context);
	context->PSSetShaderResources(0,1,texture_views);
	context->PSSetSamplers(0,1,samplers);

	// 6Indexを先頭Index0・BaseVertex0で描く。IA・Shader・定数・SRV・Samplerを元へ戻さない。
	context->DrawIndexed(m_indexCount,0,0);
}