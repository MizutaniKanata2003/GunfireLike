#include "../H/ObjModelRenderer.h"

// OBJを位置・UVの単一メッシュへ変換し、GPU生成・定数転送・Bind・描画・所有参照解放を実装する。
// 解析やサイズ検証の制限をコメントで明示するが、新しい検証・例外捕捉・復旧処理は追加しない。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // offsetofで頂点属性のバイト位置、std::size_tで面の走査位置を扱う。
#include <fstream> // std::ifstreamでOBJを行単位に読み込む。
#include <sstream> // std::stringstreamでOBJ行とslash区切りの面トークンを解析する。
#include <string> // 文字列・getline・stoi・to_stringでパス、Index、共有Keyとログを扱う。
#include <unordered_map> // 位置・UV・法線Indexの文字列KeyからGPU頂点Indexを検索する。
#include <vector> // OBJの位置・UV・頂点・Index、およびCSOを可変長配列に保持する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/ShaderBinaryLoader.h"
#include "Framework/DirectX/H/TextureLoader.h"
#include "Framework/Etc/H/Logger.h"

// Shaderパス・入力属性・OBJ面トークンと、内部解析・失敗ログの補助処理をまとめる。
namespace
{
	//========= Shaderファイルパス定数=========
	// 作業ディレクトリ基準のCSOパス。位置・UV入力のVSと、画像有無で色を選ぶPSを使う。
	constexpr char ns_MODEL_VERTEX_SHADER_CSO_PATH[] = "Shaders/BasicColorVS.cso";
	constexpr char ns_MODEL_PIXEL_SHADER_CSO_PATH[] = "Shaders/BasicColorPS.cso";

	//========= OBJ形式定数=========
	// CPUのposition/UVを、VSのPOSITION0/TEXCOORD0へ対応付けるsemantic名。
	constexpr char ns_POSITION_SEMANTIC_NAME[] = "POSITION";
	constexpr char ns_TEXCOORD_SEMANTIC_NAME[] = "TEXCOORD";

	//========= OBJ読込用構造体=========
	// 面トークン1頂点のIndex組。未指定成分は0で、法線値そのものは保持しない。
	struct OBJ_INDEX
	{
		// 位置は正の1始まりが前提。負数相対参照を解決する処理はない。
		int positionIndex{};
		// UV Index。0以下は後段でUV(0,0)として扱う。
		int UVIndex{};
		// 法線Index。法線値は描画しないが、頂点の共有条件へ含める。
		int normalIndex{};
	};

	//========= 補助関数=========
	// Asset失敗理由とパスを記録する。Resourceの解放は呼出元の責務。
	// errorText: 有効な終端付き失敗理由。nullptr不可。
	// filePath: ログに連結するパス。参照自体は保持しない。
	void WriteOBJModelAssetError(const wchar_t* errorText,const std::wstring& filePath)
	{
		// 理由とパスを結合する。ログ用メモリ確保の例外は捕捉しない。
		std::wstring message{errorText};
		message += L": ";
		message += filePath;

		// Asset分類へ送る。GPU生成失敗のHRESULTログとは分類を分ける。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,message);
	}

	// Graphics分類に生成失敗を記録する。
	// functionName: 有効な終端付きAPI名。nullptr不可。
	// result: 上位桁から16進数8桁で記録するHRESULT。
	void WriteOBJModelGraphicsError(const wchar_t* functionName,HRESULT result)
	{
		// API名とHRESULTの接頭辞を組み立てる。
		std::wstring message{functionName};
		message += L" に失敗しました。HRESULT: 0x";

		// 4bit値0〜15を数字とA〜Fへ変換する表と、右シフト用unsigned表現を用意する。
		constexpr wchar_t HEX_DIGITS[]{L"0123456789ABCDEF"};
		const unsigned long result_value = static_cast<unsigned long>(result);

		// 桁番号7から0まで逆順に走査し、上位から8桁を出力する。
		for(int digit_index = 7; digit_index >= 0; --digit_index){
			// 1桁は4bit。対象桁を最下位へ移動し、0x0fでその4bitだけを抽出する。
			const unsigned long digit = (result_value >> digit_index * 4) & 0x0f;
			message += HEX_DIGITS[digit]; // 抽出値を対応する16進数字へ変換して末尾に加える。
		}

		// 完成したログを出力する。例外捕捉やResource解放はこの関数で行わない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
	}

