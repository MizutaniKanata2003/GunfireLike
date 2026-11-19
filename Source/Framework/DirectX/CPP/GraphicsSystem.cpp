#include "../H/GraphicsSystem.h"

// Device・SwapChainと描画先・Stateを生成し、Bind/Clear/Present、Resize、解放を実装する。
// 既存設定・API順・失敗経路を維持し、Flip Model化やResizeのBind解除などの修正は混ぜない。
// 描画領域は、BackBufferの内側に収めた16:9のViewportとして生成する。

//========= C++標準ライブラリ インクルード=========
#include <iomanip> // setfill・setwでHRESULTを0埋め8桁に整える。
#include <sstream> // wostringstreamへ数値と書式指定を渡し、失敗値の表示文字列を生成する。
#include <string> // wstringでAPI名・HRESULTを連結してLoggerへ渡す。

//========= 外部ライブラリ インクルード=========
#include <d3d11_1.h> // ID3D11DeviceContext1::ClearViewで、RTVの一部の矩形だけを塗る。

//========= Framework インクルード=========
#include "Framework/DirectX/H/ViewportArea.h"
#include "Framework/Etc/H/Logger.h"

// HRESULT表示、Device生成のDebug有無、Blend設定用の静的配列を内部処理としてまとめる。
namespace
{
	//========= ログ補助関数=========
	// HRESULTを0x付き大文字16進数8桁へ変換する。
	// result: 表示するHRESULT。unsigned表現へ変換して既存の桁表記を維持する。
	std::wstring GetHRESULTText(HRESULT result)
	{
		// 16進・大文字・0埋めを設定する。setw(8)は後続の数値部分の最小幅で、0xは別に付加する。
		std::wostringstream stream{};
		stream << L"0x" << std::uppercase << std::hex << std::setfill(L'0') << std::setw(8) << static_cast<unsigned long>(result);
		return stream.str();
	}

	// API名と失敗値をGraphics分類へ出力する。Resourceの後始末は呼出元が行う。
	// functionName: 有効な終端付きAPI名。nullptr不可。
	// result: 記録するHRESULT。
	void WriteGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		// API名と結果を同じメッセージへ連結する。ログ生成の例外をここで捕捉する処理はない。
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: ";
		message += GetHRESULTText(result);
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}

	//========= Device生成関数=========
	// Hardware Driverを使い、Debug LayerなしでDevice・即時Context・SwapChainを同時生成する。
	// swapChainDesc: 表示先Window・寸法・Buffer等の記述。呼出中だけ参照する。
	// requestedFeatureLevels: 要求Feature Levelの有効な配列。
	// requestedFeatureLevelCount: 上の配列の要素数。バイト数ではない。
	// createdFeatureLevel: 採用されたFeature Levelの出力先。
	// swapChain: SwapChain所有参照の出力先。
	// device: Device所有参照の出力先。
	// context: 即時Context所有参照の出力先。
	// 戻り値: D3D11CreateDeviceAndSwapChainのHRESULT。ログや巻き戻しは呼出元で行う。
	HRESULT CreateDeviceAndSwapChain(const DXGI_SWAP_CHAIN_DESC& swapChainDesc,const D3D_FEATURE_LEVEL requestedFeatureLevels[],
																	 UINT requestedFeatureLevelCount,D3D_FEATURE_LEVEL* createdFeatureLevel,
																	 IDXGISwapChain** swapChain,ID3D11Device** device,ID3D11DeviceContext** context)
	{
		// Adapterは指定せずHardwareを要求する。SDK版と要求配列、生成記述、出力先を対応させる。
		return D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,requestedFeatureLevels,
																				 requestedFeatureLevelCount,D3D11_SDK_VERSION,&swapChainDesc,swapChain,device,createdFeatureLevel,context);
	}

	// Hardware Driverを使い、Debug Layer付きでDevice・即時Context・SwapChainを同時生成する。
	// swapChainDesc: 表示先Window・寸法・Buffer等の記述。呼出中だけ参照する。
	// requestedFeatureLevels: 要求Feature Levelの有効な配列。
	// requestedFeatureLevelCount: 上の配列の要素数。バイト数ではない。
	// createdFeatureLevel: 採用されたFeature Levelの出力先。
	// swapChain: SwapChain所有参照の出力先。
	// device: Device所有参照の出力先。
	// context: 即時Context所有参照の出力先。
	// 戻り値: D3D11CreateDeviceAndSwapChainのHRESULT。ログや巻き戻しは呼出元で行う。
	HRESULT CreateDebugDeviceAndSwapChain(const DXGI_SWAP_CHAIN_DESC& swapChainDesc,const D3D_FEATURE_LEVEL requestedFeatureLevels[],
																				UINT requestedFeatureLevelCount,D3D_FEATURE_LEVEL* createdFeatureLevel,
																				IDXGISwapChain** swapChain,ID3D11Device** device,ID3D11DeviceContext** context)
	{
		// Adapterは指定せずHardwareを要求する。SDK版と要求配列、生成記述、出力先を対応させる。
		return D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_DEBUG,requestedFeatureLevels,
																				 requestedFeatureLevelCount,D3D11_SDK_VERSION,&swapChainDesc,swapChain,device,createdFeatureLevel,context);
	}

	//========= Blend設定補助関数=========
	// Bindへ渡す4成分配列へのポインターを返す。静的寿命であり、呼出側は書換え・解放しない。
	const float* GetBlendFactor()
	{
		// 全成分0の既存設定。使用するBlend式がBLEND_FACTORを参照しない場合、RGB合成の係数には使われない。
		static const float BLEND_FACTOR[4]{};
		return BLEND_FACTOR;
	}
}

