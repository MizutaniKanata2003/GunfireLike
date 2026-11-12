#include "../H/GraphicsSystem.h"

// Direct3D基盤と描画Stateの生成、フレーム処理、Resize、終了を実装する。
// 生成設定・Bind・失敗経路の処理順は既存のまま維持する。

//========= C++標準ライブラリ インクルード=========
#include <iomanip>
#include <sstream>
#include <string>

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

// HRESULTログ、Device生成、Blend設定用の内部補助処理をまとめる。
namespace
{
	//========= ログ補助関数=========
	// HRESULTを0x付き8桁の大文字16進数文字列へ変換して返す。
	// result: 表示用に変換するHRESULT。
	std::wstring GetHRESULTText(HRESULT result)
	{
		// 0埋めと表示幅を指定し、ログ内のHRESULT表記を揃える。
		std::wostringstream stream{};
		stream << L"0x" << std::uppercase << std::hex << std::setfill(L'0') << std::setw(8) << static_cast<unsigned long>(result);

		return stream.str();
	}

	// GraphicsカテゴリでHRESULTを含むエラーログを出力する。
	// functionName: 失敗した処理を示す終端付きワイド文字列。nullptr不可。
	// result: 失敗理由を示すHRESULT。
	void WriteGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		// 処理名とHRESULTを連結し、Graphicsカテゴリへ一つのメッセージとして出力する。
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: ";
		message += GetHRESULTText(result);

		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}

	//========= Device生成関数=========
	// Debug LayerなしのDirect3D Device、Context、SwapChainを生成する。
	// swapChainDesc: Window・サイズ・BackBufferなどの生成設定。
	// requestedFeatureLevels: 要求するFeature Levelの有効な配列。
	// requestedFeatureLevelCount: 配列の要素数。
	// createdFeatureLevel: 採用されたFeature Levelの出力先。
	// swapChain: 生成したSwapChainを受け取る出力先。
	// device: 生成したDeviceを受け取る出力先。
	// context: 生成した即時Contextを受け取る出力先。
	// 戻り値: D3D11CreateDeviceAndSwapChainのHRESULTをそのまま返す。
	HRESULT CreateDeviceAndSwapChain(const DXGI_SWAP_CHAIN_DESC& swapChainDesc,const D3D_FEATURE_LEVEL requestedFeatureLevels[],
																	 UINT requestedFeatureLevelCount,D3D_FEATURE_LEVEL* createdFeatureLevel,
																	 IDXGISwapChain** swapChain,ID3D11Device** device,ID3D11DeviceContext** context)
	{
		// Hardware Driverを使い、指定された出力先へResourceを生成する。
		return D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,requestedFeatureLevels,requestedFeatureLevelCount,
																				 D3D11_SDK_VERSION,&swapChainDesc,swapChain,device,createdFeatureLevel,context);
	}

	// Debug LayerありのDirect3D Device、Context、SwapChainを生成する。
	// swapChainDesc: Window・サイズ・BackBufferなどの生成設定。
	// requestedFeatureLevels: 要求するFeature Levelの有効な配列。
	// requestedFeatureLevelCount: 配列の要素数。
	// createdFeatureLevel: 採用されたFeature Levelの出力先。
	// swapChain: 生成したSwapChainを受け取る出力先。
	// device: 生成したDeviceを受け取る出力先。
	// context: 生成した即時Contextを受け取る出力先。
	// 戻り値: D3D11CreateDeviceAndSwapChainのHRESULTをそのまま返す。
	HRESULT CreateDebugDeviceAndSwapChain(const DXGI_SWAP_CHAIN_DESC& swapChainDesc,const D3D_FEATURE_LEVEL requestedFeatureLevels[],
																				UINT requestedFeatureLevelCount,D3D_FEATURE_LEVEL* createdFeatureLevel,IDXGISwapChain** swapChain,
																				ID3D11Device** device,ID3D11DeviceContext** context)
	{
		// Hardware Driverを使い、指定された出力先へResourceを生成する。
		return D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_DEBUG,requestedFeatureLevels,
																				 requestedFeatureLevelCount,D3D11_SDK_VERSION,&swapChainDesc,swapChain,
																				 device,createdFeatureLevel,context);
	}

	//========= Blend設定補助関数=========
	// Alpha Blendへ渡す固定の4成分Blend Factorを返す。返却先は静的領域で呼出側は解放しない。
	const float* GetBlendFactor()
	{
		// 全成分0の配列を共有し、関数から戻った後も参照先を有効に保つ。
		static const float BLEND_FACTOR[4]{};
		return BLEND_FACTOR;
	}
}

