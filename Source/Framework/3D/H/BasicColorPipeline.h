#pragma once

// BasicColorのShader（BasicColorVS.cso・BasicColorPS.cso）と、位置とUVの頂点のInput Layout、Camera・Object・Materialの3つの定数Bufferを所有する。
// 頂点Buffer・画像・Sampler・描画の呼出しは担当しない。描画するRenderer（DebugLineRenderer・BasicMeshRenderer）が、メンバーとして所有して使う。
// Bind先は、Camera=VS b0、Object=VS/PS b1、Material=VS/PS b2。描画先・Render Pass・Topologyは、呼出側が設定する。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // Shader・Layout・ContextのCOM型を使用する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/ConstantBuffer.h"
#include "Framework/DirectX/H/GraphicsConstants.h"
#include "Framework/Etc/H/MathTypes.h"

// BasicColorVSの入力と対応する、位置とUVの1頂点。BasicColorを使うRendererは、この型で頂点Bufferを作る。
struct BASIC_COLOR_VERTEX
{
  // Local座標のfloat3。POSITION0をR32G32B32_FLOATで読む。
  XMFLOAT3 position{};
  // float2のUV。TEXCOORD0をR32G32_FLOATで読む。外部略語UVを維持する。
  XMFLOAT2 UV{};
};

// BasicColorのShader・Input Layout・定数Bufferを所有し、Bindと定数の転送を行うクラス。
class BasicColorPipeline final
{
public:
  //========= ライフサイクル関数=========
  // 旧ResourceをUninitした後、VS・Input Layout・PS・3つの定数Bufferを、この順に生成する。
  // device: 生成に使うDeviceの借用ポインター。nullptrなら生成せずfalse。参照は保持しない。
  // 戻り値: 全生成に成功すればtrue。検出した読込・生成の失敗は、API名とHRESULTをログへ記録し、部分Resourceを解放してfalse。旧状態へは復帰しない。
  [[nodiscard]] bool Initialize(ID3D11Device* device);
  // メンバーの所有参照を解放する。未初期化・部分初期化・終了済みでも呼出可能。ContextのBind解除・GPU完了待機は行わない。
  void Uninit();

  //========= 描画関数=========
  // 描画に必要なResourceが全て揃っているかを返す。揃っていなければ、Bind・転送を呼んではならない。
  [[nodiscard]] bool IsValid() const;
  // 3つの定数Bufferへ、値をBuffer全体として転送する。更新だけではBindされない。
  // context: 更新に使う即時Context。生成に使ったDeviceと対応すること。
  // camera: 転置済みViewProjectionを持つ値。
  // object: 転置済みWorldと色を持つ値。
  // material: UV倍率と画像使用値を持つ値。
  void UpdateConstants(ID3D11DeviceContext* context,const CAMERA_CONSTANTS& camera,const OBJECT_CONSTANTS& object,const MATERIAL_CONSTANTS& material) const;
  // Input Layout・VS・PSを設定し、3つの定数Bufferを、各Stageのslotへ接続する。呼出前の設定へは復元しない。
  // 頂点Buffer・Topology・画像のSRV・Samplerは設定しない。
  // context: 設定先の即時Context。
  void Bind(ID3D11DeviceContext* context) const;

private:
  //========= 生成の補助関数=========
  // VS・Input Layout・PSを、CSOの読込から順に生成する。失敗はログへ記録する。
  // device: 生成に使うDevice。
  // 戻り値: 全て成功したらtrue。
  [[nodiscard]] bool CreateShaders(ID3D11Device* device);
  // Camera・Object・Materialの定数Bufferを、この順に生成する。失敗はログへ記録する。
  // device: 生成に使うDevice。
  // 戻り値: 全て成功したらtrue。
  [[nodiscard]] bool CreateConstantBuffers(ID3D11Device* device);

  //========= Shader関連=========
  // BasicColorVS.csoから生成するVSの所有参照。位置とUVを入力に使う。
  ComPtr<ID3D11VertexShader> m_vertexShader{};
  // BasicColorPS.csoから生成するPSの所有参照。Materialの画像使用値に従い、出力色を選ぶ。
  ComPtr<ID3D11PixelShader> m_pixelShader{};
  // BASIC_COLOR_VERTEXとVSの入力signatureを対応付けるInput Layoutの所有参照。
  ComPtr<ID3D11InputLayout> m_inputLayout{};

  //========= 定数Buffer関連=========
  // 64バイトのCamera転送型。転置済みViewProjectionをVS b0へ設定する。
  ConstantBuffer<CAMERA_CONSTANTS> m_cameraBuffer{};
  // 80バイトのObject転送型。転置済みWorldと色をVS/PS b1へ設定する。
  ConstantBuffer<OBJECT_CONSTANTS> m_objectBuffer{};
  // 32バイトのMaterial転送型。UV倍率と画像使用値をVS/PS b2へ設定する。
  ConstantBuffer<MATERIAL_CONSTANTS> m_materialBuffer{};
};
