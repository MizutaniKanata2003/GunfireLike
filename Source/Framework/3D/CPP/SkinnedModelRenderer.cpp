#include "../H/SkinnedModelRenderer.h"

// スキニング付きモデルの、GPU Bufferと材質ごとの画像の生成・ボーン行列の転送・Bind・描画範囲ごとの描画・解放を実装する。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // offsetofで、頂点の属性のOffsetを、Input Layoutへ指定する。
#include <cstdint> // std::uint32_tで、画像のない材質に使う、1画素の色を表す。
#include <string> // std::stringで、材質の画像のパスを扱い、std::wstringで、画像の名前を作る。
#include <unordered_map> // 画像のパスから、読み込み済みの材質の番号を引き、同じ画像を共有する。
#include <vector> // std::vector<char>で、CSOのバイナリデータを連続領域へ保持する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsErrorLog.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/MeshBuffers.h"
#include "Framework/DirectX/H/SamplerFactory.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/DirectX/H/TextureLoader.h"
#include "Framework/Etc/H/Logger.h"

// スキニング用のShaderパスと、定数Bufferのslotをまとめる。
namespace
{
  //========= Shaderファイルパス定数=========
  // スキニング用のVertex ShaderのCSOファイルパス。
  constexpr char ns_SKINNED_VERTEX_SHADER_CSO_PATH[] = "Shaders/SkinnedVS.cso";
  // スキニング用のPixel ShaderのCSOファイルパス。
  constexpr char ns_SKINNED_PIXEL_SHADER_CSO_PATH[] = "Shaders/SkinnedPS.cso";

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

  //========= Pixel Shader・光の設定=========
  // 光の定数BufferをBindするPixel Shaderのslot。ShaderCommon.hlslの、Light用の割当と同じ。
  constexpr UINT ns_LIGHT_CONSTANT_SLOT = 3;
  // 光の進む向き（正規化前）。奥（+Z）へ向かい、やや下へ。
  constexpr float ns_LIGHT_DIRECTION_X = 0.2f;
  constexpr float ns_LIGHT_DIRECTION_Y = -0.7f;
  constexpr float ns_LIGHT_DIRECTION_Z = 0.7f;
  // 平行光源の強さと、環境光の強さ。
  constexpr float ns_LIGHT_INTENSITY = 0.8f;
  constexpr float ns_AMBIENT_INTENSITY = 0.45f;

  //========= 画像のない材質=========
  // 画像のない材質に使う、1画素の画像の色。灰色で、不透明。メモリ上の並びは、R・G・B・Aの順（下位から）で、各8bit。画像が1枚でもあるモデルで、画像のない材質が、透明になって消えないようにする。
  constexpr std::uint32_t ns_MISSING_TEXTURE_PIXEL = 0xFF969696;

  // 光の定数を作る。向きは、正規化して保存する。
  LIGHT_CONSTANTS MakeLightConstants()
  {
    LIGHT_CONSTANTS light{};
    XMStoreFloat3(&light.direction,XMVector3Normalize(XMVectorSet(ns_LIGHT_DIRECTION_X,ns_LIGHT_DIRECTION_Y,ns_LIGHT_DIRECTION_Z,0.0f)));
    light.intensity = ns_LIGHT_INTENSITY;
    light.color = XMFLOAT3{1.0f,0.97f,0.92f};
    light.ambientIntensity = ns_AMBIENT_INTENSITY;
    return light;
  }

  // 1画素（灰色）の画像を作り、Shader Resource Viewを返す。画像のない材質に使う。
  // device: 生成に使うDevice。
  // outView: 出力先。失敗した場合は、空のまま。
  // 戻り値: 生成できたらtrue。失敗の理由は、ログへ記録する。
  bool CreateMissingTextureView(ID3D11Device* device,ComPtr<ID3D11ShaderResourceView>& outView)
  {
    D3D11_TEXTURE2D_DESC texture_desc{};
    texture_desc.Width = 1;
    texture_desc.Height = 1;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_IMMUTABLE;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    const std::uint32_t pixel = ns_MISSING_TEXTURE_PIXEL;
    const D3D11_SUBRESOURCE_DATA initial_data{&pixel,sizeof(pixel),0};
    ComPtr<ID3D11Texture2D> texture;
    const HRESULT texture_result = device->CreateTexture2D(&texture_desc,&initial_data,texture.GetAddressOf());
    if(FAILED(texture_result)){
      GraphicsErrorLog::Write(L"ID3D11Device::CreateTexture2D(MissingTexture)",texture_result);
      return false;
    }

    const HRESULT view_result = device->CreateShaderResourceView(texture.Get(),nullptr,outView.GetAddressOf());
    if(FAILED(view_result)){
      GraphicsErrorLog::Write(L"ID3D11Device::CreateShaderResourceView(MissingTexture)",view_result);
      return false;
    }
    return true;
  }
}

