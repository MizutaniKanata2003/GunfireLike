#include "../H/HudSpriteRenderer.h"

// 単位Quad・Shader・定数Buffer・Samplerの生成と、画像の読込、Canvas座標からpixelへの変換、スプライトの描画を実装する。
// BasicColorのShaderを再利用する。描画Stateの復元や、画像の拡縮以外の加工は行わない。

//========= C++標準ライブラリ インクルード=========
#include <array> // 単位Quadの4頂点と6Indexを、固定要素数の連続配列に保持する。
#include <cstddef> // offsetofで頂点属性のOffsetをInput Layoutへ指定する。
#include <string> // std::wstringでAPI名と失敗値を含むログ文字列を組み立てる。
#include <vector> // std::vector<char>でCSOのバイナリデータを連続領域へ保持する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/DirectX/H/TextureLoader.h"
#include "Framework/Etc/H/Logger.h"

// Shaderのパス・入力属性名と、生成失敗ログをまとめる。
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

  //========= 補助関数=========
  // GraphicsカテゴリでHudSprite描画Resource生成の失敗を出力する。
  // functionName: 失敗APIを示す有効な終端付き文字列。nullptr不可。
  // result: 16進数8桁で記録するHRESULT。後始末は呼出元が担当する。
  void WriteHudSpriteGraphicsError(const wchar_t* functionName,HRESULT result)
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