//========= ライフサイクル・フレーム制御関数=========
// DirectX 11のDevice、Context、SwapChain、RenderTargetを初期化する。
bool GraphicsSystem::Init(HWND windowHandle,unsigned int width,unsigned int height)
{
	// 二重初期化時に、既存のDirect3Dリソースを安全に解放する。
	Uninit();

	// DeviceとSwapChainの生成に使用する設定を作成する。
	DXGI_SWAP_CHAIN_DESC swap_chain_desc{};
	swap_chain_desc.BufferCount = 2;
	swap_chain_desc.BufferDesc.Width = width;
	swap_chain_desc.BufferDesc.Height = height;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = windowHandle;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.Windowed = TRUE;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// 使用するDirect3D Feature Levelを指定する。
	const D3D_FEATURE_LEVEL requested_feature_levels[]{D3D_FEATURE_LEVEL_11_0};
	D3D_FEATURE_LEVEL created_feature_level{};

	// Debug構成ではD3D11 Debug Layerを有効にしてDeviceを生成する。
	#if defined(_DEBUG)
	HRESULT result = CreateDebugDeviceAndSwapChain(swap_chain_desc,requested_feature_levels,ARRAYSIZE(requested_feature_levels),
																								 &created_feature_level,m_swapChain.GetAddressOf(),m_device.GetAddressOf(),m_context.GetAddressOf());

	// SDK Layers未導入環境では、Debug LayerなしでDevice生成を再試行する。
	if(FAILED(result)){
		Logger::Write(e_LogLevel::e_WARNING,e_LogCategory::e_GRAPHICS,L"D3D11 Debug Layerの有効化に失敗しました。通常Device作成へ再試行します。");

		result = CreateDeviceAndSwapChain(swap_chain_desc,requested_feature_levels,ARRAYSIZE(requested_feature_levels),&created_feature_level,
																			m_swapChain.GetAddressOf(),m_device.GetAddressOf(),m_context.GetAddressOf());
	}
	#else
			// Release構成ではDebug LayerなしでDeviceを生成する。
	const HRESULT result = CreateDeviceAndSwapChain(swap_chain_desc,requested_feature_levels,ARRAYSIZE(requested_feature_levels),&created_feature_level,
																									m_swapChain.GetAddressOf(),m_device.GetAddressOf(),m_context.GetAddressOf());
	#endif

	if(FAILED(result)){
		WriteGraphicsError(L"D3D11CreateDeviceAndSwapChain",result);
		Uninit();
		return false;
	}

	#if defined(_DEBUG)
	Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_GRAPHICS,L"Direct3D 11 Deviceを初期化しました。");
	#endif

	// 半透明HUDやフェードを描画するためのAlpha Blend Stateを生成する。
	D3D11_BLEND_DESC blend_description{};
	blend_description.AlphaToCoverageEnable = FALSE;
	blend_description.IndependentBlendEnable = FALSE;

	D3D11_RENDER_TARGET_BLEND_DESC& render_target_blend = blend_description.RenderTarget[0];
	render_target_blend.BlendEnable = TRUE;
	render_target_blend.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	render_target_blend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	render_target_blend.BlendOp = D3D11_BLEND_OP_ADD;
	render_target_blend.SrcBlendAlpha = D3D11_BLEND_ONE;
	render_target_blend.DestBlendAlpha = D3D11_BLEND_ONE;
	render_target_blend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	render_target_blend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	const HRESULT blend_state_result = m_device->CreateBlendState(&blend_description,m_alphaBlendState.GetAddressOf());
	if(FAILED(blend_state_result)){
		WriteGraphicsError(L"ID3D11Device::CreateBlendState",blend_state_result);
		Uninit();
		return false;
	}

	// HUDとSkyを描画するときにDepth TestとDepth Writeを無効化するStateを生成する。
	D3D11_DEPTH_STENCIL_DESC depth_disabled_description{};
	depth_disabled_description.DepthEnable = FALSE;
	depth_disabled_description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depth_disabled_description.DepthFunc = D3D11_COMPARISON_ALWAYS;
	depth_disabled_description.StencilEnable = FALSE;

	const HRESULT depth_disabled_state_result = m_device->CreateDepthStencilState(&depth_disabled_description,m_depthDisabledState.GetAddressOf());
	if(FAILED(depth_disabled_state_result)){
		WriteGraphicsError(L"ID3D11Device::CreateDepthStencilState",depth_disabled_state_result);
		Uninit();
		return false;
	}

	// 半透明3D ObjectでDepth Testを維持し、Depth Writeだけ無効化するStateを生成する。
	D3D11_DEPTH_STENCIL_DESC depth_read_only_description{};
	depth_read_only_description.DepthEnable = TRUE;
	depth_read_only_description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depth_read_only_description.DepthFunc = D3D11_COMPARISON_LESS;
	depth_read_only_description.StencilEnable = FALSE;

	const HRESULT depth_read_only_state_result = m_device->CreateDepthStencilState(&depth_read_only_description,m_depthReadOnlyState.GetAddressOf());
	if(FAILED(depth_read_only_state_result)){
		WriteGraphicsError(L"ID3D11Device::CreateDepthStencilState",depth_read_only_state_result);
		Uninit();
		return false;
	}

	// 背面を除外する3D描画用Rasterizer Stateを生成する。
	D3D11_RASTERIZER_DESC cull_back_rasterizer_description{};
	cull_back_rasterizer_description.FillMode = D3D11_FILL_SOLID;
	cull_back_rasterizer_description.CullMode = D3D11_CULL_BACK;
	cull_back_rasterizer_description.DepthClipEnable = TRUE;

	const HRESULT cull_back_rasterizer_state_result = m_device->CreateRasterizerState(&cull_back_rasterizer_description,m_cullBackRasterizerState.GetAddressOf());
	if(FAILED(cull_back_rasterizer_state_result)){
		WriteGraphicsError(L"ID3D11Device::CreateRasterizerState",cull_back_rasterizer_state_result);
		Uninit();
		return false;
	}

	// 表裏を除外しないSkyと画面UI用Rasterizer Stateを生成する。
	D3D11_RASTERIZER_DESC cull_none_rasterizer_description{};
	cull_none_rasterizer_description.FillMode = D3D11_FILL_SOLID;
	cull_none_rasterizer_description.CullMode = D3D11_CULL_NONE;
	cull_none_rasterizer_description.DepthClipEnable = TRUE;

	const HRESULT cull_none_rasterizer_state_result = m_device->CreateRasterizerState(&cull_none_rasterizer_description,m_cullNoneRasterizerState.GetAddressOf());
	if(FAILED(cull_none_rasterizer_state_result)){
		WriteGraphicsError(L"ID3D11Device::CreateRasterizerState",cull_none_rasterizer_state_result);
		Uninit();
		return false;
	}

	// 現在のウィンドウサイズでRenderTarget、DepthStencil、Viewportを生成する。
	if(!CreateRenderTargets(width,height)){
		Uninit();
		return false;
	}

	// 初期Stateを不透明3D描画用へ設定する。
	SetRenderPass(e_RenderPass::e_OPAQUE);

	return true;
}

