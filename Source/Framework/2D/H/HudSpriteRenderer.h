#pragma once

// 画像（Texture）を矩形のスプライトとして、Virtual Canvas上の位置と大きさで描画するRenderer。画像を使うUIの描画に使う。
// BasicColorのShader・定数Bufferは、BasicColorPipelineが所有する。新しいShaderは追加しない。
// 描画先・Render Pass・Blendは呼出側が設定する（e_SCREEN_UIを想定）。GraphicsSystemは借用し、保持しない。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tで、読み込んだ画像を指す識別値を保持する。
#include <string_view> // 画像のパスを、呼出側の文字列を所有せずに受け取る。
#include <vector> // 読み込んだ画像のShader Resource Viewを、識別値の順に保持する。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // Buffer・Sampler・Shader Resource ViewのCOM型と、描画APIの定義を使用する。

//========= Framework インクルード=========
#include "Framework/2D/H/HudCanvas.h"
#include "Framework/2D/H/HudRect.h"
#include "Framework/3D/H/BasicColorPipeline.h"
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class GraphicsSystem;

//========= 画像の識別値=========
// 読み込んだ画像を指す識別値。1から順に割り当て、0は無効値。Uninitで全て無効になる。
using HudTextureHandle = std::uint32_t;

//========= スプライトの見た目=========
// スプライトの見た目の指定。DrawSpriteの引数の数を減らすため、色と、画像を描く横幅の割合を1つにまとめる。
struct HUD_SPRITE_APPEARANCE
{
	// 画像の色へ乗算するRGBA色。既定(1,1,1,1)で画像本来の色。Alphaは画像のAlphaと掛け合わせる。
	XMFLOAT4 color{1.0f,1.0f,1.0f,1.0f};
	// 画像の左端から、横方向に何割を描くか。0〜1。既定1で全体。0以下なら何も描かない。バーの表示割合を表すために使う。
	float visibleWidthRatio{1.0f};
};

// 画像を貼った矩形を、Canvas座標で描画するRenderer。
class HudSpriteRenderer final
{
public:
	//========= 初期化・読込関数=========
	// 旧ResourceをUninitした後、固定の単位Quad・BasicColorPipeline・Samplerを生成する。
	// graphicsSystem: 生成Deviceの取得元。Resourceと対応する描画基盤を渡す。参照は保持しない。
	// 戻り値: 全生成に成功すればtrue。検出した読込・生成の失敗では、部分Resourceを解放してfalse。
	// 旧状態へは復帰しない。下位処理の例外は捕捉しない。
	[[nodiscard]] bool Initialize(GraphicsSystem& graphicsSystem);
	// 画像ファイルを読み込み、描画に使う識別値を返す。
	// graphicsSystem: 生成Deviceの取得元。Initializeと同じ描画基盤を渡す。
	// filePath: 画像のパス。作業ディレクトリ基準。WICが読める形式（PNGなど）で、Alphaを含められる。
	// 戻り値: 読み込んだ画像の識別値。未初期化・Device未取得・読込失敗では0。読込失敗はTextureLoaderがログへ記録する。
	// COMの初期化は呼出側が済ませておくこと。Scene初期化中にだけ呼ぶ。
	[[nodiscard]] HudTextureHandle LoadTexture(GraphicsSystem& graphicsSystem,std::wstring_view filePath);

	//========= 描画関数=========
	// 画像を、Canvas座標の位置と大きさで描画する。画像の左側の一部分だけを描くことで、バーの表示割合も表せる。
	// graphicsSystem: Contextと描画先サイズの取得元。Render Target・Viewport・Blendは呼出側が設定する。
	// texture: LoadTextureが返した識別値。無効値や範囲外なら何もしない。
	// rectangle: 画像全体を描いた場合の、Canvas基準の左上の位置と、幅・高さ。幅・高さのどちらかが0以下なら何もしない。
	// appearance: 色と、画像を描く横幅の割合。描く矩形の幅は、rectangleの幅にこの割合を掛けた値になる。
	// 必須Resourceがない、またはContextがない、または描画先サイズが0なら何もしない。
	// Bind状態（Topology・Shader・Layout・定数Buffer・画像・Sampler）は描画後も元へ戻さない。
	void DrawSprite(GraphicsSystem& graphicsSystem,HudTextureHandle texture,const HUD_RECT& rectangle,const HUD_SPRITE_APPEARANCE& appearance);

	//========= 終了関数=========
	// 読み込んだ画像と、所有する全Resourceを解放する。識別値は全て無効になる。未初期化・終了済みでも呼出可能。
	// ContextのBind解除・状態復元・GPU完了待機は行わない。
	void Uninit();

private:
	//========= 生成・描画の補助関数=========
	// 固定4頂点のVertex Bufferと、6要素のIndex Bufferを、この順に生成する。失敗はログへ記録する。
	// device: 生成に使うDevice。
	// 戻り値: 両方成功したらtrue。
	[[nodiscard]] bool CreateQuadBuffers(ID3D11Device* device);
	// 画像の端を繰り返さない、Linear補間・CLAMPのSamplerを生成する。失敗はログへ記録する。
	// device: 生成に使うDevice。
	// 戻り値: 成功したらtrue。
	[[nodiscard]] bool CreateSampler(ID3D11Device* device);
	// 単位Quadと、画像・Samplerを接続して描画する。IA・Shader・定数・SRV・Samplerを、元へ戻さない。
	// context: 描画に使う即時Context。
	// texture: 描画する画像のSRV。範囲は呼出側が確認済みであること。
	void BindAndDraw(ID3D11DeviceContext* context,ID3D11ShaderResourceView* texture) const;

	//========= Pipeline関連=========
	// BasicColorのShader・Input Layout・定数Bufferを所有する。
	BasicColorPipeline m_pipeline{};

	//========= Buffer関連=========
	// 固定4頂点のIMMUTABLE Bufferの所有参照。描画ごとには更新しない。
	ComPtr<ID3D11Buffer> m_vertexBuffer{};
	// 2三角形・6要素の16bit Index Bufferの所有参照。
	ComPtr<ID3D11Buffer> m_indexBuffer{};

	//========= Texture関連=========
	// Linear補間・CLAMPのSamplerの所有参照。画像の端を繰り返さない。
	ComPtr<ID3D11SamplerState> m_sampler{};
	// 読み込んだ画像のSRV。識別値-1の位置に格納する（識別値1が先頭）。
	std::vector<ComPtr<ID3D11ShaderResourceView>> m_textures{};

	//========= 描画情報=========
	// DrawIndexedへ渡すIndex数。既定0、全生成成功後6、Uninitで0へ戻す。
	unsigned int m_indexCount{};
};