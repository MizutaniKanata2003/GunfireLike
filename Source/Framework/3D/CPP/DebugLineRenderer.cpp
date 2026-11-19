#include "../H/DebugLineRenderer.h"

// 線分用の動的頂点Buffer・Shader・定数Bufferを生成し、描画ごとに端点を転送してLine Listで描画する。
// BasicColorのShaderを再利用する。描画Stateの復元や、色ごとの頂点色対応は行わない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで1回の描画へ渡す頂点数をBufferの容量以下に制限する。
#include <cstddef> // offsetofで頂点属性のOffsetをInput Layoutへ指定し、std::size_tで頂点数を扱う。
#include <string> // std::wstringでAPI名と失敗値を含むログ文字列を組み立てる。
#include <vector> // std::vector<char>でCSOのバイナリデータを連続領域へ保持する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/Etc/H/Logger.h"

// 線描画のShaderパス・頂点Buffer容量・入力属性名と、生成失敗ログをまとめる。
namespace
{
  //========= Shaderファイルパス定数=========
  // BasicColor描画に使用するShaderのCSOファイルパス。BasicMeshRendererと同じファイルを使う。
  constexpr char ns_BASIC_COLOR_VERTEX_SHADER_CSO_PATH[] = "Shaders/BasicColorVS.cso";
  constexpr char ns_BASIC_COLOR_PIXEL_SHADER_CSO_PATH[] = "Shaders/BasicColorPS.cso";

  //========= Input Layout定数=========
  // VERTEX Shader入力に使用する頂点属性のセマンティック名。BasicColorVSの入力と対応する。
  constexpr char ns_POSITION_SEMANTIC_NAME[] = "POSITION";
  constexpr char ns_TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";

  //========= 頂点Buffer容量=========
  // 頂点Bufferに格納できる最大頂点数。2点で1本の線分のため偶数にする。1024本の線分に相当する。
  // 超える端点は、複数回に分けて描画する。値を変更する場合も偶数を維持する。
  constexpr std::size_t ns_MAX_VERTEX_COUNT = 2048;

