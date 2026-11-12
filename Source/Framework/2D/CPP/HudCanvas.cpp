#include "../H/HudCanvas.h"

// Virtual Canvasの均一拡縮と中央寄せ、および各値の変換式を実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

// 指定したRender TargetサイズからCanvas Scaleと中央寄せOffsetを計算する。
HudCanvas::HudCanvas(unsigned int renderWidth,unsigned int renderHeight)
{
	// 最小化中などは倍率・Offsetを0に保ち、描画先がない状態の変換結果を0にする。
	if(renderWidth == 0 || renderHeight == 0)return;

	const float render_width_float = static_cast<float>(renderWidth);
	const float render_height_float = static_cast<float>(renderHeight);
	const float width_scale = render_width_float / VIRTUAL_WIDTH;
	const float height_scale = render_height_float / VIRTUAL_HEIGHT;

	// 小さい方の倍率を使い、縦横比を保ったまま基準領域全体を収める。
	m_scale = std::min(width_scale,height_scale);

	const float canvas_width = VIRTUAL_WIDTH * m_scale;
	const float canvas_height = VIRTUAL_HEIGHT * m_scale;

	// 拡縮後に残る領域を両側へ均等に割り当てる。
	m_offset.x = (render_width_float - canvas_width) * 0.5f;
	m_offset.y = (render_height_float - canvas_height) * 0.5f;
}

// Virtual Canvas座標をRender Target座標へ変換する。
XMFLOAT2 HudCanvas::ToScreenPosition(const XMFLOAT2& virtualPosition) const
{
	return XMFLOAT2{m_offset.x + virtualPosition.x * m_scale,m_offset.y + virtualPosition.y * m_scale};
}

// Virtual CanvasサイズをRender Targetサイズへ変換する。
XMFLOAT2 HudCanvas::ToScreenSize(const XMFLOAT2& virtualSize) const
{
	return XMFLOAT2{virtualSize.x * m_scale,virtualSize.y * m_scale};
}

// Virtual Canvas基準の文字ScaleをRender Target基準のScaleへ変換する。
float HudCanvas::ToScreenScale(float virtualScale) const
{
	return virtualScale * m_scale;
}