//========= ライフサイクル・フレーム制御関数=========
// 旧所有参照を終了してから基盤・State・描画先を順に生成する。
bool GraphicsSystem::Init(HWND windowHandle,unsigned int width,unsigned int height)
{
	// 出力ComPtrを旧参照から解放する。以後の初期化失敗で旧描画状態へ戻す処理はない。
	Uninit();

	// Buffer数2、RGBA各8bit、非MSAA、Windowed、DISCARDという既存のBlt Modelを維持する。
	// Width/Heightはpixel、Usageは描画先用途。その他のRefreshRate等はゼロ初期化のままで、選択理由は推測しない。
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

	// 要求は11_0の1要素。採用Levelの出力先は用意するが、別Levelへの新しい分岐は追加しない。
	const D3D_FEATURE_LEVEL requested_feature_levels[]{D3D_FEATURE_LEVEL_11_0};
	D3D_FEATURE_LEVEL created_feature_level{};

	// Debug構成では診断付き生成を先に試す。プリプロセッサは左端に配置する。
	#if defined(_DEBUG)
	HRESULT result = CreateDebugDeviceAndSwapChain(swap_chain_desc,requested_feature_levels,ARRAYSIZE(requested_feature_levels),
																								 &created_feature_level,m_swapChain.GetAddressOf(),m_device.GetAddressOf(),m_context.GetAddressOf());
	// Debug生成が失敗した場合は原因を限定せず通常生成へ再試行する。SDK Layers不足だけを判別する条件ではない。
	if(FAILED(result)){
		// 既存警告を記録して同じ設定で再試行する。ここに新しいResetやAdapter選択は追加しない。
		Logger::Write(e_LogLevel::e_WARNING,e_LogCategory::e_GRAPHICS,L"D3D11 Debug Layerの有効化に失敗しました。通常Device作成へ再試行します。");
		result = CreateDeviceAndSwapChain(swap_chain_desc,requested_feature_levels,ARRAYSIZE(requested_feature_levels),&created_feature_level,
																			m_swapChain.GetAddressOf(),m_device.GetAddressOf(),m_context.GetAddressOf());
	}
	#else
		// Debug以外では診断付き生成を試さず、通常生成の結果を受け取る。
	const HRESULT result = CreateDeviceAndSwapChain(swap_chain_desc,requested_feature_levels,ARRAYSIZE(requested_feature_levels),&created_feature_level,
																									m_swapChain.GetAddressOf(),m_device.GetAddressOf(),m_context.GetAddressOf());
	#endif

	// 最終的な基盤生成が失敗した場合は、ログ後に部分ResourceをUninitしてfalseを返す。
	if(FAILED(result)){
		WriteGraphicsError(L"D3D11CreateDeviceAndSwapChain",result);
		Uninit();
		return false;
	}

	// Debug構成では成功を記録する。通常生成への再試行で成功した場合も同じ文言を使う。
	#if defined(_DEBUG)
	Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_GRAPHICS,L"Direct3D 11 Deviceを初期化しました。");
	#endif

	// AlphaToCoverageなし、独立Blendなし。RT0のRGBはSrc*SrcAlpha + Dest*(1-SrcAlpha)。
	// Alpha自体はSrcAlpha + DestAlphaというONE/ONEの加算で、RGBと同じ係数には変えない。全RGBAを書込可能にする。
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
	// Blend State生成失敗なら、先行するDevice等もUninitして失敗を返す。
	if(FAILED(blend_state_result)){
		WriteGraphicsError(L"ID3D11Device::CreateBlendState",blend_state_result);
		Uninit();
		return false;
	}

	// Sky/UI用はDepth無効・書込なし・Stencil無効。比較値ALWAYSも既存設定として維持する。
	D3D11_DEPTH_STENCIL_DESC depth_disabled_description{};
	depth_disabled_description.DepthEnable = FALSE;
	depth_disabled_description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depth_disabled_description.DepthFunc = D3D11_COMPARISON_ALWAYS;
	depth_disabled_description.StencilEnable = FALSE;
	const HRESULT depth_disabled_state_result = m_device->CreateDepthStencilState(&depth_disabled_description,m_depthDisabledState.GetAddressOf());
	// Depth無効State生成失敗なら、成功済みBlendと基盤も終了する。
	if(FAILED(depth_disabled_state_result)){
		WriteGraphicsError(L"ID3D11Device::CreateDepthStencilState",depth_disabled_state_result);
		Uninit();
		return false;
	}

	// 半透明3D用はLESS比較で奥行き判定するが、Depthの書込を止める。Stencilは使わない。
	D3D11_DEPTH_STENCIL_DESC depth_read_only_description{};
	depth_read_only_description.DepthEnable = TRUE;
	depth_read_only_description.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depth_read_only_description.DepthFunc = D3D11_COMPARISON_LESS;
	depth_read_only_description.StencilEnable = FALSE;
	const HRESULT depth_read_only_state_result = m_device->CreateDepthStencilState(&depth_read_only_description,m_depthReadOnlyState.GetAddressOf());
	// 読取専用Depth State生成失敗なら、全所有Resourceを部分終了する。
	if(FAILED(depth_read_only_state_result)){
		WriteGraphicsError(L"ID3D11Device::CreateDepthStencilState",depth_read_only_state_result);
		Uninit();
		return false;
	}

	// 3D用はSOLID・背面Cull・DepthClip有効。ゼロ初期化によりFrontCounterClockwise等はFALSEのまま。
	D3D11_RASTERIZER_DESC cull_back_rasterizer_description{};
	cull_back_rasterizer_description.FillMode = D3D11_FILL_SOLID;
	cull_back_rasterizer_description.CullMode = D3D11_CULL_BACK;
	cull_back_rasterizer_description.DepthClipEnable = TRUE;
	const HRESULT cull_back_rasterizer_state_result = m_device->CreateRasterizerState(&cull_back_rasterizer_description,
																																										m_cullBackRasterizerState.GetAddressOf());
	// 背面Cull State生成失敗では先行Stateを含めて解放し、falseへ戻る。
	if(FAILED(cull_back_rasterizer_state_result)){
		WriteGraphicsError(L"ID3D11Device::CreateRasterizerState",cull_back_rasterizer_state_result);
		Uninit();
		return false;
	}

	// Sky/UI用は両面を描くCullなし設定。SOLIDとDepthClip、その他のゼロ初期値は維持する。
	D3D11_RASTERIZER_DESC cull_none_rasterizer_description{};
	cull_none_rasterizer_description.FillMode = D3D11_FILL_SOLID;
	cull_none_rasterizer_description.CullMode = D3D11_CULL_NONE;
	cull_none_rasterizer_description.DepthClipEnable = TRUE;
	const HRESULT cull_none_rasterizer_state_result = m_device->CreateRasterizerState(&cull_none_rasterizer_description,
																																										m_cullNoneRasterizerState.GetAddressOf());
	// CullなしState生成失敗では全所有Resourceを部分終了する。
	if(FAILED(cull_none_rasterizer_state_result)){
		WriteGraphicsError(L"ID3D11Device::CreateRasterizerState",cull_none_rasterizer_state_result);
		Uninit();
		return false;
	}

	// 描画先生成失敗では、下位関数が解放したView以外の基盤とStateもUninitして失敗を返す。
	if(!CreateRenderTargets(width,height)){
		Uninit();
		return false;
	}

	// 全生成後にだけ不透明3D用Stateへ移す。Shaderや描画内容は設定しない。
	SetRenderPass(e_RenderPass::e_OPAQUE);
	return true;
}

