#pragma once

// OBJの位置・UV・面Indexから単一メッシュを作り、Shader・Buffer・画像SRV・Samplerを所有する。
// MTLによるMaterial分割、法線値・照明・衝突判定、Scene進行、Render Passの選択は担当しない。
// GraphicsSystemを借用するが保持しない。boolによる失敗と、捕捉しないC++例外の経路は区別する。

//========= C++標準ライブラリ インクルード=========
#include <string> // std::wstringでOBJ・画像パスを受け取る。参照自体は保持しない。
#include <vector> // OBJ解析の出力頂点とunsigned int Indexを可変長配列で受け渡す。

//========= 外部ライブラリ インクルード=========
#include <d3d11.h> // Shader・Layout・Buffer・SRV・SamplerのDirect3D型を使用する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/DirectX/H/ConstantBuffer.h"
#include "Framework/DirectX/H/GraphicsConstants.h"
#include "Framework/DirectX/H/VertexFormat.h"

//========= 前方宣言=========
class GraphicsSystem;

// OBJモデルを単一画像または単色で描くRenderer。解析するOBJ文法はLoadOBJFileの契約に限定する。
class ObjModelRenderer final
{
public:
	//========= Getter関数=========
	// 位置float3とUV float2の頂点形式を表す識別値を返す。法線入力は持たない。
	[[nodiscard]] VERTEX_FORMAT GetVertexFormat() const { return VERTEX_FORMAT::e_POSITION_TEXTURE; }

	//========= ライフサイクル・描画関数=========
	// 旧Resourceを先にUninitし、OBJからBuffer・Shader・必要な画像・Samplerを生成する。
	// graphicsSystem: 有効な生成Deviceの取得元。描画に使う基盤と対応するDeviceであること。
	// OBJFilePath: OBJファイルのパス。相対パスはプロセスの作業ディレクトリ基準。
	// textureFilePath: モデル全体へ使う画像パス。空なら読込を省く。MTLから取得する処理はない。
	// 戻り値: 全生成完了ならtrue。検出した読込・GPU生成失敗では部分ResourceをUninitしてfalse。
	// Device未取得でもfalse。旧状態は復元しない。stoi・メモリ確保・下位処理の例外は捕捉しない。
	// 例外時はbool経路と同じ解放を保証しない。画像読込時はLoaderが必要とするCOM初期化を利用側で済ませる。
	bool Initialize(GraphicsSystem& graphicsSystem,const std::wstring& OBJFilePath,const std::wstring& textureFilePath);
	// モデルを指定変換と色で描く。変更したIA・VS/PS・定数Buffer・SRV・Samplerは元へ戻さない。
	// graphicsSystem: Context取得元。Render Target・Viewport・Blend・Depth・Rasterizerは利用側が設定する。
	// worldMatrix: Local→World行列。内部で転置してObject転送型へ保存する。
	// viewMatrix: World→View行列。viewMatrix*projectionMatrixの順で合成する。
	// projectionMatrix: View→Clip行列。合成後に転置してCamera転送型へ保存する。
	// color: RGBA色。既定(1,1,1,1)は不透明な白。単色出力または画像色との積に使い、clampは追加しない。
	// 必須Resource・Context・Index数がなければ何もしない。UV倍率は(1,1)で、SRVの有無から画像使用値を作る。
	// Bind先はCamera=VS b0、Object=VS/PS b1、Material=VS/PS b2、画像=PS t0、Sampler=PS s0。
	void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
						const XMMATRIX& projectionMatrix,const XMFLOAT4& color = XMFLOAT4{1.0f,1.0f,1.0f,1.0f});
	// メンバーの所有参照を解放してIndex数を0にする。未初期化・部分初期化・終了済みでも呼出可能。
	// ContextのBind解除・状態復元・GPU完了待機はしない。Context等の別参照は残り得る。
	void Uninit();