// RenderTargetとDepthStencilをクリアし、フレーム描画を開始する。
void GraphicsSystem::BeginFrame(const float clearColor[4])
{
	// 描画先またはContextがない場合は、BindとClearを行わない。
	if(!m_context || !m_renderTargetView || !m_depthStencilView)return;

	// 描画先としてBackBufferとDepthStencilを設定する。
	m_context->OMSetRenderTargets(1,m_renderTargetView.GetAddressOf(),m_depthStencilView.Get());

	// 指定色と初期Depth値で描画先をクリアする。
	m_context->ClearRenderTargetView(m_renderTargetView.Get(),clearColor);
	m_context->ClearDepthStencilView(m_depthStencilView.Get(),D3D11_CLEAR_DEPTH,1.0f,0);

	// 各Frame開始時に不透明3D描画用Stateへ戻す。
	SetRenderPass(e_RenderPass::e_OPAQUE);
}

// 描画済みフレームをSwapChainへ表示する。
void GraphicsSystem::EndFrame()
{
	// SwapChainがない場合は表示処理を行わない。
	if(!m_swapChain)return;

	// 同期間隔1、追加Flagなしの既存設定で表示し、失敗はログへ記録する。
	const HRESULT present_result = m_swapChain->Present(1,0);
	if(FAILED(present_result))WriteGraphicsError(L"IDXGISwapChain::Present",present_result);
}