// 描画先をBindして色・DepthをClearし、フレームの基本Stateを設定する。
void GraphicsSystem::BeginFrame(const float clearColor[4])
{
	// Context、RTV、DSVのどれかがなければ、BindとClearをまとめて省く。
	if(!m_context || !m_renderTargetView || !m_depthStencilView)return;

	// RTVを先頭から1個とDSVを設定する。配列の個数でありバイト数ではない。Bind状態は以後も残る。
	m_context->OMSetRenderTargets(1,m_renderTargetView.GetAddressOf(),m_depthStencilView.Get());

	// BackBuffer全体を黒で塗る。16:9の描画領域の外側は、この色のまま黒帯として残る。
	const float bar_color[4]{0.0f,0.0f,0.0f,1.0f};
	m_context->ClearRenderTargetView(m_renderTargetView.Get(),bar_color);

	// D3D11.1のContextを取得できた場合は、16:9の描画領域だけを呼出側の色で塗る。取得できない場合は、全体を呼出側の色で塗り、黒帯を作らない。
	ComPtr<ID3D11DeviceContext1> context_1{};
	if(SUCCEEDED(m_context.As(&context_1))){
		const D3D11_RECT area_rect{static_cast<LONG>(m_viewportLeft),static_cast<LONG>(m_viewportTop),
			static_cast<LONG>(m_viewportLeft + m_renderWidth),static_cast<LONG>(m_viewportTop + m_renderHeight)};
		context_1->ClearView(m_renderTargetView.Get(),clearColor,&area_rect,1);
	}
	else{
		m_context->ClearRenderTargetView(m_renderTargetView.Get(),clearColor);
	}

	// Depthだけを1へClearし、Stencil値0の引数を渡すがStencil Clear Flagは付けない。Depthは描画領域より広いBackBuffer全体を塗る。
	m_context->ClearDepthStencilView(m_depthStencilView.Get(),D3D11_CLEAR_DEPTH,1.0f,0);

	// 前フレームのBlend・Depth・Cullを引き継がずOPAQUEへ明示設定する。ViewportとShaderの再設定はここではしない。
	SetRenderPass(e_RenderPass::e_OPAQUE);
}

