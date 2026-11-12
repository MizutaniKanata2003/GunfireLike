#pragma once

// OBJの位置・UVを読み込み、GPU Resourceの生成・所有とモデル描画を担当する。
// Scene進行は担当しない。GraphicsSystemとDevice/Contextは所有せず、必要な呼出中だけ参照する。

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
	// 既存Resourceを解放してOBJ描画Resourceを生成する。通常の失敗経路では途中のResourceも解放する。
	// graphicsSystem: 初期化済みの描画基盤。Deviceを取得するための非所有参照。
	// OBJFilePath: 読み込むOBJファイルのパス。
	// textureFilePath: Textureのパス。空文字列の場合はTextureを読み込まず単色描画にする。
	// 戻り値: 全処理に成功すればtrue、読込・Resource生成に失敗すればfalse。
	// 注意: 不正な数値トークンによるstd::stoiの例外は捕捉しない。
	bool Initialize(GraphicsSystem& graphicsSystem,const std::wstring& OBJFilePath,const std::wstring& textureFilePath);
	// OBJモデルを描画する。Resource未準備またはContext未取得なら何もしない。
	// graphicsSystem: Contextを取得する描画基盤。Render TargetやViewport等は呼出側で設定する。
	// worldMatrix: モデルのWorld変換行列。転送用transposeは内部で行う。
	// viewMatrix: World空間からView空間への変換行列。
	// projectionMatrix: View空間からの射影行列。
	// color: Objectへ適用するRGBA色。省略時は不透明な白。
	// 注意: Shader・Buffer・Texture・SamplerのBind状態を変更し、元の状態へは戻さない。
	void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
						const XMMATRIX& projectionMatrix,const XMFLOAT4& color = XMFLOAT4{1.0f,1.0f,1.0f,1.0f});
	// 所有するGPU Resourceを解放し、Index数を0へ戻す。未初期化・解放済みでも呼出可能。
	void Uninit();

private:
	//========= 構造体=========
	// OBJモデルの頂点座標とUV座標。
	struct VERTEX
	{
		// OBJから読み込んだローカル位置座標。
		XMFLOAT3 position{};
		// 読込時にV成分を反転したTexture座標。UV未指定時は0。
		XMFLOAT2 UV{};
	};

	//========= 補助関数=========
	// OBJのv・vt・fを読み込み、面を扇形分割して配列へ追加する。法線値は描画頂点へ格納しない。
	// OBJFilePath: 読込対象のOBJパス。位置Indexは正の1始まりを使用する。
	// vertices: 頂点を追加する出力配列。呼出時に空であることを前提とする。
	// indices: 三角形Indexを追加する出力配列。失敗時の部分追加は巻き戻さない。
	// 戻り値: 読込後の両配列が空でなければtrue。ファイル未取得・位置/UV範囲外などではfalse。
	// 注意: 数値変換の例外は呼出側へ伝播する。
	bool LoadOBJFile(const std::wstring& OBJFilePath,std::vector<VERTEX>& vertices,std::vector<unsigned int>& indices);
	// TextureLoaderへ読込を委譲し、生成されたSRVを所有する。
	// device: 生成に使う非所有Deviceポインター。Initializeで取得した有効なDeviceを渡す。
	// textureFilePath: 読込対象画像のパス。
	// 戻り値: TextureLoaderの読込結果。
	bool LoadTextureFromFile(ID3D11Device* device,const std::wstring& textureFilePath);

	//========= Shader関連=========
	// OBJ描画に使用するVertex Shader。
	ComPtr<ID3D11VertexShader> m_vertexShader{};
	// OBJ描画に使用するPixel Shader。
	ComPtr<ID3D11PixelShader> m_pixelShader{};
	// OBJ頂点形式とVertex Shader入力を対応付けるInput Layout。
	ComPtr<ID3D11InputLayout> m_inputLayout{};

	//========= Buffer関連=========
	// OBJ頂点情報を保持するVertex Buffer。
	ComPtr<ID3D11Buffer> m_vertexBuffer{};
	// OBJ三角形のIndex情報を保持するIndex Buffer。
	ComPtr<ID3D11Buffer> m_indexBuffer{};
	// ViewProjection行列をShaderへ渡す定数バッファ。
	ConstantBuffer<CAMERA_CONSTANTS> m_cameraBuffer{};
	// World行列と色をShaderへ渡す定数バッファ。
	ConstantBuffer<OBJECT_CONSTANTS> m_objectBuffer{};
	// UVタイリングとTexture使用有無をShaderへ渡す定数バッファ。
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