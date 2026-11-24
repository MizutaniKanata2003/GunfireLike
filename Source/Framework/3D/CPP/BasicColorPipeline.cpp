#include "../H/BasicColorPipeline.h"

// BasicColorのShader・Input Layout・定数Bufferの生成・解放と、Bind・定数の転送を実装する。
// 既存のShaderパス・Input Layoutの属性・定数Bufferのslot・失敗時のログのAPI名を維持する。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // offsetofで頂点属性のOffsetをInput Layoutへ指定する。
#include <vector> // std::vector<char>でCSOのバイナリデータを連続領域へ保持する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsErrorLog.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"

// BasicColorのShaderパスと、入力属性名をまとめる。
namespace
{
  //========= Shaderファイルパス定数=========
  // BasicColor描画に使用するShaderのCSOファイルパス。
  constexpr char ns_BASIC_COLOR_VERTEX_SHADER_CSO_PATH[] = "Shaders/BasicColorVS.cso";
  constexpr char ns_BASIC_COLOR_PIXEL_SHADER_CSO_PATH[] = "Shaders/BasicColorPS.cso";

  //========= Input Layout定数=========
  // VERTEX Shader入力に使用する頂点属性のセマンティック名。BasicColorVSの入力と対応する。
  constexpr char ns_POSITION_SEMANTIC_NAME[] = "POSITION";
  constexpr char ns_TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";
}

//========= ライフサイクル関数=========
// VS・Input Layout・PS・3つの定数Bufferを生成する。
bool BasicColorPipeline::Initialize(ID3D11Device* device)
{
  // 旧所有参照を先に解放する。以後の生成失敗でも旧描画状態へは復帰しない。
  Uninit();

  // Device未取得なら生成せずfalseを返す。先頭のUninitにより旧Resourceは既に解放済み。
  if(device == nullptr)return false;

  // Shaderと定数Bufferを、この順に生成する。最初に失敗した時点で、部分Resourceを解放してfalseを返す。
  if(!CreateShaders(device) || !CreateConstantBuffers(device)){
    Uninit();
    return false;
  }
  return true;
}

// 所有参照を解放する。
void BasicColorPipeline::Uninit()
{
  // 定数Bufferを解放する。Material→Object→Cameraの順。
  m_materialBuffer.Uninit();
  m_objectBuffer.Uninit();
  m_cameraBuffer.Uninit();

  // Input Layout、Shaderの所有参照を解放する。Contextの別参照は残り得るためUnbindやGPU完了待機とは区別する。
  m_inputLayout.Reset();
  m_pixelShader.Reset();
  m_vertexShader.Reset();
}

//========= 描画関数=========
// 描画に必要なResourceが全て揃っているかを返す。
bool BasicColorPipeline::IsValid() const
{
  return m_vertexShader && m_pixelShader && m_inputLayout && m_cameraBuffer.IsValid() && m_objectBuffer.IsValid() && m_materialBuffer.IsValid();
}

// 3つの定数Bufferへ、値をBuffer全体として転送する。
void BasicColorPipeline::UpdateConstants(ID3D11DeviceContext* context,const CAMERA_CONSTANTS& camera,const OBJECT_CONSTANTS& object,
                                         const MATERIAL_CONSTANTS& material) const
{
  // Camera→Object→Materialの順でBuffer全体を転送する。更新だけではBindされない。
  m_cameraBuffer.Update(context,camera);
  m_objectBuffer.Update(context,object);
  m_materialBuffer.Update(context,material);
}

// World行列・カメラの行列・見た目から、3つの定数を組み立てて転送する。
void BasicColorPipeline::UpdateModelConstants(ID3D11DeviceContext* context,const XMMATRIX& worldMatrix,const CAMERA_MATRICES& camera,
                                              const BASIC_COLOR_APPEARANCE& appearance) const
{
  // View*Projectionの順で合成し、Worldと共に転置する。HLSLへの格納契約を合わせる処理で、座標系変更ではない。
  const XMMATRIX view_projection_matrix = XMMatrixTranspose(camera.view * camera.projection);
  const XMMATRIX transposed_world_matrix = XMMatrixTranspose(worldMatrix);

  // 転置済みのWorld→Clip行列を、VS b0の配置へ保存する。
  CAMERA_CONSTANTS camera_constants{};
  XMStoreFloat4x4(&camera_constants.viewProjectionMatrix,view_projection_matrix);

  // 転置済みのLocal→World行列とRGBA色を、b1の配置へ保存する。色のclampやAlpha操作は追加しない。
  OBJECT_CONSTANTS object_constants{};
  XMStoreFloat4x4(&object_constants.worldMatrix,transposed_world_matrix);
  object_constants.color = appearance.color;

  // UV倍率と、画像使用値（使う場合は1、使わない場合は0）を保存する。baseColorとpaddingは、既定値のまま残して、CPU/HLSL間の配置を保つ。
  MATERIAL_CONSTANTS material_constants{};
  material_constants.uvTiling = appearance.uvTiling;
  material_constants.useTexture = appearance.useTexture ? 1.0f : 0.0f;

  // 3つの定数Bufferへ転送する。更新だけではBindされない。
  UpdateConstants(context,camera_constants,object_constants,material_constants);
}