// 既存の同期間隔1・FlagなしでPresentを要求する。
void GraphicsSystem::EndFrame()
{
	// SwapChain未生成・終了後は表示APIを呼ばない。
	if(!m_swapChain)return;

	// HRESULTを受け取る。Device消失検出後の再生成や、Presentの再試行は追加しない。
	const HRESULT present_result = m_swapChain->Present(1,0);
	// 失敗ならAPI名とHRESULTを記録するだけで、所有状態や戻り値を変更しない。
	if(FAILED(present_result))WriteGraphicsError(L"IDXGISwapChain::Present",present_result);
}

// 旧Viewを手放してからBufferをResizeし、新サイズの描画先を生成する。
void GraphicsSystem::Resize(unsigned int width,unsigned int height)
{
	// 0寸法やSwapChainなしでは何も変更しない。最小化時でも、この経路だけで保持サイズを0にはしない。
	if(width == 0 || height == 0 || !m_swapChain)return;

	// メンバーViewと保持サイズを先に解除する。Contextの旧Bind参照解除はここでは行わない既存構成。
	ReleaseRenderTargets();

	// Buffer数0で既存数を維持し、Format UNKNOWNで既存形式を維持する。Flag 0も既存値のまま。
	const HRESULT resize_buffers_result = m_swapChain->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0);
	// ResizeBuffers失敗時は旧Viewを復元せず戻る。保持サイズは既に0だが、Context側の参照は別に残り得る。
	if(FAILED(resize_buffers_result)){
		WriteGraphicsError(L"IDXGISwapChain::ResizeBuffers",resize_buffers_result);
		return;
	}

	// 新View生成が失敗した場合も、メンバーViewとサイズを空へ揃える。旧描画先への復帰は行わない。
	if(!CreateRenderTargets(width,height))ReleaseRenderTargets();
}

