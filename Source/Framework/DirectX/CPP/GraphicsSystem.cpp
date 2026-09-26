#include "../H/GraphicsSystem.h"

//========= C++標準ライブラリ インクルード=========
#include <iomanip>
#include <sstream>
#include <string>

//========= Framework インクルード=========
#include "../../Etc/H/Logger.h"

namespace
{
	// HRESULTを16進数の表示用文字列へ変換する。
	std::wstring GetHRESULTText( HRESULT result )
	{
		std::wostringstream stream{};
		stream << L"0x" << std::uppercase << std::hex << std::setfill( L'0' ) << std::setw( 8 )
			<< static_cast<unsigned long>( result );

		return stream.str();
	}

	// GraphicsカテゴリでHRESULTを含むエラーログを出力する。
	void WriteGraphicsError( const wchar_t* functionName, HRESULT result )
	{
		std::wstring message{ functionName };
		message += L" に失敗しました。HRESULT: ";
		message += GetHRESULTText( result );

		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_GRAPHICS, message );
	}

	// Debug LayerなしのDirect3D Device、Context、SwapChainを生成する。
	HRESULT CreateDeviceAndSwapChain(
		const DXGI_SWAP_CHAIN_DESC& swapChainDesc,
		const D3D_FEATURE_LEVEL requestedFeatureLevels[],
		UINT requestedFeatureLevelCount,
		D3D_FEATURE_LEVEL* createdFeatureLevel,
		IDXGISwapChain** swapChain,
		ID3D11Device** device,
		ID3D11DeviceContext** context )
	{
		return D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			0,
			requestedFeatureLevels,
			requestedFeatureLevelCount,
			D3D11_SDK_VERSION,
			&swapChainDesc,
			swapChain,
			device,
			createdFeatureLevel,
			context );
	}

	// Debug LayerありのDirect3D Device、Context、SwapChainを生成する。
	HRESULT CreateDebugDeviceAndSwapChain(
		const DXGI_SWAP_CHAIN_DESC& swapChainDesc,
		const D3D_FEATURE_LEVEL requestedFeatureLevels[],
		UINT requestedFeatureLevelCount,
		D3D_FEATURE_LEVEL* createdFeatureLevel,
		IDXGISwapChain** swapChain,
		ID3D11Device** device,
		ID3D11DeviceContext** context )
	{
		return D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			D3D11_CREATE_DEVICE_DEBUG,
			requestedFeatureLevels,
			requestedFeatureLevelCount,
			D3D11_SDK_VERSION,
			&swapChainDesc,
			swapChain,
			device,
			createdFeatureLevel,
			context );
	}

	// Alpha Blendに渡す固定のBlend Factorを返す。
	const float* GetBlendFactor()
	{
		static const float blendFactor[ 4 ]{};
		return blendFactor;
	}
}

