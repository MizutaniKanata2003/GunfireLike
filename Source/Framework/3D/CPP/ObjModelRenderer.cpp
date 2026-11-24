#include "../H/ObjModelRenderer.h"

// OBJを位置・UVの単一メッシュへ変換し、GPU生成・定数転送・Bind・描画・所有参照解放を実装する。BasicColorのShader・定数Bufferは、BasicColorPipelineを使う。
// 解析やサイズ検証の制限をコメントで明示するが、新しい検証・例外捕捉・復旧処理は追加しない。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // std::size_tで面の走査位置を扱う。
#include <fstream> // std::ifstreamでOBJを行単位に読み込む。
#include <istream> // std::istreamで、OBJ行の残りの面トークンを受け取る。
#include <sstream> // std::stringstreamでOBJ行とslash区切りの面トークンを解析する。
#include <string> // std::string・getline・stoi・to_stringでパス、Index、共有Keyとログを扱う。
#include <unordered_map> // 位置・UV・法線Indexの文字列KeyからGPU頂点Indexを検索する。
#include <vector> // OBJの位置・UV・頂点・Indexを可変長配列に保持する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/DirectX/H/MeshBuffers.h"
#include "Framework/DirectX/H/SamplerFactory.h"
#include "Framework/DirectX/H/TextureLoader.h"
#include "Framework/Etc/H/Logger.h"

// OBJ面トークンと、内部解析・失敗ログの補助処理をまとめる。
namespace
{
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

  // OBJの読込中に、行をまたいで保持する値。法線配列は作らない。
  struct OBJ_PARSE_STATE
  {
    // v行の位置を、OBJの読込順に保持する。
    std::vector<XMFLOAT3> positions{};
    // vt行のUVを、V反転後の値で、OBJの読込順に保持する。
    std::vector<XMFLOAT2> UVs{};
    // 位置・UV・法線Indexの組のKeyから、GPU頂点Indexを検索する共有対応表。
    std::unordered_map<std::string,unsigned int> vertexMap{};
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

  // 面行の残りの空白区切りトークンを、位置Indexが非0のものだけ集める。
  // lineStream: 面を示す先頭の語を読み終えた行。
  // 戻り値: 有効なIndex組。空成分は0のまま残す。stoiが投げる例外は、ここで捕捉しない。
  std::vector<OBJ_INDEX> ParseFaceIndices(std::istream& lineStream)
  {
    // 面1枚の有効Index組を保存する。Parseのfalseはそのトークンだけを追加対象から外す。
    std::vector<OBJ_INDEX> face_indices{};
    std::string token{};
    // 面行の残りを空白区切りで最後まで取り出し、slash成分へ分解する。
    while(lineStream >> token){
      OBJ_INDEX OBJ_index{};
      // 位置Indexが非0と解析されたトークンだけを面の頂点候補へ追加する。
      if(ParseOBJIndex(token,OBJ_index))face_indices.push_back(OBJ_index);
    }
    return face_indices;
  }

