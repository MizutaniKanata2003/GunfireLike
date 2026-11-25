#include "../H/SkinnedModelRenderer.h"

// スキニング付きモデルの、GPU Bufferの生成・ボーン行列の転送・Bind・描画・解放を実装する。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // offsetofで、頂点の属性のOffsetを、Input Layoutへ指定する。
#include <vector> // std::vector<char>で、CSOのバイナリデータを連続領域へ保持する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsErrorLog.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/MeshBuffers.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/Etc/H/Logger.h"

// スキニング用のShaderパスと、定数Bufferのslotをまとめる。
namespace
{
  //========= Shaderファイルパス定数=========
  // スキニング用のVertex ShaderのCSOファイルパス。
  constexpr char ns_SKINNED_VERTEX_SHADER_CSO_PATH[] = "Shaders/SkinnedVS.cso";

  //========= 定数Bufferのslot=========
  // ボーン行列の定数BufferをBindするVertex Shaderのslot。b3・b4はLightとPostProcessの割当のため、b5を使う。
  constexpr UINT ns_BONE_CONSTANT_SLOT = 5;

  //========= Input Layout定数=========
  // 頂点属性のセマンティック名。SkinnedVS.hlslの入力と対応する。
  constexpr char ns_POSITION_SEMANTIC_NAME[] = "POSITION";
  constexpr char ns_NORMAL_SEMANTIC_NAME[] = "NORMAL";
  constexpr char ns_TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";
  constexpr char ns_BLEND_INDICES_SEMANTIC_NAME[] = "BLENDINDICES";
  constexpr char ns_BLEND_WEIGHT_SEMANTIC_NAME[] = "BLENDWEIGHT";
}

//========= ライフサイクル・描画関数=========
// 旧Resourceを解放した後、モデルから描画Resourceを生成する。
bool SkinnedModelRenderer::Initialize(GraphicsSystem& graphicsSystem,const SKINNED_MODEL_DATA& model)
{
  // 旧所有参照を先に解放する。以後の失敗でも旧状態へは復帰しない。
  Uninit();

  // Deviceがない場合と、頂点・Indexが空の場合は、生成できない。
  ID3D11Device* device = graphicsSystem.GetDevice();
  if(device == nullptr)return false;
  if(model.vertices.empty() || model.indices.empty()){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,L"SkinnedModelRendererの初期化に失敗しました。頂点またはIndexが空です。");
    return false;
  }

  // BasicColorの共用部分と、スキニング用のShader、Buffer、ボーンの定数Bufferを、この順に生成する。最初に失敗した時点で、全て解放してfalseを返す。
  HRESULT bone_buffer_result{};
  if(!m_pipeline.Initialize(device) || !CreateShader(device) || !CreateMeshBuffers(device,model)){
    Uninit();
    return false;
  }
  if(!m_boneBuffer.Init(device,bone_buffer_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreateBuffer(BONE_CONSTANTS)",bone_buffer_result);
    Uninit();
    return false;
  }

  // 全生成後にだけ、Indexの数を確定する。
  m_indexCount = static_cast<unsigned int>(model.indices.size());
  return true;
}

// 所有参照を解放する。
void SkinnedModelRenderer::Uninit()
{
  m_boneBuffer.Uninit();
  m_indexBuffer.Reset();
  m_vertexBuffer.Reset();
  m_inputLayout.Reset();
  m_vertexShader.Reset();
  m_pipeline.Uninit();
  m_indexCount = 0;
}

// ボーン行列で変形したモデルを、単色で描く。
void SkinnedModelRenderer::Draw(GraphicsSystem& graphicsSystem,const CAMERA_MATRICES& camera,const SKINNED_DRAW_PARAMS& params)
{
  // 必須のResourceと、ボーン行列がなければ、描かない。
  if(params.boneMatrices == nullptr || !m_vertexBuffer || !m_indexBuffer || !m_vertexShader || !m_inputLayout
     || !m_boneBuffer.IsValid() || !m_pipeline.IsValid() || m_indexCount == 0)return;
  ID3D11DeviceContext* context = graphicsSystem.GetContext();
  if(context == nullptr)return;

  // ボーン行列を、HLSLの行列の格納に合わせて、転置して転送する。
  BONE_CONSTANTS bone_constants{};
  for(std::size_t bone_index = 0; bone_index < params.boneMatrices->size(); ++bone_index){
    XMStoreFloat4x4(&bone_constants.boneMatrices[bone_index],XMMatrixTranspose(XMLoadFloat4x4(&(*params.boneMatrices)[bone_index])));
  }
  m_boneBuffer.Update(context,bone_constants);

  // Camera・Object・Materialの定数を転送する。画像は使わず、色だけで描く。
  const BASIC_COLOR_APPEARANCE appearance{params.color,XMFLOAT2{1.0f,1.0f},false};
  m_pipeline.UpdateModelConstants(context,XMLoadFloat4x4(&params.worldMatrix),camera,appearance);

  BindAndDraw(context);
}