// DirectX 11のDevice、Context、SwapChain、RenderTargetを初期化する。
bool GraphicsSystem::Init( HWND windowHandle, unsigned int width, unsigned int height )
{
	// 二重初期化時に、既存のDirect3Dリソースを安全に解放する。
	Uninit();

	// DeviceとSwapChainの生成に使用する設定を作成する。
	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = windowHandle;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// 使用するDirect3D Feature Levelを指定する。
	const D3D_FEATURE_LEVEL requestedFeatureLevels[]{ D3D_FEATURE_LEVEL_11_0 };
	D3D_FEATURE_LEVEL createdFeatureLevel{};

	// Debug構成ではD3D11 Debug Layerを有効にしてDeviceを生成する。
#if defined( _DEBUG )
	HRESULT result = CreateDebugDeviceAndSwapChain(
		swapChainDesc,
		requestedFeatureLevels,
		ARRAYSIZE( requestedFeatureLevels ),
		&createdFeatureLevel,
		m_SwapChain.GetAddressOf(),
		m_Device.GetAddressOf(),
		m_Context.GetAddressOf() );

	// SDK Layers未導入環境では、Debug LayerなしでDevice生成を再試行する。
	if ( FAILED( result ) )
	{
		Logger::Write( e_LogLevel::e_WARNING, e_LogCategory::e_GRAPHICS, L"D3D11 Debug Layerの有効化に失敗しました。通常Device作成へ再試行します。" );

		result = CreateDeviceAndSwapChain(
			swapChainDesc,
			requestedFeatureLevels,
			ARRAYSIZE( requestedFeatureLevels ),
			&createdFeatureLevel,
			m_SwapChain.GetAddressOf(),
			m_Device.GetAddressOf(),
			m_Context.GetAddressOf() );
	}
#else
	// Release構成ではDebug LayerなしでDeviceを生成する。
	const HRESULT result = CreateDeviceAndSwapChain(
		swapChainDesc,
		requestedFeatureLevels,
		ARRAYSIZE( requestedFeatureLevels ),
		&createdFeatureLevel,
		m_SwapChain.GetAddressOf(),
		m_Device.GetAddressOf(),
		m_Context.GetAddressOf() );
#endif

	if ( FAILED( result ) )
	{
		WriteGraphicsError( L"D3D11CreateDeviceAndSwapChain", result );
		Uninit();
		return false;
	}

#if defined( _DEBUG )
	Logger::Write( e_LogLevel::e_INFO, e_LogCategory::e_GRAPHICS, L"Direct3D 11 Deviceを初期化しました。" );
#endif

	// 半透明HUDやフェードを描画するためのAlpha Blend Stateを生成する。
	D3D11_BLEND_DESC blendDescription{};
	blendDescription.AlphaToCoverageEnable = FALSE;
	blendDescription.IndependentBlendEnable = FALSE;

	D3D11_RENDER_TARGET_BLEND_DESC& renderTargetBlend = blendDescription.RenderTarget[ 0 ];
	renderTargetBlend.BlendEnable = TRUE;
	renderTargetBlend.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	renderTargetBlend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	renderTargetBlend.BlendOp = D3D11_BLEND_OP_ADD;
	renderTargetBlend.SrcBlendAlpha = D3D11_BLEND_ONE;
	renderTargetBlend.DestBlendAlpha = D3D11_BLEND_ONE;
	renderTargetBlend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	renderTargetBlend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	const HRESULT blendStateResult = m_Device->CreateBlendState( &blendDescription, m_AlphaBlendState.GetAddressOf() );

	if ( FAILED( blendStateResult ) )
	{
		WriteGraphicsError( L"ID3D11Device::CreateBlendState", blendStateResult );
		Uninit();
		return false;
	}

	// HUDとSkyを描画するときにDepth TestとDepth Writeを無効化するStateを生成する。
	D3D11_DEPTH_STENCIL_DESC depthDisabledDescription{};
	depthDisabledDescription.DepthEnable = FALSE;
	depthDisabledDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDisabledDescription.DepthFunc = D3D11_COMPARISON_ALWAYS;
	depthDisabledDescription.StencilEnable = FALSE;

	const HRESULT depthDisabledStateResult = m_Device->CreateDepthStencilState( &depthDisabledDescription, m_DepthDisabledState.GetAddressOf() );

	if ( FAILED( depthDisabledStateResult ) )
	{
		WriteGraphicsError( L"ID3D11Device::CreateDepthStencilState", depthDisabledStateResult );
		Uninit();
		return false;
	}

	// 半透明3D ObjectでDepth Testを維持し、Depth Writeだけ無効化するStateを生成する。
	D3D11_DEPTH_STENCIL_DESC depthReadOnlyDescription{};
	depthReadOnlyDescription.DepthEnable = TRUE;
	depthReadOnlyDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthReadOnlyDescription.DepthFunc = D3D11_COMPARISON_LESS;
	depthReadOnlyDescription.StencilEnable = FALSE;

	const HRESULT depthReadOnlyStateResult = m_Device->CreateDepthStencilState( &depthReadOnlyDescription, m_DepthReadOnlyState.GetAddressOf() );

	if ( FAILED( depthReadOnlyStateResult ) )
	{
		WriteGraphicsError( L"ID3D11Device::CreateDepthStencilState", depthReadOnlyStateResult );
		Uninit();
		return false;
	}

	// 背面を除外する3D描画用Rasterizer Stateを生成する。
	D3D11_RASTERIZER_DESC cullBackRasterizerDescription{};
	cullBackRasterizerDescription.FillMode = D3D11_FILL_SOLID;
	cullBackRasterizerDescription.CullMode = D3D11_CULL_BACK;
	cullBackRasterizerDescription.DepthClipEnable = TRUE;

	const HRESULT cullBackRasterizerStateResult = m_Device->CreateRasterizerState( &cullBackRasterizerDescription, m_CullBackRasterizerState.GetAddressOf() );

	if ( FAILED( cullBackRasterizerStateResult ) )
	{
		WriteGraphicsError( L"ID3D11Device::CreateRasterizerState", cullBackRasterizerStateResult );
		Uninit();
		return false;
	}

	// 表裏を除外しないSkyと画面UI用Rasterizer Stateを生成する。
	D3D11_RASTERIZER_DESC cullNoneRasterizerDescription{};
	cullNoneRasterizerDescription.FillMode = D3D11_FILL_SOLID;
	cullNoneRasterizerDescription.CullMode = D3D11_CULL_NONE;
	cullNoneRasterizerDescription.DepthClipEnable = TRUE;

	const HRESULT cullNoneRasterizerStateResult = m_Device->CreateRasterizerState( &cullNoneRasterizerDescription, m_CullNoneRasterizerState.GetAddressOf() );

	if ( FAILED( cullNoneRasterizerStateResult ) )
	{
		WriteGraphicsError( L"ID3D11Device::CreateRasterizerState", cullNoneRasterizerStateResult );
		Uninit();
		return false;
	}

	// 現在のウィンドウサイズでRenderTarget、DepthStencil、Viewportを生成する。
	if ( !CreateRenderTargets( width, height ) )
	{
		Uninit();
		return false;
	}

	// 初期Stateを不透明3D描画用へ設定する。
	SetRenderPass( e_RenderPass::e_OPAQUE );

	return true;
}

