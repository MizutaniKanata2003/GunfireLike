#pragma once

//========= DirectX インクルード=========
#include <DirectXMath.h>

// 1280×720のVirtual Canvas座標を現在のRender Target座標へ変換する。
class HudCanvas final
{
public:
	//========= Canvas定数=========
	// HUDとScene UIが使用するVirtual Canvasの基準幅。
	static constexpr float VIRTUAL_WIDTH = 1280.0f;
	// HUDとScene UIが使用するVirtual Canvasの基準高さ。
	static constexpr float VIRTUAL_HEIGHT = 720.0f;

	//========= 生成関数=========
	// 指定したRender TargetサイズからCanvas Scaleと中央寄せOffsetを計算する。
	HudCanvas( unsigned int renderWidth, unsigned int renderHeight );

	//========= 座標変換関数=========
	// Virtual Canvas座標をRender Target座標へ変換する。
	[[nodiscard]] DirectX::XMFLOAT2 ToScreenPosition( const DirectX::XMFLOAT2& virtualPosition ) const;
	// Virtual CanvasサイズをRender Targetサイズへ変換する。
	[[nodiscard]] DirectX::XMFLOAT2 ToScreenSize( const DirectX::XMFLOAT2& virtualSize ) const;
	// Virtual Canvas基準の文字ScaleをRender Target基準のScaleへ変換する。
	[[nodiscard]] float ToScreenScale( float virtualScale ) const;

	//========= Getter関数=========
	// Virtual CanvasをRender Targetへ収める均一Scaleを返す。
	[[nodiscard]] float GetScale() const { return m_Scale; }
	// Render Target内でVirtual Canvasを中央寄せするOffsetを返す。
	[[nodiscard]] const DirectX::XMFLOAT2& GetOffset() const { return m_Offset; }
private:
	//========= Canvas変換状態=========
	// Virtual CanvasをRender Targetへ収める均一Scale。
	float m_Scale{};
	// Render Target内でVirtual Canvasを中央寄せするOffset。
	DirectX::XMFLOAT2 m_Offset{};
};
