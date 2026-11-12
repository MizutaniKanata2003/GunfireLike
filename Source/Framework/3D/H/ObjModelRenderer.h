#pragma once

// OBJの位置・UV・面Indexを単一メッシュへ変換し、Shader・Buffer・Texture・Samplerを所有する。
// MTLによるMaterial分割・法線値の読込・照明・衝突判定は担当しない。
// Scene進行やRender Passの選択は担当しない。GraphicsSystemは借用し、参照を保持しない。
// Init失敗のbool経路と例外伝播の経路を区別する。所有参照の解放はGPUの処理完了待機を意味しない。

//========= C++標準ライブラリ インクルード=========
#include <string>
#include <vector>

//========= 外部ライブラリ インクルード=========
#include <d3d11.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/DirectX/H/ConstantBuffer.h"
#include "Framework/DirectX/H/GraphicsConstants.h"
#include "Framework/DirectX/H/VertexFormat.h"

//========= 前方宣言=========
class GraphicsSystem;

// OBJモデルを読み込み、テクスチャまたは単色で描画する3Dモデルレンダラー。
class ObjModelRenderer final
{
public:
	//========= Getter関数=========
	// OBJモデル描画で使用する頂点形式を返す。
	[[nodiscard]] VERTEX_FORMAT GetVertexFormat() const { return VERTEX_FORMAT::e_POSITION_TEXTURE; }

	//========= ライフサイクル・描画関数=========
	// OBJ、Shader、Buffer、必要なTexture、Samplerを初期化する。
	// graphicsSystem: 初期化済みDeviceの取得先。非所有で、生成Resourceと対応する描画基盤を渡す。
	// OBJFilePath: OBJファイルのパス。相対パスはプロセスの作業ディレクトリ基準。
	// textureFilePath: モデル全体の画像パス。空なら画像を読み込まず単色描画にする。MTLからは取得しない。
	// 戻り値: 全生成成功ならtrue。検出した読込・GPU生成失敗では部分ResourceをUninitしてfalse。
	// 最初に旧Resourceを解放するため、再初期化失敗時の旧状態への復帰は行わない。
	// 下位処理・メモリ確保などの例外は捕捉しない。例外経路ではbool失敗時と同じ解放を保証しない。
	bool Initialize(GraphicsSystem& graphicsSystem,const std::wstring& OBJFilePath,const std::wstring& textureFilePath);
	// 指定したWorld、View、Projection行列と色でOBJモデルを描画する。
	// graphicsSystem: Context取得先。Render Target・Viewport・Blend・Depth・Rasterizerは呼出側が準備する。
	// worldMatrix: LocalからWorldへの行列。内部で転置してObject Bufferへ格納する。
	// viewMatrix: WorldからViewへの行列。Projectionとの積をCamera Bufferへ格納する。
	// projectionMatrix: ViewからClipへの行列。合成順はviewMatrix * projectionMatrix。
	// color: RGBA色。BasicColor PSでは単色またはTexture色との成分ごとの積に使用する。
	// colorの既定値は不透明な白。UV倍率は1倍で、SRVの有無をMaterialの使用フラグへ反映する。
	// 必須Resource・Context・Index数がない場合は何もしない。
	// IAの頂点・Index・Layout・Topology、VS/PS、定数Buffer、PSのSRVとSamplerを変更し、元には戻さない。
	// Bind先はCamera=VS b0、Object=VS/PS b1、Material=VS/PS b2、Texture=PS t0、Sampler=PS s0。
	void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
						const XMMATRIX& projectionMatrix,const XMFLOAT4& color = XMFLOAT4{1.0f,1.0f,1.0f,1.0f});
	// OBJ描画で使用したDirect3Dリソースを解放する。
	// メンバーのCOM参照を解放し、Index数を0にする。未初期化・部分初期化・解放済みでも呼出可能。
	// ContextのBind解除・状態復元・描画完了待機は行わない。Context等が別に保持する参照は残り得る。
	void Uninit();

private:
	//========= 構造体=========
	// OBJモデルの頂点座標とUV座標。
	struct VERTEX
	{
		// Local座標のfloat3。POSITIONはDXGI_FORMAT_R32G32B32_FLOATでこのOffsetを読む。
		XMFLOAT3 position{};
		// float2のUV。TEXCOORD0はDXGI_FORMAT_R32G32_FLOATでこのOffsetを読む。
		XMFLOAT2 UV{};
	};

	//========= 補助関数=========
	// OBJファイルを読み込み、頂点配列とIndex配列を生成する。
	// OBJFilePath: 読込先。位置は正の1始まりIndexを前提とし、負数相対参照は解決しない。
	// vertices: 空の頂点配列を渡す。内部ではclearせず追加する。
	// indices: 空のunsigned int配列を渡す。GPU側はR32_UINTで参照する。
	// 戻り値: 読込後の両配列が非空ならtrue。Open失敗・参照範囲外などでfalse。
	// v・vt・fのみを読み、面は先頭固定の扇形分割。凹多角形の一般的三角形化は行わない。
	// 法線値は読み込まないが、面トークンの法線Indexは共有Keyへ含める。
	// UVは読込時にVを1-Vへ反転。UV Indexが0以下なら(0,0)。途中失敗の部分追加は巻き戻さない。
	// std::stoiの例外は伝播し、v/vtの数値抽出成否や全OBJ文法を検証する処理も追加しない。
	bool LoadOBJFile(const std::wstring& OBJFilePath,std::vector<VERTEX>& vertices,std::vector<unsigned int>& indices);
	// テクスチャファイルを読み込み、Shader Resource Viewを生成する。
	// device: 非所有の生成用Device。Initializeで確認した有効な実体を渡す。
	// textureFilePath: 読込画像パス。参照自体は保持しない。
	// 戻り値: TextureLoaderの結果。Loaderがm_textureViewをResetしてから読込を行う。
	bool LoadTextureFromFile(ID3D11Device* device,const std::wstring& textureFilePath);

	//========= Shader関連=========
	// OBJ描画に使用するVertex Shader。
	// BasicColorVS.csoから生成する所有参照。入力は位置とUVで、法線入力は使用しない。
	ComPtr<ID3D11VertexShader> m_vertexShader{};
	// OBJ描画に使用するPixel Shader。
	// BasicColorPS.csoから生成する所有参照。Texture使用フラグはb2の値で受け取る。
	ComPtr<ID3D11PixelShader> m_pixelShader{};
	// OBJ頂点形式とVertex Shader入力を対応付けるInput Layout。
	ComPtr<ID3D11InputLayout> m_inputLayout{};

	//========= Buffer関連=========
	// OBJ頂点情報を保持するVertex Buffer。
	ComPtr<ID3D11Buffer> m_vertexBuffer{};
	// OBJ三角形のIndex情報を保持するIndex Buffer。
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

	//========= Texture関連=========
	// OBJに貼り付けるテクスチャ。
	ComPtr<ID3D11ShaderResourceView> m_textureView{};
	// テクスチャの拡大縮小とアドレス指定を行うSampler。
	ComPtr<ID3D11SamplerState> m_textureSampler{};

	//========= 描画情報=========
	// DrawIndexedに渡すIndex数。
	unsigned int m_indexCount{};
};