  //========= 補助関数=========
  // GraphicsカテゴリでDebugLine描画Resource生成・転送の失敗を出力する。
  // functionName: 失敗APIを示す有効な終端付き文字列。nullptr不可。
  // result: 16進数8桁で記録するHRESULT。後始末は呼出元が担当する。
  void WriteDebugLineGraphicsError(const wchar_t* functionName,HRESULT result)
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
// 動的頂点Buffer、Shader、Input Layout、定数Bufferを初期化する。
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
  vertex_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(VERTEX) * ns_MAX_VERTEX_COUNT);
  vertex_buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
  vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  vertex_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

  // 生成出力を空のメンバーComPtrで受け取る。
  const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_desc,nullptr,m_vertexBuffer.GetAddressOf());
  // Vertex Buffer生成失敗ならAPI名・結果を記録し、部分ResourceをUninitして戻る。
  if(FAILED(vertex_buffer_result)){
    WriteDebugLineGraphicsError(L"ID3D11Device::CreateBuffer(DebugLineVertexBuffer)",vertex_buffer_result);
    Uninit();
    return false;
  }

  // VERTEX ShaderのCSO読込、Shader生成、Input Layout生成を行う。
  std::vector<char> vertex_shader_binary{};
  // VSのCSO読込失敗では、生成済みのBufferを含めて部分解放しfalseを返す。
  if(!ShaderBinaryLoader::Load(ns_BASIC_COLOR_VERTEX_SHADER_CSO_PATH,vertex_shader_binary)){
    Uninit();
    return false;
  }

  // CSOの先頭とバイト長を使ってVSを生成する。ClassLinkageは指定しない。
  const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_binary.data(),vertex_shader_binary.size(),
                                                                  nullptr,m_vertexShader.GetAddressOf());
  // VS生成失敗ならAPI名・結果を記録し、先行して生成したBufferも解放する。
  if(FAILED(vertex_shader_result)){
    WriteDebugLineGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
    Uninit();
    return false;
  }

  // POSITION0=float3、TEXCOORD0=float2をSlot0の頂点単位データとして読む。offsetofはバイト位置。
  const D3D11_INPUT_ELEMENT_DESC input_elements[]
  {
    {ns_POSITION_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VERTEX,position),D3D11_INPUT_PER_VERTEX_DATA,0},
    {ns_TEXCOORD_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(VERTEX,UV),D3D11_INPUT_PER_VERTEX_DATA,0}
  };

  // CPU属性の解釈とVSの入力signatureを対応付ける。CSOはこの生成の終了まで有効に保持する。
  const HRESULT input_layout_result = device->CreateInputLayout(input_elements,ARRAYSIZE(input_elements),
                                                                vertex_shader_binary.data(),vertex_shader_binary.size(),m_inputLayout.GetAddressOf());
  // Layout生成失敗ならVSとBufferを含め所有Resourceを部分解放する。
  if(FAILED(input_layout_result)){
    WriteDebugLineGraphicsError(L"ID3D11Device::CreateInputLayout",input_layout_result);
    Uninit();
    return false;
  }

  // Pixel Shaderを読み込み、描画用のPixel Shaderを生成する。
  std::vector<char> pixel_shader_binary{};
  // PSのCSO読込失敗でも部分ResourceをUninitする。
  if(!ShaderBinaryLoader::Load(ns_BASIC_COLOR_PIXEL_SHADER_CSO_PATH,pixel_shader_binary)){
    Uninit();
    return false;
  }

  // BasicColorPSのCSOからPSを生成する。
  const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_binary.data(),pixel_shader_binary.size(),
                                                                nullptr,m_pixelShader.GetAddressOf());
  // PS生成失敗ならログ後に先行Resourceをまとめて解放し、falseを返す。
  if(FAILED(pixel_shader_result)){
    WriteDebugLineGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
    Uninit();
    return false;
  }

  // Camera、Object、Material用の定数バッファを生成する。生成時に初期値は転送せず、描画ごとにUpdateする。
  HRESULT camera_buffer_result{};
  // Camera Buffer生成失敗なら結果を記録し、Shader・頂点BufferもUninitする。
  if(!m_cameraBuffer.Init(device,camera_buffer_result)){
    WriteDebugLineGraphicsError(L"ID3D11Device::CreateBuffer(CAMERA_CONSTANTS)",camera_buffer_result);
    Uninit();
    return false;
  }

  // Object用の定数Bufferを生成する。
  HRESULT object_buffer_result{};
  // Object Buffer生成失敗なら先行Camera Bufferも含め部分解放する。
  if(!m_objectBuffer.Init(device,object_buffer_result)){
    WriteDebugLineGraphicsError(L"ID3D11Device::CreateBuffer(OBJECT_CONSTANTS)",object_buffer_result);
    Uninit();
    return false;
  }

  // Material用の定数Bufferを生成する。
  HRESULT material_buffer_result{};
  // Material Buffer生成失敗なら生成済みResourceを全てUninitして戻る。
  if(!m_materialBuffer.Init(device,material_buffer_result)){
    WriteDebugLineGraphicsError(L"ID3D11Device::CreateBuffer(MATERIAL_CONSTANTS)",material_buffer_result);
    Uninit();
    return false;
  }

  return true;
}

