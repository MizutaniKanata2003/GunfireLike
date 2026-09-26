#pragma once

//========= DirectX インクルード=========
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

//========= Framework インクルード=========
#include "Framework/DirectX/H/ConstantBuffer.h"

//========= 前方宣言=========
class GraphicsSystem;

// ImGuiに依存せず、画面座標で単色2D Quadを描画するHUDレンダラー。
class HudRenderer final
{
public:
	//========= 初期化・終了関数=========
	// HUD描画に必要なShader、Buffer、Input Layoutを初期化する。
	bool Initialize( GraphicsSystem& graphicsSystem );
	// HUD描画に使用したDirect3Dリソースを解放する。
	void Uninit();

	//========= 描画関数=========
	// 指定した画面座標、サイズ、色で単色Quadを描画する。
	void DrawQuad(
		GraphicsSystem& graphicsSystem,
		float positionX,
		float positionY,
		float width,
		float height,
		const DirectX::XMFLOAT4& color );
	// 画面中央に照準を描画する。
	void DrawCrosshair( GraphicsSystem& graphicsSystem );
	// 現在HPと最大HPに応じたプレイヤーHPバーを描画する。
	void DrawPlayerHealthBar( GraphicsSystem& graphicsSystem, float currentHp, float maxHp );
	// HPが低い場合に画面端の警告枠を描画する。
	void DrawLowHealthWarning( GraphicsSystem& graphicsSystem, float currentHp, float maxHp );

private:
	//========= 構造体=========
	// HUD Quadの頂点座標。
	struct Vertex
	{
		DirectX::XMFLOAT2 position{};
	};

	// Shaderへ渡すQuadの矩形情報と色。
	struct HudBuffer
	{
		DirectX::XMFLOAT4 rectangle{};
		DirectX::XMFLOAT4 color{};
	};

	//========= Shader関連=========
	// HUD描画に使用するVertex Shader。
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_VertexShader{};
	// HUD描画に使用するPixel Shader。
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PixelShader{};
	// Vertex構造とVertex Shader入力を対応付けるInput Layout。
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout{};

	//========= Buffer関連=========
	// HUD Quadの頂点情報を保持するVertex Buffer。
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_VertexBuffer{};
	// HUD Quadの三角形順序を保持するIndex Buffer。
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_IndexBuffer{};
	// Quadの矩形情報と色をShaderへ渡す定数バッファ。
	ConstantBuffer<HudBuffer> m_HudBuffer{};

	//========= 描画情報=========
	// DrawIndexedに渡すHUD QuadのIndex数。
	unsigned int m_IndexCount{};
};