	// slash区切りの面トークンを位置・UV・法線Indexへ分解する。
	// token: v、v/vt、v//vn、v/vt/vnなどの1頂点分。
	// result: 0初期化済み出力先。空成分は代入しない。
	// 戻り値: 位置Indexが0以外ならtrue。stoiの例外、文字列末尾までの数値検証は扱わない。
	bool ParseOBJIndex(const std::string& token,OBJ_INDEX& result)
	{
		// 空成分も区切り順に扱い、1//3の3をUVとして読まないようにする。
		std::stringstream stream(token);
		std::string value{};
		// 先頭成分を取得できなければ解析失敗とする。
		if(!std::getline(stream,value,'/'))return false;
		// 位置成分が空でなければ整数化する。結果0は最後の戻り値で不成立になる。
		if(!value.empty())result.positionIndex = std::stoi(value);
		// 第2成分を取得でき、かつ非空の場合だけUV Indexへ代入する。
		if(std::getline(stream,value,'/') && !value.empty())result.UVIndex = std::stoi(value);
		// 第3成分を取得でき、かつ非空の場合だけ法線Indexへ代入する。
		if(std::getline(stream,value,'/') && !value.empty())result.normalIndex = std::stoi(value);
		return result.positionIndex != 0;
	}

	// 位置だけではUV境界が失われるため、3つのIndexの組を共有判定Keyにする。
	// index: 位置・UV・法線Index。戻り値はslashで連結した文字列であり、座標値の同一性は比較しない。
	std::string MakeVertexKey(const OBJ_INDEX& index)
	{
		// 法線を描画へ使わなくても法線Indexは共有条件へ残す。
		return std::to_string(index.positionIndex) + "/" + std::to_string(index.UVIndex) + "/" + std::to_string(index.normalIndex);
	}
}

