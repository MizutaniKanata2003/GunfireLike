#include "../H/DebugLineRenderer.h"

// 線分用の動的頂点Bufferを生成し、描画ごとに端点を転送してLine Listで描画する。
// BasicColorのShader・定数Bufferは、BasicColorPipelineを使う。描画Stateの復元や、色ごとの頂点色対応は行わない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで1回の描画へ渡す頂点数をBufferの容量以下に制限する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsErrorLog.h"
#include "Framework/DirectX/H/GraphicsSystem.h"

// 頂点Buffer容量を定数としてまとめる。
namespace
{
	//========= 頂点Buffer容量=========
	// 頂点Bufferに格納できる最大頂点数。2点で1本の線分のため偶数にする。1024本の線分に相当する。
	// 超える端点は、複数回に分けて描画する。値を変更する場合も偶数を維持する。
	constexpr std::size_t ns_MAX_VERTEX_COUNT = 2048;
}

//========= ライフサイクル・描画関数=========
// 動的頂点Buffer、BasicColorPipelineを初期化する。
bool DebugLineRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
	// 旧所有参照を先に解放する。以後の生成失敗でも旧描画状態へは復帰しない。
	Uninit();

	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	// Device未取得なら生成せずfalseを返す。先頭のUninitにより旧Resourceは既に解放済み。
	if(device == nullptr)return false;

	// 動的頂点Bufferを生成する。ByteWidthは最大頂点数×1頂点のバイト数。
	// DYNAMICとCPU書込可能の組合せにより、描画ごとにMapのDISCARDで内容を丸ごと書き換えられる。初期データは渡さない。
	D3D11_BUFFER_DESC vertex_buffer_desc{};
	vertex_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(BASIC_COLOR_VERTEX) * ns_MAX_VERTEX_COUNT);
	vertex_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertex_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	// 生成出力を空のメンバーComPtrで受け取る。
	const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_desc,nullptr,m_vertexBuffer.GetAddressOf());
	// Vertex Buffer生成失敗ならAPI名・結果を記録し、部分ResourceをUninitして戻る。
	if(FAILED(vertex_buffer_result)){
		GraphicsErrorLog::Write(L"ID3D11Device::CreateBuffer(DebugLineVertexBuffer)",vertex_buffer_result);
		Uninit();
		return false;
	}

	// BasicColorのShader・Input Layout・定数Bufferを生成する。失敗のログは、BasicColorPipelineが記録する。失敗時は、頂点Bufferも解放してfalseを返す。
	if(!m_pipeline.Initialize(device)){
		Uninit();
		return false;
	}
	return true;
}

