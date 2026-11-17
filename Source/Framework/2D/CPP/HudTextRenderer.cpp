#include "../H/HudTextRenderer.h"

// SpriteBatchとSpriteFontの生成・解放、Canvas変換を伴う文字描画、フォント基準の計測を実装する。
// 復元元: d65e80e61cbddaf9b076465163b7beb19bffad1c。処理は維持し、コメントと書式だけを現行規約へ合わせる。

//========= C++標準ライブラリ インクルード=========
#include <memory> // std::make_uniqueでBatchとFontを生成し、メンバーのunique_ptrへ所有を渡す。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

// HUD文字列描画に使用するAssetの固定パスをまとめる。
namespace
{
	//========= Assetパス定数=========
	// 読み込むSpriteFontファイル。相対パスは実行時の作業ディレクトリを基準とする。既存パスを維持する。
	constexpr const wchar_t* ns_HUD_FONT_PATH = L"Assets/Fonts/GunfireFont.spritefont";
}

//========= 初期化関数=========
// ContextでBatch、DeviceでFontを生成し、サイズ取得用のGraphicsSystemを登録する。
bool HudTextRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
	// 描画基盤から生成用ポインターを借用する。この関数の先頭では既存Resourceを自動解放しない。
	ID3D11Device* device = graphicsSystem.GetDevice();
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	// どちらかが未取得なら生成もSystem登録も行わずfalseを返す。保持済みのメンバーは変更しない。
	if(device == nullptr || context == nullptr)return false;

	// Batchを先に生成してからFontを読み込む。両者はメンバーのunique_ptrで所有する。
	// 生成・読込の例外は捕捉しない。Font生成で例外になれば、先に代入したBatchはメンバーへ残る。
	// 再初期化前は呼出側でUninitを済ませ、使用する描画基盤とResourceの対応を維持する。
	m_spriteBatch = std::make_unique<SpriteBatch>(context);
	m_spriteFont = std::make_unique<SpriteFont>(device,ns_HUD_FONT_PATH);

	// 両生成後にだけ非所有Systemを登録する。描画時にサイズを取得するため、Uninitまで参照先を有効に保つ。
	m_graphicsSystem = &graphicsSystem;
	return true;
}

//========= 描画関数=========
// Batchの文字描画受付を開始する。呼出側がBeginとEndの対応を管理する。
void HudTextRenderer::Begin()
{
	// Batchがある場合だけ開始する。多重Beginを防ぐ状態フラグや例外捕捉は、このクラスでは追加しない。
	if(m_spriteBatch)m_spriteBatch->Begin();
}

// 基準Canvas上の位置と文字倍率を現在の描画先へ変換し、Batchへ文字列を渡す。
void HudTextRenderer::DrawText(const std::wstring& text,const XMFLOAT2& position,const XMVECTORF32& color,float scale)
{
	// 必須Resource・Systemがない場合、空文字列、0以下の倍率では描画しない。Begin済みかの検証は行わない。
	if(!m_spriteBatch || !m_spriteFont || m_graphicsSystem == nullptr || text.empty() || scale <= 0.0f)return;

	// 保持するSystemから現在のRender Targetサイズをpixel単位で取得する。
	const unsigned int render_width = m_graphicsSystem->GetRenderWidth();
	const unsigned int render_height = m_graphicsSystem->GetRenderHeight();
	// 幅または高さが0ならCanvas変換と描画登録を省く。Viewport・Render Target自体の設定は呼出側が行う。
	if(render_width == 0 || render_height == 0)return;

	// 位置には均一倍率と中央寄せOffset、文字倍率にはCanvas倍率だけを反映する。
	// この関数は描画ごとにCanvasを作るため、保持サイズの変更が次のDrawTextに反映される。
	const HudCanvas HUD_canvas{render_width,render_height};
	const XMFLOAT2 screen_position = HUD_canvas.ToScreenPosition(position);
	const float screen_scale = HUD_canvas.ToScreenScale(scale);

	// 位置はRender Targetのpixel座標、回転は0ラジアン、文字の原点は(0,0)。色と倍率は既存どおり渡す。
	// 文字列への参照は保持せず、終端付きポインターを呼出中に渡す。Batch/Fontの所有はメンバーに残る。
	m_spriteFont->DrawString(m_spriteBatch.get(),text.c_str(),screen_position,color,0.0f,XMFLOAT2{},screen_scale);
}

//========= 計測関数=========
// Fontの元サイズへ引数倍率だけを掛けた幅・高さを返す。Canvas倍率はここでは適用しない。
XMFLOAT2 HudTextRenderer::MeasureText(const std::wstring& text,float scale) const
{
	// Font未生成、空文字列、0以下の倍率なら、計測せず両成分0を返す。
	if(!m_spriteFont || text.empty() || scale <= 0.0f)return {};

	// MeasureStringのX・Y成分を幅・高さとして取り出し、呼出側のScaleだけを反映する。
	// DrawText側でCanvas倍率を適用するため、ここで重ねて掛けない。戻り値は実画面のpixel寸法とは区別する。
	const XMVECTOR text_size = m_spriteFont->MeasureString(text.c_str());
	return XMFLOAT2{XMVectorGetX(text_size) * scale,XMVectorGetY(text_size) * scale};
}

//========= 描画終了関数=========
// Beginで開始したBatchを終了する。このクラスでは描画状態を保存・復元しない。
void HudTextRenderer::End()
{
	// Batchがある場合だけEndを渡す。未開始Endの抑止は実装せず、正しい呼出順は呼出側の前提とする。
	if(m_spriteBatch)m_spriteBatch->End();
}

//========= 終了関数=========
// Font、Batchの順に所有を解放し、最後に非所有Systemの登録を解除する。
void HudTextRenderer::Uninit()
{
	// 既存の解放順を維持する。Begin/End区間を終了させてから呼ぶ。GPU完了待機やContextの状態復元は行わない。
	m_spriteFont.reset();
	m_spriteBatch.reset();

	// System本体を破棄せず参照だけ解除し、以後のDrawTextを未登録として扱う。
	m_graphicsSystem = nullptr;
}