// 端点を2点ずつの線分として、Bufferの容量ごとに分けて描画する。
void DebugLineRenderer::DrawLines(GraphicsSystem& graphicsSystem,const std::vector<XMFLOAT3>& points,const XMFLOAT4& color,
                                  const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix)
{
  // 必須のBuffer・Shader・Layoutが揃わない、または線分が1本もできない端点数なら、転送・Bind・描画を省く。
  if(!m_vertexBuffer || !m_cameraBuffer.IsValid() || !m_objectBuffer.IsValid() || !m_materialBuffer.IsValid() ||
     !m_vertexShader || !m_pixelShader || !m_inputLayout || points.size() < 2)return;

  // 描画に使用するDirect3D Contextを取得する。
  ID3D11DeviceContext* context = graphicsSystem.GetContext();
  // Contextがなければ描画しない。GraphicsSystemは借用し、このRendererに参照を登録しない。
  if(context == nullptr)return;

  // View*Projectionの順で合成して転置する。HLSLの行列格納契約へ合わせ、BasicMeshRendererと同じ手順にする。
  const XMMATRIX view_projection_matrix = XMMatrixTranspose(viewMatrix * projectionMatrix);
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

  // Camera→Object→Materialの順でBuffer全体を転送する。更新だけではBindされない。
  m_cameraBuffer.Update(context,camera_constants);
  m_objectBuffer.Update(context,object_constants);
  m_materialBuffer.Update(context,material_constants);

  // Input Assemblerへ設定する頂点Buffer情報をまとめる。Strideは頂点1個のバイト間隔、Offset0はBuffer先頭。
  const UINT vertex_stride = sizeof(VERTEX);
  const UINT vertex_offset{};
  ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};

  // Shaderへ設定する定数Bufferと、単色経路のため画像なしを示すnullptrのSRVをまとめる。
  ID3D11Buffer* camera_buffers[]{m_cameraBuffer.Get()};
  ID3D11Buffer* object_buffers[]{m_objectBuffer.Get()};
  ID3D11Buffer* material_buffers[]{m_materialBuffer.Get()};
  ID3D11ShaderResourceView* texture_view{};

  // Slot0へ頂点1本、Layout、Line Listを接続する。2頂点で1本の線分となる。Index Bufferは使わない。
  context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
  context->IASetInputLayout(m_inputLayout.Get());
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

  // VS/PSと定数Bufferを上書きする。VS b0=Camera、VS/PS b1=Object、VS/PS b2=Material。PS t0は画像なしを明示する。
  context->VSSetShader(m_vertexShader.Get(),nullptr,0);
  context->PSSetShader(m_pixelShader.Get(),nullptr,0);
  context->VSSetConstantBuffers(0,1,camera_buffers);
  context->VSSetConstantBuffers(1,1,object_buffers);
  context->VSSetConstantBuffers(2,1,material_buffers);
  context->PSSetConstantBuffers(1,1,object_buffers);
  context->PSSetConstantBuffers(2,1,material_buffers);
  context->PSSetShaderResources(0,1,&texture_view);

  // 奇数個なら最後の1点は線分にならないため捨て、2点ずつ使える個数にそろえる。
  const std::size_t usable_count = points.size() - points.size() % 2;

  // 先頭から、Bufferの容量ごとに区切って、転送と描画を繰り返す。区切りは偶数のため、線分が途中で分かれない。
  for(std::size_t start_index = 0; start_index < usable_count; start_index += ns_MAX_VERTEX_COUNT){
    // 今回描画する頂点数。残りが容量より少なければ、残り全部。
    const std::size_t chunk_count = std::min(ns_MAX_VERTEX_COUNT,usable_count - start_index);

    // 頂点Buffer全体を書込用にMapする。DISCARDは古い内容を捨てて新しい領域を得るため、発行済みの描画へ影響しない。
    D3D11_MAPPED_SUBRESOURCE mapped_resource{};
    const HRESULT map_result = context->Map(m_vertexBuffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped_resource);
    // Map失敗ならAPI名・結果を記録し、以降の分割分も描画せず戻る。Bufferは変更されていない。
    if(FAILED(map_result)){
      WriteDebugLineGraphicsError(L"ID3D11DeviceContext::Map(DebugLineVertexBuffer)",map_result);
      return;
    }

    // Map領域を頂点配列として扱い、今回の端点だけを書き込む。UVは使用しないため(0,0)にする。
    VERTEX* vertices = static_cast<VERTEX*>(mapped_resource.pData);
    // 今回の頂点数だけ、端点の位置を順にコピーする。
    for(std::size_t vertex_index = 0; vertex_index < chunk_count; ++vertex_index){
      vertices[vertex_index].position = points[start_index + vertex_index];
      vertices[vertex_index].UV = XMFLOAT2{};
    }

    // 書込を終えてMapを解除し、GPUから読めるようにしてから、今回の頂点数を先頭から描画する。
    context->Unmap(m_vertexBuffer.Get(),0);
    context->Draw(static_cast<UINT>(chunk_count),0);
  }
}

// 描画に使用したDirect3Dリソースを解放する。
void DebugLineRenderer::Uninit()
{
  // 定数Bufferを解放する。Material→Object→Cameraの順。
  m_materialBuffer.Uninit();
  m_objectBuffer.Uninit();
  m_cameraBuffer.Uninit();

  // 頂点Buffer、Input Layout、Shaderの所有参照を解放する。Contextの別参照は残り得るためUnbindやGPU完了待機とは区別する。
  m_vertexBuffer.Reset();
  m_inputLayout.Reset();
  m_pixelShader.Reset();
  m_vertexShader.Reset();
}