  // 1頂点分のIndexを、共有済みなら再利用し、未登録なら位置・UVからGPU頂点を追加する。
  // OBJIndex: 位置・UV・法線Index。
  // state: 読込済みの位置・UVと、共有対応表。未登録なら対応表へ追加する。
  // vertices: 出力する頂点配列。
  // indices: 出力するIndex配列。
  // 戻り値: 追加できたらtrue。位置・UVの参照が範囲外ならfalse。それまでに出力配列へ追加した頂点・Indexは巻き戻さない。
  bool AppendVertex(const OBJ_INDEX& OBJIndex,OBJ_PARSE_STATE& state,std::vector<BASIC_COLOR_VERTEX>& vertices,std::vector<unsigned int>& indices)
  {
    // 位置・UV・法線Indexの組で検索する。位置だけの共有に変えるとUV境界等の共有条件が変わる。
    const std::string key = MakeVertexKey(OBJIndex);
    const auto found = state.vertexMap.find(key);

    // 登録済みならIndexだけを追加し、同じGPU頂点を重複生成しない。
    if(found != state.vertexMap.end()){
      indices.push_back(found->second);
      return true;
    }

    // 正の1始まり位置Indexを0始まりへ変換する。負数相対参照や極端な不正値の安全策は追加しない。
    const int position_index = OBJIndex.positionIndex - 1;
    // 位置参照が範囲外ならfalse。
    if(position_index < 0 || position_index >= static_cast<int>(state.positions.size()))return false;

    // 未指定・0・負数UV Indexでは(0,0)を使う。正数だけが下の配列参照へ進む。
    XMFLOAT2 UV{};
    if(OBJIndex.UVIndex > 0){
      // OBJの1始まりを配列の0始まりへ合わせる。UV参照が範囲外ならfalse。
      const int UV_index = OBJIndex.UVIndex - 1;
      if(UV_index < 0 || UV_index >= static_cast<int>(state.UVs.size()))return false;

      // V反転済みのUVをそのまま使う。法線値は取得しない。
      UV = state.UVs[UV_index];
    }

    // 未登録の位置・UVを1頂点にし、追加前の要素数を新Indexとする。unsigned int上限の検証は行わない。
    const BASIC_COLOR_VERTEX vertex{state.positions[position_index],UV};
    const unsigned int new_index = static_cast<unsigned int>(vertices.size());

    // 頂点→共有表→Indexの既存順で追加する。途中の例外でも出力配列を巻き戻す処理はない。
    vertices.push_back(vertex);
    state.vertexMap.emplace(key,new_index);
    indices.push_back(new_index);
    return true;
  }

  // 面1枚を、先頭固定の扇形で三角形化して追加する。
  // faceIndices: 面の有効なIndex組。3個以上であること。
  // state: 読込済みの位置・UVと、共有対応表。
  // vertices: 出力する頂点配列。
  // indices: 出力するIndex配列。
  // 戻り値: 全て追加できたらtrue。範囲外の参照があればfalse。凹面や自己交差面の一般的三角形化は保証しない。
  bool AppendFace(const std::vector<OBJ_INDEX>& faceIndices,OBJ_PARSE_STATE& state,std::vector<BASIC_COLOR_VERTEX>& vertices,std::vector<unsigned int>& indices)
  {
    // 先頭0を固定し、(0,1,2),(0,2,3)...と末尾まで分割する。
    for(std::size_t index = 1; index + 1 < faceIndices.size(); ++index){
      // 現在の扇形三角形を、元の面トークン順のまま3頂点で表す。
      const OBJ_INDEX triangle[]{faceIndices[0],faceIndices[index],faceIndices[index + 1]};

      // 三角形の3頂点を順に処理する。失敗したら、以降の頂点も処理せずfalseを返す。
      for(const OBJ_INDEX& OBJ_index : triangle){
        if(!AppendVertex(OBJ_index,state,vertices,indices))return false;
      }
    }
    return true;
  }