// ウィンドウサイズに合わせてRenderTarget、DepthStencil、Viewportを再生成する。
void GraphicsSystem::Resize(unsigned int width,unsigned int height)
{
	// サイズ0では現在のResourceと保持サイズを変更せず、最小化中の再生成を省く。
	if(width == 0 || height == 0 || !m_swapChain)return;

	// 旧サイズのRenderTargetとDepthStencilを解放する。
	ReleaseRenderTargets();

	// 新しいウィンドウサイズでBackBufferを再生成する。
	const HRESULT resize_buffers_result = m_swapChain->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0);
	if(FAILED(resize_buffers_result)){
		WriteGraphicsError(L"IDXGISwapChain::ResizeBuffers",resize_buffers_result);
		return;
	}

	// 新しいBackBufferからRenderTarget、DepthStencil、Viewportを再生成する。
	if(!CreateRenderTargets(width,height))ReleaseRenderTargets();
}

// DirectX 11で生成したリソースを解放する。
void GraphicsSystem::Uninit()
{
	// RenderTargetとDepthStencilを先に解放する。
	ReleaseRenderTargets();

	// Contextに設定されている描画Stateと参照を解除する。
	if(m_context)m_context->ClearState();

	// 描画Stateを解放する。
	m_cullNoneRasterizerState.Reset();
	m_cullBackRasterizerState.Reset();
	m_alphaBlendState.Reset();
	m_depthReadOnlyState.Reset();
	m_depthDisabledState.Reset();

	// DirectX 11の主要リソースを解放する。
	m_swapChain.Reset();
	m_context.Reset();
	m_device.Reset();
}

//========= 描画State設定関数=========
// 指定した描画Passに必要なBlend、Depth、Rasterizer Stateをまとめて設定する。
void GraphicsSystem::SetRenderPass(e_RenderPass renderPass)
{
	// Contextがない場合は描画Stateを変更しない。
	if(!m_context)return;

	// 各Passに対応するBlend・Depth・Rasterizerをまとめて設定する。
	switch(renderPass){
	case e_RenderPass::e_SKY:
	// 奥行き判定・書込とCullを無効にし、BlendなしでSkyを描く。
	m_context->OMSetBlendState(nullptr,GetBlendFactor(),0xffffffff);
	m_context->OMSetDepthStencilState(m_depthDisabledState.Get(),0);
	m_context->RSSetState(m_cullNoneRasterizerState.Get());
	return;

	case e_RenderPass::e_OPAQUE:
	// 既定Depthと背面Cullを使い、Blendなしで不透明Objectを描く。
	m_context->OMSetBlendState(nullptr,GetBlendFactor(),0xffffffff);
	m_context->OMSetDepthStencilState(nullptr,0);
	m_context->RSSetState(m_cullBackRasterizerState.Get());
	return;

	case e_RenderPass::e_TRANSPARENT:
	// Depth Testは維持して書込だけを止め、Alpha Blendと背面Cullを使う。
	m_context->OMSetBlendState(m_alphaBlendState.Get(),GetBlendFactor(),0xffffffff);
	m_context->OMSetDepthStencilState(m_depthReadOnlyState.Get(),0);
	m_context->RSSetState(m_cullBackRasterizerState.Get());
	return;

	case e_RenderPass::e_SCREEN_UI:
	// DepthとCullを無効にし、Alpha Blendを使って画面UIを描く。
	m_context->OMSetBlendState(m_alphaBlendState.Get(),GetBlendFactor(),0xffffffff);
	m_context->OMSetDepthStencilState(m_depthDisabledState.Get(),0);
	m_context->RSSetState(m_cullNoneRasterizerState.Get());
	return;

	default:
	return;
	}
}

