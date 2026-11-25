#pragma once

// スキニング付きモデル（SKINNED_MODEL_DATA）を、GPUのBufferへ変換し、ボーン行列で変形して描くRenderer。
// 頂点Buffer・Index Buffer・ボーン行列の定数Buffer・スキニング用のVertex Shaderと、そのInput Layoutを所有する。
// Pixel Shader・Camera・Object・Materialの定数Bufferは、BasicColorPipelineを借りて共用する。BasicColorPipelineのVertex Shaderは使わず、Bindの後に、スキニング用のVertex Shaderへ上書きする。
// 画像は、まだ貼らない（単色で描く）。アニメーションの時刻・ボーン行列の計算は担当しない。描画先・Render Passは、呼出側が設定する。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // Buffer・Shader・LayoutのCOM型を使用する。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicColorPipeline.h"
#include "Framework/3D/H/CameraMatrices.h"
#include "Framework/3D/H/SkinnedAnimationPlayer.h" // ボーン行列の配列の型（BoneMatrixArray）を使用する。
#include "Framework/3D/H/SkinnedModelData.h"
#include "Framework/DirectX/H/ConstantBuffer.h"
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class GraphicsSystem;

// ボーン行列の転送型。スキニング用のVertex Shaderの b5 と対応する。行列は、転置した値を入れる。
struct BONE_CONSTANTS final
{
  // ボーンごとの最終の行列。添字は、ボーン番号。転置済み。{}は全成分0。
  XMFLOAT4X4 boneMatrices[SKINNED_MODEL::ns_MAX_BONE_COUNT]{};
};

// 1回の描画に使う、変換・ボーン行列・色の束。Drawの引数の数を減らす。
struct SKINNED_DRAW_PARAMS
{
  // モデルの空間から、ワールドへの変換行列。転置前の値。
  XMFLOAT4X4 worldMatrix{};
  // ボーンごとの最終の行列。SkinnedAnimationPlayerのEvaluateの出力。描画の呼出中だけ有効であればよい。nullptrなら描かない。
  const BoneMatrixArray* boneMatrices{};
  // 乗算するRGBA色。既定は白。
  XMFLOAT4 color{1.0f,1.0f,1.0f,1.0f};
};

// スキニング付きモデルを描くRenderer。
class SkinnedModelRenderer final
{
public:
  //========= ライフサイクル・描画関数=========
  // 旧Resourceを先にUninitし、モデルから、頂点Buffer・Index Buffer・スキニング用のShader・ボーンの定数Bufferを生成する。
  // graphicsSystem: Deviceの取得元。参照は保持しない。
  // model: 変換するモデル。頂点とIndexが、空でないこと。呼出中だけ有効であればよい（Bufferへ複製する）。
  // 戻り値: 全生成に成功すればtrue。失敗した場合は、部分Resourceを解放してfalse。
  [[nodiscard]] bool Initialize(GraphicsSystem& graphicsSystem,const SKINNED_MODEL_DATA& model);
  // メンバーの所有参照を解放する。未初期化・終了済みでも呼出可能。ContextのBind解除・GPU完了待機は行わない。
  void Uninit();
  // ボーン行列で変形したモデルを、単色で描く。変更したIA・Shader・定数Bufferは、元へ戻さない。
  // graphicsSystem: Contextの取得元。Render Target・Viewport・Blend・Depth・Rasterizerは、呼出側が設定する。
  // camera: View行列とProjection行列。
  // params: 変換・ボーン行列・色。ボーン行列がnullptrなら、何も描かない。
  void Draw(GraphicsSystem& graphicsSystem,const CAMERA_MATRICES& camera,const SKINNED_DRAW_PARAMS& params);

private:
  //========= 生成の補助関数=========
  // スキニング用のVertex Shaderと、頂点のInput Layoutを生成する。失敗はログへ記録する。
  // device: 生成に使うDevice。
  // 戻り値: 両方成功したらtrue。
  [[nodiscard]] bool CreateShader(ID3D11Device* device);
  // 頂点Bufferと、Index Bufferを、この順に生成する。失敗はログへ記録する。
  // device: 生成に使うDevice。
  // model: 初期データにするモデル。
  // 戻り値: 両方成功したらtrue。
  [[nodiscard]] bool CreateMeshBuffers(ID3D11Device* device,const SKINNED_MODEL_DATA& model);
  // 先頭の描画範囲の材質から、埋め込まれた拡散色の画像を読み、SRVとSamplerを生成する。画像がない材質は、成功とし、単色で描く。
  // device: 生成に使うDevice。
  // model: 材質と、画像のファイルの中身を持つモデル。
  // 戻り値: 成功、または、画像がなければtrue。画像があるのに展開できなければfalse。
  [[nodiscard]] bool CreateTexture(ID3D11Device* device,const SKINNED_MODEL_DATA& model);
  // BasicColorの設定を接続した後で、スキニング用のShaderと、ボーンの定数Bufferへ上書きして描画する。
  // context: 描画に使う即時Context。
  void BindAndDraw(ID3D11DeviceContext* context) const;

  //========= Pipeline関連=========
  // BasicColorのPixel Shader・定数Bufferを所有する。Vertex Shaderは使わない。
  BasicColorPipeline m_pipeline{};
  // スキニング用のVertex Shaderの所有参照。
  ComPtr<ID3D11VertexShader> m_vertexShader{};
  // SKINNED_VERTEXとVertex Shaderの入力を対応づけるInput Layoutの所有参照。
  ComPtr<ID3D11InputLayout> m_inputLayout{};

  //========= Buffer関連=========
  // スキニング付きの頂点を格納するDEFAULT Bufferを所有する。
  ComPtr<ID3D11Buffer> m_vertexBuffer{};
  // 三角形のIndexを格納するDEFAULT Bufferを所有する。unsigned intの要素幅。
  ComPtr<ID3D11Buffer> m_indexBuffer{};
  // ボーン行列の定数Buffer。Vertex Shaderのb5へ設定する。
  ConstantBuffer<BONE_CONSTANTS> m_boneBuffer{};

  //========= 画像関連=========
  // 拡散色の画像のSRVの所有参照。画像がなければ空で、色の指定だけで描く。
  ComPtr<ID3D11ShaderResourceView> m_textureView{};
  // 画像の参照用Samplerの所有参照。Linear補間・WRAP。
  ComPtr<ID3D11SamplerState> m_textureSampler{};

  //========= 描画情報=========
  // 初期0、生成完了後にIndexの数、Uninitで0。バイト数ではない。
  unsigned int m_indexCount{};
};