  // OBJの1行を解析し、v・vt・fだけを処理する。
  // line: OBJの1行。
  // state: 読込済みの位置・UVと、共有対応表。v・vt行ならここへ追加する。
  // vertices: 出力する頂点配列。f行ならここへ追加する。
  // indices: 出力するIndex配列。f行ならここへ追加する。
  // 戻り値: 継続できればtrue。f行の位置・UVの参照が範囲外ならfalse。
  bool ParseOBJLine(const std::string& line,OBJ_PARSE_STATE& state,std::vector<BASIC_COLOR_VERTEX>& vertices,std::vector<unsigned int>& indices)
  {
    // 空行・コメント・vn・mtllib・usemtl等は対応する分岐がなく読み飛ばす。
    std::stringstream line_stream(line);
    std::string type{};
    line_stream >> type;

    // 位置行なら3成分を読み、OBJの読込順に保存する。抽出成否や座標系の変換は追加しない。
    if(type == "v"){
      XMFLOAT3 position{};
      line_stream >> position.x >> position.y >> position.z;
      state.positions.push_back(position);
      return true;
    }

    // UV行なら2成分を読み、読込時にVだけを1-Vへ反転して保存する。範囲制限はせず、Shader側で二重に反転する変更もしない。
    if(type == "vt"){
      XMFLOAT2 UV{};
      line_stream >> UV.x >> UV.y;
      UV.y = 1.0f - UV.y;
      state.UVs.push_back(UV);
      return true;
    }

    // 面以外は読み飛ばす。Material・Object・Groupごとのメッシュ分割は実装しない。
    if(type != "f")return true;

    // 有効トークンが3個未満なら三角形を作れないため、この面だけを省く。
    const std::vector<OBJ_INDEX> face_indices = ParseFaceIndices(line_stream);
    if(face_indices.size() < 3)return true;

    return AppendFace(face_indices,state,vertices,indices);
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
  std::vector<BASIC_COLOR_VERTEX> vertices{};
  std::vector<unsigned int> indices{};
  // OBJのOpen・参照範囲・非空判定等が失敗したらAssetログとUninitを行いfalseを返す。
  if(!LoadOBJFile(OBJFilePath,vertices,indices)){
    WriteOBJModelAssetError(L"OBJ読込失敗",OBJFilePath);
    Uninit();
    return false;
  }

  // Mesh Buffer→Shader・定数Buffer→画像→Samplerの順に生成する。失敗のログは、各生成関数が記録する。
  // 最初に失敗した時点で、生成済みのResourceを全て解放してfalseを返す。
  if(!CreateMeshBuffers(device,vertices,indices) || !m_pipeline.Initialize(device)){
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

  // 画像なしでもSamplerを作る。Linear補間・WRAPで生成する。失敗なら画像SRVを含め全ての所有Resourceを解放する。
  if(!SamplerFactory::CreateLinear(device,D3D11_TEXTURE_ADDRESS_WRAP,m_textureSampler)){
    Uninit();
    return false;
  }

  // 全生成後にだけ要素数を確定する。unsigned intの上限検証は追加しない。CPU配列は関数終了で破棄できる。
  m_indexCount = static_cast<unsigned int>(indices.size());
  return true;
}

// 指定変換・色と、モデル全体で共有する画像の有無を使って描く。
void ObjModelRenderer::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& worldMatrix,const CAMERA_MATRICES& camera,const XMFLOAT4& color)
{
  // 必須のBuffer・Shader・Layout・SamplerとIndex数がなければ転送・Bind・描画を省く。SRVなしは有効な単色状態。
  if(!m_vertexBuffer || !m_indexBuffer || !m_pipeline.IsValid() || !m_textureSampler || m_indexCount == 0)return;

  // Contextを借用し、取得できない場合は描画しない。利用側のRender Passや描画先の検証は追加しない。
  ID3D11DeviceContext* context = graphicsSystem.GetContext();
  if(context == nullptr)return;

  // UVはU/Vとも1倍を維持する。SRVを所有すれば画像を使い、なければ画像参照を省いて単色にする。
  // 色・UV倍率・画像の使用をまとめ、定数の組立てと転送を、BasicColorPipelineへ任せる。更新はBindとは別で、このAPIから成否の戻り値は得ない。
  const BASIC_COLOR_APPEARANCE color_appearance{color,XMFLOAT2{1.0f,1.0f},m_textureView != nullptr};
  m_pipeline.UpdateModelConstants(context,worldMatrix,camera,color_appearance);

  // モデルと画像を接続して描画する。
  BindAndDraw(context);
}

// メンバーのCOM所有参照を解放する。ContextのBind解除やGPU完了待機とは区別する。
void ObjModelRenderer::Uninit()
{
  // Sampler→画像SRVの順に解放する。Context等が別に持つCOM参照は残り得る。
  m_textureSampler.Reset();
  m_textureView.Reset();

  // BasicColorのResourceと、Index→頂点の順でBufferを解放する。
  m_pipeline.Uninit();
  m_indexBuffer.Reset();
  m_vertexBuffer.Reset();

  // Resource解放後に描画数だけを残さず、次のDrawが未準備として戻るよう0にする。
  m_indexCount = {};
}

//========= 読込の補助関数=========
// v・vt・fを解析し、頂点の共有と面の扇形三角形化を行う。
bool ObjModelRenderer::LoadOBJFile(const std::wstring& OBJFilePath,std::vector<BASIC_COLOR_VERTEX>& vertices,std::vector<unsigned int>& indices)
{
  // 出力は呼出側が空にする前提。先頭でclearせず、既存データへの追記動作を維持する。
  std::ifstream file(OBJFilePath);
  // Open失敗なら出力を変更せずfalseを返す。
  if(!file)return false;

  // OBJの独立した位置・UV配列と、GPU頂点Indexへの共有対応表を用意する。法線配列は作らない。
  OBJ_PARSE_STATE state{};

  // 行単位で入力する。読込終了後の理由を検証する処理は追加しない。
  std::string line{};
  while(std::getline(file,line)){
    // 位置・UVの参照が範囲外ならfalse。ここまで出力配列へ追加した頂点・Indexは巻き戻さない。
    if(!ParseOBJLine(line,state,vertices,indices))return false;
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

//========= 生成・描画の補助関数=========
// 共有後の頂点配列とIndex配列から、Vertex BufferとIndex Bufferを、この順に生成する。
bool ObjModelRenderer::CreateMeshBuffers(ID3D11Device* device,const std::vector<BASIC_COLOR_VERTEX>& vertices,const std::vector<unsigned int>& indices)
{
  // Vertex Bufferの指定。DEFAULT・頂点用途。ByteWidthは頂点数×1頂点のバイト数で、巨大配列での積・UINT変換の上限検証は追加しない。
  const MESH_BUFFER_SOURCE vertex_source{vertices.data(),sizeof(BASIC_COLOR_VERTEX) * vertices.size(),D3D11_USAGE_DEFAULT,
    D3D11_BIND_VERTEX_BUFFER,L"ID3D11Device::CreateBuffer(VertexBuffer)"};

  // Index Bufferの指定。unsigned intの要素幅とR32_UINTを対応させる。総バイト数のUINT変換は既存式を維持し、上限検証はしない。
  const MESH_BUFFER_SOURCE index_source{indices.data(),sizeof(unsigned int) * indices.size(),D3D11_USAGE_DEFAULT,
    D3D11_BIND_INDEX_BUFFER,L"ID3D11Device::CreateBuffer(IndexBuffer)"};

  // Vertex→Indexの順に生成し、最初の失敗で止める。失敗のログは、MeshBuffersが記録する。先に生成したVertex Bufferの解放は、呼出側が行う。
  return MeshBuffers::Create(device,vertex_source,m_vertexBuffer) && MeshBuffers::Create(device,index_source,m_indexBuffer);
}

// モデル・画像・Samplerを接続して描画する。
void ObjModelRenderer::BindAndDraw(ID3D11DeviceContext* context) const
{
  // Strideは頂点間のバイト数、Offset0はBuffer先頭。配列は借用ポインターの束で所有者はメンバーのまま。
  // 画像なしなら配列のSRV要素はnullptr。
  const UINT vertex_stride = sizeof(BASIC_COLOR_VERTEX);
  const UINT vertex_offset{};
  ID3D11Buffer* vertex_buffers[]{m_vertexBuffer.Get()};
  ID3D11ShaderResourceView* texture_views[]{m_textureView.Get()};
  ID3D11SamplerState* samplers[]{m_textureSampler.Get()};

  // Slot0の頂点1本と32bit Indexを接続する。Triangle Listは3Indexで1三角形を指定する。
  context->IASetVertexBuffers(0,1,vertex_buffers,&vertex_stride,&vertex_offset);
  context->IASetIndexBuffer(m_indexBuffer.Get(),DXGI_FORMAT_R32_UINT,0);
  context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // Layout・Shader・定数Bufferを接続する。PS b0は変更しない。Bind状態はDraw後も残る。
  m_pipeline.Bind(context);

  // PS t0へ画像を設定し、画像なしでもnullptrを明示して直前のSRVを残さない。補間・繰り返しのSamplerはPS s0へ渡す。
  context->PSSetShaderResources(0,1,texture_views);
  context->PSSetSamplers(0,1,samplers);

  // 保持する要素数を先頭Index0・BaseVertex0から描く。描画先・Blend・Depth等は利用側の設定を使い、復元しない。
  context->DrawIndexed(m_indexCount,0,0);
}