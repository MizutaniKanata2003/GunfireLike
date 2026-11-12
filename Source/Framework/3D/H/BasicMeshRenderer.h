#pragma once

// Cubeを使った床・壁・単色Objectの描画と、必要なGPU Resourceの所有を担当する。
// Scene進行・衝突判定は担当しない。GraphicsSystemは各呼出中だけ参照し、保持しない。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/DirectX/H/ConstantBuffer.h"
#include "Framework/DirectX/H/GraphicsConstants.h"
#include "Framework/DirectX/H/VertexFormat.h"

//========= 前方宣言=========
class GraphicsSystem;

// 各面が独立したUVを持つCubeを、指定行列と外観で描画するRenderer。
class BasicMeshRenderer final
{
public:
	//========= 列挙型=========
	// Textureの選択。既存呼出側との互換性のため公開型名・列挙子名・順序を維持する。
	enum class TextureType
	{
		Floor,  // 床用Textureを参照する。
		Wall,   // 壁用Textureを参照する。
		Object, // Object用Textureを参照する。
		Color   // Textureを参照せず、Object色を出力する。
	};

	//========= Getter関数=========
	// 頂点属性の組合せを返す。実際のStrideとOffsetはVERTEXとInput Layoutが定義する。
	[[nodiscard]] VERTEX_FORMAT GetVertexFormat() const { return VERTEX_FORMAT::e_POSITION_TEXTURE; }

	//========= ライフサイクル・描画関数=========
	// 既存Resourceを終了し、メッシュ・Shader・Texture・Sampler・定数Bufferを生成する。
	// graphicsSystem: 初期化済みの描画基盤。Device取得用の非所有参照。
	// 戻り値: 全生成成功ならtrue。Device未取得、検出した読込・生成失敗ではfalse。
	// 生成途中の失敗ではUninitで部分解放する。下位処理の例外はここでは捕捉しない。
	bool Initialize(GraphicsSystem& graphicsSystem);
	// 指定した変換と外観でCubeを描画する。必要Resource・Contextがなければ何もしない。
	// graphicsSystem: Context取得先。Render Target・Viewport・Render Passは呼出側で準備する。
	// worldMatrix: LocalからWorldへの行列。転送用transposeは内部で行う。
	// viewMatrix: WorldからViewへの行列。Projectionと合成して転送する。
	// projectionMatrix: ViewからClipへの行列。viewMatrixとの乗算順は維持する。
	// color: ObjectのRGBA色。Texture使用時はShaderでTexture色と乗算する。
	// UVTiling: UVのU・Vに掛ける倍率。
	// textureType: Texture選択。Colorまたは未定義値ではTexture使用フラグを0にする。
	// Shader・Buffer・Texture・SamplerのBind状態を変更し、以前の状態へは戻さない。
	// 個別Textureのnull確認や、Blend・Depth・Rasterizerの設定はこの関数では行わない。
	void DrawCube(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
								const XMMATRIX& projectionMatrix,const XMFLOAT4& color,const XMFLOAT2& UVTiling,TextureType textureType);
	// 所有COM参照を解放してIndex数を0へ戻す。未初期化・解放済みでも呼出可能。
	// ContextのBind状態を解除する処理は行わない。
	void Uninit();

private:
	//========= 構造体=========
	// POSITIONとTEXCOORDに対応するCube頂点。各面4頂点ずつを独立に保持する。
	struct VERTEX
	{
		// Local座標。初期Cubeの各軸は-0.5から0.5。
		XMFLOAT3 position{};
		// 面ごとのTexture座標。Shaderへ渡す前のUV倍率は含まない。
		XMFLOAT2 UV{};
	};

	//========= Shader関連=========
	// Local頂点の変換とUV倍率反映を行うVertex Shaderを所有する。
	ComPtr<ID3D11VertexShader> m_vertexShader{};
	// 単色またはTexture色の出力を行うPixel Shaderを所有する。
	ComPtr<ID3D11PixelShader> m_pixelShader{};
	// VERTEXのPosition・UVをShader入力へ対応付けるLayoutを所有する。
	ComPtr<ID3D11InputLayout> m_inputLayout{};

	//========= Texture関連=========
	// Linear FilterとWrap設定のSamplerを所有する。
	ComPtr<ID3D11SamplerState> m_textureSampler{};
	// 床用TextureのViewを所有する。
	ComPtr<ID3D11ShaderResourceView> m_floorTextureView{};
	// 壁用TextureのViewを所有する。
	ComPtr<ID3D11ShaderResourceView> m_wallTextureView{};
	// Object用TextureのViewを所有する。
	ComPtr<ID3D11ShaderResourceView> m_objectTextureView{};

	//========= Buffer関連=========
	// Cubeの24頂点を保持するVertex Bufferを所有する。
	ComPtr<ID3D11Buffer> m_vertexBuffer{};
	// Cubeの36Indexを保持するIndex Bufferを所有する。
	ComPtr<ID3D11Buffer> m_indexBuffer{};
	// ViewProjectionを転送するCamera Buffer。DrawCubeではVSのb0へBindする。
	ConstantBuffer<CAMERA_CONSTANTS> m_cameraBuffer{};
	// World行列とRGBA色を転送するObject Buffer。VS・PSのb1へBindする。
	ConstantBuffer<OBJECT_CONSTANTS> m_objectBuffer{};
	// UV倍率とTexture使用フラグを転送するMaterial Buffer。VS・PSのb2へBindする。
	ConstantBuffer<MATERIAL_CONSTANTS> m_materialBuffer{};

	//========= 描画情報=========
	// DrawIndexedへ渡すIndex数。全生成成功後に36、Uninitで0へ設定する。
	unsigned int m_indexCount{};
};