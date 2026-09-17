#pragma once

#include <d3d11.h>
#include <wrl/client.h>

// DirectX 11のDevice、Context、SwapChain、RenderTarget、DepthStencilを管理する。
class GraphicsSystem final
{
public:
	// DirectX 11のDevice、Context、SwapChain、RenderTargetを初期化する。
	bool Init( HWND windowHandle, unsigned int width, unsigned int height );

	// DirectX 11で生成したリソースを解放する。
	void Uninit();

	// RenderTargetとDepthStencilをクリアし、フレーム描画を開始する。
	void BeginFrame( const float clearColor[ 4 ] );

	// 描画済みフレームをSwapChainへ表示する。
	void EndFrame();

	// ウィンドウサイズに合わせてRenderTarget、DepthStencil、Viewportを再生成する。
	void Resize( unsigned int width, unsigned int height );

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

	// Alpha Blend Stateの有効・無効を切り替える。
	void SetAlphaBlendEnabled( bool enabled );

	// Depth Testの有効・無効を切り替える。
	void SetDepthTestEnabled( bool enabled );

private:
	// BackBuffer、RenderTargetView、DepthStencilView、Viewportを生成する。
	bool CreateRenderTargets( unsigned int width, unsigned int height );

	// ウィンドウサイズ変更前にRenderTargetとDepthStencilを解放する。
	void ReleaseRenderTargets();

	// DirectX 11 Deviceを保持する。
	Microsoft::WRL::ComPtr<ID3D11Device> m_Device{};

	// 描画コマンドを発行するDevice Context。
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_Context{};

	// BackBufferの交換と画面表示を行うSwapChain。
	Microsoft::WRL::ComPtr<IDXGISwapChain> m_SwapChain{};

	// BackBufferへ描画するためのRenderTarget View。
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_RenderTargetView{};

	// 3D描画の奥行き判定に使用するDepthStencil View。
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_DepthStencilView{};

	// HUDなどを描画するときにDepth Testを無効化するState。
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_DepthDisabledState{};

	// 半透明HUDなどを描画するときに使用するAlpha Blend State。
	Microsoft::WRL::ComPtr<ID3D11BlendState> m_AlphaBlendState{};
};