#pragma once

//========= DirectX インクルード=========
#include <d3d11.h>
#include <wrl/client.h>

// 描画対象ごとに適用するDirect3D描画Stateを表す。
enum class e_RenderPass
{
	// Skyを描画する。
	e_SKY,
	// 不透明な3D Objectを描画する。
	e_OPAQUE,
	// 半透明な3D Objectを描画する。
	e_TRANSPARENT,
	// 画面固定のHUDとTextを描画する。
	e_SCREEN_UI
};

// DirectX 11のDevice、Context、SwapChain、RenderTarget、DepthStencilを管理する。
class GraphicsSystem final
{
public:
	//========= 初期化・終了関数=========
	// DirectX 11のDevice、Context、SwapChain、RenderTargetを初期化する。
	bool Init( HWND windowHandle, unsigned int width, unsigned int height );
	// DirectX 11で生成したリソースを解放する。
	void Uninit();

	//========= フレーム制御関数=========
	// RenderTargetとDepthStencilをクリアし、フレーム描画を開始する。
	void BeginFrame( const float clearColor[ 4 ] );
	// 描画済みフレームをSwapChainへ表示する。
	void EndFrame();
	// ウィンドウサイズに合わせてRenderTarget、DepthStencil、Viewportを再生成する。
	void Resize( unsigned int width, unsigned int height );

	//========= Getter関数=========
	// DirectX 11 Deviceを返す。
	[[nodiscard]] ID3D11Device* GetDevice() const
	{
		return m_Device.Get();
	}

	// DirectX 11 Device Contextを返す。
	[[nodiscard]] ID3D11DeviceContext* GetContext() const
	{
		return m_Context.Get();
	}

	//========= 描画State設定関数=========
	// 指定した描画Passに必要なBlend、Depth、Rasterizer Stateをまとめて設定する。
	void SetRenderPass( e_RenderPass renderPass );
	// Alpha Blend Stateの有効・無効を切り替える。
	void SetAlphaBlendEnabled( bool isEnabled );
	// Depth Testの有効・無効を切り替える。
	void SetDepthTestEnabled( bool isEnabled );

private:
	//========= 補助関数=========
	// BackBuffer、RenderTargetView、DepthStencilView、Viewportを生成する。
	bool CreateRenderTargets( unsigned int width, unsigned int height );
	// ウィンドウサイズ変更前にRenderTargetとDepthStencilを解放する。
	void ReleaseRenderTargets();

	//========= Direct3D主要リソース=========
	// DirectX 11 Deviceを保持する。
	Microsoft::WRL::ComPtr<ID3D11Device> m_Device{};
	// 描画コマンドを発行するDevice Context。
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_Context{};
	// BackBufferの交換と画面表示を行うSwapChain。
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain{};

	//========= RenderTarget・DepthStencil関連=========
	// BackBufferへ描画するためのRenderTarget View。
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_RenderTargetView{};
	// 3D描画の奥行き判定に使用するDepthStencil View。
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_DepthStencilView{};

	//========= 描画State関連=========
	// HUDとSkyを描画するときにDepth TestとDepth Writeを無効化するState。
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_DepthDisabledState{};
	// 半透明3D ObjectでDepth Testを維持し、Depth Writeだけ無効化するState。
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_DepthReadOnlyState{};
	// 半透明HUDなどを描画するときに使用するAlpha Blend State。
	Microsoft::WRL::ComPtr<ID3D11BlendState> m_AlphaBlendState{};
	// 不透明3D Objectと半透明3D Objectで背面を除外するRasterizer State。
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_CullBackRasterizerState{};
	// Skyと画面UIで表裏を除外しないRasterizer State。
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_CullNoneRasterizerState{};
};