#include "../H/HudRenderer.h"

// HUD用Resourceの生成・解放と、Virtual Canvasを使用したQuad描画を実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <array>
#include <string>
#include <vector>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/Etc/H/Logger.h"

// HUDの配置・外観・Shaderパス定数と、内部用の失敗ログ処理をまとめる。
namespace
{
	//========= 画面サイズ定数=========
	// HUD描画の基準画面サイズ。
	constexpr float ns_SCREEN_WIDTH = 1280.0f;
	constexpr float ns_SCREEN_HEIGHT = 720.0f;

	//========= 照準描画定数=========
	// 画面中央へ表示する照準の位置とサイズ。
	constexpr float ns_CROSSHAIR_CENTER_X = ns_SCREEN_WIDTH * 0.5f;
	constexpr float ns_CROSSHAIR_CENTER_Y = ns_SCREEN_HEIGHT * 0.5f;
	constexpr float ns_CROSSHAIR_LINE_LENGTH = 22.0f;
	constexpr float ns_CROSSHAIR_LINE_THICKNESS = 3.0f;

	//========= HPバー描画定数=========
	// プレイヤーHPバーの位置、サイズ、内側余白。
	constexpr float ns_HP_BAR_POSITION_X = 36.0f;
	constexpr float ns_HP_BAR_POSITION_Y = 650.0f;
	constexpr float ns_HP_BAR_WIDTH = 300.0f;
	constexpr float ns_HP_BAR_HEIGHT = 26.0f;
	constexpr float ns_HP_BAR_BORDER = 4.0f;

	//========= 低HP警告定数=========
	// 低HP判定の割合と画面端の警告枠の太さ。
	constexpr float ns_LOW_HP_RATIO = 0.25f;
	constexpr float ns_LOW_HP_BORDER_THICKNESS = 12.0f;

	//========= Alpha定数=========
	// HUD描画に使用する不透明度。
	constexpr float ns_FULLY_OPAQUE_ALPHA = 1.0f;
	constexpr float ns_HUD_FOREGROUND_ALPHA = 0.95f;
	constexpr float ns_HUD_BACKGROUND_ALPHA = 0.70f;
	constexpr float ns_WARNING_ALPHA = 0.28f;

	//========= Shaderファイルパス定数=========
	// HUD描画に使用するShaderのCSOファイルパス。
	constexpr const char* ns_HUD_VERTEX_SHADER_PATH = "Shaders/HudVS.cso";
	constexpr const char* ns_HUD_PIXEL_SHADER_PATH = "Shaders/HudPS.cso";

	//========= 補助関数=========
	// GraphicsカテゴリへHUD Resource生成失敗を出力する。
	// functionName: 失敗した処理を示す有効な文字列。nullptr不可。
	// result: 16進数8桁で出力するHRESULT。
	void WriteHUDGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: 0x";

		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);

		for(int digit_index = 7; digit_index >= 0; --digit_index){
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			message += HEX_DIGITS[digit];
		}

		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}
}

