#include "../H/HudRenderer.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>
#include <array>
#include <string>
#include <vector>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/Etc/H/Logger.h"

namespace
{
	//========= 画面サイズ定数=========
		// HUD描画の基準画面サイズ。
	constexpr float SCREEN_WIDTH = 1280.0f;
	constexpr float SCREEN_HEIGHT = 720.0f;

	//========= 照準描画定数=========
	// 画面中央へ表示する照準の位置とサイズ。
	constexpr float CROSSHAIR_CENTER_X = SCREEN_WIDTH * 0.5f;
	constexpr float CROSSHAIR_CENTER_Y = SCREEN_HEIGHT * 0.5f;
	constexpr float CROSSHAIR_LINE_LENGTH = 22.0f;
	constexpr float CROSSHAIR_LINE_THICKNESS = 3.0f;

	//========= HPバー描画定数=========
	// プレイヤーHPバーの位置、サイズ、内側余白。
	constexpr float HP_BAR_POSITION_X = 36.0f;
	constexpr float HP_BAR_POSITION_Y = 650.0f;
	constexpr float HP_BAR_WIDTH = 300.0f;
	constexpr float HP_BAR_HEIGHT = 26.0f;
	constexpr float HP_BAR_BORDER = 4.0f;

	//========= 低HP警告定数=========
	// 低HP判定の割合と画面端の警告枠の太さ。
	constexpr float LOW_HP_RATIO = 0.25f;
	constexpr float LOW_HP_BORDER_THICKNESS = 12.0f;

	//========= Alpha定数=========
	// HUD描画に使用する不透明度。
	constexpr float FULLY_OPAQUE_ALPHA = 1.0f;
	constexpr float HUD_FOREGROUND_ALPHA = 0.95f;
	constexpr float HUD_BACKGROUND_ALPHA = 0.70f;
	constexpr float WARNING_ALPHA = 0.28f;

	//========= Shaderファイルパス定数=========
	// HUD描画に使用するShaderのCSOファイルパス。
	constexpr const char* HUD_VERTEX_SHADER_PATH = "Shaders/HudVS.cso";
	constexpr const char* HUD_PIXEL_SHADER_PATH = "Shaders/HudPS.cso";

	//========= 補助関数=========
	// GraphicsカテゴリでShader生成失敗を出力する。
	// GraphicsカテゴリでHUD描画Resource生成失敗を出力する。
	void WriteHudGraphicsError( const wchar_t* functionName, HRESULT result )
	{
		std::wstring message{ functionName };
		message += L" に失敗しました。HRESULT: 0x";

		constexpr wchar_t hexDigits[]{ L"0123456789ABCDEF" };
		const unsigned long resultValue = static_cast<unsigned long>( result );

		for ( int digitIndex = 7; digitIndex >= 0; --digitIndex )
		{
			const unsigned long digit = ( resultValue >> digitIndex * 4 ) & 0x0f;
			message += hexDigits[ digit ];
		}

		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_GRAPHICS, message );
	}
}

