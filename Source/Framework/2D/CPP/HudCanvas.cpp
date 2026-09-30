#include "../H/HudCanvas.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

// 指定したRender TargetサイズからCanvas Scaleと中央寄せOffsetを計算する。
HudCanvas::HudCanvas( unsigned int renderWidth, unsigned int renderHeight )
{
	if ( renderWidth == 0 || renderHeight == 0 ) return;

	const float renderWidthFloat = static_cast<float>( renderWidth );
	const float renderHeightFloat = static_cast<float>( renderHeight );
	const float widthScale = renderWidthFloat / VIRTUAL_WIDTH;
	const float heightScale = renderHeightFloat / VIRTUAL_HEIGHT;

	m_Scale = std::min( widthScale, heightScale );

	const float canvasWidth = VIRTUAL_WIDTH * m_Scale;
	const float canvasHeight = VIRTUAL_HEIGHT * m_Scale;

	m_Offset.x = ( renderWidthFloat - canvasWidth ) * 0.5f;
	m_Offset.y = ( renderHeightFloat - canvasHeight ) * 0.5f;
}

// Virtual Canvas座標をRender Target座標へ変換する。
DirectX::XMFLOAT2 HudCanvas::ToScreenPosition( const DirectX::XMFLOAT2& virtualPosition ) const
{
	return DirectX::XMFLOAT2 { m_Offset.x + virtualPosition.x * m_Scale,m_Offset.y + virtualPosition.y * m_Scale };
}

// Virtual CanvasサイズをRender Targetサイズへ変換する。
DirectX::XMFLOAT2 HudCanvas::ToScreenSize( const DirectX::XMFLOAT2& virtualSize ) const
{
	return DirectX::XMFLOAT2 { virtualSize.x * m_Scale,virtualSize.y * m_Scale };
}

// Virtual Canvas基準の文字ScaleをRender Target基準のScaleへ変換する。
float HudCanvas::ToScreenScale( float virtualScale ) const
{
	return virtualScale * m_Scale;
}