// RenderTargetとDepthStencilをクリアし、フレーム描画を開始する。
void GraphicsSystem::BeginFrame( const float clearColor[ 4 ] )
{
	if ( !m_Context || !m_RenderTargetView || !m_DepthStencilView ) return;

	// 描画先としてBackBufferとDepthStencilを設定する。
	m_Context->OMSetRenderTargets( 1, m_RenderTargetView.GetAddressOf(), m_DepthStencilView.Get() );

	// 指定色と初期Depth値で描画先をクリアする。
	m_Context->ClearRenderTargetView( m_RenderTargetView.Get(), clearColor );
	m_Context->ClearDepthStencilView( m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0 );

	// 各Frame開始時に不透明3D描画用Stateへ戻す。
	SetRenderPass( e_RenderPass::e_OPAQUE );
}

// 描画済みフレームをSwapChainへ表示する。
void GraphicsSystem::EndFrame()
{
	if ( !m_SwapChain ) return;

	const HRESULT presentResult = m_SwapChain->Present( 1, 0 );

	if ( FAILED( presentResult ) ) WriteGraphicsError( L"IDXGISwapChain::Present", presentResult );
}

// ウィンドウサイズに合わせてRenderTarget、DepthStencil、Viewportを再生成する。
void GraphicsSystem::Resize( unsigned int width, unsigned int height )
{
	if ( width == 0 || height == 0 || !m_SwapChain ) return;

	// 旧サイズのRenderTargetとDepthStencilを解放する。
	ReleaseRenderTargets();

	// 新しいウィンドウサイズでBackBufferを再生成する。
	const HRESULT resizeBuffersResult = m_SwapChain->ResizeBuffers( 0, width, height, DXGI_FORMAT_UNKNOWN, 0 );

	if ( FAILED( resizeBuffersResult ) )
	{
		WriteGraphicsError( L"IDXGISwapChain::ResizeBuffers", resizeBuffersResult );
		return;
	}

	// 新しいBackBufferからRenderTarget、DepthStencil、Viewportを再生成する。
	if ( !CreateRenderTargets( width, height ) ) ReleaseRenderTargets();
}

