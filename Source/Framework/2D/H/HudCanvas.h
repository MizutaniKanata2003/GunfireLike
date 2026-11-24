#pragma once

// 1280×720のVirtual CanvasをRender Targetへ収める、位置・サイズ・文字倍率の変換を提供する。
// 縦横共通の倍率と中央寄せOffsetを保持する。描画、Viewport設定、GPU Resource所有、余白の塗りつぶしは行わない。
// 作成時の描画先サイズで変換値を確定し、Resizeを自動追跡しない。新サイズでは利用側が作り直す。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// HUDの基準座標を、縦横比を保って実描画先のpixel座標へ変換する。
class HudCanvas final
{
public:
  //========= Canvas定数=========
  // 基準幅1280。座標・幅の基準単位であり、リサイズ後の実描画先のpixel幅を表す値ではない。
  // 公開名VIRTUAL_WIDTHは既存参照との互換性のため維持し、m_付き定数名への移行は今回行わない。
  static constexpr float VIRTUAL_WIDTH = 1280.0f;
  // 基準高さ720。幅と合わせた基準比率は16:9。具体値の採用経緯は未確認のため既存設定を維持する。
  // 公開名VIRTUAL_HEIGHTも維持する。AppConfigの起動サイズと自動連動する定数ではない。
  static constexpr float VIRTUAL_HEIGHT = 720.0f;

  //========= 生成関数=========
  // 基準領域全体が収まる均一倍率を計算し、残る余白を両側へ均等配置するOffsetを保持する。
  // renderWidth: Render Target全体の幅。単位はpixel。Viewportの部分領域を別に指定する引数ではない。
  // renderHeight: Render Target全体の高さ。単位はpixel。
  // どちらかが0なら倍率0・Offset(0,0)のまま戻る。例外や成功フラグによる通知、代替サイズの選択は行わない。
  HudCanvas(unsigned int renderWidth,unsigned int renderHeight);

  //========= 座標変換関数=========
  // 基準位置へ倍率とOffsetを反映し、描画先pixel座標を値で返す。Clip座標への変換は行わない。
  // virtualPosition: 左上原点、右向きX・下向きYのCanvas座標。範囲外や非有限値の検証・clampは行わない。
  [[nodiscard]] XMFLOAT2 ToScreenPosition(const XMFLOAT2& virtualPosition) const;
  // サイズへ倍率だけを掛け、描画先でのpixel寸法を返す。位置用Offsetは加えない。
  // virtualSize: Canvas基準の幅・高さ。負値もそのまま計算し、描画可否は利用側で判断する。
  [[nodiscard]] XMFLOAT2 ToScreenSize(const XMFLOAT2& virtualSize) const;
  // 文字等の基準倍率へCanvas倍率を掛ける。Fontサイズの計測や画像の生成は行わない。
  // virtualScale: 単位なしの基準文字倍率。0以下も検証せず計算する。引数は保持しない。
  [[nodiscard]] float ToScreenScale(float virtualScale) const;

  //========= Getter関数=========
  // 構築時に確定した均一倍率を返す。無効寸法なら0。以後の描画先Resizeで自動更新はされない。
  [[nodiscard]] float GetScale() const { return m_scale; }
  // 中央寄せOffsetをpixel単位の読み取り専用参照で返す。参照先はこの実体の寿命まで有効。
  [[nodiscard]] const XMFLOAT2& GetOffset() const { return m_offset; }

private:
  //========= Canvas変換状態=========
  // 縦横共通の単位なし倍率。既定0は無効寸法で構築した場合にも維持する値で、等倍を表す1ではない。
  float m_scale{};
  // 基準領域の左上を描画先中央へ配置する移動量。既定(0,0)、成分の単位はpixel。
  XMFLOAT2 m_offset{};
};