// HUD描画に必要なShader、Buffer、Input Layoutを初期化する。
bool HudRenderer::Initialize(GraphicsSystem& graphicsSystem)
{
	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	if(device == nullptr)return false;

	// VERTEX ShaderとPixel ShaderのCSOデータを読み込む。
	std::vector<char> vertex_shader_data{};
	std::vector<char> pixel_shader_data{};

	if(!ShaderBinaryLoader::Load(ns_HUD_VERTEX_SHADER_PATH,vertex_shader_data)){
		Uninit();
		return false;
	}

	if(!ShaderBinaryLoader::Load(ns_HUD_PIXEL_SHADER_PATH,pixel_shader_data)){
		Uninit();
		return false;
	}

	// HUD Quadの描画に使用するVertex Shaderを生成する。
	const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_data.data(),vertex_shader_data.size(),
																																	nullptr,m_vertexShader.GetAddressOf());

	if(FAILED(vertex_shader_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
		Uninit();
		return false;
	}

	// HUD Quadの描画に使用するPixel Shaderを生成する。
	const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_data.data(),pixel_shader_data.size(),
																																nullptr,m_pixelShader.GetAddressOf());

	if(FAILED(pixel_shader_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
		Uninit();
		return false;
	}

	// 頂点座標をVertex Shader入力へ対応付けるInput Layoutを生成する。
	const D3D11_INPUT_ELEMENT_DESC input_elements[]
	{
		{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0}
	};

	const HRESULT input_layout_result = device->CreateInputLayout(input_elements,ARRAYSIZE(input_elements),
																															 vertex_shader_data.data(),vertex_shader_data.size(),m_inputLayout.GetAddressOf());

	if(FAILED(input_layout_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateInputLayout",input_layout_result);
		Uninit();
		return false;
	}

	// 0.0から1.0の範囲で表すHUD Quadの頂点を作成する。
	const std::array<VERTEX,4> vertices
	{
		VERTEX{XMFLOAT2{0.0f,0.0f}},
		VERTEX{XMFLOAT2{1.0f,0.0f}},
		VERTEX{XMFLOAT2{1.0f,1.0f}},
		VERTEX{XMFLOAT2{0.0f,1.0f}}
	};

	// HUD QuadのVertex Bufferを生成する。
	D3D11_BUFFER_DESC vertex_buffer_description{};
	vertex_buffer_description.ByteWidth = static_cast<UINT>(sizeof(VERTEX) * vertices.size());
	vertex_buffer_description.Usage = D3D11_USAGE_IMMUTABLE;
	vertex_buffer_description.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices.data();

	const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_description,&vertex_data,m_vertexBuffer.GetAddressOf());

	if(FAILED(vertex_buffer_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateBuffer(VertexBuffer)",vertex_buffer_result);
		Uninit();
		return false;
	}

	// HUD Quadを2つの三角形として描画するIndex配列を作成する。
	const std::array<unsigned short,6> indices{0,1,2,0,2,3};

	// HUD QuadのIndex Bufferを生成する。
	D3D11_BUFFER_DESC index_buffer_description{};
	index_buffer_description.ByteWidth = static_cast<UINT>(sizeof(unsigned short) * indices.size());
	index_buffer_description.Usage = D3D11_USAGE_IMMUTABLE;
	index_buffer_description.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA index_data{};
	index_data.pSysMem = indices.data();

	const HRESULT index_buffer_result = device->CreateBuffer(&index_buffer_description,&index_data,m_indexBuffer.GetAddressOf());

	if(FAILED(index_buffer_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateBuffer(IndexBuffer)",index_buffer_result);
		Uninit();
		return false;
	}

	// Quadの画面座標、サイズ、色を渡す定数バッファを生成する。
	HRESULT HUD_buffer_result{};
	if(!m_HUDBuffer.Init(device,HUD_buffer_result)){
		WriteHUDGraphicsError(L"ID3D11Device::CreateBuffer(HudBuffer)",HUD_buffer_result);
		Uninit();
		return false;
	}

	m_indexCount = static_cast<unsigned int>(indices.size());

	return true;
}

// 指定したVirtual Canvas座標、サイズ、色で単色Quadを描画する。
void HudRenderer::DrawQuad(GraphicsSystem& graphicsSystem,float positionX,float positionY,float width,float height,
													 const XMFLOAT4& color)
{
	if(width <= 0.0f || height <= 0.0f)return;

	// 描画に使用するDirect3D ContextとRender Targetサイズを確認する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	const unsigned int render_width = graphicsSystem.GetRenderWidth();
	const unsigned int render_height = graphicsSystem.GetRenderHeight();
	if(context == nullptr || render_width == 0 || render_height == 0 ||
		 !m_vertexShader || !m_pixelShader || !m_inputLayout ||
		 !m_vertexBuffer || !m_indexBuffer || !m_HUDBuffer.IsValid())return;

	// Virtual Canvas座標とサイズを現在のRender Target座標へ変換する。
	const HudCanvas HUD_canvas{render_width,render_height};
	const XMFLOAT2 screen_position = HUD_canvas.ToScreenPosition(XMFLOAT2{positionX,positionY});
	const XMFLOAT2 screen_size = HUD_canvas.ToScreenSize(XMFLOAT2{width,height});

	// Shaderへ渡すQuadの画面座標、サイズ、色、Render Targetサイズをまとめる。
	const HUD_BUFFER HUD_buffer
	{
		XMFLOAT4{screen_position.x,screen_position.y,screen_size.x,screen_size.y},
		color,
		XMFLOAT2{static_cast<float>(render_width),static_cast<float>(render_height)},
		XMFLOAT2{}
	};

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	const UINT stride = sizeof(VERTEX);
	const UINT offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};

	// Shaderへ設定する定数Bufferをまとめる。
	ID3D11Buffer* constant_buffers[]{m_HUDBuffer.Get()};

	m_HUDBuffer.Update(context,HUD_buffer);

	context->IASetInputLayout(m_inputLayout.Get());
	context->IASetVertexBuffers(0,1,vertex_buffers,&stride,&offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R16_UINT,0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->VSSetShader(m_vertexShader.Get(),nullptr,0);
	context->PSSetShader(m_pixelShader.Get(),nullptr,0);
	context->VSSetConstantBuffers(0,1,constant_buffers);
	context->PSSetConstantBuffers(0,1,constant_buffers);

	// 半透明のQuadではAlpha Blendを有効化する。
	graphicsSystem.SetAlphaBlendEnabled(color.w < ns_FULLY_OPAQUE_ALPHA);

	context->DrawIndexed(m_indexCount,0,0);
}

// 画面中央に照準を描画する。
void HudRenderer::DrawCrosshair(GraphicsSystem& graphicsSystem)
{
	// 横線と縦線で構成する照準の色を設定する。
	const XMFLOAT4 crosshair_color{1.0f,1.0f,1.0f,ns_HUD_FOREGROUND_ALPHA};

	// 画面中央へ横線と縦線のQuadを描画する。
	DrawQuad(graphicsSystem,ns_CROSSHAIR_CENTER_X - ns_CROSSHAIR_LINE_LENGTH * 0.5f,ns_CROSSHAIR_CENTER_Y - ns_CROSSHAIR_LINE_THICKNESS * 0.5f,
					 ns_CROSSHAIR_LINE_LENGTH,ns_CROSSHAIR_LINE_THICKNESS,crosshair_color);

	DrawQuad(graphicsSystem,ns_CROSSHAIR_CENTER_X - ns_CROSSHAIR_LINE_THICKNESS * 0.5f,ns_CROSSHAIR_CENTER_Y - ns_CROSSHAIR_LINE_LENGTH * 0.5f,
					 ns_CROSSHAIR_LINE_THICKNESS,ns_CROSSHAIR_LINE_LENGTH,crosshair_color);
}

// 現在HPと最大HPに応じたプレイヤーHPバーを描画する。
void HudRenderer::DrawPlayerHealthBar(GraphicsSystem& graphicsSystem,float currentHP,float maxHP)
{
	if(maxHP <= 0.0f)return;

	// 現在HPを0.0から1.0の表示割合へ変換する。
	const float HP_ratio = std::clamp(currentHP / maxHP,0.0f,ns_FULLY_OPAQUE_ALPHA);

	// HPバーの背景を描画する。
	DrawQuad(graphicsSystem,ns_HP_BAR_POSITION_X,ns_HP_BAR_POSITION_Y,ns_HP_BAR_WIDTH,ns_HP_BAR_HEIGHT,XMFLOAT4{0.0f,0.0f,0.0f,ns_HUD_BACKGROUND_ALPHA});

	// 現在HPに応じたHPバーの幅と色を決定する。
	const float health_width = (ns_HP_BAR_WIDTH - ns_HP_BAR_BORDER * 2.0f) * HP_ratio;
	const XMFLOAT4 health_color = HP_ratio > ns_LOW_HP_RATIO ?
		XMFLOAT4{0.10f,0.95f,0.20f,ns_HUD_FOREGROUND_ALPHA} :
		XMFLOAT4{ns_FULLY_OPAQUE_ALPHA,0.15f,0.10f,ns_HUD_FOREGROUND_ALPHA};

	// HPバーの現在値を背景の内側へ描画する。
	DrawQuad(graphicsSystem,ns_HP_BAR_POSITION_X + ns_HP_BAR_BORDER,ns_HP_BAR_POSITION_Y + ns_HP_BAR_BORDER,health_width,ns_HP_BAR_HEIGHT - ns_HP_BAR_BORDER * 2.0f,health_color);
}

// HPが低い場合に画面端の警告枠を描画する。
void HudRenderer::DrawLowHealthWarning(GraphicsSystem& graphicsSystem,float currentHP,float maxHP)
{
	if(maxHP <= 0.0f || currentHP / maxHP > ns_LOW_HP_RATIO)return;

	// 画面端へ描画する半透明の赤い警告色を設定する。
	const XMFLOAT4 warning_color{ns_FULLY_OPAQUE_ALPHA,0.0f,0.0f,ns_WARNING_ALPHA};

	// 上下左右のQuadで低HP警告枠を描画する。
	DrawQuad(graphicsSystem,0.0f,0.0f,ns_SCREEN_WIDTH,ns_LOW_HP_BORDER_THICKNESS,warning_color);
	DrawQuad(graphicsSystem,0.0f,ns_SCREEN_HEIGHT - ns_LOW_HP_BORDER_THICKNESS,ns_SCREEN_WIDTH,ns_LOW_HP_BORDER_THICKNESS,warning_color);
	DrawQuad(graphicsSystem,0.0f,0.0f,ns_LOW_HP_BORDER_THICKNESS,ns_SCREEN_HEIGHT,warning_color);
	DrawQuad(graphicsSystem,ns_SCREEN_WIDTH - ns_LOW_HP_BORDER_THICKNESS,0.0f,ns_LOW_HP_BORDER_THICKNESS,ns_SCREEN_HEIGHT,warning_color);
}

// HUD描画に使用したDirect3Dリソースを解放する。
void HudRenderer::Uninit()
{
	// HUD描画に使用したGPU Bufferを解放する。
	m_HUDBuffer.Uninit();
	m_indexBuffer.Reset();
	m_vertexBuffer.Reset();

	// HUD描画に使用したShaderとInput Layoutを解放する。
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_vertexShader.Reset();

	m_indexCount = {};
}