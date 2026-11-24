#pragma once

// 単色Quad・照準・HPバー・低HP警告用のGPU Resourceを所有し、Canvas座標から実描画先へ変換して描画する。
// HP更新・Scene進行は担当しない。GraphicsSystemは各呼出中だけ借用し、参照を保持しない。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // Shader・Buffer・Input LayoutのCOM型と、描画APIの定義を使用する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/2D/H/HudCanvas.h"
#include "Framework/2D/H/HudRect.h"
#include "Framework/DirectX/H/ConstantBuffer.h"
#include "Framework/DirectX/H/VertexFormat.h"

//========= 前方宣言=========
class GraphicsSystem;

// 固定の単位Quadを、矩形・色の定数転送で配置するHUD Renderer。公開名HudRendererは既存参照との互換性のため維持する。
class HudRenderer final
{
public:
  //========= Getter関数=========
  // Positionのみの形式識別値。実際の入力はHUD専用のfloat2であり、この値だけでStrideは決まらない。
  [[nodiscard]] VERTEX_FORMAT GetVertexFormat() const { return VERTEX_FORMAT::e_POSITION; }

  //========= 初期化関数=========
  // CSOを読み、Shader・Layout・単位Quad Buffer・矩形用定数Bufferを生成する。再初期化前はUninitが必要。
  // graphicsSystem: 生成Deviceの取得先。Resource使用中は対応する描画基盤を利用側が有効に保つ。
  // 戻り値: 全生成成功ならtrue。Device未取得、検出した読込・生成失敗ならfalse。
  // Deviceなしでは既存状態を解放せず戻り、それ以外の検出失敗ではUninitで部分解放する。例外は捕捉しない。
  bool Initialize(GraphicsSystem& graphicsSystem);

  //========= 描画関数=========
  // Canvasの矩形を実pixelへ変換し、単色Quadを描画する。初期化済みResourceを同じ描画基盤で使うこと。
  // graphicsSystem: Context・描画先サイズの取得先とBlend設定先。非所有で借用する。
  // rectangle: Canvas基準の矩形。左上のX・Yは範囲制限を行わず、幅・高さのどちらかが0以下なら何もしない。
  // color: RGBA色。Alpha<1ならBlend有効、それ以外は無効。色のclamp・有限性検証は行わない。
  // Context・正の描画先サイズ・必要Resourceが揃わなければ何もしない。
  // IA・VS/PS・VS/PS b0・Blendを変更して元に戻さない。Render Target・Viewport・Depth・Cull等は呼出側で準備する。
  void DrawQuad(GraphicsSystem& graphicsSystem,const HUD_RECT& rectangle,const XMFLOAT4& color);
  // 基準画面中央へ横線・縦線の順に照準を描く。交点の重複描画も維持する。
  // graphicsSystem: 各DrawQuadで借用する描画基盤。Bind/Blendの副作用を引き継ぐ。
  void DrawCrosshair(GraphicsSystem& graphicsSystem);
  // 背景の後に現在値を描く。比率を0〜1に制限し、25%以下は警告側の色にする。HP自体は変更しない。
  // graphicsSystem: 各DrawQuadの描画基盤。元のStateへ復元しない。
  // currentHP: 表示する現在HP。最大HPとの比を幅の計算へ使う。
  // maxHP: 最大HP。0以下なら全体を描画しない。
  void DrawPlayerHealthBar(GraphicsSystem& graphicsSystem,float currentHP,float maxHP);
  // HP比率25%以下でCanvas四辺へ警告枠を描く。実描画先の余白外周へ自動拡張はしない。
  // graphicsSystem: 各DrawQuadの描画基盤。四隅の重なりとState変更を維持する。
  // currentHP: 現在HP。警告判定では比率をclampしない。
  // maxHP: 最大HP。0以下なら除算も描画も行わない。
  void DrawLowHealthWarning(GraphicsSystem& graphicsSystem,float currentHP,float maxHP);

  //========= 終了関数=========
  // 所有COM参照を解放しIndex数を0へ戻す。未初期化・部分初期化・解放済みでも呼出可能。
  // ContextのUnbind・State復元・GPU完了待機はしない。ほかの所有者が保持するCOM参照は別に残り得る。
  void Uninit();

private:
  //========= 構造体=========
  // HUD専用の2成分Position。3Dのfloat3位置とは異なる入力形式。
  struct VERTEX
  {
    // 左上(0,0)から右下(1,1)の単位Quad位置。pixelの位置と寸法はHudVSで適用する。
    XMFLOAT2 position{};
  };

  // HudVSのb0 HudBufferに対応する48バイトの配置。順序・型はHLSLと揃える。HudPSはこのBufferを直接参照しない。
  struct HUD_BUFFER
  {
    // 実pixel座標の左上X・Y・幅・高さ。Canvas変換済みの値を格納する。
    XMFLOAT4 rectangle{};
    // QuadのRGBA色。VSからPSへ頂点出力として渡す。
    XMFLOAT4 color{};
    // Render Target全体の幅・高さ。pixel→Clip座標の正規化に使用する。
    XMFLOAT2 renderSize{};
    // 48バイトの総サイズとHLSL末尾の配置を維持する余白。DrawQuadで0を転送する。
    XMFLOAT2 padding{};
  };

  // 総サイズだけをコンパイル時検証する。各Offset・Shader配置・Bind先の正しさをすべて保証するものではない。
  static_assert(sizeof(HUD_BUFFER) == 48,"HUD_BUFFERは48バイトである必要があります。");

  //========= 生成の補助関数=========
  // VS・PSのCSOを読み、VS・PS・Input Layoutを、この順に生成する。失敗はログへ記録する。
  // device: 生成に使うDevice。
  // 戻り値: 全て成功したらtrue。
  [[nodiscard]] bool CreateShaders(ID3D11Device* device);
  // 固定4頂点のVertex Bufferと、6要素のIndex Bufferを、この順に生成する。失敗はログへ記録する。
  // device: 生成に使うDevice。
  // 戻り値: 両方成功したらtrue。
  [[nodiscard]] bool CreateQuadBuffers(ID3D11Device* device);

  //========= Shader関連=========
  // 単位Quadへ矩形を適用し、Clip位置と色を出力するHudVSの所有参照。
  ComPtr<ID3D11VertexShader> m_vertexShader{};
  // 渡された色をそのまま返すHudPSの所有参照。
  ComPtr<ID3D11PixelShader> m_pixelShader{};
  // Slot0・Offset0のfloat2 POSITIONをVSへ対応付けるLayoutの所有参照。
  ComPtr<ID3D11InputLayout> m_inputLayout{};

  //========= Buffer関連=========
  // 固定4頂点のIMMUTABLE Bufferを所有する。各Quadで頂点の更新・再生成はしない。
  ComPtr<ID3D11Buffer> m_vertexBuffer{};
  // 2三角形・6要素の16bit Index Bufferを所有する。Draw時のFormatはR16_UINT。
  ComPtr<ID3D11Buffer> m_indexBuffer{};
  // 各Quadの可変情報を全体更新する定数Buffer。VS/PS b0へBindするが、現行Shaderで直接読むのはVS。
  ConstantBuffer<HUD_BUFFER> m_HUDBuffer{};

  //========= 描画情報=========
  // DrawIndexedへ渡すIndex数。既定0、全生成成功後6、Uninitで0へ戻す。
  unsigned int m_indexCount{};
};