// 所有参照とContextのBind状態を、既存の明示終了順で解除する。
void GraphicsSystem::Uninit()
{
	// 先にViewの所有参照と保持サイズを解除する。Context側のBind解除は次のClearStateで扱う。
	ReleaseRenderTargets();

	// Contextがある場合だけ全StateとBind参照を解除する。GPU処理の完了待機を意味するものではない。
	if(m_context)m_context->ClearState();

	// Contextを保持したまま、各描画Stateの所有参照を解放する。
	m_cullNoneRasterizerState.Reset();
	m_cullBackRasterizerState.Reset();
	m_alphaBlendState.Reset();
	m_depthReadOnlyState.Reset();
	m_depthDisabledState.Reset();

	// SwapChain→即時Context→Deviceの順に所有参照を解放する。外部が持つ別参照はこのResetでは消さない。
	m_swapChain.Reset();
	m_context.Reset();
	m_device.Reset();
}

//========= 描画State設定関数=========
// Blend・Depth・Rasterizerを指定Passの組合せへ変更する。
void GraphicsSystem::SetRenderPass(e_RenderPass renderPass)
{
	// ContextなしではState設定APIを呼ばない。
	if(!m_context)return;

	// SampleMaskの全bit 1は全Sampleを許可する指定。Stencil参照値0も既存値を維持する。
	// nullptrのBlend/Depthはそれぞれ既定State。GetBlendFactorは静的配列で、以前の値の保存先ではない。
	switch(renderPass){
		// 奥行き比較・書込とCullを止め、BlendなしでSkyを描く。以前のStateは復元しない。
	case e_RenderPass::e_SKY:
	m_context->OMSetBlendState(nullptr,GetBlendFactor(),0xffffffff);
	m_context->OMSetDepthStencilState(m_depthDisabledState.Get(),0);
	m_context->RSSetState(m_cullNoneRasterizerState.Get());
	return;

	// 既定Depth、背面Cull、Blendなしへ揃える。以前のStateは復元しない。
	case e_RenderPass::e_OPAQUE:
	m_context->OMSetBlendState(nullptr,GetBlendFactor(),0xffffffff);
	m_context->OMSetDepthStencilState(nullptr,0);
	m_context->RSSetState(m_cullBackRasterizerState.Get());
	return;

	// Depth比較を維持して書込だけを止め、Alpha Blendと背面Cullを使う。以前のStateは復元しない。
	case e_RenderPass::e_TRANSPARENT:
	m_context->OMSetBlendState(m_alphaBlendState.Get(),GetBlendFactor(),0xffffffff);
	m_context->OMSetDepthStencilState(m_depthReadOnlyState.Get(),0);
	m_context->RSSetState(m_cullBackRasterizerState.Get());
	return;

	// DepthとCullを止め、Alpha Blendで画面UIを描く。以前のStateは復元しない。
	case e_RenderPass::e_SCREEN_UI:
	m_context->OMSetBlendState(m_alphaBlendState.Get(),GetBlendFactor(),0xffffffff);
	m_context->OMSetDepthStencilState(m_depthDisabledState.Get(),0);
	m_context->RSSetState(m_cullNoneRasterizerState.Get());
	return;

	// 未定義値では新しいStateを設定せず、そのまま戻る。
	default:
	return;
	}
}

// Blendだけを切り替え、他のStateは維持する。
void GraphicsSystem::SetAlphaBlendEnabled(bool isEnabled)
{
	// Context未取得なら何も設定しない。
	if(!m_context)return;

	// trueは所有するAlpha Blend、falseは既定Blend。SampleMaskは全bit有効で、Depth・Cullは変更しない。
	m_context->OMSetBlendState(isEnabled ? m_alphaBlendState.Get() : nullptr,GetBlendFactor(),0xffffffff);
}

// Depthだけを切り替え、Blend・Cullは維持する。
void GraphicsSystem::SetDepthTestEnabled(bool isEnabled)
{
	// Context未取得なら何も設定しない。
	if(!m_context)return;

	// trueは既定Depth、falseは比較と書込が無効なState。既定Depthへの復帰であり、呼出前Stateの復元ではない。
	m_context->OMSetDepthStencilState(isEnabled ? nullptr : m_depthDisabledState.Get(),0);
}