// Alpha Blend Stateの有効・無効を切り替える。
void GraphicsSystem::SetAlphaBlendEnabled(bool isEnabled)
{
	// Contextがない場合は描画Stateを変更しない。
	if(!m_context)return;

	// 有効時は生成済みAlpha Blend、無効時は既定Blendへ切り替える。
	m_context->OMSetBlendState(isEnabled ? m_alphaBlendState.Get() : nullptr,GetBlendFactor(),0xffffffff);
}

// Depth Testの有効・無効を切り替える。
void GraphicsSystem::SetDepthTestEnabled(bool isEnabled)
{
	// Contextがない場合は描画Stateを変更しない。
	if(!m_context)return;

	// 有効時は既定Depth、無効時はDepth Testと書込を止めるStateを使用する。
	m_context->OMSetDepthStencilState(isEnabled ? nullptr : m_depthDisabledState.Get(),0);
}

//========= 補助関数=========
// BackBuffer、RenderTargetView、DepthStencilView、Viewportを生成する。
bool GraphicsSystem::CreateRenderTargets(unsigned int width,unsigned int height)
{
	// 基盤Resourceが揃っていない場合は、描画先の生成へ進まない。
	if(!m_device || !m_context || !m_swapChain)return false;

	// SwapChainのBackBufferを取得してRenderTarget Viewを生成する。
	ComPtr<ID3D11Texture2D> back_buffer{};
	const HRESULT get_buffer_result = m_swapChain->GetBuffer(0,IID_PPV_ARGS(back_buffer.GetAddressOf()));
	if(FAILED(get_buffer_result)){
		WriteGraphicsError(L"IDXGISwapChain::GetBuffer",get_buffer_result);
		ReleaseRenderTargets();
		return false;
	}

	// 取得したBackBufferを描画先Viewへ変換し、失敗時は部分生成した描画先を解放する。
	const HRESULT render_target_view_result = m_device->CreateRenderTargetView(back_buffer.Get(),nullptr,m_renderTargetView.GetAddressOf());
	if(FAILED(render_target_view_result)){
		WriteGraphicsError(L"ID3D11Device::CreateRenderTargetView",render_target_view_result);
		ReleaseRenderTargets();
		return false;
	}

	// 3D描画の奥行き判定に使用するDepthStencil用Textureを生成する。
	D3D11_TEXTURE2D_DESC depth_texture_desc{};
	depth_texture_desc.Width = width;
	depth_texture_desc.Height = height;
	depth_texture_desc.MipLevels = 1;
	depth_texture_desc.ArraySize = 1;
	depth_texture_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depth_texture_desc.SampleDesc.Count = 1;
	depth_texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	ComPtr<ID3D11Texture2D> depth_texture{};
	const HRESULT depth_texture_result = m_device->CreateTexture2D(&depth_texture_desc,nullptr,depth_texture.GetAddressOf());
	if(FAILED(depth_texture_result)){
		WriteGraphicsError(L"ID3D11Device::CreateTexture2D",depth_texture_result);
		ReleaseRenderTargets();
		return false;
	}

	// Depth用TextureからViewを作り、失敗時にはRender Target側もまとめて解放する。
	const HRESULT depth_stencil_view_result = m_device->CreateDepthStencilView(depth_texture.Get(),nullptr,m_depthStencilView.GetAddressOf());
	if(FAILED(depth_stencil_view_result)){
		WriteGraphicsError(L"ID3D11Device::CreateDepthStencilView",depth_stencil_view_result);
		ReleaseRenderTargets();
		return false;
	}

	// 新しい画面サイズに合わせてViewportを設定する。
	D3D11_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	m_context->RSSetViewports(1,&viewport);

	// 描画先の生成がすべて成功した後にだけ、新しい保持サイズを確定する。
	m_renderWidth = width;
	m_renderHeight = height;

	return true;
}

// ウィンドウサイズ変更前にRenderTargetとDepthStencilを解放する。
void GraphicsSystem::ReleaseRenderTargets()
{
	// Viewを解放する。ContextのBind解除はここでは追加せず、既存の処理を維持する。
	m_depthStencilView.Reset();
	m_renderTargetView.Reset();

	// 描画先がない状態をサイズ0で表し、Scene側の描画判定へ伝える。
	m_renderWidth = {};
	m_renderHeight = {};
}