private:
	//========= 構造体=========
	// GPUへ渡す1頂点。位置・UV以外のOBJ属性は格納しない。
	struct VERTEX
	{
		// Local位置のfloat3。POSITION0をR32G32B32_FLOAT、offsetofのバイト位置で読む。
		XMFLOAT3 position{};
		// float2のUV。TEXCOORD0をR32G32_FLOATで読む。V反転はOBJ読込時だけ適用する。
		XMFLOAT2 UV{};
	};

	//========= 補助関数=========
	// OBJを読み、位置・UV・法線Indexの組で頂点を共有し、面を先頭固定の扇形で三角形化する。
	// OBJFilePath: 読込先。位置は正の1始まりIndexが前提で、負数相対参照は解決しない。
	// vertices: 空配列を渡す。内部ではclearせず追加し、途中失敗の追加分も巻き戻さない。
	// indices: 空のunsigned int配列を渡す。GPUではR32_UINTを使う。途中失敗の追加分は残る。
	// 戻り値: Open・参照範囲等の検出失敗でfalse、最後に両配列が非空ならtrue。
	// v・vt・fを扱う。凹多角形の一般的分割、数値抽出の完全検証、ストリーム終了理由の検証は行わない。
	// 法線値は読み込まず、法線Indexだけを共有Keyへ含める。UVはV=1-V、UV Indexが0以下なら(0,0)。
	// stoi等の例外と巨大配列の整数変換上限は別途扱う必要があり、ここでは捕捉・上限検証を追加しない。
	bool LoadOBJFile(const std::wstring& OBJFilePath,std::vector<VERTEX>& vertices,std::vector<unsigned int>& indices);
	// 共通TextureLoaderを使い、画像のSRVをメンバーへ受け取る。
	// device: Initializeで有効性を確認した非所有Device。
	// textureFilePath: 読込画像パス。参照自体は保持しない。
	// 戻り値: Loaderの成否。Loaderはm_textureViewをResetしてから読込を行う。
	bool LoadTextureFromFile(ID3D11Device* device,const std::wstring& textureFilePath);

	//========= Shader関連=========
	// BasicColorVS.csoから生成するVSの所有参照。入力は位置とUV。
	ComPtr<ID3D11VertexShader> m_vertexShader{};
	// BasicColorPS.csoから生成するPSの所有参照。画像使用値はMaterial b2に対応する。
	ComPtr<ID3D11PixelShader> m_pixelShader{};
	// CPU頂点配置とVS入力signatureを対応付けるLayoutの所有参照。
	ComPtr<ID3D11InputLayout> m_inputLayout{};

	//========= Buffer関連=========
	// OBJの共有後頂点を格納するDEFAULT Bufferを所有する。
	ComPtr<ID3D11Buffer> m_vertexBuffer{};
	// unsigned intで三角形の参照順を格納するDEFAULT Bufferを所有する。
	ComPtr<ID3D11Buffer> m_indexBuffer{};
	// 64バイトのCamera転送型。転置済みViewProjectionをVS b0へ渡す。
	ConstantBuffer<CAMERA_CONSTANTS> m_cameraBuffer{};
	// 80バイトのObject転送型。転置済みWorldとRGBA色をVS/PS b1へ渡す。
	ConstantBuffer<OBJECT_CONSTANTS> m_objectBuffer{};
	// 32バイトのMaterial転送型。UV倍率と画像使用値をVS/PS b2へ渡し、未参照フィールドの配置も保つ。
	ConstantBuffer<MATERIAL_CONSTANTS> m_materialBuffer{};

	//========= Texture関連=========
	// モデル全体に使う画像のSRVを所有する。空パスの初期化後は未生成のまま。
	ComPtr<ID3D11ShaderResourceView> m_textureView{};
	// Linear参照・WRAPのSamplerを所有する。画像なしの場合も生成する。
	ComPtr<ID3D11SamplerState> m_textureSampler{};

	//========= 描画情報=========
	// 初期0、生成完了後にIndex配列の要素数、Uninitで0。バイト数ではない。
	unsigned int m_indexCount{};
};