#include "../H/ObjModelRenderer.h"

// OBJの位置・UVと面Indexから描画用配列を作り、GPU Buffer・Shader・Textureの生成と描画を行う。
// この整理では読込仕様、数値変換例外、部分追加、GPU生成設定、転送とBindの順序を変更しない。

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

// Shaderパス、頂点属性の識別名、OBJ Indexの分解・共有Key作成・失敗ログをまとめる。
namespace
{
	//========= Shaderファイルパス定数=========
	// PositionとUVを変換する既存BasicColor VSを共用する。法線用のShaderを追加するものではない。
	constexpr char ns_MODEL_VERTEX_SHADER_CSO_PATH[] = "Shaders/BasicColorVS.cso";
	// Texture使用フラグに応じて単色またはTexture色を出力する既存PS。
	constexpr char ns_MODEL_PIXEL_SHADER_CSO_PATH[] = "Shaders/BasicColorPS.cso";

	//========= OBJ形式定数=========
	// OBJ頂点形式で使用する位置座標とUV座標のセマンティック名。
	constexpr char ns_POSITION_SEMANTIC_NAME[] = "POSITION";
	constexpr char ns_TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";

	//========= OBJ読込用構造体=========
	// OBJのv/vt/vn形式から取得した位置、UV、法線のIndex。
	struct OBJ_INDEX
	{
		// OBJの位置Index。0は未指定、正値は1始まり。負数相対参照の解決処理は行わない。
		int positionIndex{};
		// OBJのUV Index。後段は正値だけを参照し、0や負値の場合はUVを0のままにする。
		int UVIndex{};
		// 法線Index。共有Keyへ含めるが、法線配列の読込や頂点への格納は行わない。
		int normalIndex{};
	};

	//========= 補助関数=========
	// AssetカテゴリへOBJまたはTextureの読込失敗を記録する。
	// errorText: 失敗理由の終端付きワイド文字列。nullptr不可。
	// filePath: ログに連結する対象パス。参照は保持しない。
	void WriteOBJModelAssetError(const wchar_t* errorText,const std::wstring& filePath)
	{
		// 対象パスを付加し、どのAssetの読込に失敗したかを識別できる形にする。
		std::wstring message{errorText};
		message += L": ";
		message += filePath;

		// 完成したメッセージを既存カテゴリのデバッグ出力へ送る。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,message);
	}

	// GraphicsカテゴリへGPU Resourceの生成失敗を記録する。
	// functionName: 失敗したAPIを表す終端付きワイド文字列。nullptr不可。
	// result: 16進数8桁で出力するHRESULT。
	void WriteOBJModelGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		// GPU生成API名と、符号付きの10進数ではなく16進数の失敗値を組み立てる。
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: 0x";

		// 4bitずつ上位から抽出する。括弧や演算順は既存のまま保ち、8桁すべてを出力する。
		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);

		for(int digit_index = 7; digit_index >= 0; --digit_index){
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			message += HEX_DIGITS[digit];
		}

		// 完成したメッセージを既存カテゴリのデバッグ出力へ送る。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}

	// 面を構成する1頂点分のv/vt/vnトークンを3つのIndexへ分解する。
	// token: 1、1/2、1//3、1/2/3などの文字列。完全なOBJ文法検証は行わない。
	// result: 0初期化済みの出力先。未指定成分は代入せず、元の値を残す。
	// 戻り値: 位置Indexが0以外ならtrue。std::stoiの例外は呼出側へ伝播する。
	bool ParseOBJIndex(const std::string& token,OBJ_INDEX& result)
	{
		// slash区切りの空要素を残すことで、1//3でもUVと法線の対応をずらさず読み取る。
		std::stringstream stream(token);
		std::string value{};

		// 指定されている成分だけ数値化する。負数の解決や文字列全体の消費確認は追加しない。
		if(!std::getline(stream,value,'/'))return false;
		if(!value.empty()) result.positionIndex = std::stoi(value);
		if(std::getline(stream,value,'/') && !value.empty()) result.UVIndex = std::stoi(value);
		if(std::getline(stream,value,'/') && !value.empty()) result.normalIndex = std::stoi(value);

		// ここでは0だけを無効とする。負の位置Indexは後段の配列範囲判定で失敗する。
		return result.positionIndex != 0;
	}

	// 位置・UV・法線Indexの3要素を区別できる共有Keyを返す。
	// index: トークン解析で得たIndexの組。値の等価性であり、座標の一致判定ではない。
	std::string MakeVertexKey(const OBJ_INDEX& index)
	{
		// 同じ位置でもUVが異なれば別頂点とする。描画に未使用の法線Indexも既存どおり共有条件へ含める。
		return std::to_string(index.positionIndex) + "/" + std::to_string(index.UVIndex) + "/" + std::to_string(index.normalIndex);
	}
}

