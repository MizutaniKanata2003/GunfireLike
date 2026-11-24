#include "../H/HudCanvas.h"

// 描画先に収まる均一倍率と中央寄せOffsetを計算し、位置・寸法・文字倍率ごとの変換へ使う。
// 既存の定数、float変換、演算順を維持し、整数pixelへの丸めや範囲制限は追加しない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで横・縦の倍率の小さい方を選び、基準領域全体を描画先へ収める。

//========= 生成関数=========
// Render Target寸法から、変換に使う倍率とOffsetを一度計算する。
HudCanvas::HudCanvas(unsigned int renderWidth,unsigned int renderHeight)
{
  // 片方でも0なら比率計算へ進まず、メンバー初期値の倍率0・Offset(0,0)を維持する。
  if(renderWidth == 0 || renderHeight == 0)return;

  // 整数pixel寸法をfloatへ変換し、各軸を単独で基準サイズへ合わせた場合の倍率を求める。
  // 実幅/1280と実高さ/720を別々に算出し、次の共通倍率の選択に使う。
  const float render_width_float = static_cast<float>(renderWidth);
  const float render_height_float = static_cast<float>(renderHeight);
  const float width_scale = render_width_float / VIRTUAL_WIDTH;
  const float height_scale = render_height_float / VIRTUAL_HEIGHT;

  // 小さい倍率を両軸へ使い、片側がはみ出す拡縮と縦横別倍率による変形を避ける。
  // 拡大も縮小も同じ式で行い、等倍以下に制限する処理はない。
  m_scale = std::min(width_scale,height_scale);

  // 共通倍率を掛けたCanvasのpixel寸法を求める。この寸法はRender Target全体ではなく、配置される基準領域の大きさ。
  const float canvas_width = VIRTUAL_WIDTH * m_scale;
  const float canvas_height = VIRTUAL_HEIGHT * m_scale;

  // 実寸法との差が余白の合計。0.5倍して左右・上下へ均等に割り当て、Canvas左上の移動量とする。
  // 小数pixelもfloatのまま保持する。余白を描画・Clearしたり、Canvas外をClipしたりする処理ではない。
  m_offset.x = (render_width_float - canvas_width) * 0.5f;
  m_offset.y = (render_height_float - canvas_height) * 0.5f;
}

//========= 座標変換関数=========
// Canvas位置を描画先のpixel位置へ変換する。
XMFLOAT2 HudCanvas::ToScreenPosition(const XMFLOAT2& virtualPosition) const
{
  // 各座標へ倍率を掛け、その後に中央寄せOffsetを加える。原点は配置後Canvasの左上へ移る。
  // 範囲外の入力も計算する。倍率0時は有限の通常入力で(0,0)となるが、NaN等を0へ置換する安全策はない。
  return XMFLOAT2{m_offset.x + virtualPosition.x * m_scale,m_offset.y + virtualPosition.y * m_scale};
}

// Canvas寸法を描画先のpixel寸法へ変換する。
XMFLOAT2 HudCanvas::ToScreenSize(const XMFLOAT2& virtualSize) const
{
  // 長さには原点移動が影響しないため、位置用Offsetを加えず幅・高さへ同じ倍率だけを適用する。
  return XMFLOAT2{virtualSize.x * m_scale,virtualSize.y * m_scale};
}

// 基準の文字倍率を現在の描画先に対応する倍率へ変換する。
float HudCanvas::ToScreenScale(float virtualScale) const
{
  // 基準倍率とCanvasの共通倍率を乗算する。中央寄せ位置やFontの元サイズを加算する処理は行わない。
  return virtualScale * m_scale;
}