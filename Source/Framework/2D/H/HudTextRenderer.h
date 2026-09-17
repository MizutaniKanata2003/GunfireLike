#pragma once

//========= C++標準ライブラリ インクルード=========
#include <memory>
#include <string>

//========= DirectX インクルード=========
#include <DirectXMath.h>

//========= DirectXTK インクルード=========
#include <SpriteBatch.h>
#include <SpriteFont.h>

//========= 前方宣言=========
class GraphicsSystem;

// DirectXTKのSpriteBatchとSpriteFontを使い、ImGuiに依存しないHUD文字列を描画する。
class HudTextRenderer final
{
public:
	//========= 汎用関数=========
	// SpriteBatchとSpriteFontを生成し、HUD文字列描画を使用可能にする。
	bool Initialize( GraphicsSystem& graphicsSystem );
	// 文字列描画の開始処理を行う。
	void Begin();
	// 指定した文字列を画面座標へ描画する。
	void DrawText(
		const std::wstring& text,
		const DirectX::XMFLOAT2& position,
		const DirectX::XMVECTORF32& color,
		float scale = 1.0f );
	// 指定文字列を指定Scaleで描画した場合の幅と高さを返す。
	[[nodiscard]] DirectX::XMFLOAT2 MeasureText( const std::wstring& text, float scale = 1.0f ) const;
	// 文字列描画の終了処理を行う。
	void End();
	// HUD文字列描画で使用したリソースを解放する。
	void Uninit();

private:
	//========= メンバー変数=========
	// 2D文字列描画の描画バッチ。
	std::unique_ptr<DirectX::SpriteBatch> m_SpriteBatch{};
	// SpriteFont形式のフォント情報。
	std::unique_ptr<DirectX::SpriteFont> m_SpriteFont{};
};