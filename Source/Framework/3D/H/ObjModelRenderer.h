#pragma once

// OBJの位置・UVと面情報から単一メッシュを生成し、Shader・Buffer・Texture・Samplerを所有する。
// Scene進行・物理衝突・MTLによるMaterial分割・法線を使う照明処理は担当しない。
// GraphicsSystemとDevice/Contextは所有せず、使用中だけ参照する。

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

// OBJ読込とGPU描画を仲介するRenderer。公開クラス名は既存参照との互換性のため維持する。
class ObjModelRenderer final
{
public:
	//========= Getter関数=========
	// PositionとUVの組合せを返す。実配置はVERTEXとInput Layoutで定義する。
	[[nodiscard]] VERTEX_FORMAT GetVertexFormat() const { return VERTEX_FORMAT::e_POSITION_TEXTURE; }

	//========= ライフサイクル・描画関数=========
	// 旧Resourceを先に終了し、OBJ読込、Buffer・Shader・定数Buffer・必要なTexture・Samplerを生成する。
	// graphicsSystem: 初期化済みの描画基盤。Device取得用の非所有参照。
	// OBJFilePath: OBJの読込パス。相対パスはプロセスの作業ディレクトリを基準に扱う。
	// textureFilePath: モデル全体へ使う画像パス。空なら画像を読まず単色描画にする。MTLからは取得しない。
	// 戻り値: 全生成成功ならtrue。検出した読込・生成失敗ならfalse。false経路では部分Resourceを解放する。
	// 旧モデルへの復帰は行わない。std::stoi・コンテナ確保などの例外は捕捉せず、bool失敗の解放経路とは異なる。
	bool Initialize(GraphicsSystem& graphicsSystem,const std::wstring& OBJFilePath,const std::wstring& textureFilePath);
	// 全Indexを一回のDrawIndexedで描画する。必要Resource・Context・Index数がなければ何もしない。
	// graphicsSystem: Context取得先。Render Target・Viewport・Render Passは呼出側で準備する。
	// worldMatrix: LocalからWorldへの変換行列。内部でtransposeして転送する。
	// viewMatrix: WorldからViewへの変換行列。Projectionとの合成順はviewMatrix * projectionMatrix。
	// projectionMatrix: ViewからClipへの変換行列。合成後にtransposeする。
	// color: ObjectのRGBA色。既定は不透明な白。Texture使用時はShaderでTexture色と乗算する。
	// Textureの有無から使用フラグを決め、UV倍率は1倍とする。Textureなしは正常な単色経路。
	// Shader・Buffer・SRV・SamplerのBind状態を変更し、元には戻さない。Blend・Depth・Rasterizerは設定しない。
	void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
						const XMMATRIX& projectionMatrix,const XMFLOAT4& color = XMFLOAT4{1.0f,1.0f,1.0f,1.0f});
	// 所有COM参照を解放し、Index数を0にする。未初期化・部分初期化・解放済みでも呼出可能。
	// ContextへBindされた参照を解除する処理ではない。デストラクターからの自動Uninitは追加しない。
	void Uninit();

private:
	//========= 構造体=========
	// GPUへ渡す頂点。OBJの独立IndexをPosition・UVの組に展開した結果を保持する。
	struct VERTEX
	{
		// OBJのvから取得するLocal座標。座標軸の変換や中心化は行わない。
		XMFLOAT3 position{};
		// vt読込時にVを1-Vへ反転したUV。未指定・0以下のUV Indexでは(0,0)。
		XMFLOAT2 UV{};
	};

	//========= 補助関数=========
	// v・vt・fだけを読み、面を(0,i,i+1)の扇形で分割して出力へ追加する。
	// OBJFilePath: 読込先のパス。位置Indexは正の1始まりを前提とし、負数相対参照は解決しない。
	// vertices: 頂点の追加先。呼出時に空であることを前提とし、この関数内ではclearしない。
	// indices: unsigned intの三角形Index追加先。生成途中のfalse・例外で部分追加を巻き戻さない。
	// 戻り値: 読込後の両配列が非空ならtrue。Open失敗や参照範囲外などではfalse。
	// vnの値やMTLは読まないが、面トークンの法線Indexは頂点共有Keyに含める。
	// 不正な数値によるstd::stoiの例外は伝播する。v/vtの抽出成否や全OBJ文法を検証するLoaderではない。
	bool LoadOBJFile(const std::wstring& OBJFilePath,std::vector<VERTEX>& vertices,std::vector<unsigned int>& indices);
	// 共通TextureLoaderへWIC読込を委譲し、生成されたSRVの所有をメンバーで受け取る。
	// device: Initializeで取得した有効な非所有Device。
	// textureFilePath: 読込画像パス。参照自体は保持しない。
	// 戻り値: 共通Loaderの結果をそのまま返す。
	bool LoadTextureFromFile(ID3D11Device* device,const std::wstring& textureFilePath);

	//========= Shader関連=========
	// BasicColor VSを所有する。Position・UV入力で、法線入力は使用しない。
	ComPtr<ID3D11VertexShader> m_vertexShader{};
	// 単色またはTexture色を出力するBasicColor PSを所有する。
	ComPtr<ID3D11PixelShader> m_pixelShader{};
	// float3 Positionとfloat2 UVの実OffsetをShader入力へ対応付けるLayoutを所有する。
	ComPtr<ID3D11InputLayout> m_inputLayout{};

	//========= Buffer関連=========
	// VERTEX配列から生成するDEFAULT使用法のVertex Bufferを所有する。
	ComPtr<ID3D11Buffer> m_vertexBuffer{};
	// unsigned int配列から生成し、R32_UINTとしてBindするIndex Bufferを所有する。
	ComPtr<ID3D11Buffer> m_indexBuffer{};
	// 転置済みViewProjection用。VSのb0へ設定する。
	ConstantBuffer<CAMERA_CONSTANTS> m_cameraBuffer{};
	// 転置済みWorld行列とRGBA色用。VS・PSのb1へ設定する。
	ConstantBuffer<OBJECT_CONSTANTS> m_objectBuffer{};
	// UV倍率とTexture使用フラグ用。VS・PSのb2へ設定する。
	ConstantBuffer<MATERIAL_CONSTANTS> m_materialBuffer{};

	//========= Texture関連=========
	// モデル全体で使用するSRVを所有する。未所有ならMaterialフラグを0とし、単色出力を選ぶ。
	ComPtr<ID3D11ShaderResourceView> m_textureView{};
	// Linear Filter・Wrap設定のSamplerを所有する。Textureなしでも生成・Bindする。
	ComPtr<ID3D11SamplerState> m_textureSampler{};

	//========= 描画情報=========
	// 全生成成功後に設定するIndex数。Uninitで0へ戻す。巨大モデルに対する変換上限の検証は行わない。
	unsigned int m_indexCount{};
};