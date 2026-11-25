#pragma once

// スキニング付きモデル（SKINNED_MODEL_DATA）を、GPUのBufferへ変換し、ボーン行列で変形して描くRenderer。
// 頂点Buffer・Index Buffer・ボーン行列の定数Buffer・スキニング用のVertex Shaderと、そのInput Layoutを所有する。材質ごとの拡散色の画像（Mip付き）も所有し、描画範囲ごとに切り替えて描く。
// Pixel Shader・Camera・Object・Materialの定数Bufferは、BasicColorPipelineを借りて共用する。BasicColorPipelineのVertex Shaderは使わず、Bindの後に、スキニング用のVertex Shaderへ上書きする。
// アニメーションの時刻・ボーン行列の計算は担当しない。描画先・Render Passは、呼出側が設定する。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // Buffer・Shader・LayoutのCOM型を使用する。

//========= C++標準ライブラリ インクルード=========
#include <vector> // 描画範囲と、材質ごとの画像のViewを、可変長配列で保持する。

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
  // 旧Resourceを先にUninitし、モデルから、頂点Buffer・Index Buffer・スキニング用のShader・ボーンの定数Buffer・材質ごとの画像を生成する。
  // graphicsSystem: Deviceの取得元。参照は保持しない。
  // model: 変換するモデル。頂点・Index・描画範囲が、空でないこと。呼出中だけ有効であればよい（Bufferへ複製する）。
  // 戻り値: 全生成に成功すればtrue。失敗した場合は、部分Resourceを解放してfalse。
  [[nodiscard]] bool Initialize(GraphicsSystem& graphicsSystem,const SKINNED_MODEL_DATA& model);
  // メンバーの所有参照を解放する。未初期化・終了済みでも呼出可能。ContextのBind解除・GPU完了待機は行わない。
  void Uninit();
  // ボーン行列で変形したモデルを、材質ごとの画像（なければ色）で描く。変更したIA・Shader・定数Bufferは、元へ戻さない。
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
  // Samplerと、材質ごとの拡散色の画像（Mip付き）を生成する。同じパスの材質は、同じ画像を共有する。画像がない材質は、画像なしとして、成功とする。
  // device: 生成に使うDevice。
  // model: 材質と、画像のファイルの中身を持つモデル。
  // 戻り値: 成功、または、画像がなければtrue。画像があるのに展開できなければfalse。
  [[nodiscard]] bool CreateMaterialTextures(ID3D11Device* device,const SKINNED_MODEL_DATA& model);
  // BasicColorの設定を接続した後で、スキニング用のShaderと、ボーンの定数Bufferへ上書きし、描画範囲ごとに、画像を切り替えて描画する。
  // context: 描画に使う即時Context。
  void BindAndDraw(ID3D11DeviceContext* context) const;

  //========= Pipeline関連=========
  // BasicColorのPixel Shader・定数Bufferを所有する。Vertex Shaderは使わない。
  BasicColorPipeline m_pipeline{};
  // スキニング用のVertex Shaderの所有参照。
  ComPtr<ID3D11VertexShader> m_vertexShader{};
  // スキニング用のPixel Shaderの所有参照。拡散色に、光を掛ける。
  ComPtr<ID3D11PixelShader> m_pixelShader{};
  // SKINNED_VERTEXとVertex Shaderの入力を対応づけるInput Layoutの所有参照。
  ComPtr<ID3D11InputLayout> m_inputLayout{};

  //========= Buffer関連=========
  // スキニング付きの頂点を格納するDEFAULT Bufferを所有する。
  ComPtr<ID3D11Buffer> m_vertexBuffer{};
  // 三角形のIndexを格納するDEFAULT Bufferを所有する。unsigned intの要素幅。
  ComPtr<ID3D11Buffer> m_indexBuffer{};
  // ボーン行列の定数Buffer。Vertex Shaderのb5へ設定する。
  ConstantBuffer<BONE_CONSTANTS> m_boneBuffer{};
  // 光の定数Buffer。Pixel Shaderのb3へ設定する。
  ConstantBuffer<LIGHT_CONSTANTS> m_lightBuffer{};

  //========= 画像関連=========
  // 材質ごとの拡散色の画像のView。添字は、材質の番号。画像がない材質は空。同じ画像の材質は、同じTextureを共有する。
  std::vector<ComPtr<ID3D11ShaderResourceView>> m_materialViews{};
  // 画像の参照用Samplerの所有参照。Linear補間・WRAP。Mipを使う。
  ComPtr<ID3D11SamplerState> m_textureSampler{};
  // 1枚でも画像を持つか。falseなら、画像を使わず、色だけで描く。
  bool m_hasTexture{};

  //========= 描画情報=========
  // 画像ごとのIndexの範囲。モデルの描画範囲の複製。空なら、描かない。
  std::vector<SKINNED_SUBMESH> m_submeshes{};
};