//========= ライフサイクル・描画関数=========
// 旧Resourceを解放した後、OBJと描画Resourceを生成する。
bool ObjModelRenderer::Initialize(GraphicsSystem& graphicsSystem,const std::wstring& OBJFilePath,const std::wstring& textureFilePath)
{
	// GetAddressOfの出力先を空にする。以後の失敗でも旧モデルへは復帰しない。
	Uninit();

	// Deviceを借用する。RendererにSystemやDeviceポインターを登録する処理はない。
	ID3D11Device* device = graphicsSystem.GetDevice();
	// Deviceがなければ何も生成せずfalse。旧Resourceは先頭のUninitで解放済み。
	if(device == nullptr)return false;

	// 空の出力配列へ解析結果を受け取る。例外時は関数外へ伝播し、ローカル配列の寿命に従って破棄される。
	std::vector<VERTEX> vertices{};
	std::vector<unsigned int> indices{};
	// OBJのOpen・参照範囲・非空判定等が失敗したらAssetログとUninitを行いfalseを返す。
	if(!LoadOBJFile(OBJFilePath,vertices,indices)){
		WriteOBJModelAssetError(L"OBJ読込失敗",OBJFilePath);
		Uninit();
		return false;
	}

	// DEFAULT・頂点用途で生成する。ByteWidthは頂点数×1頂点のバイト数をUINTへ変換する。
	// 巨大配列での積・UINT変換の上限検証は追加しない。CPUAccess等の残りはゼロ設定を維持する。
	D3D11_BUFFER_DESC vertex_buffer_desc{};
	vertex_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(VERTEX) * vertices.size());
	vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	// 初期データは連続するCPU頂点配列。所有権は渡さず、CreateBuffer中に有効に保持する。
	// Bufferなので画像用Pitch類はゼロのまま。生成先はメンバーのComPtr。
	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices.data();
	const HRESULT vertex_buffer_result = device->CreateBuffer(&vertex_buffer_desc,&vertex_data,m_vertexBuffer.GetAddressOf());
	// Vertex Buffer生成失敗ならAPI名とHRESULTを記録し、部分Resourceを解放する。
	if(FAILED(vertex_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(VertexBuffer)",vertex_buffer_result);
		Uninit();
		return false;
	}

	// unsigned intの要素幅とR32_UINTを対応させる。総バイト数のUINT変換は既存式を維持し、上限検証はしない。
	D3D11_BUFFER_DESC index_buffer_desc{};
	index_buffer_desc.ByteWidth = static_cast<UINT>(sizeof(unsigned int) * indices.size());
	index_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	index_buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	// 三角形の頂点参照順を初期データに渡す。配列の所有は移さず、Index Bufferの参照をメンバーで受け取る。
	D3D11_SUBRESOURCE_DATA index_data{};
	index_data.pSysMem = indices.data();
	const HRESULT index_buffer_result = device->CreateBuffer(&index_buffer_desc,&index_data,m_indexBuffer.GetAddressOf());
	// Index Buffer生成失敗では先に生成済みのVertex BufferもUninitで解放する。
	if(FAILED(index_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(IndexBuffer)",index_buffer_result);
		Uninit();
		return false;
	}

	// VSのCSOをローカル配列へ読み、Shaderと入力signature照合に使う。ここではHLSLをコンパイルしない。
	std::vector<char> vertex_shader_binary{};
	// VS読込失敗ならメッシュBufferを含めて部分解放する。Loaderの失敗通知をそのまま扱う。
	if(!ShaderBinaryLoader::Load(ns_MODEL_VERTEX_SHADER_CSO_PATH,vertex_shader_binary)){
		Uninit();
		return false;
	}

	// CSO先頭とバイト長を渡してVSを生成する。ClassLinkageはnullptr、所有参照はメンバーで受け取る。
	const HRESULT vertex_shader_result = device->CreateVertexShader(vertex_shader_binary.data(),vertex_shader_binary.size(),
																																	nullptr,m_vertexShader.GetAddressOf());
	// VS生成失敗ならAPI名とHRESULTを記録し、先行するBufferも解放する。
	if(FAILED(vertex_shader_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateVertexShader",vertex_shader_result);
		Uninit();
		return false;
	}

	// Slot0から頂点単位でPOSITION0=float3とTEXCOORD0=float2を読む。offsetofはCPU頂点内のバイト位置。
	// PER_VERTEX_DATA・StepRate0でInstance属性にはしない。法線・頂点Colorを追加しない。
	const D3D11_INPUT_ELEMENT_DESC input_elements[]
	{
		{ns_POSITION_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(VERTEX,position),D3D11_INPUT_PER_VERTEX_DATA,0},
		{ns_TEXCOORD_SEMANTIC_NAME,0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(VERTEX,UV),D3D11_INPUT_PER_VERTEX_DATA,0}
	};

	// CPU属性記述とVS入力signatureを対応付ける。CSOはこの生成完了まで有効に保つ。StrideはDraw時に別途渡す。
	const HRESULT input_layout_result = device->CreateInputLayout(input_elements,ARRAYSIZE(input_elements),vertex_shader_binary.data(),
																																vertex_shader_binary.size(),m_inputLayout.GetAddressOf());
	// Layout生成失敗ならVS・メッシュBufferもまとめて部分解放する。
	if(FAILED(input_layout_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateInputLayout",input_layout_result);
		Uninit();
		return false;
	}

	// PSのCSOをローカル配列へ読み込む。
	std::vector<char> pixel_shader_binary{};
	// PS読込失敗なら生成済みLayout・VS・メッシュBufferを含めて解放する。
	if(!ShaderBinaryLoader::Load(ns_MODEL_PIXEL_SHADER_CSO_PATH,pixel_shader_binary)){
		Uninit();
		return false;
	}

	// BasicColorPSのバイナリからPSを生成する。Textureや定数BufferのBindは生成時ではなくDrawで行う。
	const HRESULT pixel_shader_result = device->CreatePixelShader(pixel_shader_binary.data(),pixel_shader_binary.size(),
																																nullptr,m_pixelShader.GetAddressOf());
	// PS生成失敗ならAPI名とHRESULTを記録し、先行ResourceもUninitする。
	if(FAILED(pixel_shader_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreatePixelShader",pixel_shader_result);
		Uninit();
		return false;
	}

	// 64バイトのCamera転送型に対応するVS b0用Buffer。初期データを渡さず、描画前のUpdateで設定する。
	HRESULT camera_buffer_result{};
	// Camera Buffer生成失敗なら結果を記録し、メッシュとShaderも部分解放する。
	if(!m_cameraBuffer.Init(device,camera_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(CameraConstants)",camera_buffer_result);
		Uninit();
		return false;
	}

	// 80バイトのObject転送型に対応するVS/PS b1用Buffer。Worldと色を描画ごとに更新する。
	HRESULT object_buffer_result{};
	// Object Buffer生成失敗なら先行Camera Bufferも含めて解放する。
	if(!m_objectBuffer.Init(device,object_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(ObjectConstants)",object_buffer_result);
		Uninit();
		return false;
	}

	// 32バイトのMaterial転送型に対応するVS/PS b2用Buffer。UV倍率・使用値と既存の転送配置を維持する。
	HRESULT material_buffer_result{};
	// Material Buffer生成失敗なら先行する全ての所有ResourceをUninitする。
	if(!m_materialBuffer.Init(device,material_buffer_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateBuffer(MaterialConstants)",material_buffer_result);
		Uninit();
		return false;
	}

	// 非空パスの場合だけ画像を読む。空パスは短絡で読込を省き、SRVなしの単色経路を選ぶ。
	// 非空パスの読込失敗ではAssetログを記録し、先行Buffer・Shader等も解放してfalseを返す。
	if(!textureFilePath.empty() && !LoadTextureFromFile(device,textureFilePath)){
		WriteOBJModelAssetError(L"Texture読込失敗",textureFilePath);
		Uninit();
		return false;
	}

	// 縮小・拡大・Mip間のLinear参照と、U/V/WのWRAPを指定する。Mip画像を自動生成する設定ではない。
	// 許可LODは0〜FLOAT32_MAX、実際のMip数は画像次第。その他はゼロ設定で既存値を維持する。
	// 画像なしでもSamplerを作る。2D参照で未使用のWも既存のWRAP設定を維持する。
	D3D11_SAMPLER_DESC sampler_desc{};
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_desc.MinLOD = 0.0f;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
	const HRESULT sampler_state_result = device->CreateSamplerState(&sampler_desc,m_textureSampler.GetAddressOf());
	// Sampler生成失敗なら画像SRVを含め全ての所有Resourceを解放する。
	if(FAILED(sampler_state_result)){
		WriteOBJModelGraphicsError(L"ID3D11Device::CreateSamplerState",sampler_state_result);
		Uninit();
		return false;
	}

	// 全生成後にだけ要素数を確定する。unsigned intの上限検証は追加しない。CPU配列は関数終了で破棄できる。
	m_indexCount = static_cast<unsigned int>(indices.size());
	return true;
}

// 指定変換・色と、モデル全体で共有する画像の有無を使って描く。
void ObjModelRenderer::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const XMMATRIX& viewMatrix,
														const XMMATRIX& projectionMatrix,const XMFLOAT4& color)
{
	// 必須のBuffer・Shader・Layout・SamplerとIndex数がなければ転送・Bind・描画を省く。SRVなしは有効な単色状態。
	if(!m_vertexBuffer || !m_indexBuffer || !m_cameraBuffer.IsValid() ||
		 !m_objectBuffer.IsValid() || !m_materialBuffer.IsValid() || !m_vertexShader ||
		 !m_pixelShader || !m_inputLayout || !m_textureSampler || m_indexCount == 0)return;

	// Contextを借用し、取得できない場合は描画しない。利用側のRender Passや描画先の検証は追加しない。
	ID3D11DeviceContext* context = graphicsSystem.GetContext();
	// Context未取得なら以後のGPU転送・Bindを実行しない。
	if(context == nullptr)return;

	// View*Projectionの順で合成し、Worldと共に転置する。HLSLへの格納契約を合わせる処理で、座標系変更ではない。
	const XMMATRIX view_projection_matrix = XMMatrixTranspose(viewMatrix * projectionMatrix);
	const XMMATRIX transposed_world_matrix = XMMatrixTranspose(worldMatrix);

	// World→Clipの転置済み行列をCamera転送型のXMFLOAT4X4へ保存する。
	CAMERA_CONSTANTS camera_constants{};
	XMStoreFloat4x4(&camera_constants.viewProjectionMatrix,view_projection_matrix);

	// Local→Worldの転置済み行列とRGBA色をObject転送型へ保存する。色の合成はPS側へ任せる。
	OBJECT_CONSTANTS object_constants{};
	XMStoreFloat4x4(&object_constants.worldMatrix,transposed_world_matrix);
	object_constants.color = color;

	// UVはU/Vとも1倍を維持する。SRVを所有すれば使用値1、なければ0で画像参照を省く。
	// baseColorは現行BasicColorで未参照でも既定値を残し、paddingと共にCPU/HLSL間の配置を維持する。
	MATERIAL_CONSTANTS material_constants{};
	material_constants.uvTiling = XMFLOAT2{1.0f,1.0f};
	material_constants.useTexture = m_textureView ? 1.0f : 0.0f;

	// Strideは頂点間のバイト数、Offset0はBuffer先頭。配列は借用ポインターの束で所有者はメンバーのまま。
	const UINT vertex_stride = sizeof(VERTEX);
	const UINT vertex_offset{};
	ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};

	// 各定数BufferとPSの画像・Samplerを1要素配列へまとめる。画像なしなら配列のSRV要素はnullptr。
	ID3D11Buffer* camera_buffers[]{m_cameraBuffer.Get()};
	ID3D11Buffer* object_buffers[]{m_objectBuffer.Get()};
	ID3D11Buffer* material_buffers[]{m_materialBuffer.Get()};
	ID3D11ShaderResourceView* texture_views[]{m_textureView.Get()};
	ID3D11SamplerState* samplers[]{m_textureSampler.Get()};

	// UpdateSubresourceで3つのBuffer全体を更新する。更新はBindとは別で、このAPIから成否の戻り値は得ない。
	m_cameraBuffer.Update(context,camera_constants);
	m_objectBuffer.Update(context,object_constants);
	m_materialBuffer.Update(context,material_constants);

	// Slot0の頂点1本と32bit Indexを接続する。Layoutで属性を解釈し、Triangle Listは3Indexで1三角形を指定する。
	context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R32_UINT,0);
	context->IASetInputLayout(m_inputLayout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// VS/PSと各Stageの定数slotを上書きする。Camera=VS b0、Object=VS/PS b1、Material=VS/PS b2。
	// 動的リンク用ClassInstanceは指定せず、PS b0は変更しない。Bind状態はDraw後も残る。
	context->VSSetShader(m_vertexShader.Get(),nullptr,0);
	context->PSSetShader(m_pixelShader.Get(),nullptr,0);
	context->VSSetConstantBuffers(0,1,camera_buffers);
	context->VSSetConstantBuffers(1,1,object_buffers);
	context->VSSetConstantBuffers(2,1,material_buffers);
	context->PSSetConstantBuffers(1,1,object_buffers);
	context->PSSetConstantBuffers(2,1,material_buffers);

	// PS t0へ画像を設定し、画像なしでもnullptrを明示して直前のSRVを残さない。補間・繰り返しのSamplerはPS s0へ渡す。
	context->PSSetShaderResources(0,1,texture_views);
	context->PSSetSamplers(0,1,samplers);
	// 保持する要素数を先頭Index0・BaseVertex0から描く。描画先・Blend・Depth等は利用側の設定を使い、復元しない。
	context->DrawIndexed(m_indexCount,0,0);
}

// メンバーのCOM所有参照を解放する。ContextのBind解除やGPU完了待機とは区別する。
void ObjModelRenderer::Uninit()
{
	// Sampler→画像SRVの順に解放する。Context等が別に持つCOM参照は残り得る。
	m_textureSampler.Reset();
	m_textureView.Reset();

	// Material→Object→Camera、Index→頂点の順にBufferを解放する。
	m_materialBuffer.Uninit();
	m_objectBuffer.Uninit();
	m_cameraBuffer.Uninit();
	m_indexBuffer.Reset();
	m_vertexBuffer.Reset();

	// Layout→PS→VSの順に所有参照を解放する。
	m_inputLayout.Reset();
	m_pixelShader.Reset();
	m_vertexShader.Reset();

	// Resource解放後に描画数だけを残さず、次のDrawが未準備として戻るよう0にする。
	m_indexCount = {};
}

//========= 補助関数=========
// v・vt・fを解析し、頂点の共有と面の扇形三角形化を行う。
bool ObjModelRenderer::LoadOBJFile(const std::wstring& OBJFilePath,std::vector<VERTEX>& vertices,std::vector<unsigned int>& indices)
{
	// 出力は呼出側が空にする前提。先頭でclearせず、既存データへの追記動作を維持する。
	std::ifstream file(OBJFilePath);
	// Open失敗なら出力を変更せずfalseを返す。
	if(!file)return false;

	// OBJの独立した位置・UV配列と、GPU頂点Indexへの共有対応表を用意する。法線配列は作らない。
	std::vector<XMFLOAT3> positions{};
	std::vector<XMFLOAT2> UVs{};
	std::unordered_map<std::string,unsigned int> vertex_map{};

	// 行単位で入力する。読込終了後の理由を検証する処理は追加しない。
	std::string line{};
	// 取得できる各行を先頭から走査し、v・vt・fだけを処理する。
	while(std::getline(file,line)){
		// 空行・コメント・vn・mtllib・usemtl等は対応する分岐がなく読み飛ばす。
		std::stringstream line_stream(line);
		std::string type{};
		line_stream >> type;

		// 位置行なら3成分を読み、OBJの読込順に保存して次行へ進む。抽出成否や座標系の変換は追加しない。
		if(type == "v"){
			XMFLOAT3 position{};
			line_stream >> position.x >> position.y >> position.z;
			positions.push_back(position);
			continue;
		}

		// UV行なら2成分を読み、読込時にVだけを反転して次行へ進む。
		if(type == "vt"){
			// 初期0へ成分を抽出する。数値抽出の完全な成否判定は行わない。
			XMFLOAT2 UV{};
			line_stream >> UV.x >> UV.y;

			// Vを1-Vへ写し上下を反転する。範囲制限はせず、Shader側で二重に反転する変更もしない。
			UV.y = 1.0f - UV.y;
			UVs.push_back(UV);
			continue;
		}

		// 面以外は読み飛ばす。Material・Object・Groupごとのメッシュ分割は実装しない。
		if(type != "f")continue;

		// 面1枚の有効Index組を保存する。Parseのfalseはそのトークンだけを追加対象から外す。
		std::vector<OBJ_INDEX> face_indices{};
		std::string token{};
		// 面行の残りを空白区切りで最後まで取り出し、slash成分へ分解する。
		while(line_stream >> token){
			// 空成分は0のまま残す。stoiが投げる例外はここで捕捉しない。
			OBJ_INDEX OBJ_index{};
			// 位置Indexが非0と解析されたトークンだけを面の頂点候補へ追加する。
			if(ParseOBJIndex(token,OBJ_index))face_indices.push_back(OBJ_index);
		}

		// 有効トークンが3個未満なら三角形を作れないため、この面だけを省く。
		if(face_indices.size() < 3)continue;

		// 先頭0を固定し、(0,1,2),(0,2,3)...と末尾まで分割する。凹面や自己交差面の一般的三角形化は保証しない。
		for(std::size_t index = 1; index + 1 < face_indices.size(); ++index){
			// 現在の扇形三角形を、元の面トークン順のまま3頂点で表す。
			const OBJ_INDEX triangle[]
			{
				face_indices[0],
				face_indices[index],
				face_indices[index + 1]
			};

			// 三角形の3頂点を順に処理し、共有済みなら再利用、未登録なら位置・UVからGPU頂点を追加する。
			for(const OBJ_INDEX& OBJ_index : triangle){
				// 位置・UV・法線Indexの組で検索する。位置だけの共有に変えるとUV境界等の共有条件が変わる。
				const std::string key = MakeVertexKey(OBJ_index);
				const auto found = vertex_map.find(key);

				// 登録済みならIndexだけを追加し、同じGPU頂点を重複生成しない。
				if(found != vertex_map.end()){
					indices.push_back(found->second);
					continue;
				}

				// 正の1始まり位置Indexを0始まりへ変換する。負数相対参照や極端な不正値の安全策は追加しない。
				const int position_index = OBJ_index.positionIndex - 1;
				// 位置参照が範囲外ならfalse。ここまで出力配列へ追加した頂点・Indexは巻き戻さない。
				if(position_index < 0 || position_index >= static_cast<int>(positions.size()))return false;

				// 未指定・0・負数UV Indexでは(0,0)を使う。正数だけが下の配列参照へ進む。
				XMFLOAT2 UV{};
				// 正のUV Indexがあれば0始まりへ変換し、対応する読込済みUVを取得する。
				if(OBJ_index.UVIndex > 0){
					// OBJの1始まりを配列の0始まりへ合わせる。
					const int UV_index = OBJ_index.UVIndex - 1;
					// UV参照が範囲外ならfalse。位置と同様、既存の部分追加は残す。
					if(UV_index < 0 || UV_index >= static_cast<int>(UVs.size()))return false;

					// V反転済みのUVをそのまま使う。法線値は取得しない。
					UV = UVs[UV_index];
				}

				// 未登録の位置・UVを1頂点にし、追加前の要素数を新Indexとする。unsigned int上限の検証は行わない。
				const VERTEX vertex{positions[position_index],UV};
				const unsigned int new_index = static_cast<unsigned int>(vertices.size());

				// 頂点→共有表→Indexの既存順で追加する。途中の例外でも出力配列を巻き戻す処理はない。
				vertices.push_back(vertex);
				vertex_map.emplace(key,new_index);
				indices.push_back(new_index);
			}
		}
	}

	// 最終判定は両出力の非空だけ。ストリーム終了理由やOBJ全体の文法が正しいことまでは保証しない。
	return !vertices.empty() && !indices.empty();
}

// 共通WIC Loaderの成否を返し、画像Viewの所有参照をメンバーへ受け取る。
bool ObjModelRenderer::LoadTextureFromFile(ID3D11Device* device,const std::wstring& textureFilePath)
{
	// Loaderが既存SRVをResetして読み込む。Deviceと文字列を所有せず、独自の復旧や再試行も行わない。
	return TextureLoader::LoadWicTexture(device,textureFilePath,m_textureView);
}