//========= ライフサイクル・描画関数=========
// 旧Resourceを解放した後、モデルから描画Resourceを生成する。
bool SkinnedModelRenderer::Initialize(GraphicsSystem& graphicsSystem,const SKINNED_MODEL_DATA& model)
{
  // 旧所有参照を先に解放する。以後の失敗でも旧状態へは復帰しない。
  Uninit();

  // Deviceがない場合と、頂点・Index・描画範囲が空の場合は、生成できない。
  ID3D11Device* device = graphicsSystem.GetDevice();
  if(device == nullptr)return false;
  if(model.vertices.empty() || model.indices.empty() || model.submeshes.empty()){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,L"SkinnedModelRendererの初期化に失敗しました。頂点・Index・描画範囲のどれかが空です。");
    return false;
  }

  // BasicColorの共用部分と、スキニング用のShader、Buffer、画像を、この順に生成する。最初に失敗した時点で、全て解放してfalseを返す。
  if(!m_pipeline.Initialize(device) || !CreateShader(device) || !CreateMeshBuffers(device,model) || !CreateMaterialTextures(device,model)){
    Uninit();
    return false;
  }

  // ボーンの定数Bufferを生成する。
  HRESULT bone_buffer_result{};
  if(!m_boneBuffer.Init(device,bone_buffer_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreateBuffer(BONE_CONSTANTS)",bone_buffer_result);
    Uninit();
    return false;
  }

  // 光の定数Bufferを生成する。
  HRESULT light_buffer_result{};
  if(!m_lightBuffer.Init(device,light_buffer_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreateBuffer(LIGHT_CONSTANTS)",light_buffer_result);
    Uninit();
    return false;
  }
  // 全生成後にだけ、描画範囲を確定する。
  m_submeshes = model.submeshes;
  return true;
}

// 所有参照を解放する。
void SkinnedModelRenderer::Uninit()
{
  m_lightBuffer.Uninit();
  m_pixelShader.Reset();
  m_submeshes.clear();
  m_materialViews.clear();
  m_hasTexture = false;
  m_textureSampler.Reset();
  m_boneBuffer.Uninit();
  m_indexBuffer.Reset();
  m_vertexBuffer.Reset();
  m_inputLayout.Reset();
  m_vertexShader.Reset();
  m_pipeline.Uninit();
}

// ボーン行列で変形したモデルを、材質ごとの画像で描く。
void SkinnedModelRenderer::Draw(GraphicsSystem& graphicsSystem,const CAMERA_MATRICES& camera,const SKINNED_DRAW_PARAMS& params)
{
  // 必須のResourceと、ボーン行列がなければ、描かない。
  if(params.boneMatrices == nullptr || !m_vertexBuffer || !m_indexBuffer || !m_vertexShader || !m_inputLayout
     || !m_boneBuffer.IsValid() || !m_pipeline.IsValid() || m_submeshes.empty() || !m_pixelShader || !m_lightBuffer.IsValid())return;
  ID3D11DeviceContext* context = graphicsSystem.GetContext();
  if(context == nullptr)return;

  // ボーン行列を、HLSLの行列の格納に合わせて、転置して転送する。
  BONE_CONSTANTS bone_constants{};
  for(std::size_t bone_index = 0; bone_index < params.boneMatrices->size(); ++bone_index){
    XMStoreFloat4x4(&bone_constants.boneMatrices[bone_index],XMMatrixTranspose(XMLoadFloat4x4(&(*params.boneMatrices)[bone_index])));
  }
  m_boneBuffer.Update(context,bone_constants);
  m_lightBuffer.Update(context,MakeLightConstants());

  // Camera・Object・Materialの定数を転送する。画像を持つ場合は、画像の色に、指定色を乗算する。
  const BASIC_COLOR_APPEARANCE appearance{params.color,XMFLOAT2{1.0f,1.0f},m_hasTexture};
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

  // スキニング用のPixel Shaderを読み込み、生成する。読込の失敗のログは、ShaderBinaryLoaderが記録する。
  std::vector<char> pixel_shader_binary{};
  if(!ShaderBinaryLoader::Load(ns_SKINNED_PIXEL_SHADER_CSO_PATH,pixel_shader_binary))return false;
  const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_binary.data(),pixel_shader_binary.size(),nullptr,m_pixelShader.GetAddressOf());
  if(FAILED(pixel_shader_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreatePixelShader(SkinnedPS)",pixel_shader_result);
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

// Samplerと、材質ごとの拡散色の画像（Mip付き）を生成する。
bool SkinnedModelRenderer::CreateMaterialTextures(ID3D11Device* device,const SKINNED_MODEL_DATA& model)
{
  // Samplerは、画像の有無にかかわらず、生成する。
  if(!SamplerFactory::CreateLinear(device,D3D11_TEXTURE_ADDRESS_WRAP,m_textureSampler))return false;

  // 材質ごとに、画像を読む。同じパスの材質は、読み込み済みの画像を共有する。画像がない材質は、空のままにする。
  std::unordered_map<std::string,std::size_t> loaded_materials{};
  m_materialViews.resize(model.materials.size());
  for(std::size_t material_index = 0; material_index < model.materials.size(); ++material_index){
    const SKINNED_MATERIAL& material = model.materials[material_index];
    if(material.diffuseImageData.empty())continue;

    const auto found = loaded_materials.find(material.diffuseTexturePath);
    if(found != loaded_materials.end()){
      m_materialViews[material_index] = m_materialViews[found->second];
      continue;
    }

    // 縮小表示のちらつきを防ぐため、Mipを生成して読む。
    const std::wstring image_name = L"SkinnedModel material " + std::to_wstring(material_index);
    const ENCODED_IMAGE_VIEW image{material.diffuseImageData.data(),material.diffuseImageData.size(),image_name,true};
    if(!TextureLoader::LoadWicTextureFromMemory(device,image,m_materialViews[material_index]))return false;
    loaded_materials[material.diffuseTexturePath] = material_index;
    m_hasTexture = true;
  }

  // 画像が1枚でもあるモデルでは、全ての描画範囲が、画像で色を決めるため、画像のない材質は、灰色の1画素の画像で描く。空のままだと、画像の色が透明の黒になり、その部分が、消えてしまう。
  if(!m_hasTexture)return true;

  ComPtr<ID3D11ShaderResourceView> missing_view;
  for(ComPtr<ID3D11ShaderResourceView>& view : m_materialViews){
    if(view)continue;

    // 最初に必要になった時に、1回だけ作り、以後は共有する。
    if(!missing_view && !CreateMissingTextureView(device,missing_view))return false;
    view = missing_view;
  }
  return true;
}

// BasicColorの設定を接続した後で、スキニング用のShaderとボーンの定数Bufferへ上書きし、描画範囲ごとに描画する。
void SkinnedModelRenderer::BindAndDraw(ID3D11DeviceContext* context) const
{
  // 頂点Bufferのstrideと、定数Bufferの借用ポインターを、設定用の配列へまとめる。
  const UINT vertex_stride = sizeof(SKINNED_VERTEX);
  const UINT vertex_offset{};
  ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};
  ID3D11Buffer* bone_buffers[]{m_boneBuffer.Get()};
  ID3D11SamplerState* samplers[]{m_textureSampler.Get()};

  // Slot0の頂点1本と、32bit Indexを接続する。
  context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
  context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R32_UINT,0);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // BasicColorのPixel Shaderと定数Bufferを接続した後、Input Layoutと、Vertex Shaderを、スキニング用に上書きし、ボーンの定数Bufferを接続する。
  m_pipeline.Bind(context);
  context->IASetInputLayout(m_inputLayout.Get());
  context->VSSetShader(m_vertexShader.Get(),nullptr,0);
  context->VSSetConstantBuffers(ns_BONE_CONSTANT_SLOT,1,bone_buffers);
  ID3D11Buffer* light_buffers[]{m_lightBuffer.Get()};
  context->PSSetShader(m_pixelShader.Get(),nullptr,0);
  context->PSSetConstantBuffers(ns_LIGHT_CONSTANT_SLOT,1,light_buffers);
  context->PSSetSamplers(0,1,samplers);

  // 描画範囲ごとに、材質の画像を設定して描く。画像がない材質は、nullptrを設定し、直前のSRVを残さない。
  for(const SKINNED_SUBMESH& submesh : m_submeshes){
    ID3D11ShaderResourceView* texture_views[]{submesh.materialIndex < m_materialViews.size() ? m_materialViews[submesh.materialIndex].Get() : nullptr};
    context->PSSetShaderResources(0,1,texture_views);
    context->DrawIndexed(submesh.indexCount,submesh.indexStart,0);
  }
}