// DirectX 11で生成したリソースを解放する。
void GraphicsSystem::Uninit()
{
	// RenderTargetとDepthStencilを先に解放する。
	ReleaseRenderTargets();

	// Contextに設定されている描画Stateと参照を解除する。
	if ( m_Context ) m_Context->ClearState();

	// 描画Stateを解放する。
	m_CullNoneRasterizerState.Reset();
	m_CullBackRasterizerState.Reset();
	m_AlphaBlendState.Reset();
	m_DepthReadOnlyState.Reset();
	m_DepthDisabledState.Reset();

	// DirectX 11の主要リソースを解放する。
	m_SwapChain.Reset();
	m_Context.Reset();
	m_Device.Reset();
}
// BackBuffer、RenderTargetView、DepthStencilView、Viewportを生成する。
bool GraphicsSystem::CreateRenderTargets( unsigned int width, unsigned int height )
{
	if ( !m_Device || !m_Context || !m_SwapChain ) return false;

	// SwapChainのBackBufferを取得してRenderTarget Viewを生成する。
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer{};
	const HRESULT getBufferResult = m_SwapChain->GetBuffer( 0, IID_PPV_ARGS( backBuffer.GetAddressOf() ) );

	if ( FAILED( getBufferResult ) )
	{
		WriteGraphicsError( L"IDXGISwapChain::GetBuffer", getBufferResult );
		ReleaseRenderTargets();
		return false;
	}

	const HRESULT renderTargetViewResult = m_Device->CreateRenderTargetView(
		backBuffer.Get(),
		nullptr,
		m_RenderTargetView.GetAddressOf() );

	if ( FAILED( renderTargetViewResult ) )
	{
		WriteGraphicsError( L"ID3D11Device::CreateRenderTargetView", renderTargetViewResult );
		ReleaseRenderTargets();
		return false;
	}

	// 3D描画の奥行き判定に使用するDepthStencil用Textureを生成する。
	D3D11_TEXTURE2D_DESC depthTextureDesc{};
	depthTextureDesc.Width = width;
	depthTextureDesc.Height = height;
	depthTextureDesc.MipLevels = 1;
	depthTextureDesc.ArraySize = 1;
	depthTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthTextureDesc.SampleDesc.Count = 1;
	depthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture{};
	const HRESULT depthTextureResult = m_Device->CreateTexture2D( &depthTextureDesc, nullptr, depthTexture.GetAddressOf() );

	if ( FAILED( depthTextureResult ) )
	{
		WriteGraphicsError( L"ID3D11Device::CreateTexture2D", depthTextureResult );
		ReleaseRenderTargets();
		return false;
	}

	const HRESULT depthStencilViewResult = m_Device->CreateDepthStencilView(
		depthTexture.Get(),
		nullptr,
		m_DepthStencilView.GetAddressOf() );

	if ( FAILED( depthStencilViewResult ) )
	{
		WriteGraphicsError( L"ID3D11Device::CreateDepthStencilView", depthStencilViewResult );
		ReleaseRenderTargets();
		return false;
	}

	// 新しい画面サイズに合わせてViewportを設定する。
	D3D11_VIEWPORT viewport{};
	viewport.Width = static_cast<float>( width );
	viewport.Height = static_cast<float>( height );
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	m_Context->RSSetViewports( 1, &viewport );

	return true;
}

// ウィンドウサイズ変更前にRenderTargetとDepthStencilを解放する。
void GraphicsSystem::ReleaseRenderTargets()
{
	m_DepthStencilView.Reset();
	m_RenderTargetView.Reset();
}

// 指定した描画Passに必要なBlend、Depth、Rasterizer Stateをまとめて設定する。
void GraphicsSystem::SetRenderPass( e_RenderPass renderPass )
{
	if ( !m_Context ) return;

	switch ( renderPass )
	{
		case e_RenderPass::e_SKY:
		m_Context->OMSetBlendState( nullptr, GetBlendFactor(), 0xffffffff );
		m_Context->OMSetDepthStencilState( m_DepthDisabledState.Get(), 0 );
		m_Context->RSSetState( m_CullNoneRasterizerState.Get() );
		return;

		case e_RenderPass::e_OPAQUE:
		m_Context->OMSetBlendState( nullptr, GetBlendFactor(), 0xffffffff );
		m_Context->OMSetDepthStencilState( nullptr, 0 );
		m_Context->RSSetState( m_CullBackRasterizerState.Get() );
		return;

		case e_RenderPass::e_TRANSPARENT:
		m_Context->OMSetBlendState( m_AlphaBlendState.Get(), GetBlendFactor(), 0xffffffff );
		m_Context->OMSetDepthStencilState( m_DepthReadOnlyState.Get(), 0 );
		m_Context->RSSetState( m_CullBackRasterizerState.Get() );
		return;

		case e_RenderPass::e_SCREEN_UI:
		m_Context->OMSetBlendState( m_AlphaBlendState.Get(), GetBlendFactor(), 0xffffffff );
		m_Context->OMSetDepthStencilState( m_DepthDisabledState.Get(), 0 );
		m_Context->RSSetState( m_CullNoneRasterizerState.Get() );
		return;

		default:
		return;
	}
}

// Alpha Blend Stateの有効・無効を切り替える。
void GraphicsSystem::SetAlphaBlendEnabled( bool isEnabled )
{
	if ( !m_Context ) return;

	m_Context->OMSetBlendState( isEnabled ? m_AlphaBlendState.Get() : nullptr, GetBlendFactor(), 0xffffffff );
}

// Depth Testの有効・無効を切り替える。
void GraphicsSystem::SetDepthTestEnabled( bool isEnabled )
{
	if ( !m_Context ) return;

	m_Context->OMSetDepthStencilState( isEnabled ? nullptr : m_DepthDisabledState.Get(), 0 );
}