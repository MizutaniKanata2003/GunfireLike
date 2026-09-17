#include "../H/GraphicsSystem.h"

// DirectX 11のDevice、Context、SwapChain、RenderTargetを初期化する。
bool GraphicsSystem::Init( HWND windowHandle, unsigned int width, unsigned int height )
{
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

	const D3D_FEATURE_LEVEL requestedFeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0
	};

	D3D_FEATURE_LEVEL createdFeatureLevel{};

	const HRESULT result = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		requestedFeatureLevels,
		ARRAYSIZE( requestedFeatureLevels ),
		D3D11_SDK_VERSION,
		&swapChainDesc,
		m_SwapChain.GetAddressOf(),
		m_Device.GetAddressOf(),
		&createdFeatureLevel,
		m_Context.GetAddressOf() );

	if ( FAILED( result ) ) return false;

	// 半透明HUDやフェードを描画するための通常Alpha Blend Stateを生成する。
	D3D11_BLEND_DESC blendDescription{};
	blendDescription.AlphaToCoverageEnable = FALSE;
	blendDescription.IndependentBlendEnable = FALSE;

	D3D11_RENDER_TARGET_BLEND_DESC& renderTargetBlend = blendDescription.RenderTarget[ 0 ];
	renderTargetBlend.BlendEnable = TRUE;
	renderTargetBlend.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	renderTargetBlend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	renderTargetBlend.BlendOp = D3D11_BLEND_OP_ADD;
	renderTargetBlend.SrcBlendAlpha = D3D11_BLEND_ONE;
	renderTargetBlend.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	renderTargetBlend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	renderTargetBlend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	const HRESULT blendResult = m_Device->CreateBlendState( &blendDescription, m_AlphaBlendState.GetAddressOf() );

	if ( FAILED( blendResult ) )
	{
		Uninit();
		return false;
	}

	// HUD描画時にDepth Testを無効化するDepthStencil Stateを生成する。
	D3D11_DEPTH_STENCIL_DESC depthDisabledDescription{};
	depthDisabledDescription.DepthEnable = FALSE;
	depthDisabledDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDisabledDescription.DepthFunc = D3D11_COMPARISON_ALWAYS;
	depthDisabledDescription.StencilEnable = FALSE;

	const HRESULT depthDisabledResult = m_Device->CreateDepthStencilState(
		&depthDisabledDescription,
		m_DepthDisabledState.GetAddressOf() );

	if ( FAILED( depthDisabledResult ) )
	{
		Uninit();
		return false;
	}

	if ( !CreateRenderTargets( width, height ) )
	{
		Uninit();
		return false;
	}

	return true;
}

// DirectX 11で生成したリソースを解放する。
void GraphicsSystem::Uninit()
{
	ReleaseRenderTargets();

	if ( m_Context ) m_Context->ClearState();

	m_AlphaBlendState.Reset();
	m_DepthDisabledState.Reset();

	m_SwapChain.Reset();
	m_Context.Reset();
	m_Device.Reset();
}

// RenderTargetとDepthStencilをクリアし、フレーム描画を開始する。
void GraphicsSystem::BeginFrame( const float clearColor[ 4 ] )
{
	if ( !m_Context || !m_RenderTargetView || !m_DepthStencilView ) return;

	m_Context->OMSetRenderTargets( 1, m_RenderTargetView.GetAddressOf(), m_DepthStencilView.Get() );
	m_Context->ClearRenderTargetView( m_RenderTargetView.Get(), clearColor );
	m_Context->ClearDepthStencilView( m_DepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0 );
}

// 描画済みフレームをSwapChainへ表示する。
void GraphicsSystem::EndFrame()
{
	if ( m_SwapChain ) m_SwapChain->Present( 1, 0 );
}

// ウィンドウサイズに合わせてRenderTarget、DepthStencil、Viewportを再生成する。
void GraphicsSystem::Resize( unsigned int width, unsigned int height )
{
	if ( width == 0 || height == 0 || !m_SwapChain ) return;

	ReleaseRenderTargets();

	if ( FAILED( m_SwapChain->ResizeBuffers( 0, width, height, DXGI_FORMAT_UNKNOWN, 0 ) ) ) return;

	CreateRenderTargets( width, height );
}

// BackBuffer、RenderTargetView、DepthStencilView、Viewportを生成する。
bool GraphicsSystem::CreateRenderTargets( unsigned int width, unsigned int height )
{
	if ( !m_Device || !m_Context || !m_SwapChain ) return false;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer{};

	if ( FAILED( m_SwapChain->GetBuffer( 0, IID_PPV_ARGS( backBuffer.GetAddressOf() ) ) ) ) return false;

	if ( FAILED( m_Device->CreateRenderTargetView(
		backBuffer.Get(), nullptr, m_RenderTargetView.GetAddressOf() ) ) ) return false;

	D3D11_TEXTURE2D_DESC depthTextureDesc{};
	depthTextureDesc.Width = width;
	depthTextureDesc.Height = height;
	depthTextureDesc.MipLevels = 1;
	depthTextureDesc.ArraySize = 1;
	depthTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthTextureDesc.SampleDesc.Count = 1;
	depthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture{};

	if ( FAILED( m_Device->CreateTexture2D(
		&depthTextureDesc, nullptr, depthTexture.GetAddressOf() ) ) ) return false;

	if ( FAILED( m_Device->CreateDepthStencilView(
		depthTexture.Get(), nullptr, m_DepthStencilView.GetAddressOf() ) ) ) return false;

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

// Alpha Blend Stateの有効・無効を切り替える。
void GraphicsSystem::SetAlphaBlendEnabled( bool enabled )
{
	if ( !m_Context ) return;

	const float blendFactor[ 4 ]{};

	m_Context->OMSetBlendState(
		enabled ? m_AlphaBlendState.Get() : nullptr,
		blendFactor,
		0xffffffff );
}

// Depth Testの有効・無効を切り替える。
void GraphicsSystem::SetDepthTestEnabled( bool enabled )
{
	if ( !m_Context ) return;

	m_Context->OMSetDepthStencilState( enabled ? nullptr : m_DepthDisabledState.Get(), 0 );
}