// Input Layout・VS・PSを設定し、3つの定数Bufferを各Stageのslotへ接続する。
void BasicColorPipeline::Bind(ID3D11DeviceContext* context) const
{
  // 定数Bufferの借用ポインターを、設定用の配列へまとめる。
  ID3D11Buffer* camera_buffers[]{m_cameraBuffer.Get()};
  ID3D11Buffer* object_buffers[]{m_objectBuffer.Get()};
  ID3D11Buffer* material_buffers[]{m_materialBuffer.Get()};

  // Layoutと、VS/PSを上書きする。
  context->IASetInputLayout(m_inputLayout.Get());
  context->VSSetShader(m_vertexShader.Get(),nullptr,0);
  context->PSSetShader(m_pixelShader.Get(),nullptr,0);

  // 定数Bufferを上書きする。VS b0=Camera、VS/PS b1=Object、VS/PS b2=Material。
  context->VSSetConstantBuffers(0,1,camera_buffers);
  context->VSSetConstantBuffers(1,1,object_buffers);
  context->VSSetConstantBuffers(2,1,material_buffers);
  context->PSSetConstantBuffers(1,1,object_buffers);
  context->PSSetConstantBuffers(2,1,material_buffers);
}

//========= 生成の補助関数=========
// VS・Input Layout・PSを、CSOの読込から順に生成する。
bool BasicColorPipeline::CreateShaders(ID3D11Device* device)
{
  // VERTEX ShaderのCSO読込、Shader生成、Input Layout生成を行う。
  std::vector<char> vertex_shader_binary{};
  // VSのCSO読込失敗では、falseを返す。読込の失敗のログは、ShaderBinaryLoaderが記録する。
  if(!ShaderBinaryLoader::Load(ns_BASIC_COLOR_VERTEX_SHADER_CSO_PATH,vertex_shader_binary))return false;

  // CSOの先頭とバイト長を使ってVSを生成する。ClassLinkageは指定しない。
  const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_binary.data(),vertex_shader_binary.size(),
                                                                  nullptr,m_vertexShader.GetAddressOf());
  // VS生成失敗ならAPI名・結果を記録して失敗を返す。
  if(FAILED(vertex_shader_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
    return false;
  }

  // POSITION0=float3、TEXCOORD0=float2をSlot0の頂点単位データとして読む。offsetofはバイト位置。
  const D3D11_INPUT_ELEMENT_DESC input_elements[]
  {
    {ns_POSITION_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(BASIC_COLOR_VERTEX,position),D3D11_INPUT_PER_VERTEX_DATA,0},
    {ns_TEXCOORD_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(BASIC_COLOR_VERTEX,UV),D3D11_INPUT_PER_VERTEX_DATA,0}
  };

  // CPU属性の解釈とVSの入力signatureを対応付ける。CSOはこの生成の終了まで有効に保持する。
  const HRESULT input_layout_result = device->CreateInputLayout(input_elements,ARRAYSIZE(input_elements),
                                                                vertex_shader_binary.data(),vertex_shader_binary.size(),m_inputLayout.GetAddressOf());
  // Layout生成失敗ならAPI名・結果を記録して失敗を返す。
  if(FAILED(input_layout_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreateInputLayout",input_layout_result);
    return false;
  }

  // Pixel Shaderを読み込み、描画用のPixel Shaderを生成する。
  std::vector<char> pixel_shader_binary{};
  // PSのCSO読込失敗では、falseを返す。読込の失敗のログは、ShaderBinaryLoaderが記録する。
  if(!ShaderBinaryLoader::Load(ns_BASIC_COLOR_PIXEL_SHADER_CSO_PATH,pixel_shader_binary))return false;

  // BasicColorPSのCSOからPSを生成する。
  const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_binary.data(),pixel_shader_binary.size(),
                                                                nullptr,m_pixelShader.GetAddressOf());
  // PS生成失敗ならAPI名・結果を記録して失敗を返す。
  if(FAILED(pixel_shader_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
    return false;
  }
  return true;
}

// Camera・Object・Materialの定数Bufferを、この順に生成する。
bool BasicColorPipeline::CreateConstantBuffers(ID3D11Device* device)
{
  // Camera、Object、Material用の定数バッファを生成する。生成時に初期値は転送せず、描画ごとにUpdateする。
  HRESULT camera_buffer_result{};
  // Camera Buffer生成失敗なら結果を記録して失敗を返す。
  if(!m_cameraBuffer.Init(device,camera_buffer_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreateBuffer(CAMERA_CONSTANTS)",camera_buffer_result);
    return false;
  }

  // Object用の定数Bufferを生成する。
  HRESULT object_buffer_result{};
  // Object Buffer生成失敗なら結果を記録して失敗を返す。
  if(!m_objectBuffer.Init(device,object_buffer_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreateBuffer(OBJECT_CONSTANTS)",object_buffer_result);
    return false;
  }

  // Material用の定数Bufferを生成する。
  HRESULT material_buffer_result{};
  // Material Buffer生成失敗なら結果を記録して失敗を返す。
  if(!m_materialBuffer.Init(device,material_buffer_result)){
    GraphicsErrorLog::Write(L"ID3D11Device::CreateBuffer(MATERIAL_CONSTANTS)",material_buffer_result);
    return false;
  }
  return true;
}