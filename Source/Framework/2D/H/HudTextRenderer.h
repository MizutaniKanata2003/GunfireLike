#pragma once

// SpriteBatchとSpriteFontを所有し、Virtual Canvas座標での文字列描画と文字サイズの計測を提供する。
// Scene進行や描画先の設定は担当しない。描画サイズ取得用のGraphicsSystemは非所有で参照する。

//========= C++標準ライブラリ インクルード=========
#include <memory>
#include <string>

//========= 外部ライブラリ インクルード=========
#include <SpriteBatch.h>
#include <SpriteFont.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/2D/H/HudCanvas.h"

//========= 前方宣言=========
class GraphicsSystem;

// DirectXTKを使用して、ImGuiに依存しないHUD文字列を描画する。
class HudTextRenderer final
{
public:
	//========= 初期化関数=========
	// SpriteBatchとSpriteFontを生成し、描画サイズの取得先を登録する。
	// graphicsSystem: DeviceとContextを取得する描画基盤。Uninitまで有効な実体を渡す。
	// 戻り値: DeviceまたはContextが未取得ならfalse、両Resourceを生成できればtrue。
	// 注意: 生成時の例外は捕捉しない。再初期化する場合は先にUninitを呼んでおく。
	bool Initialize(GraphicsSystem& graphicsSystem);

	//========= 描画関数=========
	// 文字列描画の受付を開始する。SpriteBatch未生成なら何もしない。
	// 正常に開始した後はEndを1回呼ぶ。Beginの多重呼出は行わない。
	// Render Target・Viewport・適切なRender Passは呼出側で準備する。
	void Begin();
	// BeginからEndまでの間に、Virtual Canvas座標へ文字列を描画登録する。
	// text: 描画する文字列。空文字列なら何もしない。参照自体は保持しない。
	// position: Virtual Canvas上の左上座標。右向きX・下向きY。Canvasによる中央寄せを反映する。
	// color: DirectXMathのRGBA色。型名は既存APIと外部ライブラリの契約を維持する。
	// scale: Virtual Canvas基準の文字倍率。0以下なら描画しない。省略時は1。
	// Resource未生成・描画基盤未登録・描画先サイズ0の場合は何もしない。
	void DrawText(const std::wstring& text,const XMFLOAT2& position,const XMVECTORF32& color,float scale = 1.0f);

	//========= 計測関数=========
	// フォントから計測した幅と高さへ指定倍率を反映して返す。Canvas倍率は掛けない。
	// text: 計測する文字列。空文字列なら幅・高さとも0を返す。
	// scale: フォントの元サイズへ掛ける倍率。0以下なら幅・高さとも0。省略時は1。
	// 戻り値: Virtual Canvas基準の幅と高さ。Font未生成時は両成分0。
	[[nodiscard]] XMFLOAT2 MeasureText(const std::wstring& text,float scale = 1.0f) const;

	//========= 描画終了関数=========
	// Beginで開始した文字列描画を終了する。SpriteBatch未生成なら何もしない。
	// SpriteBatchが設定する描画状態は、このクラスでは元へ戻さない。
	void End();

	//========= 終了関数=========
	// Font、Batchを解放し、描画基盤への非所有ポインターを解除する。
	// BeginからEndまでの途中では呼ばない。解放済み状態で再度呼び出してもよい。
	void Uninit();

private:
	//========= 所有する文字描画Resource=========
	// 文字列をまとめて描画するBatchを所有する。Uninitまたは自身の破棄で解放する。
	std::unique_ptr<SpriteBatch> m_spriteBatch{};
	// 字形とTextureを使用するSpriteFontを所有する。Batchより先に解放する。
	std::unique_ptr<SpriteFont> m_spriteFont{};

	//========= 非所有の描画基盤参照=========
	// 現在のRender Targetサイズを取得する参照先。Initialize成功後からUninitまで有効であること。
	GraphicsSystem* m_graphicsSystem{};
};