#pragma once

// Direct3D 11の基盤Resource、描画先、描画Stateを所有し、フレーム処理とResizeを提供する。
// SceneやMeshの生成・描画命令は呼出側が担当する。Windowは所有しない。

//========= Windows インクルード=========
#include <windows.h>

//========= 外部ライブラリ インクルード=========
#include <d3d11.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 列挙型=========
// 描画対象ごとのBlend・Depth・Rasterizer設定を選ぶ。既存の公開型名を互換性のため維持する。
enum class e_RenderPass
{
	e_SKY,         // DepthとCullを無効にしたSky用。Alpha Blendは使用しない。
	e_OPAQUE,      // 既定Depthと背面Cullを使う不透明3D用。
	e_TRANSPARENT, // Depth書込を止め、Alpha Blendと背面Cullを使う半透明3D用。
	e_SCREEN_UI    // DepthとCullを無効にし、Alpha Blendを使う画面UI用。
};

// Device・Context・SwapChainと描画先Resourceのライフサイクルを管理する。
class GraphicsSystem final
{
public:
	//========= ライフサイクル・フレーム制御関数=========
	// 既存Resourceを終了後、基盤と描画先を生成してOPAQUE Passへ設定する。
	// windowHandle: SwapChainの表示先となる有効なWindow。所有しない。
	// width: 初期描画先の幅。単位はpixel。
	// height: 初期描画先の高さ。単位はpixel。
	// 戻り値: すべて生成できればtrue。検出した生成失敗ではUninit後にfalse。
	// Debug構成はDebug Layer付き生成を試し、失敗時は通常生成へ再試行する。
	bool Init(HWND windowHandle,unsigned int width,unsigned int height);
	// BackBufferとDepthをBind・Clearし、OPAQUE Passへ設定する。必要Resourceがなければ何もしない。
	// clearColor: 有効な4成分RGBA配列。nullptr不可。呼出中だけ参照し、保持しない。
	void BeginFrame(const float clearColor[4]);
	// Present(1,0)で表示する。SwapChainがなければ何もしない。失敗はログへ記録する。
	void EndFrame();
	// 旧描画先を解放し、BackBufferと描画先を新しい大きさで生成する。
	// width: 新しい幅。pixel単位。0なら現在状態を変更しない。
	// height: 新しい高さ。pixel単位。0なら現在状態を変更しない。
	// SwapChainがなければ何もしない。再生成失敗時に旧描画先を復元する処理は行わない。
	void Resize(unsigned int width,unsigned int height);
	// 描画先、ContextのState、各描画State、SwapChain・Context・Deviceの順で終了する。
	// 未初期化・部分初期化・終了済みでも呼出可能。呼出側の借用ポインターは使用を終えておく。
	void Uninit();

	//========= Getter関数=========
	// 所有Deviceの借用ポインターを返す。未生成・終了後はnullptr。呼出側へ所有権は移さない。
	[[nodiscard]] ID3D11Device* GetDevice() const { return m_device.Get(); }
	// 所有Contextの借用ポインターを返す。未生成・終了後はnullptr。再初期化・終了後は再取得する。
	[[nodiscard]] ID3D11DeviceContext* GetContext() const { return m_context.Get(); }
	// 生成済み描画先の幅をpixel単位で返す。ReleaseRenderTargets後は0。
	[[nodiscard]] unsigned int GetRenderWidth() const { return m_renderWidth; }
	// 生成済み描画先の高さをpixel単位で返す。ReleaseRenderTargets後は0。
	[[nodiscard]] unsigned int GetRenderHeight() const { return m_renderHeight; }

	//========= 描画State設定関数=========
	// Passに応じたBlend・Depth・RasterizerをBindする。以前の状態は保存・復元しない。
	// renderPass: 適用する公開Pass。未定義値またはContextなしの場合は変更しない。
	void SetRenderPass(e_RenderPass renderPass);
	// Blend Stateだけを切り替える。Contextがなければ何もしない。
	// isEnabled: trueで生成済みAlpha Blend、falseで既定のBlendを使用する。
	void SetAlphaBlendEnabled(bool isEnabled);
	// Depth Stencil Stateだけを切り替える。Contextがなければ何もしない。
	// isEnabled: trueで既定Depth、falseでDepth TestとDepth書込の両方を無効にする。
	void SetDepthTestEnabled(bool isEnabled);

private:
	//========= 補助関数=========
	// BackBufferのView、Depth用TextureとViewを生成し、Viewportと保持サイズを設定する。
	// width: 作成するDepth領域とViewportの幅。pixel単位。
	// height: 作成するDepth領域とViewportの高さ。pixel単位。
	// 戻り値: 成功ならtrue。基盤不足ならfalse、検出した生成失敗なら描画先を部分解放してfalse。
	bool CreateRenderTargets(unsigned int width,unsigned int height);
	// 描画先Viewを解放して保持サイズを0へ戻す。ContextのBind解除はここでは行わない。
	void ReleaseRenderTargets();

	//========= Direct3D主要Resource=========
	// Resource生成に使用するDeviceを所有する。
	ComPtr<ID3D11Device> m_device{};
	// 描画コマンドとState設定を行う即時Contextを所有する。
	ComPtr<ID3D11DeviceContext> m_context{};
	// BackBuffer管理と表示を行うSwapChainを所有する。
	ComPtr<IDXGISwapChain> m_swapChain{};

	//========= Render Target・Depth Stencil関連=========
	// BackBufferを描画先に使用するViewを所有する。Resizeで作り直す。
	ComPtr<ID3D11RenderTargetView> m_renderTargetView{};
	// 奥行き判定用のViewを所有する。参照するTextureはViewを通して保持する。
	ComPtr<ID3D11DepthStencilView> m_depthStencilView{};

	//========= Render Targetサイズ=========
	// 最後に生成成功した描画先の幅。単位はpixel。View解放時は0。
	unsigned int m_renderWidth{};
	// 最後に生成成功した描画先の高さ。単位はpixel。View解放時は0。
	unsigned int m_renderHeight{};

	//========= 描画State関連=========
	// Depth TestとDepth書込を無効にするStateを所有する。
	ComPtr<ID3D11DepthStencilState> m_depthDisabledState{};
	// Depth Testを有効にしたまま書込だけを止めるStateを所有する。
	ComPtr<ID3D11DepthStencilState> m_depthReadOnlyState{};
	// Src AlphaによるRGB合成に使用するBlend Stateを所有する。
	ComPtr<ID3D11BlendState> m_alphaBlendState{};
	// 背面を除外するRasterizer Stateを所有する。
	ComPtr<ID3D11RasterizerState> m_cullBackRasterizerState{};
	// 表裏どちらも除外しないRasterizer Stateを所有する。
	ComPtr<ID3D11RasterizerState> m_cullNoneRasterizerState{};
};