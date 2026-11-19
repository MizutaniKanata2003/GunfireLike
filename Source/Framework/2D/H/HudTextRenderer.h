#pragma once

// SpriteBatchとSpriteFontを所有し、Virtual Canvas上の文字描画とFont基準の寸法計測を提供する。
// Scene進行、描画先・Viewportの準備、Presentは担当しない。GraphicsSystemへの参照は非所有で保持する。

//========= C++標準ライブラリ インクルード=========
#include <memory> // std::unique_ptrでBatchとFontを所有し、Uninitまたは自身の破棄で解放する。
#include <string> // 描画・計測する終端付きワイド文字列をstd::wstring参照で受け取る。

//========= 外部ライブラリ インクルード=========
#include <SpriteBatch.h> // DirectXTKの2D描画受付・終了を行うBatch型を使用する。
#include <SpriteFont.h> // SpriteFont形式の読込、DrawString、MeasureStringを使用する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/2D/H/HudCanvas.h"

//========= 前方宣言=========
class GraphicsSystem;

// ImGuiに依存せず文字を描くRenderer。公開名HudTextRendererは既存参照との互換性のため維持する。
class HudTextRenderer final
{
public:
	//========= 初期化関数=========
	// Batch→Fontの順に生成し、両生成後だけサイズ取得用Systemを登録する。再初期化前はUninitを呼ぶこと。
	// graphicsSystem: Device・Contextと描画先サイズの取得元。登録後Uninitまで実体を有効に保つ。
	// 戻り値: DeviceかContextがなければfalse、両生成を完了すればtrue。生成・読込例外は捕捉しない。
	// Font生成で例外になれば、先に代入したBatchはメンバーへ残る。例外をfalse経路と同じ後始末とは扱わない。
	bool Initialize(GraphicsSystem& graphicsSystem);

	//========= 描画関数=========
	// Batchがあれば描画受付を開始する。正常に開始した後はEndを対応させ、多重Beginは行わない。
	// 開始状態を追跡する専用フラグはない。利用側がRender Target・Viewport等を準備してから使う。
	void Begin();
	// Begin/End間にCanvas位置・倍率を変換して文字描画を登録する。文字列参照は保持しない。
	// text: 描くワイド文字列。空なら何もしない。DrawStringへc_str()を渡し、埋込NULLの独自検査はしない。
	// position: 基準Canvasの位置。左上原点、右向きX・下向きY。内部で倍率と中央寄せOffsetを適用する。
	// color: RGBA色。外部型XMVECTORF32を維持し、値のclampや独自のAlpha加工は行わない。
	// scale: Font元サイズへ掛けるCanvas基準倍率。既定1は等倍指定で、描画時にさらにCanvas倍率が掛かる。
	// 必須Resource・登録Systemなし、空文字列、scale<=0、描画先寸法0では何もしない。Begin済みかは検証しない。
	void DrawText(const std::wstring& text,const XMFLOAT2& position,const XMVECTORF32& color,float scale = 1.0f);

	//========= 計測関数=========
	// Font計測値に引数Scaleだけを掛ける。DrawTextと違いCanvas倍率を含まないため、実画面のpixel寸法とは区別する。
	// text: 計測する文字列。空なら(0,0)。MeasureStringへc_str()を渡す。
	// scale: Font元寸法に掛ける倍率。0以下なら(0,0)、既定は1。追加の有限性検証は行わない。
	// 戻り値: Canvas基準の幅・高さ。Font未生成なら(0,0)。System登録や描画区間の開始を必要としない。
	[[nodiscard]] XMFLOAT2 MeasureText(const std::wstring& text,float scale = 1.0f) const;

	//========= 描画終了関数=========
	// BatchがあればEndを渡す。未開始Endを自動で防止する処理はないため、利用側でBegin/Endを対応させる。
	// このクラス独自のState保存・復元や、SwapChainのPresentは行わない。
	void End();

	//========= 終了関数=========
	// Font→Batchを解放し、最後に非所有Systemを解除する。Begin/Endの区間を終えてから呼ぶ。
	// 解放済みでも呼出可能。System本体は終了しない。Device/Contextは依存する描画Resourceの利用終了まで有効に保つ。
	void Uninit();

private:
	//========= 所有する文字描画Resource=========
	// Contextを使って構築するBatchを所有する。既定は空、Uninitまたは自身の破棄で解放する。
	std::unique_ptr<SpriteBatch> m_spriteBatch{};
	// 字形と描画用Textureを扱うFontを所有する。明示終了時はBatchより先に解放する。
	std::unique_ptr<SpriteFont> m_spriteFont{};

	//========= 非所有の描画基盤参照=========
	// 描画時に現在のRender Target寸法を取得する借用先。既定は未登録、成功後に登録し、Uninitで解除する。
	GraphicsSystem* m_graphicsSystem{};
};