//========= ライフサイクル・描画関数=========
// CPU読込を完了してからGPU Resourceを生成し、成功後にだけ描画Index数を確定する。
bool ObjModelRenderer::Initialize(GraphicsSystem& graphicsSystem,const std::wstring& OBJFilePath,const std::wstring& textureFilePath)
{
	// 再初期化では旧Resourceを先に解放する。失敗時に旧モデルへ復帰する方式ではない。
	Uninit();

	// GPUリソースの生成に使用するDirect3D Deviceを取得する。
	ID3D11Device* device = graphicsSystem.GetDevice();
	if(device == nullptr)return false;

	// OBJファイルからGPU Buffer生成に使用する頂点・Index情報を読み込む。
	std::vector<VERTEX> vertices{};
	std::vector<unsigned int> indices{};

	if(!LoadOBJFile(OBJFilePath,vertices,indices)){
		WriteOBJModelAssetError(L"OBJ読込失敗",OBJFilePath);
		Uninit();
		return false;
	}

	// CPU頂点を連続配置したVertex Bufferを生成する。UsageはDEFAULT、初期データは配列先頭を渡す。
	// ByteWidthのUINT変換と乗算は既存のまま。巨大モデルのサイズ検証はここでは追加しない。
	D3D11_BUFFER_DESC vertex_buffer_desc{};
	vertex_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(VERTEX) * vertices.size());
	vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices.data();
	const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_desc,&vertex_data,m_vertexBuffer.GetAddressOf());
	if(FAILED(vertex_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(VertexBuffer)",vertex_buffer_result);
		Uninit();
		return false;
	}

	// Index配列はunsigned intで保持し、DrawではR32_UINTとして読む。R16へ狭める変更はしない。
	D3D11_BUFFER_DESC index_buffer_desc{};
	index_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * indices.size());
	index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA index_data{};
	index_data.pSysMem = indices.data();
	const HRESULT index_buffer_result = device->CreateBuffer(&index_buffer_desc,&index_data,m_indexBuffer.GetAddressOf());
	if(FAILED(index_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(IndexBuffer)",index_buffer_result);
		Uninit();
		return false;
	}

	// VERTEX ShaderのCSO読込、Shader生成、Input Layout生成を行う。
	std::vector<char> vertex_shader_binary{};

	if(!ShaderBinaryLoader::Load(ns_MODEL_VERTEX_SHADER_CSO_PATH,vertex_shader_binary)){
		Uninit();
		return false;
	}

	// CSOからVertex Shaderを生成する。読込済みデータは後のInput Layout作成にも使用する。
	const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_binary.data(),vertex_shader_binary.size(),
																																	nullptr,m_vertexShader.GetAddressOf());

	if(FAILED(vertex_shader_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
		Uninit();
		return false;
	}

	// VERTEXのPositionはfloat3、UVはfloat2。offsetofで実配置を指定し、法線入力は追加しない。
	const D3D11_INPUT_ELEMENT_DESC input_elements[]
	{
		{ns_POSITION_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VERTEX,position),D3D11_INPUT_PER_VERTEX_DATA,0},
		{ns_TEXCOORD_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(VERTEX,UV),D3D11_INPUT_PER_VERTEX_DATA,0}
	};

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

	// 共用のBasicColor PSを生成する。Textureの使用有無は描画時のMaterial値で切り替える。
	const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_binary.data(),pixel_shader_binary.size(),
																																nullptr,m_pixelShader.GetAddressOf());

	if(FAILED(pixel_shader_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
		Uninit();
		return false;
	}

	// VSのb0へ渡すViewProjection用Buffer。Init内に初期データは渡さず、各DrawでUpdateする。
	HRESULT camera_buffer_result{};

	if(!m_cameraBuffer.Init(device,camera_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(CameraConstants)",camera_buffer_result);
		Uninit();
		return false;
	}

	// VS・PSのb1へ渡すWorld行列と色のBuffer。Init内に初期データは渡さず、各DrawでUpdateする。
	HRESULT object_buffer_result{};

	if(!m_objectBuffer.Init(device,object_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(ObjectConstants)",object_buffer_result);
		Uninit();
		return false;
	}

	// VS・PSのb2へ渡すUV倍率とTexture使用フラグのBuffer。Init内に初期データは渡さず、各DrawでUpdateする。
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

	// 全生成の成功後にだけ描画数を確定する。読み込んだCPU配列自体は関数終了時に破棄される。
	m_indexCount = static_cast<unsigned int>(indices.size());

	return true;
}

// 指定したWorld、View、Projection行列と色でOBJモデルを描画する。
void ObjModelRenderer::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
														const XMMATRIX& projectionMatrix,const XMFLOAT4& color)
{
	// 必須Resourceと描画数を確認する。Textureなしは単色描画なので、ここでは必須にしない。
	if(!m_vertexBuffer || !m_indexBuffer || !m_cameraBuffer.IsValid() ||
	!m_objectBuffer.IsValid() || !m_materialBuffer.IsValid() || !m_vertexShader ||
	!m_pixelShader || !m_inputLayout || !m_textureSampler || m_indexCount == 0)return;

	// 描画に使用するDirect3D Contextを取得する。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	if(context == nullptr)return;

	// 既存HLSLとの行列転送契約に合わせてtransposeする。View*ProjectionとWorldは別々に転送する。
	const XMMATRIX view_projection_matrix = XMMatrixTranspose(viewMatrix * projectionMatrix);
	const XMMATRIX transposed_world_matrix = XMMatrixTranspose(worldMatrix);

	// Camera用の保存型へViewProjectionを格納する。
	CAMERA_CONSTANTS camera_constants{};
	XMStoreFloat4x4(&camera_constants.viewProjectionMatrix,view_projection_matrix);

	// Object用の保存型へWorld行列と指定色を格納する。
	OBJECT_CONSTANTS object_constants{};
	XMStoreFloat4x4(&object_constants.worldMatrix,transposed_world_matrix);
	object_constants.color = color;

	// UV倍率は1倍とし、SRVの有無だけでTexture使用フラグを設定する。baseColorは既定値を保つ。
	MATERIAL_CONSTANTS material_constants{};
	material_constants.uvTiling = XMFLOAT2{1.0f,1.0f};
	material_constants.useTexture = m_textureView ? 1.0f : 0.0f;

	// Input Assemblerへ設定する頂点Buffer情報をまとめる。
	const UINT vertex_stride = sizeof(VERTEX);
	const UINT vertex_offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};

	// Shaderへ設定する定数Buffer、Texture、Samplerをまとめる。
	ID3D11Buffer* camera_buffers[]{m_cameraBuffer.Get()};
	ID3D11Buffer* object_buffers[]{m_objectBuffer.Get()};
	ID3D11Buffer* material_buffers[]{m_materialBuffer.Get()};
	ID3D11ShaderResourceView* texture_views[]{m_textureView.Get()};
	ID3D11SamplerState* samplers[]{m_textureSampler.Get()};

	// 各描画で内容を更新してからBindする。Updateの結果を受け取る新しい分岐は追加しない。
	m_cameraBuffer.Update(context,camera_constants);
	m_objectBuffer.Update(context,object_constants);
	m_materialBuffer.Update(context,material_constants);

	// VertexのStrideと32bit Indexを設定する。Topologyは面の扇形分割結果に対応するTriangle List。
	context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R32_UINT,0);
	context->IASetInputLayout(m_inputLayout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// BasicColor Shaderへ切り替え、既存のStage・slotへ定数Bufferを設定する。状態の復元は行わない。
	context->VSSetShader(m_vertexShader.Get(),nullptr,0);
	context->PSSetShader(m_pixelShader.Get(),nullptr,0);
	context->VSSetConstantBuffers(0,1,camera_buffers);
	context->VSSetConstantBuffers(1,1,object_buffers);
	context->VSSetConstantBuffers(2,1,material_buffers);
	context->PSSetConstantBuffers(1,1,object_buffers);
	context->PSSetConstantBuffers(2,1,material_buffers);

	// Textureなしの場合もnullptrを設定し、直前のSRV参照を残さない。
	context->PSSetShaderResources(0,1,texture_views);
	context->PSSetSamplers(0,1,samplers);
	// SamplerとSRVを設定した後、保持する全Indexを一回の描画命令として発行する。
	context->DrawIndexed(m_indexCount,0,0);
}