//========= 補助関数=========
// BackBufferのRTV、Depth用Texture/DSV、16:9のViewportと保持サイズを準備する。
bool GraphicsSystem::CreateRenderTargets(unsigned int width,unsigned int height)
{
	// Device・Context・SwapChainが揃わなければ生成を始めない。この分岐自体では既存Viewを解除しない。
	if(!m_device || !m_context || !m_swapChain)return false;

	// Index 0のBackBufferを一時ComPtrで所有する。要求Interfaceと出力先をIID_PPV_ARGSで対応させる。
	ComPtr<ID3D11Texture2D> back_buffer{};
	const HRESULT get_buffer_result = m_swapChain->GetBuffer(0,IID_PPV_ARGS(back_buffer.GetAddressOf()));
	// BackBuffer取得失敗ならViewの所有参照と保持サイズを解除してfalseを返す。
	if(FAILED(get_buffer_result)){
		WriteGraphicsError(L"IDXGISwapChain::GetBuffer",get_buffer_result);
		ReleaseRenderTargets();
		return false;
	}

	// nullptrのView記述でResourceに基づく既定RTVを作る。Texture本体とViewは別のCOM所有参照。
	const HRESULT render_target_view_result = m_device->CreateRenderTargetView(back_buffer.Get(),nullptr,m_renderTargetView.GetAddressOf());
	// RTV生成失敗なら描画先を部分解放する。一時BackBuffer参照もローカルの寿命に従って解放する。
	if(FAILED(render_target_view_result)){
		WriteGraphicsError(L"ID3D11Device::CreateRenderTargetView",render_target_view_result);
		ReleaseRenderTargets();
		return false;
	}

	// Depthは指定pixel寸法、1Mip・1画像・非MSAA。D24_UNORM_S8_UINTは24bit Depthと8bit Stencilの形式。
	// BindはDepth Stencil用途。ゼロ初期値のUsage=DEFAULT、Quality/CPUAccessFlags/MiscFlags=0を維持する。
	// 寸法はBackBuffer全体で、描画領域（Viewport）より小さくならない。
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
	// Depth Texture生成失敗なら、成功済みRTVを含め描画先を解放して戻る。StateやDeviceはこの関数では解放しない。
	if(FAILED(depth_texture_result)){
		WriteGraphicsError(L"ID3D11Device::CreateTexture2D",depth_texture_result);
		ReleaseRenderTargets();
		return false;
	}

	// 既定記述のDSVを作る。成功したDSVがTextureを参照するため、関数終了後にローカルTexture参照を解放できる。
	const HRESULT depth_stencil_view_result = m_device->CreateDepthStencilView(depth_texture.Get(),nullptr,m_depthStencilView.GetAddressOf());
	// DSV生成失敗ならRTV側も解放する。初期化元がInitなら、戻った先でさらに基盤全体を終了する。
	if(FAILED(depth_stencil_view_result)){
		WriteGraphicsError(L"ID3D11Device::CreateDepthStencilView",depth_stencil_view_result);
		ReleaseRenderTargets();
		return false;
	}

	// BackBufferへ収まる最大の16:9の領域を求め、その位置・大きさのViewportを1個Bindする。Depth範囲は0〜1。
	// 領域の外側はViewportの外のため描かれず、BeginFrameで塗った黒のまま残る。
	const VIEWPORT_AREA area = ViewportFit::Calculate(width,height);
	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = static_cast<float>(area.left);
	viewport.TopLeftY = static_cast<float>(area.top);
	viewport.Width = static_cast<float>(area.width);
	viewport.Height = static_cast<float>(area.height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_context->RSSetViewports(1,&viewport);

	// 全生成後にだけ領域の位置と寸法を確定する。この関数はRTV/DSVのOM BindやClearまでは行わない。
	m_viewportLeft = area.left;
	m_viewportTop = area.top;
	m_renderWidth = area.width;
	m_renderHeight = area.height;
	return true;
}

// メンバーの描画先Viewを解除し、利用側へ未生成サイズを返す状態へ戻す。
void GraphicsSystem::ReleaseRenderTargets()
{
	// メンバーのCOM参照だけを解放する。Resizeでも使うため、ここにContextのUnbindやClearStateは追加しない。
	m_depthStencilView.Reset();
	m_renderTargetView.Reset();

	// 保持する描画領域を0に揃える。Context側に別参照が残る場合でも、Getterはこの保持値を返す。
	m_viewportLeft = {};
	m_viewportTop = {};
	m_renderWidth = {};
	m_renderHeight = {};
}