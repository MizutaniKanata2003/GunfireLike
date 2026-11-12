#include "../H/ObjModelRenderer.h"

// CPUメッシュをGPU Resourceへ変換し、定数転送・Bind・DrawIndexed・所有参照の解放を実装する。
// API設定と既存の失敗経路を維持する。以下の説明は未実装の安全策を保証するものではない。

//========= C++標準ライブラリ インクルード=========
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/DirectX/H/TextureLoader.h"
#include "Framework/Etc/H/Logger.h"

// Shader/Textureパス、頂点属性とメッシュの内部定義、および失敗ログをまとめる。
namespace
{
	//========= Shaderファイルパス定数=========
	constexpr char ns_MODEL_VERTEX_SHADER_CSO_PATH[] = "Shaders/BasicColorVS.cso";
	constexpr char ns_MODEL_PIXEL_SHADER_CSO_PATH[] = "Shaders/BasicColorPS.cso";

	//========= OBJ形式定数=========
	// OBJ頂点形式で使用する位置座標とUV座標のセマンティック名。
	constexpr char ns_POSITION_SEMANTIC_NAME[] = "POSITION";
	constexpr char ns_TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";

	//========= OBJ読込用構造体=========
	// OBJのv/vt/vn形式から取得した位置、UV、法線のIndex。
	struct OBJ_INDEX
	{
		// 面トークンの位置Index。正数は1始まり。負数の相対位置を解決する処理はない。
		int positionIndex{};
		// 面トークンのUV Index。未指定は0。後段では正数だけを参照する。
		int UVIndex{};
		// 値を読み込まない法線のIndex。共有Keyへ含める既存仕様だけを維持する。
		int normalIndex{};
	};

	//========= 補助関数=========
	// AssetカテゴリでOBJまたはTextureの読込失敗を出力する。
	// errorText: 失敗理由の有効な終端付きワイド文字列。nullptr不可。
	// filePath: ログへ連結するパス。参照は保持しない。
	void WriteOBJModelAssetError(const wchar_t* errorText,const std::wstring& filePath)
	{
		std::wstring message{errorText};
		message += L": ";
		message += filePath;

		// 組立済み文字列をデバッグ出力へ渡す。ログ生成自体の例外捕捉は追加しない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,message);
	}

	// GraphicsカテゴリでOBJ描画Resource生成失敗を出力する。
	// functionName: 失敗したAPIを識別する有効な終端付きワイド文字列。nullptr不可。
	// result: 32bitのHRESULT値。上位4bitから8桁の16進数へ変換して記録する。
	void WriteOBJModelGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		// API名とHRESULTを一つのメッセージへ組み立てる。呼出元ごとにAssetとGPU生成の失敗を区別する。
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: 0x";

		// unsignedへ変換し、右シフトと0x0fのマスクで各桁を取り出す。元の演算順は変更しない。
		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);

		for(int digit_index = 7; digit_index >= 0; --digit_index){
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			message += HEX_DIGITS[digit];
		}

		// 組立済み文字列をデバッグ出力へ渡す。ログ生成自体の例外捕捉は追加しない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}

	// OBJのv/vt/vn形式を位置、UV、法線のIndexへ分解する。
	// token: 面の1頂点分の文字列。v、v/vt、v//vn、v/vt/vnなどをslashで分解する。
	// result: 0初期化済みの出力先。空成分は代入せず元の値を残す。
	// 戻り値: 位置Indexが0以外ならtrue。数値範囲や完全な文法をここでは保証しない。
	// std::stoiは例外を投げ得る。末尾まで数値だったかの確認や例外捕捉は追加しない。
	bool ParseOBJIndex(const std::string& token,OBJ_INDEX& result)
	{
		// 空の区切り成分も順番に読むため、1//3でも法線の3をUVとして扱わずに済む。
		std::stringstream stream(token);
		std::string value{};

		if(!std::getline(stream,value,'/'))return false;
		if(!value.empty()) result.positionIndex = std::stoi(value);
		if(std::getline(stream,value,'/') && !value.empty()) result.UVIndex = std::stoi(value);
		if(std::getline(stream,value,'/') && !value.empty()) result.normalIndex = std::stoi(value);

		return result.positionIndex != 0;
	}

	// 同じ位置、UV、法線の組み合わせを共有するための文字列Keyを作る。
	// index: 位置・UV・法線Indexの組。戻り値は3つの値をslashで連結した共有判定用Key。
	// 座標値が同じかではなくIndexの組が同じかを判定する。法線を描画しなくてもそのIndexは共有条件に残す。
	std::string MakeVertexKey(const OBJ_INDEX& index)
	{
		return std::to_string(index.positionIndex) + "/" + std::to_string(index.UVIndex) + "/" + std::to_string(index.normalIndex);
	}
}

