#pragma once

// 画像（Texture）を矩形のスプライトとして、Virtual Canvas上の位置と大きさで描画するRenderer。画像を使うUIの描画に使う。
// 既存のBasicColorのShader（BasicColorVS.cso・BasicColorPS.cso）を再利用し、新しいShaderは追加しない。
// 描画先・Render Pass・Blendは呼出側が設定する（e_SCREEN_UIを想定）。GraphicsSystemは借用し、保持しない。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tで、読み込んだ画像を指す識別値を保持する。
#include <string_view> // 画像のパスを、呼出側の文字列を所有せずに受け取る。
#include <vector> // 読み込んだ画像のShader Resource Viewを、識別値の順に保持する。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // Shader・Buffer・Sampler・Shader Resource ViewのCOM型と、描画APIの定義を使用する。

//========= Framework インクルード=========
#include "Framework/2D/H/HudCanvas.h"
#include "Framework/DirectX/H/ConstantBuffer.h"
#include "Framework/DirectX/H/GraphicsConstants.h"
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class GraphicsSystem;

//========= 画像の識別値=========
// 読み込んだ画像を指す識別値。1から順に割り当て、0は無効値。Uninitで全て無効になる。
using HudTextureHandle = std::uint32_t;

// 画像を貼った矩形を、Canvas座標で描画するRenderer。
class HudSpriteRenderer final
{
public:
  //========= 初期化・読込関数=========
  // 旧ResourceをUninitした後、固定の単位Quad・Shader・Input Layout・定数Buffer・Samplerを生成する。
  // graphicsSystem: 生成Deviceの取得元。Resourceと対応する描画基盤を渡す。参照は保持しない。
  // 戻り値: 全生成に成功すればtrue。検出した読込・生成の失敗では、部分Resourceを解放してfalse。
  // 旧状態へは復帰しない。下位処理の例外は捕捉しない。
  [[nodiscard]] bool Initialize(GraphicsSystem& graphicsSystem);
  // 画像ファイルを読み込み、描画に使う識別値を返す。
  // graphicsSystem: 生成Deviceの取得元。Initializeと同じ描画基盤を渡す。
  // filePath: 画像のパス。作業ディレクトリ基準。WICが読める形式（PNGなど）で、Alphaを含められる。
  // 戻り値: 読み込んだ画像の識別値。未初期化・Device未取得・読込失敗では0。読込失敗はTextureLoaderがログへ記録する。
  // COMの初期化は呼出側が済ませておくこと。Scene初期化中にだけ呼ぶ。
  [[nodiscard]] HudTextureHandle LoadTexture(GraphicsSystem& graphicsSystem,std::wstring_view filePath);

  //========= 描画関数=========
  // 画像を、Canvas座標の位置と大きさで描画する。画像の左側の一部分だけを描くことで、バーの表示割合も表せる。
  // graphicsSystem: Contextと描画先サイズの取得元。Render Target・Viewport・Blendは呼出側が設定する。
  // texture: LoadTextureが返した識別値。無効値や範囲外なら何もしない。
  // position: 矩形の左上のCanvas座標。右向きX・下向きY。
  // size: 画像全体を描いた場合の、Canvas基準の幅と高さ。どちらかが0以下なら何もしない。
  // color: 画像の色へ乗算するRGBA色。(1,1,1,1)で画像本来の色。Alphaは画像のAlphaと掛け合わせる。
  // visibleWidthRatio: 画像の左端から、横方向に何割を描くか。0〜1。1で全体。0以下なら何もしない。描く矩形の幅は、sizeの幅にこの割合を掛けた値になる。
  // 必須Resourceがない、またはContextがない、または描画先サイズが0なら何もしない。
  // Bind状態（Topology・Shader・Layout・定数Buffer・画像・Sampler）は描画後も元へ戻さない。
  void DrawSprite(GraphicsSystem& graphicsSystem,HudTextureHandle texture,const XMFLOAT2& position,const XMFLOAT2& size,
                  const XMFLOAT4& color,float visibleWidthRatio);

  //========= 終了関数=========
  // 読み込んだ画像と、所有する全Resourceを解放する。識別値は全て無効になる。未初期化・終了済みでも呼出可能。
  // ContextのBind解除・状態復元・GPU完了待機は行わない。
  void Uninit();

private:
  //========= 構造体=========
  // 単位Quadの1頂点。BasicColorVSの入力（位置とUV）と対応する。
  struct VERTEX
  {
    // 左上(0,0)から右下(1,1)の単位Quad上の位置。Zは0。大きさと位置はWorld行列で与える。
    XMFLOAT3 position{};
    // 画像のUV。左上(0,0)から右下(1,1)。位置のX・Yと同じ値。
    XMFLOAT2 UV{};
  };

  //========= Shader関連=========
  // BasicColorVS.csoから生成するVSの所有参照。
  ComPtr<ID3D11VertexShader> m_vertexShader{};
  // BasicColorPS.csoから生成するPSの所有参照。Material b2の画像使用値が1のため、画像の色とObject色の積を出力する。
  ComPtr<ID3D11PixelShader> m_pixelShader{};
  // VERTEXとVSの入力signatureを対応付けるInput Layoutの所有参照。
  ComPtr<ID3D11InputLayout> m_inputLayout{};

  //========= Buffer関連=========
  // 固定4頂点のIMMUTABLE Bufferの所有参照。描画ごとには更新しない。
  ComPtr<ID3D11Buffer> m_vertexBuffer{};
  // 2三角形・6要素の16bit Index Bufferの所有参照。
  ComPtr<ID3D11Buffer> m_indexBuffer{};
  // 64バイトのCamera転送型。Render Target上のpixel座標をClip座標へ変換する行列をVS b0へ設定する。
  ConstantBuffer<CAMERA_CONSTANTS> m_cameraBuffer{};
  // 80バイトのObject転送型。単位Quadをpixelの矩形へ変換するWorld行列と色をVS/PS b1へ設定する。
  ConstantBuffer<OBJECT_CONSTANTS> m_objectBuffer{};
  // 32バイトのMaterial転送型。画像の表示割合を表すUV倍率と画像使用値1をVS/PS b2へ設定する。
  ConstantBuffer<MATERIAL_CONSTANTS> m_materialBuffer{};

  //========= Texture関連=========
  // Linear補間・CLAMPのSamplerの所有参照。画像の端を繰り返さない。
  ComPtr<ID3D11SamplerState> m_sampler{};
  // 読み込んだ画像のSRV。識別値-1の位置に格納する（識別値1が先頭）。
  std::vector<ComPtr<ID3D11ShaderResourceView>> m_textures{};

  //========= 描画情報=========
  // DrawIndexedへ渡すIndex数。既定0、全生成成功後6、Uninitで0へ戻す。
  unsigned int m_indexCount{};
};