// 端点を2点ずつの線分として、Bufferの容量ごとに分けて描画する。
void DebugLineRenderer::DrawLines(GraphicsSystem& graphicsSystem,const std::vector<XMFLOAT3>& points,const XMFLOAT4& color,const CAMERA_MATRICES& camera)
{
	// 必須のBuffer・Shader・Layoutが揃わない、または線分が1本もできない端点数なら、転送・Bind・描画を省く。
	if(!m_vertexBuffer || !m_pipeline.IsValid() || points.size() < 2)return;

	// 描画に使用するDirect3D Contextを取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	// Contextがなければ描画しない。GraphicsSystemは借用し、このRendererに参照を登録しない。
	if(context == nullptr)return;

	// View*Projectionの順で合成して転置する。HLSLの行列格納契約へ合わせ、BasicMeshRendererと同じ手順にする。
	const XMMATRIX view_projection_matrix = XMMatrixTranspose(camera.view * camera.projection);
	CAMERA_CONSTANTS camera_constants{};
	XMStoreFloat4x4(&camera_constants.viewProjectionMatrix,view_projection_matrix);

	// 端点はワールド座標で渡すため、Worldは単位行列にする。単位行列は転置しても同じ。色は線の色をそのまま転送する。
	OBJECT_CONSTANTS object_constants{};
	object_constants.worldMatrix = XMFLOAT4X4{1.0f,0.0f,0.0f,0.0f,
		0.0f,1.0f,0.0f,0.0f,
		0.0f,0.0f,1.0f,0.0f,
		0.0f,0.0f,0.0f,1.0f};
	object_constants.color = color;

	// 画像使用値0で、BasicColorPSにObject色を出力させる。UV倍率は使用しないが、既定の(1,1)を維持する。
	MATERIAL_CONSTANTS material_constants{};
	material_constants.useTexture = 0.0f;

	// 3つの定数Bufferへ転送する。更新だけではBindされない。
	m_pipeline.UpdateConstants(context,camera_constants,object_constants,material_constants);

	// Slot0へ頂点1本、Line Listを接続する。2頂点で1本の線分となる。Index Bufferは使わない。Strideは頂点1個のバイト間隔、Offset0はBuffer先頭。
	const UINT vertex_stride = sizeof(BASIC_COLOR_VERTEX);
	const UINT vertex_offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};
	context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// Layout・Shader・定数Bufferを接続し、単色経路のため画像なしを示すnullptrのSRVをPS t0へ明示する。
	m_pipeline.Bind(context);
	ID3D11ShaderResourceView* texture_view{};
	context->PSSetShaderResources(0,1,&texture_view);

	// 奇数個なら最後の1点は線分にならないため捨て、2点ずつ使える個数にそろえる。
	const std::size_t usable_count = points.size() - points.size() % 2;

	// 先頭から、Bufferの容量ごとに区切って、転送と描画を繰り返す。区切りは偶数のため、線分が途中で分かれない。
	for(std::size_t start_index = 0; start_index < usable_count; start_index += ns_MAX_VERTEX_COUNT){
		// 今回描画する頂点数。残りが容量より少なければ、残り全部。
		const std::size_t chunk_count = std::min(ns_MAX_VERTEX_COUNT,usable_count - start_index);

		// 転送に失敗したら、以降の分割分も描画せず戻る。
		if(!DrawChunk(context,points,start_index,chunk_count))return;
	}
}

// 描画に使用したDirect3Dリソースを解放する。
void DebugLineRenderer::Uninit()
{
	// BasicColorのResourceと、頂点Bufferの所有参照を解放する。Contextの別参照は残り得るためUnbindやGPU完了待機とは区別する。
	m_pipeline.Uninit();
	m_vertexBuffer.Reset();
}

//========= 描画の補助関数=========
// 頂点Bufferへ、端点の一部を書き込み、その分を描画する。
bool DebugLineRenderer::DrawChunk(ID3D11DeviceContext* context,const std::vector<XMFLOAT3>& points,std::size_t startIndex,std::size_t vertexCount)
{
	// 頂点Buffer全体を書込用にMapする。DISCARDは古い内容を捨てて新しい領域を得るため、発行済みの描画へ影響しない。
	D3D11_MAPPED_SUBRESOURCE mapped_resource{};
	const HRESULT map_result = context->Map(m_vertexBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped_resource);
	// Map失敗ならAPI名・結果を記録し、falseを返す。Bufferは変更されていない。
	if(FAILED(map_result)){
		GraphicsErrorLog::Write(L"ID3D11DeviceContext::Map(DebugLineVertexBuffer)",map_result);
		return false;
	}

	// Map領域を頂点配列として扱い、今回の端点だけを書き込む。UVは使用しないため(0,0)にする。
	BASIC_COLOR_VERTEX* vertices = static_cast<BASIC_COLOR_VERTEX*>(mapped_resource.pData);
	// 今回の頂点数だけ、端点の位置を順にコピーする。
	for(std::size_t vertex_index = 0; vertex_index < vertexCount; ++vertex_index){
		vertices[vertex_index].position = points[startIndex + vertex_index];
		vertices[vertex_index].UV = XMFLOAT2{};
	}

	// 書込を終えてMapを解除し、GPUから読めるようにしてから、今回の頂点数を先頭から描画する。
	context->Unmap(m_vertexBuffer.Get(),0);
	context->Draw(static_cast<UINT>(vertexCount),0);
	return true;
}