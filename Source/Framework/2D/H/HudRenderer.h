#pragma once

// Virtual Canvas座標で単色Quad、照準、HPバー、低HP警告を描画するGPU Resourceを所有する。
// HPの更新やScene進行は担当しない。GraphicsSystemは非所有で、各呼出中だけ参照する。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/2D/H/HudCanvas.h"
#include "Framework/DirectX/H/ConstantBuffer.h"
#include "Framework/DirectX/H/VertexFormat.h"

//========= 前方宣言=========
class GraphicsSystem;

// ImGuiに依存せず、画面座標で単色2D Quadを描画するHUDレンダラー。
class HudRenderer final
{
public:
	//========= Getter関数=========
	// HUD Quad描画で使用する頂点形式を返す。
	[[nodiscard]] VERTEX_FORMAT GetVertexFormat() const { return VERTEX_FORMAT::e_POSITION; }

	//========= 初期化関数=========
	// Shader、Buffer、Input Layoutを生成する。再初期化前にはUninitを呼んでおく。
	// graphicsSystem: 初期化済み描画基盤。Resource生成用Deviceを取得するための非所有参照。
	// 戻り値: 生成成功ならtrue。Device未取得、Shader読込・Resource生成失敗ならfalse。
	// 注意: Device未取得時はそのまま戻り、それ以外の検出した失敗時はUninitで部分解放する。
	bool Initialize(GraphicsSystem& graphicsSystem);

	//========= 描画関数=========
	// Virtual Canvas上の矩形をRender Targetへ変換し、単色Quadを描画する。
	// graphicsSystem: Context・描画サイズ・Blend設定に使う非所有参照。
	// positionX: 矩形左上のX座標。単位はVirtual Canvasの基準座標。
	// positionY: 矩形左上のY座標。下向きを正とする。
	// width: 矩形幅。0以下なら描画しない。
	// height: 矩形高さ。0以下なら描画しない。
	// color: RGBA色。Alphaが1未満ならAlpha Blendを有効にする。
	// 注意: Context・描画サイズ・Resourceが無効なら描画しない。Bind/Blend状態は元へ戻さない。
	// Render Target・Viewport・適切なRender Passは呼出側で準備する。
	void DrawQuad(GraphicsSystem& graphicsSystem,float positionX,float positionY,float width,float height,const XMFLOAT4& color);
	// Virtual Canvas中央に、横線と縦線を組み合わせた照準を描画する。
	// graphicsSystem: DrawQuadへ渡す描画基盤。Bind/Blend状態を変更する。
	void DrawCrosshair(GraphicsSystem& graphicsSystem);
	// 背景と現在HPの割合に応じたバーを描画する。
	// graphicsSystem: DrawQuadへ渡す描画基盤。Bind/Blend状態を変更する。
	// currentHP: 現在HP。表示割合は0〜1へ制限する。
	// maxHP: 最大HP。0以下の場合は描画しない。
	void DrawPlayerHealthBar(GraphicsSystem& graphicsSystem,float currentHP,float maxHP);
	// HP割合が25%以下の場合、Virtual Canvasの四辺へ警告枠を描画する。
	// graphicsSystem: DrawQuadへ渡す描画基盤。Bind/Blend状態を変更する。
	// currentHP: 現在HP。低HP判定では最大HPとの比をそのまま使用する。
	// maxHP: 最大HP。0以下の場合は描画しない。
	void DrawLowHealthWarning(GraphicsSystem& graphicsSystem,float currentHP,float maxHP);

	//========= 終了関数=========
	// 所有するGPU Resourceを解放し、Index数を0へ戻す。未初期化・解放済みでも呼出可能。
	void Uninit();

private:
	//========= 構造体=========
	// HUD Quadの頂点座標。
	struct VERTEX
	{
		// 0〜1で表すQuad内の位置。Shaderで矩形サイズと画面座標を反映する。
		XMFLOAT2 position{};
	};

	// Shaderへ渡すQuadの矩形情報、色、Render Targetサイズ。b0のHudBufferと配置を一致させる。
	struct HUD_BUFFER
	{
		// Render Target座標の左上X・左上Y・幅・高さ。単位はpixel。
		XMFLOAT4 rectangle{};
		// QuadのRGBA色。
		XMFLOAT4 color{};
		// Render Targetの幅と高さ。Shaderでの座標正規化に使う。
		XMFLOAT2 renderSize{};
		// HLSLとの配置と48バイトの構造体サイズを維持する余白。
		XMFLOAT2 padding{};
	};

	// CPU側の転送サイズをHudVS/HudPSのcbuffer配置に合わせて維持する。
	static_assert(sizeof(HUD_BUFFER) == 48,"HUD_BUFFERは48バイトである必要があります。");

	//========= Shader関連=========
	// HUD描画に使用するVertex Shader。
	ComPtr<ID3D11VertexShader> m_vertexShader{};
	// HUD描画に使用するPixel Shader。
	ComPtr<ID3D11PixelShader> m_pixelShader{};
	// Vertex構造とVertex Shader入力を対応付けるInput Layout。
	ComPtr<ID3D11InputLayout> m_inputLayout{};

	//========= Buffer関連=========
	// HUD Quadの頂点情報を保持するVertex Buffer。
	ComPtr<ID3D11Buffer> m_vertexBuffer{};
	// HUD Quadの三角形順序を保持するIndex Buffer。
	ComPtr<ID3D11Buffer> m_indexBuffer{};
	// Quadの矩形情報と色をShaderへ渡す定数バッファ。
	ConstantBuffer<HUD_BUFFER> m_HUDBuffer{};

	//========= 描画情報=========
	// DrawIndexedに渡すHUD QuadのIndex数。
	unsigned int m_indexCount{};
};