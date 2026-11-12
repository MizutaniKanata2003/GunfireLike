#include "../H/HudTextRenderer.h"

// FontとBatchの生成・解放、Canvas変換を伴う文字描画、フォント基準の計測を実装する。

//========= C++標準ライブラリ インクルード=========
#include <memory>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

// HUD文字列描画で読み込むAssetのパスをまとめる。
namespace
{
	//========= Assetパス定数=========
	// HUD文字列の描画に使用するSpriteFontファイル。
	constexpr const wchar_t* ns_HUD_FONT_PATH = L"Assets/Fonts/GunfireFont.spritefont";
}

// SpriteBatchとSpriteFontを生成し、HUD文字列描画を使用可能にする。
bool HudTextRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
	// SpriteBatchとSpriteFontの生成に必要なDirect3Dリソースを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	if(device == nullptr || context == nullptr)return false;

	// HUD文字列描画に使用するSpriteBatchとSpriteFontを生成する。
	m_spriteBatch = std::make_unique<SpriteBatch>(context);
	m_spriteFont = std::make_unique<SpriteFont>(device,ns_HUD_FONT_PATH);

	m_graphicsSystem = &graphicsSystem;

	return true;
}

// 文字列描画の開始処理を行う。
void HudTextRenderer::Begin()
{
	if(m_spriteBatch) m_spriteBatch->Begin();
}

// 指定したVirtual Canvas座標へ文字列を描画する。
void HudTextRenderer::DrawText(const std::wstring& text,const XMFLOAT2& position,const XMVECTORF32& color,float scale)
{
	if(!m_spriteBatch || !m_spriteFont || m_graphicsSystem == nullptr || text.empty() || scale <= 0.0f)return;

	const unsigned int render_width = m_graphicsSystem->GetRenderWidth();
	const unsigned int render_height = m_graphicsSystem->GetRenderHeight();
	if(render_width == 0 || render_height == 0)return;

	const HudCanvas HUD_canvas{render_width,render_height};
	const XMFLOAT2 screen_position = HUD_canvas.ToScreenPosition(position);
	const float screen_scale = HUD_canvas.ToScreenScale(scale);

	m_spriteFont->DrawString(m_spriteBatch.get(),text.c_str(),screen_position,color,0.0f,XMFLOAT2{},screen_scale);
}

// 指定文字列を指定Scaleで描画した場合の幅と高さを返す。
XMFLOAT2 HudTextRenderer::MeasureText(const std::wstring& text,float scale) const
{
	if(!m_spriteFont || text.empty() || scale <= 0.0f)return {};

	// DrawText側がCanvas倍率を適用するため、計測値には呼出側から渡された倍率だけを反映する。
	const XMVECTOR text_size = m_spriteFont->MeasureString(text.c_str());

	return XMFLOAT2{XMVectorGetX(text_size) * scale,XMVectorGetY(text_size) * scale};
}

// 文字列描画の終了処理を行う。
void HudTextRenderer::End()
{
	if(m_spriteBatch) m_spriteBatch->End();
}

// HUD文字列描画で使用したリソースを解放する。
void HudTextRenderer::Uninit()
{
	m_spriteFont.reset();
	m_spriteBatch.reset();
	m_graphicsSystem = nullptr;
}