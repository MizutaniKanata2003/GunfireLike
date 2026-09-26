#include "../H/HudTextRenderer.h"

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

namespace
{
	//========= Assetパス定数=========
	// HUD文字列の描画に使用するSpriteFontファイル。
	constexpr const wchar_t* HUD_FONT_PATH = L"Assets/Fonts/GunfireFont.spritefont";
}

// SpriteBatchとSpriteFontを生成し、HUD文字列描画を使用可能にする。
bool HudTextRenderer::Initialize( GraphicsSystem& graphicsSystem )
{
	// SpriteBatchとSpriteFontの生成に必要なDirect3Dリソースを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	ID3D11DeviceContext* context = graphicsSystem.GetContext();

	if ( device == nullptr || context == nullptr ) return false;

	// HUD文字列描画に使用するSpriteBatchとSpriteFontを生成する。
	m_SpriteBatch = std::make_unique<DirectX::SpriteBatch>( context );
	m_SpriteFont = std::make_unique<DirectX::SpriteFont>( device, HUD_FONT_PATH );

	return true;
}

// 文字列描画の開始処理を行う。
void HudTextRenderer::Begin()
{
	if ( m_SpriteBatch ) m_SpriteBatch->Begin();
}

// 指定した文字列を画面座標へ描画する。
void HudTextRenderer::DrawText(
	const std::wstring& text,
	const DirectX::XMFLOAT2& position,
	const DirectX::XMVECTORF32& color,
	float scale )
{
	if ( !m_SpriteBatch || !m_SpriteFont || text.empty() || scale <= 0.0f ) return;

	m_SpriteFont->DrawString(
		m_SpriteBatch.get(),
		text.c_str(),
		position,
		color,
		0.0f,
		DirectX::XMFLOAT2{},
		scale );
}

// 指定文字列を指定Scaleで描画した場合の幅と高さを返す。
DirectX::XMFLOAT2 HudTextRenderer::MeasureText( const std::wstring& text, float scale ) const
{
	if ( !m_SpriteFont || text.empty() || scale <= 0.0f ) return {};

	// フォントの元サイズを取得して、描画Scaleを反映する。
	const DirectX::XMVECTOR textSize = m_SpriteFont->MeasureString( text.c_str() );

	return
	{
		DirectX::XMVectorGetX( textSize ) * scale,
		DirectX::XMVectorGetY( textSize ) * scale
	};
}

// 文字列描画の終了処理を行う。
void HudTextRenderer::End()
{
	if ( m_SpriteBatch ) m_SpriteBatch->End();
}

// HUD文字列描画で使用したリソースを解放する。
void HudTextRenderer::Uninit()
{
	m_SpriteFont.reset();
	m_SpriteBatch.reset();
}