#pragma once

// BackBufferの内側へ収まる最大の16:9の描画領域（位置と大きさ）を計算する。
// 3D・HUD・文字を描く領域の決定だけを担当する。Viewportの設定・黒帯の塗りつぶし・Resourceの所有は行わない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで、領域の幅と高さを1pixel以上にする。

// BackBufferの内側に収めた16:9の描画領域。左上のBackBuffer上の位置と大きさ。単位はpixel。
struct VIEWPORT_AREA
{
  // 領域の左端のX。
  unsigned int left{};
  // 領域の上端のY。
  unsigned int top{};
  // 領域の幅。計算結果は1以上。
  unsigned int width{};
  // 領域の高さ。計算結果は1以上。
  unsigned int height{};
};

// 16:9の描画領域を計算する。インスタンスは作らない。
class ViewportFit final
{
public:
  //========= 比率定数=========
  // 基準比率の横。16:9の16。
  static constexpr unsigned int m_ASPECT_WIDTH = 16;
  // 基準比率の縦。16:9の9。
  static constexpr unsigned int m_ASPECT_HEIGHT = 9;

  //========= 計算関数=========
  // BackBuffer全体へ収まる最大の16:9の領域を求め、余白を左右または上下へ均等に配る。端数は切り捨てる。
  // targetWidth: BackBufferの幅。単位はpixel。1以上であること。
  // targetHeight: BackBufferの高さ。単位はpixel。1以上であること。
  // 戻り値: 収まる最大の16:9の領域。幅と高さは1以上。
  [[nodiscard]] static VIEWPORT_AREA Calculate(unsigned int targetWidth,unsigned int targetHeight)
  {
    // 幅いっぱいに使った場合の高さを求める。幅 × 9 ÷ 16。
    const unsigned int height_by_width = targetWidth * m_ASPECT_HEIGHT / m_ASPECT_WIDTH;

    // 幅いっぱいで高さが収まるなら、幅を基準にする。収まらない（縦が足りない）なら、高さを基準にして、左右へ黒帯を作る。
    VIEWPORT_AREA area{};
    if(height_by_width <= targetHeight){
      area.width = targetWidth;
      area.height = std::max(1u,height_by_width);
    }
    else{
      area.height = targetHeight;
      area.width = std::max(1u,targetHeight * m_ASPECT_WIDTH / m_ASPECT_HEIGHT);
    }

    // 余白を半分ずつ、左右と上下へ配り、領域を中央へ置く。
    area.left = (targetWidth - area.width) / 2;
    area.top = (targetHeight - area.height) / 2;
    return area;
  }

private:
  //========= 生成禁止関数=========
  // ViewportFitのインスタンス生成を禁止する。
  ViewportFit() = delete;
};
