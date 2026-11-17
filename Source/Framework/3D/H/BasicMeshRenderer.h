#pragma once

// 各面に独立したUVを持つ固定Cubeと、描画用Shader・Buffer・画像SRV・Samplerを所有する。
// CPU配列からGPU Resourceを作り、描画ごとに行列・色・UV倍率・画像使用値を転送する。
// Scene進行、Render Pass選択、描画先やBlend・Depth・Cull設定は担当しない。GraphicsSystemは借用し保持しない。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // Shader・Layout・Buffer・Texture View・SamplerのCOM型を使用する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/DirectX/H/ConstantBuffer.h"
#include "Framework/DirectX/H/GraphicsConstants.h"
#include "Framework/DirectX/H/VertexFormat.h"

//========= 前方宣言=========
class GraphicsSystem;

// CubeをWorld行列で配置し、床・壁・画像付きまたは単色のObjectとして描画するRenderer。
class BasicMeshRenderer final
{
public:
	//========= 列挙型=========
	// 画像選択。公開名TextureType・列挙子・列挙順は、呼出側との互換性のため命名例外として維持する。
	enum class TextureType
	{
		Floor, // 床用SRVと画像使用値1を選ぶ。
		Wall, // 壁用SRVと画像使用値1を選ぶ。
		Object, // Object用SRVと画像使用値1を選ぶ。
		Color // SRVなし・画像使用値0でObject色の単色経路を選ぶ。
	};

	//========= Getter関数=========
	// 位置とUVを持つ形式の識別値。実際のfloat3/float2とOffset・Strideは専用Layoutで定義する。
	[[nodiscard]] VERTEX_FORMAT GetVertexFormat() const { return VERTEX_FORMAT::e_POSITION_TEXTURE; }

	//========= ライフサイクル・描画関数=========
	// 旧ResourceをUninit後、Cube・Shader・定数Buffer・画像・Samplerを生成する。
	// graphicsSystem: 有効な生成Deviceの取得元。生成Resourceと対応する描画基盤を渡す。
	// 戻り値: 全生成成功ならtrue。検出したDevice未取得・読込・生成失敗はfalse。
	// 検出した失敗では部分Resourceを解放する。旧状態へ復帰せず、下位処理・メモリ確保等の例外も捕捉しない。
	// TextureLoaderを使うため呼出スレッドのCOM初期化は利用側で済ませる。例外経路の解放はfalse経路と区別する。
	bool Initialize(GraphicsSystem& graphicsSystem);
	// Cubeを指定変換と外観で描く。各Bind状態は描画後も残り、呼出前の設定へ復元しない。
	// graphicsSystem: Context取得元。描画先・Viewport・Blend・Depth・Rasterizerは呼出側が設定する。
	// worldMatrix: Local→World行列。内部で転置してObject転送型へ保存する。
	// viewMatrix: World→View行列。viewMatrix*projectionMatrixの順で合成する。
	// projectionMatrix: View→Clip行列。合成後に転置してCamera転送型へ保存する。
	// color: RGBA色。単色またはTexture色との積に使う。値のclampやAlpha加工はしない。
	// UVTiling: U・Vへ掛ける単位なし倍率。画像の繰り返しはSampler設定と組み合わせる。
	// textureType: 画像選択。Colorと未定義値は単色扱い。選んだ個別SRVのnull検証は行わない。
	// 必須Resource・Context・Index数がなければ何もしない。追加の行列や有限値検証は行わない。
	// Bind先はCamera=VS b0、Object=VS/PS b1、Material=VS/PS b2、画像=PS t0、Sampler=PS s0。
	void DrawCube(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
								const XMMATRIX& projectionMatrix,const XMFLOAT4& color,const XMFLOAT2& UVTiling,TextureType textureType);
	// メンバーの所有参照とIndex数を解除する。未初期化・部分初期化・終了済みでも呼出可能。
	// ContextのUnbind・State復元・GPU完了待機は行わず、Context等の別参照は残り得る。
	void Uninit();

private:
	//========= 構造体=========
	// 各面4頂点のLocal位置とUV。法線や頂点Colorは含まない。
	struct VERTEX
	{
		// Local座標のfloat3。POSITION0をR32G32B32_FLOATで読み、バイト位置はoffsetofで求める。
		XMFLOAT3 position{};
		// float2のUV。TEXCOORD0をR32G32_FLOATで読む。外部略語UVを維持する。
		XMFLOAT2 UV{};
	};

	//========= Shader関連=========
	// BasicColorVS.csoから生成するVSの所有参照。位置とUVを入力に使う。
	ComPtr<ID3D11VertexShader> m_vertexShader{};
	// BasicColorPS.csoから生成するPSの所有参照。b2の画像使用値に従い出力色を選ぶ。
	ComPtr<ID3D11PixelShader> m_pixelShader{};
	// CPUのVERTEXとVS signatureを対応付けるInput Layoutの所有参照。
	ComPtr<ID3D11InputLayout> m_inputLayout{};

	//========= Texture関連=========
	// Linear参照・WRAPを指定するSamplerの所有参照。画像Resourceとは別にBindする。
	ComPtr<ID3D11SamplerState> m_textureSampler{};
	// 床画像のSRVを所有する。参照する画像Textureの寿命もViewが保持する。
	ComPtr<ID3D11ShaderResourceView> m_floorTextureView{};
	// 壁画像のSRVを所有する。
	ComPtr<ID3D11ShaderResourceView> m_wallTextureView{};
	// Object画像のSRVを所有する。
	ComPtr<ID3D11ShaderResourceView> m_objectTextureView{};

	//========= Buffer関連=========
	// 面ごとに独立した24頂点のDEFAULT Bufferを所有する。描画ごとには作り直さない。
	ComPtr<ID3D11Buffer> m_vertexBuffer{};
	// 12三角形・36要素の16bit Index Bufferを所有する。
	ComPtr<ID3D11Buffer> m_indexBuffer{};
	// 64バイトのCamera転送型。転置済みViewProjectionをVS b0へ設定する。
	ConstantBuffer<CAMERA_CONSTANTS> m_cameraBuffer{};
	// 80バイトのObject転送型。転置済みWorldと色をVS/PS b1へ設定する。
	ConstantBuffer<OBJECT_CONSTANTS> m_objectBuffer{};
	// 32バイトのMaterial転送型。UV倍率と画像使用値をVS/PS b2へ設定する。未参照フィールドも配置のため維持する。
	ConstantBuffer<MATERIAL_CONSTANTS> m_materialBuffer{};

	//========= 描画情報=========
	// 初期0、全生成成功後36、Uninitで0。DrawIndexedの要素数でありバイト数ではない。
	unsigned int m_indexCount{};
};