//========= ライフサイクル・描画関数=========
// OBJ、Shader、Buffer、必要なTexture、Samplerを初期化する。
bool ObjModelRenderer::Initialize(GraphicsSystem& graphicsSystem,const std::wstring& OBJFilePath,const std::wstring& textureFilePath)
{
	// 先に旧所有参照を解放し、GetAddressOfへ渡す出力先を空にする。失敗しても旧モデルへは復帰しない。
	Uninit();

	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	// Device未取得なら何も生成しない。GraphicsSystemは借用し、このRendererでは保存しない。
	if(device == nullptr)return false;

	// OBJファイルからGPU Buffer生成に使用する頂点・Index情報を読み込む。
	std::vector<VERTEX> vertices{};
	std::vector<unsigned int> indices{};

	if(!LoadOBJFile(OBJFilePath,vertices,indices)){
		WriteOBJModelAssetError(L"OBJ読込失敗",OBJFilePath);
		Uninit();
		return false;
	}

	// OBJモデルのVertex Bufferを生成する。
	// ByteWidthは頂点数ではなく総バイト数。Usage=DEFAULTはGPU側のResource使用法を指定する。
	// BindFlagsはIAの頂点Buffer用途。ゼロ初期化によりCPUAccessFlags・MiscFlags・StructureByteStrideは0を維持する。
	// CPUから直接Mapする構成ではなく、初期データをCreateBufferへ渡す。Usage選択の設計意図は推測しない。
	D3D11_BUFFER_DESC vertex_buffer_desc{};
	vertex_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(VERTEX) * vertices.size());
	vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	// pSysMemはCPU側の連続頂点配列。CreateBuffer呼出中に有効である必要があり、配列の所有権は渡さない。
	// 画像ではないBufferの初期データなので、SysMemPitch・SysMemSlicePitchはゼロ初期化のままにする。
	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices.data();

	const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_desc,&vertex_data,m_vertexBuffer.GetAddressOf());
	if(FAILED(vertex_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(VertexBuffer)",vertex_buffer_result);
		Uninit();
		return false;
	}

	// OBJモデルのIndex Bufferを生成する。
	// Indexの総バイト数を指定する。BindFlagsをIndex用途にし、Draw時のFormatと要素型を対応させる。
	// BasicMeshはunsigned short/R16_UINT、OBJはunsigned int/R32_UINTという既存の違いを維持する。
	D3D11_BUFFER_DESC index_buffer_desc{};
	index_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * indices.size());
	index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	// 三角形の頂点参照順を初期データとして渡す。Vertexと同様、CPU配列の所有をDeviceへ移さない。
	D3D11_SUBRESOURCE_DATA index_data{};
	index_data.pSysMem = indices.data();

	const HRESULT index_buffer_result = device->CreateBuffer(&index_buffer_desc,&index_data,m_indexBuffer.GetAddressOf());
	if(FAILED(index_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(IndexBuffer)",index_buffer_result);
		Uninit();
		return false;
	}

	// Vertex ShaderのCSO読込、Shader生成、Input Layout生成を行う。
	std::vector<char> vertex_shader_binary{};

	if(!ShaderBinaryLoader::Load(ns_MODEL_VERTEX_SHADER_CSO_PATH,vertex_shader_binary)){
		Uninit();
		return false;
	}

	// CreateVertexShaderにはHLSL本文ではなくCSOの先頭とバイト長を渡す。ここではShaderのコンパイルを行わない。
	// ClassLinkageはnullptrで、動的リンク用の設定は渡さない。失敗時は作成済みBufferもUninitで解放する。
	const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_binary.data(),vertex_shader_binary.size(),
																																	nullptr,m_vertexShader.GetAddressOf());

	if(FAILED(vertex_shader_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
		Uninit();
		return false;
	}

	// POSITION0=float3とTEXCOORD0=float2を、同一Input Slot 0の頂点ごとのデータとして読む。
	// offsetofはCPUのVERTEX内のバイト位置。PER_VERTEX_DATAとStepRate 0で、Instanceごとの属性にはしない。
	// 名前・番号・FormatはBasicColorVSの入力semanticへ対応する。法線や色の頂点入力は追加しない。
	const D3D11_INPUT_ELEMENT_DESC input_elements[]
	{
		{ns_POSITION_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VERTEX,position),D3D11_INPUT_PER_VERTEX_DATA,0},
		{ns_TEXCOORD_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(VERTEX,UV),D3D11_INPUT_PER_VERTEX_DATA,0}
	};

	// VSバイナリの入力signatureと上の記述を対応付けてLayoutを生成する。頂点のStrideはDraw時に別途指定する。
	// CSO配列はLayout生成が終わるまで有効なローカルデータとして保持する。
	const HRESULT input_layout_result = device->CreateInputLayout(input_elements,ARRAYSIZE(input_elements),vertex_shader_binary.data(),
																																vertex_shader_binary.size(),m_inputLayout.GetAddressOf());

	if(FAILED(input_layout_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateInputLayout",input_layout_result);
		Uninit();
		return false;
	}

	// Pixel Shaderを読み込み、OBJ描画用のPixel Shaderを生成する。
	std::vector<char> pixel_shader_binary{};

	if(!ShaderBinaryLoader::Load(ns_MODEL_PIXEL_SHADER_CSO_PATH,pixel_shader_binary)){
		Uninit();
		return false;
	}

	// BasicColorPSのCSOからPSを生成する。Texture・Sampler・定数Bufferの接続は生成時ではなくDrawで行う。
	const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_binary.data(),pixel_shader_binary.size(),
																																nullptr,m_pixelShader.GetAddressOf());

	if(FAILED(pixel_shader_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
		Uninit();
		return false;
	}

	// CameraはVS b0に対応し、ViewProjectionの保存型を転送する。ConstantBufferのInitはデータの初期値をGPUへ渡さない。
	// 失敗したBufferだけでなく、それまで生成したResourceも終了する。初回Draw前にUpdateで内容を設定する。
	HRESULT camera_buffer_result{};
	if(!m_cameraBuffer.Init(device,camera_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(CameraConstants)",camera_buffer_result);
		Uninit();
		return false;
	}

	// ObjectはVS/PS b1に対応し、World行列とRGBA色を転送する。ConstantBufferのInitはデータの初期値をGPUへ渡さない。
	// 失敗したBufferだけでなく、それまで生成したResourceも終了する。初回Draw前にUpdateで内容を設定する。
	HRESULT object_buffer_result{};
	if(!m_objectBuffer.Init(device,object_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(ObjectConstants)",object_buffer_result);
		Uninit();
		return false;
	}

	// MaterialはVS/PS b2に対応し、UV倍率とTexture使用フラグを転送する。ConstantBufferのInitはデータの初期値をGPUへ渡さない。
	// 失敗したBufferだけでなく、それまで生成したResourceも終了する。初回Draw前にUpdateで内容を設定する。
	HRESULT material_buffer_result{};
	if(!m_materialBuffer.Init(device,material_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(MaterialConstants)",material_buffer_result);
		Uninit();
		return false;
	}

	// 空パスの場合はTextureを読み込まず、単色描画として扱う。
	if(!textureFilePath.empty() && !LoadTextureFromFile(device,textureFilePath)){
		WriteOBJModelAssetError(L"Texture読込失敗",textureFilePath);
		Uninit();
		return false;
	}

	// Texture参照時のフィルタリングとアドレス指定を行うSamplerを生成する。
	// MIN_MAG_MIP_LINEARは縮小・拡大・Mip間の参照をLinearにする設定。画像にMipを追加生成する機能ではない。
	// AddressU/V/W=WRAPは範囲外のTexture座標を繰り返す設定。UV倍率によるタイル表示に対応する。
	// MinLOD=0からMaxLOD=FLOAT32_MAXを許可し、実際に使えるMipは画像Resource側に従う。
	// その他のフィールドはゼロ初期化を維持する。2D参照では未使用のAddressWも既存値のまま設定する。
	D3D11_SAMPLER_DESC sampler_desc{};
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.MinLOD = 0.0f;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

	const HRESULT sampler_state_result = device->CreateSamplerState(&sampler_desc,m_textureSampler.GetAddressOf());
	if(FAILED(sampler_state_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateSamplerState",sampler_state_result);
		Uninit();
		return false;
	}

	// 全Resourceが揃った後にだけIndex数を確定する。この後、初期データ用CPU配列は関数終了で破棄できる。
	m_indexCount = static_cast<unsigned int>(indices.size());

	return true;
}

// 指定したWorld、View、Projection行列と色でOBJモデルを描画する。
void ObjModelRenderer::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
														const XMMATRIX& projectionMatrix,const XMFLOAT4& color)
{
	// 必須のメッシュ・Shader・定数Buffer・Samplerと描画数を確認する。Blendや描画先の検証は呼出側の責務。
	if(!m_vertexBuffer || !m_indexBuffer || !m_cameraBuffer.IsValid() ||
	!m_objectBuffer.IsValid() || !m_materialBuffer.IsValid() || !m_vertexShader ||
	!m_pixelShader || !m_inputLayout || !m_textureSampler || m_indexCount == 0)return;

	// 描画に使用するDirect3D Contextを取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	if(context == nullptr)return;

	// Shaderへ渡すCamera、Object、Material定数を作成する。
	// BasicColorVSはmul(position,world)の後にViewProjectionを掛ける。CPUではView*Projectionをこの順で合成する。
	// 転送時の行列格納契約に合わせて各行列をtransposeし、XMFLOAT4X4へ保存する。座標系変更ではない。
	const XMMATRIX view_projection_matrix = XMMatrixTranspose(viewMatrix * projectionMatrix);
	const XMMATRIX transposed_world_matrix = XMMatrixTranspose(worldMatrix);

	// b0のCameraConstants.viewProjectionMatrixへ対応するCPUデータを作る。
	CAMERA_CONSTANTS camera_constants{};
	XMStoreFloat4x4(&camera_constants.viewProjectionMatrix,view_projection_matrix);

	// b1のObjectConstantsへWorldとObject色を格納する。Texture色との合成はPSの計算に任せる。
	OBJECT_CONSTANTS object_constants{};
	XMStoreFloat4x4(&object_constants.worldMatrix,transposed_world_matrix);
	object_constants.color = color;

	// CPU/HLSLでbaseColor・uvTiling・useTexture・paddingの順を一致させる。
	// baseColorは現行BasicColorでは未参照だが、転送配置を保つため既定値のまま残す。
	MATERIAL_CONSTANTS material_constants{};
	material_constants.uvTiling = XMFLOAT2{1.0f,1.0f};
	material_constants.useTexture = m_textureView ? 1.0f : 0.0f;

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	// Strideは次の頂点へ進むバイト数、Offset 0はBufferの先頭から読む指定。
	// 下の配列はBind呼出へ渡す借用ポインターの束であり、Resource所有者はメンバーのComPtrのまま。
	const UINT vertex_stride = sizeof(VERTEX);
	const UINT vertex_offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};

	// Shaderへ設定する定数Buffer、Texture、Samplerをまとめる。
	ID3D11Buffer* camera_buffers[]{m_cameraBuffer.Get()};
	ID3D11Buffer* object_buffers[]{m_objectBuffer.Get()};
	ID3D11Buffer* material_buffers[]{m_materialBuffer.Get()};
	ID3D11ShaderResourceView* texture_views[]{m_textureView.Get()};
	ID3D11SamplerState* samplers[]{m_textureSampler.Get()};

	// UpdateSubresourceを使って3つのBuffer全体を更新する。Shader slotへのBindはこの更新だけでは行われない。
	// Buffer更新APIはここでHRESULTを返さない。成否通知や新しい失敗分岐は追加せず、既存の順序を維持する。
	m_cameraBuffer.Update(context,camera_constants);
	m_objectBuffer.Update(context,object_constants);
	m_materialBuffer.Update(context,material_constants);

	// IAにはSlot 0から1本の頂点Bufferを設定し、Index FormatはCPU配列の要素幅と合わせる。
	// Triangle Listでは3個のIndexで1三角形を指定する。Layoutは各要素の解釈、Strideは頂点間隔を定義する。
	context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R32_UINT,0);
	context->IASetInputLayout(m_inputLayout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// VS/PSを今回のメッシュ用へ切り替える。nullptrのClassInstance配列と個数0で動的リンク指定は行わない。
	// 定数BufferのslotはStageごとに独立。VS b0=Camera、VS/PS b1=Object、VS/PS b2=Materialへ接続する。
	// PS b0はこの関数では変更しない。Bindされた状態はDraw後も残り、以前のShaderへは自動復元しない。
	context->VSSetShader(m_vertexShader.Get(),nullptr,0);
	context->PSSetShader(m_pixelShader.Get(),nullptr,0);
	context->VSSetConstantBuffers(0,1,camera_buffers);
	context->VSSetConstantBuffers(1,1,object_buffers);
	context->VSSetConstantBuffers(2,1,material_buffers);
	context->PSSetConstantBuffers(1,1,object_buffers);
	context->PSSetConstantBuffers(2,1,material_buffers);

	// Textureなしの場合もnullptrを設定し、直前のSRV参照を残さない。
	// PS t0へSRVを設定する。画像なしでもnullptrを明示し、以前のSRVを誤って参照し続けることを防ぐ。
	// SamplerはPS s0へ別にBindする。画像データと、座標の繰返し・補間方法の設定を混同しない。
	context->PSSetShaderResources(0,1,texture_views);
	context->PSSetSamplers(0,1,samplers);
	// DrawIndexed(Index数,先頭Index,頂点Base)を(保持数,0,0)で発行する。CPU配列へのアクセスではなく、Bind済みGPU Bufferを参照する。
	// 戻り値で描画成否は通知されない。Render TargetやBlend・Depth等のStateは呼出側が設定したものを使用する。
	context->DrawIndexed(m_indexCount,0,0);
}

// OBJ描画で使用したDirect3Dリソースを解放する。
void ObjModelRenderer::Uninit()
{
	// TextureとSamplerを解放する。
	// ここで解放するのはメンバーが持つCOM参照。ContextへBind中なら、Context側の参照も別途残り得る。
	// Shader ResourceのUnbind・ClearState・GPU完了待機は追加せず、他の描画への影響を変えない。
	m_textureSampler.Reset();
	m_textureView.Reset();

	// 定数バッファとメッシュBufferを解放する。
	m_materialBuffer.Uninit();
	m_objectBuffer.Uninit();
	m_cameraBuffer.Uninit();
	m_indexBuffer.Reset();
	m_vertexBuffer.Reset();

	// ShaderとInput Layoutを解放する。
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_vertexShader.Reset();

	// 次の描画で未準備状態を識別できるよう、GPU所有参照とともにIndex数を0へ戻す。
	m_indexCount = {};
}

//========= 補助関数=========
// OBJファイルを読み込み、頂点配列とIndex配列を生成する。
bool ObjModelRenderer::LoadOBJFile(const std::wstring& OBJFilePath,std::vector<VERTEX>& vertices,std::vector<unsigned int>& indices)
{
	// 出力配列は呼出側で空にする前提。Openに失敗した場合は配列を変更せずfalseを返す。
	std::ifstream file(OBJFilePath);
	if(!file)return false;

	// OBJから読み込む位置、UV、重複頂点の対応表を保持する。
	std::vector<XMFLOAT3> positions{};
	std::vector<XMFLOAT2> UVs{};
	std::unordered_map<std::string,unsigned int> vertex_map{};

	// positionsとUVsはOBJの独立した配列、vertex_mapはGPU頂点配列への共有対応表。法線配列は作らない。
	std::string line{};

	while(std::getline(file,line)){
		std::stringstream line_stream(line);
		std::string type{};

		// 先頭の種別だけで処理を選ぶ。空行・コメント・vn・mtllib・usemtl等は対応する分岐がなく読み飛ばす。
		line_stream >> type;

		// 位置を読込順に保存し、面の位置Indexから参照する。抽出失敗や座標軸変換の追加処理は行わない。
		if(type == "v"){
			XMFLOAT3 position{};
			line_stream >> position.x >> position.y >> position.z;
			positions.push_back(position);
			continue;
		}

		// UVのV反転はこの読込時に一度だけ行う。頂点化とShaderで再び反転する変更はしない。
		if(type == "vt"){
			XMFLOAT2 UV{};
			line_stream >> UV.x >> UV.y;

			// OBJのUV座標をDirect3Dの上下反転したUV座標へ変換する。
			UV.y = 1.0f - UV.y;
			UVs.push_back(UV);
			continue;
		}

		// この先は面だけを対象とする。Material分割や別Objectへのメッシュ分割は実装しない。
		if(type != "f") continue;

		// 面を構成するOBJ Indexを読み込む。
		std::vector<OBJ_INDEX> face_indices{};
		std::string token{};

		while(line_stream >> token){
			// 0初期化により未指定成分を0のまま保つ。解析falseなら追加しないが、例外は関数外へ伝播する。
			OBJ_INDEX OBJ_index{};
			if(ParseOBJIndex(token,OBJ_index)) face_indices.push_back(OBJ_index);
		}

		// 有効トークンが3個未満なら面を読み飛ばす。入力全体の失敗にはしない既存仕様。
		if(face_indices.size() < 3) continue;

		// 四角形以上の面も三角形の扇形分割としてIndex化する。
		// (0,1,2)、(0,2,3)のように先頭を共有する扇形。凹面や自己交差面に正しい一般的分割を保証しない。
		for(std::size_t index = 1; index + 1 < face_indices.size(); ++index){
			const OBJ_INDEX triangle[]
			{
				face_indices[0],
				face_indices[index],
				face_indices[index + 1]
			};

			for(const OBJ_INDEX& OBJ_index : triangle){
				// 位置だけで共有するとUV境界が失われるため、位置・UV・法線Indexを含めたKeyで探す。
				const std::string key = MakeVertexKey(OBJ_index);
				const auto found = vertex_map.find(key);

				// 登録済みの頂点ならIndexだけを再利用し、頂点を重複追加しない。
				if(found != vertex_map.end()){
					indices.push_back(found->second);
					continue;
				}

				// 正の1始まりIndexを0始まりへ変換して範囲確認する。負数の相対参照や極端な不正値への新しい安全策は追加しない。
				const int position_index = OBJ_index.positionIndex - 1;
				if(position_index < 0 || position_index >= static_cast<int>(positions.size()))return false;

				// UV Indexが0以下なら(0,0)。正数だけを1減算してUV配列の範囲を確認する。
				XMFLOAT2 UV{};

				if(OBJ_index.UVIndex > 0){
					const int UV_index = OBJ_index.UVIndex - 1;
					if(UV_index < 0 || UV_index >= static_cast<int>(UVs.size()))return false;

					UV = UVs[UV_index];
				}

				// GPU頂点は位置とUVだけで構成する。新しいIndexを共有表と出力へ登録する。
				// 途中でfalseや例外になっても出力済み配列は巻き戻さない。巨大配列のunsigned intへの変換上限も検証しない。
				const VERTEX vertex{positions[position_index],UV};
				const unsigned int new_index = static_cast<unsigned int>(vertices.size());

				vertices.push_back(vertex);
				vertex_map.emplace(key,new_index);
				indices.push_back(new_index);
			}
		}
	}

	// 最後は両配列の非空だけで判定し、ストリーム終了理由の追加検証は行わない。
	return !vertices.empty() && !indices.empty();
}

// テクスチャファイルを読み込み、Shader Resource Viewを生成する。
bool ObjModelRenderer::LoadTextureFromFile(ID3D11Device* device,const std::wstring& textureFilePath)
{
	// 共通LoaderがSRVのResetとWIC読込を行う。生成されたViewの所有はこのRendererへ残る。
	return TextureLoader::LoadWicTexture(device,textureFilePath,m_textureView);
}