// HUD描画に必要なShader、Buffer、Input Layoutを初期化する。
bool HudRenderer::Initialize( GraphicsSystem& graphicsSystem )
{
	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	if ( device == nullptr ) return false;

	// Vertex ShaderとPixel ShaderのCSOデータを読み込む。
	std::vector<char> vertexShaderData{};
	std::vector<char> pixelShaderData{};

	if ( !ShaderBinaryLoader::Load( HUD_VERTEX_SHADER_PATH, vertexShaderData ) )
	{
		Uninit();
		return false;
	}

	if ( !ShaderBinaryLoader::Load( HUD_PIXEL_SHADER_PATH, pixelShaderData ) )
	{
		Uninit();
		return false;
	}

	// HUD Quadの描画に使用するVertex Shaderを生成する。
	const HRESULT vertexShaderResult = device->CreateVertexShader(
	vertexShaderData.data(),
	vertexShaderData.size(),
	nullptr,
	m_VertexShader.GetAddressOf() );

	if ( FAILED( vertexShaderResult ) )
	{
		WriteHudGraphicsError( L"ID3D11Device::CreateVertexShader", vertexShaderResult );
		Uninit();
		return false;
	}

	// HUD Quadの描画に使用するPixel Shaderを生成する。
	const HRESULT pixelShaderResult = device->CreatePixelShader(
	pixelShaderData.data(),
	pixelShaderData.size(),
	nullptr,
	m_PixelShader.GetAddressOf() );

	if ( FAILED( pixelShaderResult ) )
	{
		WriteHudGraphicsError( L"ID3D11Device::CreatePixelShader", pixelShaderResult );
		Uninit();
		return false;
	}

	// 頂点座標をVertex Shader入力へ対応付けるInput Layoutを生成する。
	const D3D11_INPUT_ELEMENT_DESC inputElements[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	const HRESULT inputLayoutResult = device->CreateInputLayout(
	inputElements,
	ARRAYSIZE( inputElements ),
	vertexShaderData.data(),
	vertexShaderData.size(),
	m_InputLayout.GetAddressOf() );

	if ( FAILED( inputLayoutResult ) )
	{
		WriteHudGraphicsError( L"ID3D11Device::CreateInputLayout", inputLayoutResult );
		Uninit();
		return false;
	}

	// 0.0から1.0の範囲で表すHUD Quadの頂点を作成する。
	const std::array<Vertex, 4> vertices
	{
		Vertex{ DirectX::XMFLOAT2{ 0.0f, 0.0f } },
		Vertex{ DirectX::XMFLOAT2{ 1.0f, 0.0f } },
		Vertex{ DirectX::XMFLOAT2{ 1.0f, 1.0f } },
		Vertex{ DirectX::XMFLOAT2{ 0.0f, 1.0f } }
	};

	// HUD QuadのVertex Bufferを生成する。
	D3D11_BUFFER_DESC vertexBufferDescription{};
	vertexBufferDescription.ByteWidth = static_cast<UINT>( sizeof( Vertex ) * vertices.size() );
	vertexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexData{};
	vertexData.pSysMem = vertices.data();

	const HRESULT vertexBufferResult = device->CreateBuffer(
	&vertexBufferDescription,
	&vertexData,
	m_VertexBuffer.GetAddressOf() );

	if ( FAILED( vertexBufferResult ) )
	{
		WriteHudGraphicsError( L"ID3D11Device::CreateBuffer(VertexBuffer)", vertexBufferResult );
		Uninit();
		return false;
	}

	// HUD Quadを2つの三角形として描画するIndex配列を作成する。
	const std::array<unsigned short, 6> indices
	{
		0, 1, 2,
		0, 2, 3
	};

	// HUD QuadのIndex Bufferを生成する。
	D3D11_BUFFER_DESC indexBufferDescription{};
	indexBufferDescription.ByteWidth = static_cast<UINT>( sizeof( unsigned short ) * indices.size() );
	indexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
	indexBufferDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices.data();

	const HRESULT indexBufferResult = device->CreateBuffer(
	&indexBufferDescription,
	&indexData,
	m_IndexBuffer.GetAddressOf() );

	if ( FAILED( indexBufferResult ) )
	{
		WriteHudGraphicsError( L"ID3D11Device::CreateBuffer(IndexBuffer)", indexBufferResult );
		Uninit();
		return false;
	}

	// Quadの画面座標、サイズ、色を渡す定数バッファを生成する。
	HRESULT hudBufferResult{};

	if ( !m_HudBuffer.Init( device, hudBufferResult ) )
	{
		WriteHudGraphicsError( L"ID3D11Device::CreateBuffer(HudBuffer)", hudBufferResult );
		Uninit();
		return false;
	}

	m_IndexCount = static_cast<unsigned int>( indices.size() );

	return true;
}

// HUD描画に使用したDirect3Dリソースを解放する。
void HudRenderer::Uninit()
{
	// HUD描画に使用したGPU Bufferを解放する。
	m_HudBuffer.Uninit();
	m_IndexBuffer.Reset();
	m_VertexBuffer.Reset();

	// HUD描画に使用したShaderとInput Layoutを解放する。
	m_InputLayout.Reset();
	m_PixelShader.Reset();
	m_VertexShader.Reset();

	m_IndexCount = {};
}

// 指定した画面座標、サイズ、色で単色Quadを描画する。
void HudRenderer::DrawQuad(
	GraphicsSystem& graphicsSystem,
	float positionX,
	float positionY,
	float width,
	float height,
	const DirectX::XMFLOAT4& color )
{
	if ( width <= 0.0f || height <= 0.0f ) return;

	// 描画に使用するDirect3D ContextとGPUリソースを確認する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	if ( context == nullptr || !m_VertexShader || !m_PixelShader || !m_InputLayout ||
	!m_VertexBuffer || !m_IndexBuffer || !m_HudBuffer.IsValid() ) return;

	// Shaderへ渡すQuadの画面座標、サイズ、色をまとめる。
	const HudBuffer hudBuffer
	{
		DirectX::XMFLOAT4{ positionX, positionY, width, height },
		color
	};

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	const UINT stride = sizeof( Vertex );
	const UINT offset{};
	ID3D11Buffer* vertexBuffers[]{ m_VertexBuffer.Get() };

	// Shaderへ設定する定数Bufferをまとめる。
	ID3D11Buffer* constantBuffers[]{ m_HudBuffer.Get() };

	m_HudBuffer.Update( context, hudBuffer );

	context->IASetInputLayout( m_InputLayout.Get() );
	context->IASetVertexBuffers( 0, 1, vertexBuffers, &stride, &offset );
	context->IASetIndexBuffer( m_IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0 );
	context->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	context->VSSetShader( m_VertexShader.Get(), nullptr, 0 );
	context->PSSetShader( m_PixelShader.Get(), nullptr, 0 );
	context->VSSetConstantBuffers( 0, 1, constantBuffers );
	context->PSSetConstantBuffers( 0, 1, constantBuffers );

	// 半透明のQuadではAlpha Blendを有効化する。
	graphicsSystem.SetAlphaBlendEnabled( color.w < FULLY_OPAQUE_ALPHA );

	context->DrawIndexed( m_IndexCount, 0, 0 );
}