//========= 初期化・読込関数=========
// 単位Quad、Shader、Input Layout、定数Buffer、Samplerを生成する。
bool HudSpriteRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
  // 旧所有参照を先に解放する。以後の生成失敗でも旧描画状態へは復帰しない。
  Uninit();

  // GPUリソースの生成に使用するDirect3D Deviceを取得する。
  ID3D11Device* device = graphicsSystem.GetDevice();
  // Device未取得なら生成せずfalseを返す。先頭のUninitにより旧Resourceは既に解放済み。
  if(device == nullptr)return false;

  // 左上から順に固定4頂点を作る。位置は0〜1の単位Quadで、UVも同じ値。大きさと位置はWorld行列が与える。
  const std::array<VERTEX,4> vertices
  {
    VERTEX{XMFLOAT3{0.0f,0.0f,0.0f},XMFLOAT2{0.0f,0.0f}},
    VERTEX{XMFLOAT3{1.0f,0.0f,0.0f},XMFLOAT2{1.0f,0.0f}},
    VERTEX{XMFLOAT3{1.0f,1.0f,0.0f},XMFLOAT2{1.0f,1.0f}},
    VERTEX{XMFLOAT3{0.0f,1.0f,0.0f},XMFLOAT2{0.0f,1.0f}}
  };

  // ByteWidthは総バイト数。IMMUTABLEなので生成時に初期データを渡し、以後は頂点Bufferを更新しない。
  D3D11_BUFFER_DESC vertex_buffer_desc{};
  vertex_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(VERTEX) * vertices.size());
  vertex_buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
  vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

  // CPU配列を読取元として渡し、所有は移さない。BufferなのでPitch類はゼロのまま。
  D3D11_SUBRESOURCE_DATA vertex_data{};
  vertex_data.pSysMem = vertices.data();
  const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_desc,&vertex_data,m_vertexBuffer.GetAddressOf());
  // Vertex Buffer生成失敗ならAPI名・結果を記録し、部分ResourceをUninitして戻る。
  if(FAILED(vertex_buffer_result)){
    WriteHudSpriteGraphicsError(L"ID3D11Device::CreateBuffer(HudSpriteVertexBuffer)",vertex_buffer_result);
    Uninit();
    return false;
  }

  // 4頂点を(0,1,2)と(0,2,3)の2三角形へ分割する16bit Indexを作り、不変Bufferを生成する。
  const std::array<unsigned short,6> indices{0,1,2,0,2,3};
  D3D11_BUFFER_DESC index_buffer_desc{};
  index_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(unsigned short) * indices.size());
  index_buffer_desc.Usage = D3D11_USAGE_IMMUTABLE;
  index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  D3D11_SUBRESOURCE_DATA index_data{};
  index_data.pSysMem = indices.data();
  const HRESULT index_buffer_result = device->CreateBuffer(&index_buffer_desc,&index_data,m_indexBuffer.GetAddressOf());
  // Index Buffer生成失敗なら、先に生成したVertex Bufferも含めて解放する。
  if(FAILED(index_buffer_result)){
    WriteHudSpriteGraphicsError(L"ID3D11Device::CreateBuffer(HudSpriteIndexBuffer)",index_buffer_result);
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
    WriteHudSpriteGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
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
    WriteHudSpriteGraphicsError(L"ID3D11Device::CreateInputLayout",input_layout_result);
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
    WriteHudSpriteGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
    Uninit();
    return false;
  }

  // Camera、Object、Material用の定数バッファを生成する。生成時に初期値は転送せず、描画ごとにUpdateする。
  HRESULT camera_buffer_result{};
  // Camera Buffer生成失敗なら結果を記録し、Shader・Bufferも解放する。
  if(!m_cameraBuffer.Init(device,camera_buffer_result)){
    WriteHudSpriteGraphicsError(L"ID3D11Device::CreateBuffer(CAMERA_CONSTANTS)",camera_buffer_result);
    Uninit();
    return false;
  }

  // Object用の定数Bufferを生成する。
  HRESULT object_buffer_result{};
  // Object Buffer生成失敗なら先行Camera Bufferも含め部分解放する。
  if(!m_objectBuffer.Init(device,object_buffer_result)){
    WriteHudSpriteGraphicsError(L"ID3D11Device::CreateBuffer(OBJECT_CONSTANTS)",object_buffer_result);
    Uninit();
    return false;
  }

  // Material用の定数Bufferを生成する。
  HRESULT material_buffer_result{};
  // Material Buffer生成失敗なら生成済みResourceを全てUninitして戻る。
  if(!m_materialBuffer.Init(device,material_buffer_result)){
    WriteHudSpriteGraphicsError(L"ID3D11Device::CreateBuffer(MATERIAL_CONSTANTS)",material_buffer_result);
    Uninit();
    return false;
  }

  // 画像の端を繰り返さないよう、Linear補間・CLAMPのSamplerを生成する。LODは0〜FLOAT32_MAXを許可し、実際のMipは画像Resourceに従う。
  D3D11_SAMPLER_DESC sampler_desc{};
  sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.MinLOD = 0.0f;
  sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
  const HRESULT sampler_result = device->CreateSamplerState(&sampler_desc,m_sampler.GetAddressOf());
  // Sampler生成失敗なら、生成済みの全Resourceを解放してfalseを返す。
  if(FAILED(sampler_result)){
    WriteHudSpriteGraphicsError(L"ID3D11Device::CreateSamplerState",sampler_result);
    Uninit();
    return false;
  }

  // 全生成後にだけIndex数を6へ確定する。失敗経路ではUninitにより0へ戻る。
  m_indexCount = static_cast<unsigned int>(indices.size());
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
void HudSpriteRenderer::DrawSprite(GraphicsSystem& graphicsSystem,HudTextureHandle texture,const XMFLOAT2& position,const XMFLOAT2& size,
                                   const XMFLOAT4& color,float visibleWidthRatio)
{
  // 描く大きさが0以下、または表示割合が0以下なら、何も描かない。
  if(size.x <= 0.0f || size.y <= 0.0f || visibleWidthRatio <= 0.0f)return;

  // 識別値が無効値、または範囲外なら、描く画像がないため何も描かない。
  if(texture == 0 || texture > m_textures.size())return;

  // 必須のBuffer・Shader・Layout・Sampler・画像が揃わなければ、転送・Bind・描画を省く。
  if(!m_vertexBuffer || !m_indexBuffer || !m_vertexShader || !m_pixelShader || !m_inputLayout || !m_sampler ||
     !m_cameraBuffer.IsValid() || !m_objectBuffer.IsValid() || !m_materialBuffer.IsValid() || m_indexCount == 0)return;

  // 描画に使用するDirect3D Contextと、現在の描画先のpixel寸法を取得する。
  ID3D11DeviceContext* context = graphicsSystem.GetContext();
  const unsigned int render_width = graphicsSystem.GetRenderWidth();
  const unsigned int render_height = graphicsSystem.GetRenderHeight();
  // Contextがない、または描画先の幅か高さが0なら、変換の除算も描画も行わず戻る。
  if(context == nullptr || render_width == 0 || render_height == 0)return;

  // Canvas座標の位置と大きさを、描画先のpixel座標へ変換する。位置には倍率と中央寄せのOffset、大きさには倍率だけを適用する。
  const HudCanvas canvas{render_width,render_height};
  const XMFLOAT2 screen_position = canvas.ToScreenPosition(position);
  const XMFLOAT2 screen_size = canvas.ToScreenSize(size);

  // 単位Quadを、表示割合で縮めた幅と高さに拡縮し、左上のpixel座標へ移動するWorld行列を作る。行ベクトル形式で、拡縮→移動の順に合成する。
  const XMMATRIX world_matrix = XMMatrixScaling(screen_size.x * visibleWidthRatio,screen_size.y,1.0f) *
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
  object_constants.color = color;

  // UV倍率のXに表示割合を入れ、Quadの横方向のUVを0〜割合へ縮める。縮めた幅と合わせて、画像が伸び縮みせず左側の一部分だけが表示される。
  // 画像使用値を1にして、BasicColorPSに画像の色とObject色の積を出力させる。
  MATERIAL_CONSTANTS material_constants{};
  material_constants.uvTiling = XMFLOAT2{visibleWidthRatio,1.0f};
  material_constants.useTexture = 1.0f;

  // Camera→Object→Materialの順でBuffer全体を転送する。更新だけではBindされない。
  m_cameraBuffer.Update(context,camera_constants);
  m_objectBuffer.Update(context,object_constants);
  m_materialBuffer.Update(context,material_constants);

  // Input Assemblerへ設定する頂点Buffer情報と、Shaderへ設定する定数Buffer・画像・Samplerをまとめる。
  const UINT vertex_stride = sizeof(VERTEX);
  const UINT vertex_offset{};
  ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};
  ID3D11Buffer* camera_buffers[]{m_cameraBuffer.Get()};
  ID3D11Buffer* object_buffers[]{m_objectBuffer.Get()};
  ID3D11Buffer* material_buffers[]{m_materialBuffer.Get()};
  ID3D11ShaderResourceView* texture_views[]{m_textures[texture - 1].Get()};
  ID3D11SamplerState* samplers[]{m_sampler.Get()};

  // Slot0へ頂点1本、16bit Index、Layout、Triangle Listを接続する。
  context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
  context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R16_UINT,0);
  context->IASetInputLayout(m_inputLayout.Get());
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // VS/PSと定数Buffer・画像・Samplerを上書きする。VS b0=Camera、VS/PS b1=Object、VS/PS b2=Material、PS t0=画像、PS s0=Sampler。
  context->VSSetShader(m_vertexShader.Get(),nullptr,0);
  context->PSSetShader(m_pixelShader.Get(),nullptr,0);
  context->VSSetConstantBuffers(0,1,camera_buffers);
  context->VSSetConstantBuffers(1,1,object_buffers);
  context->VSSetConstantBuffers(2,1,material_buffers);
  context->PSSetConstantBuffers(1,1,object_buffers);
  context->PSSetConstantBuffers(2,1,material_buffers);
  context->PSSetShaderResources(0,1,texture_views);
  context->PSSetSamplers(0,1,samplers);

  // 6Indexを先頭Index0・BaseVertex0で描く。IA・Shader・定数・SRV・Samplerを元へ戻さない。
  context->DrawIndexed(m_indexCount,0,0);
}

//========= 終了関数=========
// 画像・定数Buffer・Buffer・Shader・Samplerの所有参照を解放する。
void HudSpriteRenderer::Uninit()
{
  // 読み込んだ画像のSRVを全て解放し、識別値を全て無効にする。
  m_textures.clear();

  // Sampler・定数Buffer・Index Buffer・Vertex Bufferの所有参照を解放する。Contextの別参照は残り得るためUnbindやGPU完了待機とは区別する。
  m_sampler.Reset();
  m_materialBuffer.Uninit();
  m_objectBuffer.Uninit();
  m_cameraBuffer.Uninit();
  m_indexBuffer.Reset();
  m_vertexBuffer.Reset();

  // Input Layoutと、Shaderの所有参照を解放し、Index数を0へ戻す。
  m_inputLayout.Reset();
  m_pixelShader.Reset();
  m_vertexShader.Reset();
  m_indexCount = {};
}
