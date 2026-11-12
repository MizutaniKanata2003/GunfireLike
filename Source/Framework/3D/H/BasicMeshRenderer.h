#pragma once

// Cubeを使った床・壁・単色Objectの描画と、それに必要なGPU Resourceの所有を担当する。
// Scene進行や衝突判定は担当しない。GraphicsSystemは各呼出中のみ参照し、保持しない。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/DirectX/H/ConstantBuffer.h"
#include "Framework/DirectX/H/GraphicsConstants.h"
#include "Framework/DirectX/H/VertexFormat.h"

//========= 前方宣言=========
class GraphicsSystem;

// Cubeを使い、床、壁、単色オブジェクトを描画する基本3Dメッシュレンダラー。
class BasicMeshRenderer final
{
public:
	//========= 列挙型=========
	// Cube描画に使用するテクスチャの種類。呼出側との互換性のため既存の型名・列挙子名を維持する。
	enum class TextureType
	{
		Floor, // 床Textureを使用する。
		Wall, // 壁Textureを使用する。
		Object, // Object用Textureを使用する。
		Color // Textureを使用せず、指定色で描画する。
	};

	//========= Getter関数=========
	// Cube描画の頂点属性の組み合わせを返す。実際の配置はVERTEXとInput Layoutで定義する。
	[[nodiscard]] VERTEX_FORMAT GetVertexFormat() const { return VERTEX_FORMAT::e_POSITION_TEXTURE; }

	//========= ライフサイクル・描画関数=========
	// Cube描画Resourceを再生成する。既存Resourceは最初に解放し、生成途中の失敗時にも解放する。
	// graphicsSystem: 初期化済みの描画基盤。Deviceを取得するための非所有参照。
	// 戻り値: 全Resourceを生成できればtrue、Device未取得または生成・読込失敗ならfalse。
	bool Initialize(GraphicsSystem& graphicsSystem);
	// 指定した変換と外観でCubeを描画する。Resource未準備またはContext未取得なら何もしない。
	// graphicsSystem: Contextを取得する描画基盤。Render TargetやViewportなどは呼出側で準備する。
	// worldMatrix: ObjectをWorld空間へ変換する行列。転送用transposeは内部で行う。
	// viewMatrix: World空間をView空間へ変換する行列。
	// projectionMatrix: View空間を射影する行列。
	// color: Objectへ適用するRGBA色。
	// UVTiling: UV座標に掛ける縦横の倍率。
	// textureType: 使用するTextureの種類。ColorではTextureを使用しない。
	// 注意: Shader・Buffer・Texture・SamplerのBind状態を変更し、元の状態へは戻さない。
	void DrawCube(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
								const XMMATRIX& projectionMatrix,const XMFLOAT4& color,const XMFLOAT2& UVTiling,TextureType textureType);
	// 所有するGPU Resourceを解放し、Index数を0へ戻す。未初期化・解放済みでも呼出可能。
	void Uninit();

private:
	//========= 構造体=========
	// Cubeの頂点配置。Input LayoutのPOSITIONとTEXCOORDに対応する。
	struct VERTEX
	{
		// Cubeローカル空間の頂点座標。初期メッシュの各軸は-0.5〜0.5。
		XMFLOAT3 position{};
		// 各面に割り当てるTexture座標。
		XMFLOAT2 UV{};
	};

	//========= Shader関連=========
	// Cube描画に使用するVertex Shader。
	ComPtr<ID3D11VertexShader> m_vertexShader{};
	// Cube描画に使用するPixel Shader。
	ComPtr<ID3D11PixelShader> m_pixelShader{};
	// Vertex構造とVertex Shader入力を対応付けるInput Layout。
	ComPtr<ID3D11InputLayout> m_inputLayout{};

	//========= Texture関連=========
	// テクスチャ参照時のフィルタリングとアドレス指定を行うSampler。
	ComPtr<ID3D11SamplerState> m_textureSampler{};
	// 床描画に使用するテクスチャ。
	ComPtr<ID3D11ShaderResourceView> m_floorTextureView{};
	// 壁描画に使用するテクスチャ。
	ComPtr<ID3D11ShaderResourceView> m_wallTextureView{};
	// 画像付きオブジェクト描画に使用するテクスチャ。
	ComPtr<ID3D11ShaderResourceView> m_objectTextureView{};

	//========= Buffer関連=========
	// Cubeの頂点情報を保持するVertex Buffer。
	ComPtr<ID3D11Buffer> m_vertexBuffer{};
	// Cubeの三角形順序を保持するIndex Buffer。
	ComPtr<ID3D11Buffer> m_indexBuffer{};
	// ViewProjection行列をShaderへ渡す定数バッファ。
	ConstantBuffer<CAMERA_CONSTANTS> m_cameraBuffer{};
	// World行列と色をShaderへ渡す定数バッファ。
	ConstantBuffer<OBJECT_CONSTANTS> m_objectBuffer{};
	// UVタイリングとTexture使用有無をShaderへ渡す定数バッファ。
	ConstantBuffer<MATERIAL_CONSTANTS> m_materialBuffer{};

	//========= 描画情報=========
	// DrawIndexedに渡すCubeのIndex数。
	unsigned int m_indexCount{};
};