// 画面中央に照準を描画する。
void HudRenderer::DrawCrosshair( GraphicsSystem& graphicsSystem )
{
	// 横線と縦線で構成する照準の色を設定する。
	const DirectX::XMFLOAT4 crosshairColor{ 1.0f, 1.0f, 1.0f, HUD_FOREGROUND_ALPHA };

	// 画面中央へ横線と縦線のQuadを描画する。
	DrawQuad(
		graphicsSystem,
		CROSSHAIR_CENTER_X - CROSSHAIR_LINE_LENGTH * 0.5f,
		CROSSHAIR_CENTER_Y - CROSSHAIR_LINE_THICKNESS * 0.5f,
		CROSSHAIR_LINE_LENGTH,
		CROSSHAIR_LINE_THICKNESS,
		crosshairColor );
	DrawQuad(
		graphicsSystem,
		CROSSHAIR_CENTER_X - CROSSHAIR_LINE_THICKNESS * 0.5f,
		CROSSHAIR_CENTER_Y - CROSSHAIR_LINE_LENGTH * 0.5f,
		CROSSHAIR_LINE_THICKNESS,
		CROSSHAIR_LINE_LENGTH,
		crosshairColor );
}

// 現在HPと最大HPに応じたプレイヤーHPバーを描画する。
void HudRenderer::DrawPlayerHealthBar( GraphicsSystem& graphicsSystem, float currentHp, float maxHp )
{
	if ( maxHp <= 0.0f ) return;

	// 現在HPを0.0から1.0の表示割合へ変換する。
	const float hpRatio = std::clamp( currentHp / maxHp, 0.0f, FULLY_OPAQUE_ALPHA );

	// HPバーの背景を描画する。
	DrawQuad(
		graphicsSystem,
		HP_BAR_POSITION_X,
		HP_BAR_POSITION_Y,
		HP_BAR_WIDTH,
		HP_BAR_HEIGHT,
		DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, HUD_BACKGROUND_ALPHA } );

	// 現在HPに応じたHPバーの幅と色を決定する。
	const float healthWidth = ( HP_BAR_WIDTH - HP_BAR_BORDER * 2.0f ) * hpRatio;
	const DirectX::XMFLOAT4 healthColor = hpRatio > LOW_HP_RATIO ?
		DirectX::XMFLOAT4{ 0.10f, 0.95f, 0.20f, HUD_FOREGROUND_ALPHA } :
		DirectX::XMFLOAT4{ FULLY_OPAQUE_ALPHA, 0.15f, 0.10f, HUD_FOREGROUND_ALPHA };

	// HPバーの現在値を背景の内側へ描画する。
	DrawQuad(
		graphicsSystem,
		HP_BAR_POSITION_X + HP_BAR_BORDER,
		HP_BAR_POSITION_Y + HP_BAR_BORDER,
		healthWidth,
		HP_BAR_HEIGHT - HP_BAR_BORDER * 2.0f,
		healthColor );
}

// HPが低い場合に画面端の警告枠を描画する。
void HudRenderer::DrawLowHealthWarning( GraphicsSystem& graphicsSystem, float currentHp, float maxHp )
{
	if ( maxHp <= 0.0f || currentHp / maxHp > LOW_HP_RATIO ) return;

	// 画面端へ描画する半透明の赤い警告色を設定する。
	const DirectX::XMFLOAT4 warningColor{ FULLY_OPAQUE_ALPHA, 0.0f, 0.0f, WARNING_ALPHA };

	// 上下左右のQuadで低HP警告枠を描画する。
	DrawQuad( graphicsSystem, 0.0f, 0.0f, SCREEN_WIDTH, LOW_HP_BORDER_THICKNESS, warningColor );
	DrawQuad(
		graphicsSystem,
		0.0f,
		SCREEN_HEIGHT - LOW_HP_BORDER_THICKNESS,
		SCREEN_WIDTH,
		LOW_HP_BORDER_THICKNESS,
		warningColor );
	DrawQuad( graphicsSystem, 0.0f, 0.0f, LOW_HP_BORDER_THICKNESS, SCREEN_HEIGHT, warningColor );
	DrawQuad(
		graphicsSystem,
		SCREEN_WIDTH - LOW_HP_BORDER_THICKNESS,
		0.0f,
		LOW_HP_BORDER_THICKNESS,
		SCREEN_HEIGHT,
		warningColor );
}