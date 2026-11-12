#pragma once

// 各面に独立したUVを持つCubeと、Shader・Buffer・Texture・Samplerを所有する。
// 初期化時にCPU側の配列からGPU Resourceを生成し、描画時に変換と外観の定数を転送する。
// Scene進行やRender Passの選択は担当しない。GraphicsSystemは借用し、参照を保持しない。
// Init失敗のbool経路と例外伝播の経路を区別する。所有参照の解放はGPUの処理完了待機を意味しない。

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
	// Textureの選択。公開型名・列挙子名・列挙順は既存呼出側との互換性のため維持する。
	enum class TextureType
	{
		Floor, // 床用SRVを選ぶ。
		Wall, // 壁用SRVを選ぶ。
		Object, // Object用SRVを選ぶ。
		Color // SRVをnullptr、Texture使用フラグを0として単色出力を選ぶ。
	};

	//========= Getter関数=========
	// Cube描画で使用する頂点形式を返す。
	[[nodiscard]] VERTEX_FORMAT GetVertexFormat() const { return VERTEX_FORMAT::e_POSITION_TEXTURE; }

	//========= ライフサイクル・描画関数=========
	// Cubeメッシュ、Shader、Texture、Sampler、定数バッファを初期化する。
	// graphicsSystem: 初期化済みDeviceの取得先。非所有で、生成Resourceと対応する描画基盤を渡す。
	// 戻り値: 全生成成功ならtrue。検出した読込・GPU生成失敗では部分ResourceをUninitしてfalse。
	// 最初に旧Resourceを解放するため、再初期化失敗時の旧状態への復帰は行わない。
	// 下位処理・メモリ確保などの例外は捕捉しない。例外経路ではbool失敗時と同じ解放を保証しない。
	bool Initialize(GraphicsSystem& graphicsSystem);
	// 指定した行列、色、UVタイリング、テクスチャ種別でCubeを描画する。
	// graphicsSystem: Context取得先。Render Target・Viewport・Blend・Depth・Rasterizerは呼出側が準備する。
	// worldMatrix: LocalからWorldへの行列。内部で転置してObject Bufferへ格納する。
	// viewMatrix: WorldからViewへの行列。Projectionとの積をCamera Bufferへ格納する。
	// projectionMatrix: ViewからClipへの行列。合成順はviewMatrix * projectionMatrix。
	// color: RGBA色。BasicColor PSでは単色またはTexture色との成分ごとの積に使用する。
	// UVTiling: UVのU・Vへ掛ける倍率。
	// textureType: 選択する画像。Colorまたは未定義値なら単色経路。個別SRVのnull検証は追加しない。
	// 必須Resource・Context・Index数がない場合は何もしない。
	// IAの頂点・Index・Layout・Topology、VS/PS、定数Buffer、PSのSRVとSamplerを変更し、元には戻さない。
	// Bind先はCamera=VS b0、Object=VS/PS b1、Material=VS/PS b2、Texture=PS t0、Sampler=PS s0。
	void DrawCube(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
								const XMMATRIX& projectionMatrix,const XMFLOAT4& color,const XMFLOAT2& UVTiling,TextureType textureType);
	// 描画に使用したDirect3Dリソースを解放する。
	// メンバーのCOM参照を解放し、Index数を0にする。未初期化・部分初期化・解放済みでも呼出可能。
	// ContextのBind解除・状態復元・描画完了待機は行わない。Context等が別に保持する参照は残り得る。
	void Uninit();

private:
	//========= 構造体=========
	// Cubeの頂点座標とUV座標。
	struct VERTEX
	{
		// Local座標のfloat3。POSITIONはDXGI_FORMAT_R32G32B32_FLOATでこのOffsetを読む。
		XMFLOAT3 position{};
		// float2のUV。TEXCOORD0はDXGI_FORMAT_R32G32_FLOATでこのOffsetを読む。
		XMFLOAT2 UV{};
	};

	//========= Shader関連=========
	// Cube描画に使用するVertex Shader。
	// BasicColorVS.csoから生成する所有参照。入力は位置とUVで、法線入力は使用しない。
	ComPtr<ID3D11VertexShader> m_vertexShader{};
	// Cube描画に使用するPixel Shader。
	// BasicColorPS.csoから生成する所有参照。Texture使用フラグはb2の値で受け取る。
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
	// 64バイトのCPU転送型に対応する。転置済みViewProjectionをVSのb0へ渡す。
	ConstantBuffer<CAMERA_CONSTANTS> m_cameraBuffer{};
	// World行列と色をShaderへ渡す定数バッファ。
	// 80バイトのCPU転送型に対応する。転置済みWorld行列と色をVS/PSのb1へ渡す。
	ConstantBuffer<OBJECT_CONSTANTS> m_objectBuffer{};
	// UVタイリングとTexture使用有無をShaderへ渡す定数バッファ。
	// 32バイトのCPU転送型に対応する。UV倍率と使用フラグをVS/PSのb2へ渡す。
	ConstantBuffer<MATERIAL_CONSTANTS> m_materialBuffer{};

	//========= 描画情報=========
	// DrawIndexedに渡すCubeのIndex数。
	unsigned int m_indexCount{};
};