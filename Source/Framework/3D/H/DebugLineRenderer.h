#pragma once

// 線分の組をDirect3D 11の線描画（Line List）で描画するRenderer。Collisionの形状など、デバッグ表示に使う。
// 既存のBasicColorのShader（BasicColorVS.cso・BasicColorPS.cso）を再利用する。Shaderは画像なし・Object色の単色経路を使う。
// 色は描画1回につき1色。Render Pass・Depth・Blendは呼出側が設定し、このクラスは変更しない。GraphicsSystemは借用し、保持しない。

//========= C++標準ライブラリ インクルード=========
#include <vector> // 線分の端点の列を、呼出側から読み取り専用で受け取る。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // Shader・Layout・Buffer・Contextの型と、Map・Draw・Bind用のAPIを使用する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/DirectX/H/ConstantBuffer.h"
#include "Framework/DirectX/H/GraphicsConstants.h"

//========= 前方宣言=========
class GraphicsSystem;

// 同じ色の線分をまとめて描画する。
class DebugLineRenderer final
{
public:
  //========= ライフサイクル・描画関数=========
  // 旧ResourceをUninitした後、頂点Buffer・Shader・Input Layout・定数Bufferを生成する。
  // graphicsSystem: 生成Deviceの取得元。Resourceと対応する描画基盤を渡す。参照は保持しない。
  // 戻り値: 全生成に成功すればtrue。検出した読込・生成の失敗では、部分Resourceを解放してfalse。
  // 旧状態へは復帰しない。下位処理の例外は捕捉しない。
  bool Initialize(GraphicsSystem& graphicsSystem);
  // 線分をまとめて描画する。2点ずつを1本の線分として扱う。
  // graphicsSystem: Contextの取得元。描画先・Viewport・Blend・Depth・Rasterizerは呼出側が設定する。
  // points: 線分の端点の列。[0]と[1]が1本目、[2]と[3]が2本目となる。ワールド座標。奇数個なら最後の1点は無視する。
  // color: 全線分に共通のRGBA色。値のclampはしない。
  // viewMatrix: World空間からView空間への変換行列。viewMatrix*projectionMatrixの順で合成する。
  // projectionMatrix: View空間からClip空間への変換行列。
  // 必須Resourceがない、またはContextがない、または端点が2点未満なら何もしない。端点が多い場合は、Bufferの容量ごとに分けて描画する。
  // Bind状態（Topology・Shader・Layout・定数Buffer）は描画後も元へ戻さない。
  void DrawLines(GraphicsSystem& graphicsSystem,const std::vector<XMFLOAT3>& points,const XMFLOAT4& color,const XMMATRIX& viewMatrix,
                 const XMMATRIX& projectionMatrix);
  // メンバーの所有参照を解放する。未初期化・部分初期化・終了済みでも呼出可能。
  // ContextのBind解除・状態復元・GPU完了待機は行わない。
  void Uninit();

private:
  //========= 構造体=========
  // 線描画用の1頂点。BasicColorVSの入力（位置とUV）と対応する。UVは使用しないが、入力の形を揃えるために持つ。
  struct VERTEX
  {
    // Local座標のfloat3。POSITION0をR32G32B32_FLOATで読む。ここではワールド座標をそのまま格納する。
    XMFLOAT3 position{};
    // float2のUV。TEXCOORD0をR32G32_FLOATで読む。常に(0,0)。
    XMFLOAT2 UV{};
  };

  //========= Shader関連=========
  // BasicColorVS.csoから生成するVSの所有参照。
  ComPtr<ID3D11VertexShader> m_vertexShader{};
  // BasicColorPS.csoから生成するPSの所有参照。Material b2の画像使用値が0のため、Object色を出力する。
  ComPtr<ID3D11PixelShader> m_pixelShader{};
  // VERTEXとVSの入力signatureを対応付けるInput Layoutの所有参照。
  ComPtr<ID3D11InputLayout> m_inputLayout{};

  //========= Buffer関連=========
  // 描画ごとにCPUから書き換える動的な頂点Bufferの所有参照。容量は最大頂点数分。
  ComPtr<ID3D11Buffer> m_vertexBuffer{};
  // 64バイトのCamera転送型。転置済みViewProjectionをVS b0へ設定する。
  ConstantBuffer<CAMERA_CONSTANTS> m_cameraBuffer{};
  // 80バイトのObject転送型。単位行列のWorldと線の色をVS/PS b1へ設定する。
  ConstantBuffer<OBJECT_CONSTANTS> m_objectBuffer{};
  // 32バイトのMaterial転送型。UV倍率(1,1)と画像使用値0をVS/PS b2へ設定する。
  ConstantBuffer<MATERIAL_CONSTANTS> m_materialBuffer{};
};