// OBJ描画で使用したDirect3Dリソースを解放する。
void ObjModelRenderer::Uninit()
{
	// TextureとSamplerを解放する。
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

	// COM参照の解放後、描画数も0に戻す。Contextが保持する参照の解除はここでは行わない。
	m_indexCount = {};
}

//========= 補助関数=========
// OBJのv・vt・fだけを処理し、描画用頂点と三角形Indexを出力配列へ追加する。
bool ObjModelRenderer::LoadOBJFile(const std::wstring& OBJFilePath,std::vector<VERTEX>& vertices,std::vector<unsigned int>& indices)
{
	// パスのOpenに失敗した場合は配列を変更せずfalse。出力配列のclearは呼出側の前提とする。
	std::ifstream file(OBJFilePath);
	if(!file)return false;

	// OBJから読み込む位置、UV、重複頂点の対応表を保持する。
	std::vector<XMFLOAT3> positions{};
	std::vector<XMFLOAT2> UVs{};
	std::unordered_map<std::string,unsigned int> vertex_map{};

	// 行単位で種別を読む。空行・コメント・vn・mtllib・usemtlなどは対象種別に一致せず無視される。
	std::string line{};

	while(std::getline(file,line)){
		// 行頭トークンで処理を選ぶ。v/vtの数値抽出結果の追加検証は行わない。
		std::stringstream line_stream(line);
		std::string type{};

		line_stream >> type;

		// 位置を読込順に保持する。この配列の順序がOBJの1始まり位置Indexに対応する。
		if(type == "v"){
			XMFLOAT3 position{};
			line_stream >> position.x >> position.y >> position.z;
			positions.push_back(position);
			continue;
		}

		// UVは読込時にVを反転するため、頂点化やShader側で二重に反転しない。
		if(type == "vt"){
			XMFLOAT2 UV{};
			line_stream >> UV.x >> UV.y;

			// OBJのUV座標をDirect3Dの上下反転したUV座標へ変換する。
			UV.y = 1.0f - UV.y;
			UVs.push_back(UV);
			continue;
		}

		// 面以外の残りの種別は処理しない。MTLによるMaterial切替や法線読込は行わない。
		if(type != "f") continue;

		// 面を構成するOBJ Indexを読み込む。
		std::vector<OBJ_INDEX> face_indices{};
		std::string token{};

		while(line_stream >> token){
			// 未指定成分を0にする。解析がfalseのトークンは面へ追加せず、例外はそのまま伝播する。
			OBJ_INDEX OBJ_index{};
			if(ParseOBJIndex(token,OBJ_index)) face_indices.push_back(OBJ_index);
		}

		// 有効トークンが3個未満の面は、エラー返却ではなく読み飛ばす既存仕様を維持する。
		if(face_indices.size() < 3) continue;

		// 先頭頂点を固定した(0,i,i+1)で面を扇形分割する。凹多角形への一般的な三角形化は追加しない。
		for(std::size_t index = 1; index + 1 < face_indices.size(); ++index){
			const OBJ_INDEX triangle[]
			{
				face_indices[0],
				face_indices[index],
				face_indices[index + 1]
			};

			for(const OBJ_INDEX& OBJ_index : triangle){
				// 位置だけで共有するとUV境界が失われるため、位置・UV・法線Indexの組で既存頂点を探す。
				const std::string key = MakeVertexKey(OBJ_index);
				const auto found = vertex_map.find(key);

				// 登録済みなら頂点を増やさず、そのIndexだけを出力して三角形を構成する。
				if(found != vertex_map.end()){
					indices.push_back(found->second);
					continue;
				}

				// OBJの正の1始まりIndexを0始まりへ直す。負数相対参照は解決せず範囲外としてfalseにする。
				const int position_index = OBJ_index.positionIndex - 1;
				if(position_index < 0 || position_index >= static_cast<int>(positions.size()))return false;

				// UV未指定または0以下なら(0,0)。正のIndexだけを範囲確認して参照する。
				XMFLOAT2 UV{};

				if(OBJ_index.UVIndex > 0){
					const int UV_index = OBJ_index.UVIndex - 1;
					if(UV_index < 0 || UV_index >= static_cast<int>(UVs.size()))return false;

					UV = UVs[UV_index];
				}

				// 新規頂点へ位置とUVだけを格納し、同じ組を後で再利用できるようIndexを登録する。
				// 途中でfalseや例外になっても、すでに出力した頂点・Indexを巻き戻す処理は行わない。
				const VERTEX vertex{positions[position_index],UV};
				const unsigned int new_index = static_cast<unsigned int>(vertices.size());

				vertices.push_back(vertex);
				vertex_map.emplace(key,new_index);
				indices.push_back(new_index);
			}
		}
	}

	// 非空なら成功とする。ストリームの終了理由を追加判定する処理は行わない。
	return !vertices.empty() && !indices.empty();
}

// テクスチャファイルを読み込み、Shader Resource Viewを生成する。
bool ObjModelRenderer::LoadTextureFromFile(ID3D11Device* device,const std::wstring& textureFilePath)
{
	// 共通Loaderが既存SRVのResetとWIC読込を担当し、生成したViewの所有をメンバーで受け取る。
	return TextureLoader::LoadWicTexture(device,textureFilePath,m_textureView);
}