//========= 生成の補助関数=========
// スキニング用のVertex Shaderと、頂点のInput Layoutを生成する。
bool SkinnedModelRenderer::CreateShader(ID3D11Device* device)
{
  // CSOを読み込み、Vertex Shaderを生成する。読込の失敗のログは、ShaderBinaryLoaderが記録する。
  std::vector<char> vertex_shader_binary{};
  if(!ShaderBinaryLoader::Load(ns_SKINNED_VERTEX_SHADER_CSO_PATH,vertex_shader_binary))return false;
  const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_binary.data(),vertex_shader_binary.size(),nullptr,m_vertexShader.GetAddressOf());
  if(FAILED(vertex_shader_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreateVertexShader(SkinnedVS)",vertex_shader_result);
    return false;
  }

  // 位置・法線・UV・ボーン番号（4つの8bit整数）・重み（4つのfloat）を、Slot0の頂点単位データとして読む。
  const D3D11_INPUT_ELEMENT_DESC input_elements[]
  {
    {ns_POSITION_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(SKINNED_VERTEX,position),D3D11_INPUT_PER_VERTEX_DATA,0},
    {ns_NORMAL_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(SKINNED_VERTEX,normal),D3D11_INPUT_PER_VERTEX_DATA,0},
    {ns_TEXCOORD_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(SKINNED_VERTEX,uv),D3D11_INPUT_PER_VERTEX_DATA,0},
    {ns_BLEND_INDICES_SEMANTIC_NAME,0,DXGI_FORMAT_R8G8B8A8_UINT,0,offsetof(SKINNED_VERTEX,boneIndices),D3D11_INPUT_PER_VERTEX_DATA,0},
    {ns_BLEND_WEIGHT_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(SKINNED_VERTEX,boneWeights),D3D11_INPUT_PER_VERTEX_DATA,0}
  };
  const HRESULT input_layout_result = device->CreateInputLayout(input_elements,ARRAYSIZE(input_elements),
                                                                vertex_shader_binary.data(),vertex_shader_binary.size(),m_inputLayout.GetAddressOf());
  if(FAILED(input_layout_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreateInputLayout(SKINNED_VERTEX)",input_layout_result);
    return false;
  }
  return true;
}

// 頂点Bufferと、Index Bufferを、この順に生成する。
bool SkinnedModelRenderer::CreateMeshBuffers(ID3D11Device* device,const SKINNED_MODEL_DATA& model)
{
  // 頂点Bufferの指定。DEFAULT・頂点用途。
  const MESH_BUFFER_SOURCE vertex_source{model.vertices.data(),sizeof(SKINNED_VERTEX) * model.vertices.size(),D3D11_USAGE_DEFAULT,
    D3D11_BIND_VERTEX_BUFFER,L"ID3D11Device::CreateBuffer(SkinnedVertexBuffer)"};

  // Index Bufferの指定。unsigned intの要素幅と、R32_UINTを対応させる。
  const MESH_BUFFER_SOURCE index_source{model.indices.data(),sizeof(unsigned int) * model.indices.size(),D3D11_USAGE_DEFAULT,
    D3D11_BIND_INDEX_BUFFER,L"ID3D11Device::CreateBuffer(SkinnedIndexBuffer)"};

  // 頂点→Indexの順に生成し、最初の失敗で止める。失敗のログは、MeshBuffersが記録する。
  return MeshBuffers::Create(device,vertex_source,m_vertexBuffer) && MeshBuffers::Create(device,index_source,m_indexBuffer);
}

// BasicColorの設定を接続した後で、スキニング用のShaderとボーンの定数Bufferへ上書きして描画する。
void SkinnedModelRenderer::BindAndDraw(ID3D11DeviceContext* context) const
{
  // 頂点Bufferのstrideと、定数Bufferの借用ポインターを、設定用の配列へまとめる。
  const UINT vertex_stride = sizeof(SKINNED_VERTEX);
  const UINT vertex_offset{};
  ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};
  ID3D11Buffer* bone_buffers[]{m_boneBuffer.Get()};
  ID3D11ShaderResourceView* empty_texture_views[]{nullptr};

  // Slot0の頂点1本と、32bit Indexを接続する。
  context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
  context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R32_UINT,0);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // BasicColorのPixel Shaderと定数Bufferを接続した後、Input Layoutと、Vertex Shaderを、スキニング用に上書きし、ボーンの定数Bufferを接続する。
  m_pipeline.Bind(context);
  context->IASetInputLayout(m_inputLayout.Get());
  context->VSSetShader(m_vertexShader.Get(),nullptr,0);
  context->VSSetConstantBuffers(ns_BONE_CONSTANT_SLOT,1,bone_buffers);

  // 画像は使わないため、直前のSRVを残さないよう、nullptrを明示する。
  context->PSSetShaderResources(0,1,empty_texture_views);
  context->DrawIndexed(m_indexCount,0,0);
}
