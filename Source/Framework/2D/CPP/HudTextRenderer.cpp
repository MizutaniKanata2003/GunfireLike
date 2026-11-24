#include "../H/HudTextRenderer.h"

// 誤置換前の文字描画実装を基準に、Batch/Font生成、Canvas変換、計測、明示終了を実装する。
// 復元元d65e80e61cbddaf9b076465163b7beb19bffad1cの処理を維持し、コメントと書式だけを整理する。

//========= C++標準ライブラリ インクルード=========
#include <memory> // std::make_uniqueでBatchとFontを生成し、メンバーunique_ptrへ所有を渡す。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

// HUD文字描画が読み込むFont Assetの固定パスをまとめる。
namespace
{
  //========= Assetパス定数=========
  // SpriteFont形式のファイル。作業ディレクトリ基準の相対パスで、実行時に別Fontへ自動切替はしない。
  constexpr const wchar_t* ns_HUD_FONT_PATH = L"Assets/Fonts/GunfireFont.spritefont";
}

//========= 初期化関数=========
// 描画基盤から借用したContextとDeviceを使い、文字描画Resourceを生成する。
bool HudTextRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
  // 生成に必要なDeviceとContextを取得する。関数先頭でUninitはせず、再初期化前の解放は利用側に求める。
  ID3D11Device* device = graphicsSystem.GetDevice();
  ID3D11DeviceContext* context = graphicsSystem.GetContext();
  // どちらかがなければメンバーを変更せずfalse。既存Resourceの解放やSystem登録を行わない。
  if(device == nullptr || context == nullptr)return false;

  // Batchを先に生成し、次にFontを読み込む。失敗時のC++例外は捕捉せず、先に代入済みのBatchはメンバーへ残る。
  m_spriteBatch = std::make_unique<SpriteBatch>(context);
  m_spriteFont = std::make_unique<SpriteFont>(device,ns_HUD_FONT_PATH);

  // 両生成後だけ非所有Systemを登録し成功を返す。以後のDrawTextが現在の描画サイズを取得する。
  m_graphicsSystem = &graphicsSystem;
  return true;
}

//========= 描画関数=========
// 利用側が準備した描画先に対し、文字描画のBatch受付を開始する。
void HudTextRenderer::Begin()
{
  // Batchがあれば開始する。多重Beginや未初期化Fontを別途検査する処理は追加しない。
  if(m_spriteBatch)m_spriteBatch->Begin();
}

// 文字列位置と倍率を実描画先へ変換し、FontからBatchへ描画を登録する。
void HudTextRenderer::DrawText(const std::wstring& text,const XMFLOAT2& position,const XMVECTORF32& color,float scale)
{
  // ResourceかSystemがない、本文が空、倍率が非正なら描画登録を省く。有限値・Begin済み状態の追加検証はしない。
  if(!m_spriteBatch || !m_spriteFont || m_graphicsSystem == nullptr || text.empty() || scale <= 0.0f)return;

  // 借用Systemから描画時のRender Target寸法をpixel単位で取得する。
  const unsigned int render_width = m_graphicsSystem->GetRenderWidth();
  const unsigned int render_height = m_graphicsSystem->GetRenderHeight();
  // 片方でも0ならCanvas変換もDrawStringも行わない。借用System自体の寿命は利用側が維持する。
  if(render_width == 0 || render_height == 0)return;

  // 描画ごとにCanvasを作る。位置は均一倍率とOffsetで実pixelへ変換し、文字倍率にはCanvas倍率だけを掛ける。
  const HudCanvas HUD_canvas{render_width,render_height};
  const XMFLOAT2 screen_position = HUD_canvas.ToScreenPosition(position);
  const float screen_scale = HUD_canvas.ToScreenScale(scale);

  // 回転0ラジアン・原点(0,0)の既存設定で描く。Font・Batchの所有はメンバーへ残し、文字列参照は保持しない。
  // このクラスは描画先やBlend等を保存・復元しない。Batchの受付終了は対応するEndで行う。
  m_spriteFont->DrawString(m_spriteBatch.get(),text.c_str(),screen_position,color,0.0f,XMFLOAT2{},screen_scale);
}

//========= 計測関数=========
// Fontの元寸法に指定倍率だけを掛け、Canvas上の配置計算に使う幅・高さを返す。
XMFLOAT2 HudTextRenderer::MeasureText(const std::wstring& text,float scale) const
{
  // Font未生成、空本文、非正倍率なら計測を行わず0サイズを返す。BatchやSystemの登録はこの判定に含めない。
  if(!m_spriteFont || text.empty() || scale <= 0.0f)return {};

  // SIMD計測値のX=幅・Y=高さを取り出す。DrawText側でCanvas変換するため、ここへCanvas倍率を二重適用しない。
  const XMVECTOR text_size = m_spriteFont->MeasureString(text.c_str());
  return XMFLOAT2{XMVectorGetX(text_size) * scale,XMVectorGetY(text_size) * scale};
}

//========= 描画終了関数=========
// Beginで開始したBatchを終了する。Begin/Endの正しい対応は利用側が管理する。
void HudTextRenderer::End()
{
  // BatchがあればEndを渡す。未開始Endの抑止や例外捕捉は追加しない。
  if(m_spriteBatch)m_spriteBatch->End();
}

//========= 終了関数=========
// 文字描画Resourceと非所有Systemを解除する。
void HudTextRenderer::Uninit()
{
  // Font→Batchの既存順で所有を解放する。描画受付区間の途中では呼ばず、GPU完了待機やContext復元も行わない。
  m_spriteFont.reset();
  m_spriteBatch.reset();

  // 借用先本体は破棄せず、以後のDrawTextが未登録として戻るようポインターを解除する。
  m_graphicsSystem = nullptr;
}