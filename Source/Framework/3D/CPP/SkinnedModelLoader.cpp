#include "../H/SkinnedModelLoader.h"

// FBXの読込を実装する。Assimpの読込と、左手系への変換、ノードの階層・メッシュ・ボーンの重み・材質の画像のパスの取り出し、概要のログを行う。

//========= C++標準ライブラリ インクルード=========
#include <string> // std::wstringとstd::stringで、パスと名前を扱い、ログの文字列を組み立てる。
#include <unordered_map> // ボーンの名前から、ボーンの番号を引く。複数のメッシュで、同じボーンを1本にまとめる。

//========= Windows インクルード=========
#include <windows.h> // WideCharToMultiByte・MultiByteToWideCharで、パスとログの文字列を、UTF-8とワイド文字の間で変換する。

//========= 外部ライブラリ インクルード=========
#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

// 読込の設定と、読込の補助関数をまとめる。
namespace
{
  //========= 読込の設定=========
  // 読込後に行う処理。三角形化・同じ頂点の統合・ボーンの影響を4本までに制限・法線の生成・左手系への変換（座標・UV・面の向き・行列）。
  constexpr unsigned int ns_IMPORT_FLAGS = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights
    | aiProcess_GenSmoothNormals | aiProcess_ConvertToLeftHanded;

  //========= 変換の補助関数=========
  // Assimpの行列（列ベクトル向き、移動は4列目）を、DirectXMathの行列（行ベクトル向き、移動は4行目）へ、転置して変換する。
  XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& source)
  {
    return XMFLOAT4X4{source.a1,source.b1,source.c1,source.d1,
      source.a2,source.b2,source.c2,source.d2,
      source.a3,source.b3,source.c3,source.d3,
      source.a4,source.b4,source.c4,source.d4};
  }

  // ワイド文字列を、UTF-8の文字列へ変換する。Assimpが、UTF-8のパスを受け取るため。
  std::string ToUtf8(const std::wstring& text)
  {
    if(text.empty())return std::string{};
    const int size = WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    std::string result(static_cast<std::size_t>(size),'\0');
    WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),result.data(),size,nullptr,nullptr);
    return result;
  }

  // UTF-8の文字列を、ワイド文字列へ変換する。Assimpの文字列を、ログへ出すため。
  std::wstring ToWide(const std::string& text)
  {
    if(text.empty())return std::wstring{};
    const int size = MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0);
    std::wstring result(static_cast<std::size_t>(size),L'\0');
    MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),result.data(),size);
    return result;
  }

  //========= 階層の補助関数=========
  // ノードと、その子孫を、親が先に並ぶ順（深さ優先）で、modelのnodesへ追加する。
  void AppendNode(const aiNode& node,int parentIndex,SKINNED_MODEL_DATA& model)
  {
    // このノードを追加し、番号を控える。
    const int node_index = static_cast<int>(model.nodes.size());
    SKINNED_NODE skinned_node{};
    skinned_node.name = node.mName.C_Str();
    skinned_node.parentIndex = parentIndex;
    skinned_node.localTransform = ConvertMatrix(node.mTransformation);
    model.nodes.push_back(skinned_node);

    // 子を、このノードを親にして、追加する。
    for(unsigned int child_index = 0; child_index < node.mNumChildren; ++child_index){
      AppendNode(*node.mChildren[child_index],node_index,model);
    }
  }

  // 名前が一致するノードの番号を返す。見つからなければ-1。
  int FindNodeIndex(const std::vector<SKINNED_NODE>& nodes,const std::string& name)
  {
    for(std::size_t node_index = 0; node_index < nodes.size(); ++node_index){
      if(nodes[node_index].name == name)return static_cast<int>(node_index);
    }
    return -1;
  }

  //========= ボーンの補助関数=========
  // ボーンの番号を返す。まだなければ追加する。名前が同じボーンは、複数のメッシュにまたがっても、1本にまとめる。
  // 戻り値: ボーンの番号。ボーンの上限を超える場合と、対応するノードがない場合は-1。
  int FindOrAddBone(const aiBone& bone,std::unordered_map<std::string,int>& boneLookup,SKINNED_MODEL_DATA& model)
  {
    // すでに追加したボーンは、その番号を返す。
    const std::string bone_name = bone.mName.C_Str();
    const auto found = boneLookup.find(bone_name);
    if(found != boneLookup.end())return found->second;

    // 上限を超える場合と、同じ名前のノードがない場合は、追加できない。
    if(static_cast<int>(model.bones.size()) >= SKINNED_MODEL::ns_MAX_BONE_COUNT)return -1;
    const int node_index = FindNodeIndex(model.nodes,bone_name);
    if(node_index < 0)return -1;

    // 新しいボーンとして追加する。
    SKINNED_BONE skinned_bone{};
    skinned_bone.nodeIndex = node_index;
    skinned_bone.offsetMatrix = ConvertMatrix(bone.mOffsetMatrix);
    const int bone_index = static_cast<int>(model.bones.size());
    model.bones.push_back(skinned_bone);
    boneLookup[bone_name] = bone_index;
    return bone_index;
  }

  // メッシュのボーンの重みを、頂点へ書き込む。1つの頂点につき、空いている枠へ、先頭から入れる。
  // baseVertex: このメッシュの先頭の頂点の、modelのverticesでの添字。
  bool ApplyBoneWeights(const aiMesh& mesh,unsigned int baseVertex,std::unordered_map<std::string,int>& boneLookup,SKINNED_MODEL_DATA& model)
  {
    // メッシュの全てのボーンを順に処理する。
    for(unsigned int bone_slot = 0; bone_slot < mesh.mNumBones; ++bone_slot){
      const aiBone& bone = *mesh.mBones[bone_slot];
      const int bone_index = FindOrAddBone(bone,boneLookup,model);
      if(bone_index < 0)return false;

      // ボーンが影響する全ての頂点の、空いている枠へ、ボーン番号と重みを書く。範囲外の頂点と、重みが0の影響は、無視する。
      for(unsigned int weight_slot = 0; weight_slot < bone.mNumWeights; ++weight_slot){
        const aiVertexWeight& weight = bone.mWeights[weight_slot];
        const std::size_t vertex_index = static_cast<std::size_t>(baseVertex) + weight.mVertexId;
        if(vertex_index >= model.vertices.size() || weight.mWeight <= 0.0f)continue;

        SKINNED_VERTEX& vertex = model.vertices[vertex_index];
        for(std::size_t slot = 0; slot < SKINNED_MODEL::ns_BONES_PER_VERTEX; ++slot){
          if(vertex.boneWeights[slot] > 0.0f)continue;
          vertex.boneIndices[slot] = static_cast<std::uint8_t>(bone_index);
          vertex.boneWeights[slot] = weight.mWeight;
          break;
        }
      }
    }
    return true;
  }

  // 全ての頂点の重みの合計を1へそろえ、重みが全て0の頂点を数える。
  void NormalizeWeights(SKINNED_MODEL_DATA& model)
  {
    for(SKINNED_VERTEX& vertex : model.vertices){
      float total_weight = 0.0f;
      for(const float weight : vertex.boneWeights){
        total_weight += weight;
      }

      // 重みがなければ数え、あれば、合計が1になるよう割る。
      if(total_weight <= 0.0f){
        ++model.unweightedVertexCount;
        continue;
      }
      for(float& weight : vertex.boneWeights){
        weight /= total_weight;
      }
    }
  }

  //========= メッシュ・材質の補助関数=========
  // メッシュの頂点・Index・描画範囲を、modelへ追加し、ボーンの重みを書き込む。位置か面がないメッシュは、何も追加せず成功とする。
  bool AppendMesh(const aiMesh& mesh,std::unordered_map<std::string,int>& boneLookup,SKINNED_MODEL_DATA& model)
  {
    if(!mesh.HasPositions() || !mesh.HasFaces())return true;

    // 頂点を追加する。法線とUVがない場合は、0のままにする。
    const unsigned int base_vertex = static_cast<unsigned int>(model.vertices.size());
    for(unsigned int vertex_index = 0; vertex_index < mesh.mNumVertices; ++vertex_index){
      SKINNED_VERTEX vertex{};
      vertex.position = XMFLOAT3{mesh.mVertices[vertex_index].x,mesh.mVertices[vertex_index].y,mesh.mVertices[vertex_index].z};
      if(mesh.HasNormals()){
        vertex.normal = XMFLOAT3{mesh.mNormals[vertex_index].x,mesh.mNormals[vertex_index].y,mesh.mNormals[vertex_index].z};
      }
      if(mesh.HasTextureCoords(0)){
        vertex.uv = XMFLOAT2{mesh.mTextureCoords[0][vertex_index].x,mesh.mTextureCoords[0][vertex_index].y};
      }
      model.vertices.push_back(vertex);
    }

    // 三角形のIndexを、全メッシュを通した頂点の添字で追加し、描画範囲を記録する。三角形でない面は、無視する。
    SKINNED_SUBMESH submesh{};
    submesh.indexStart = static_cast<unsigned int>(model.indices.size());
    for(unsigned int face_index = 0; face_index < mesh.mNumFaces; ++face_index){
      const aiFace& face = mesh.mFaces[face_index];
      if(face.mNumIndices != 3)continue;
      for(unsigned int corner = 0; corner < 3; ++corner){
        model.indices.push_back(base_vertex + face.mIndices[corner]);
      }
    }
    submesh.indexCount = static_cast<unsigned int>(model.indices.size()) - submesh.indexStart;
    submesh.materialIndex = mesh.mMaterialIndex;
    model.submeshes.push_back(submesh);

    return ApplyBoneWeights(mesh,base_vertex,boneLookup,model);
  }

  // パスの名前で、FBXに埋め込まれた画像を探し、ファイルの中身を、そのままコピーして返す。圧縮された形式（高さ0）だけ、対応する。なければ空。
  std::vector<std::uint8_t> ExtractEmbeddedImage(const aiScene& scene,const char* texturePath)
  {
    const aiTexture* texture = scene.GetEmbeddedTexture(texturePath);
    if(texture == nullptr || texture->mHeight != 0 || texture->pcData == nullptr)return std::vector<std::uint8_t>{};

    const std::uint8_t* head = reinterpret_cast<const std::uint8_t*>(texture->pcData);
    return std::vector<std::uint8_t>(head,head + texture->mWidth);
  }

  // 材質ごとに、拡散色の画像のパスを、modelへ書き込む。
  void AppendMaterials(const aiScene& scene,SKINNED_MODEL_DATA& model)
  {
    for(unsigned int material_index = 0; material_index < scene.mNumMaterials; ++material_index){
      SKINNED_MATERIAL material{};
      aiString texture_path;
      if(scene.mMaterials[material_index]->GetTexture(aiTextureType_DIFFUSE,0,&texture_path) == AI_SUCCESS){
        material.diffuseTexturePath = texture_path.C_Str();
        material.diffuseImageData = ExtractEmbeddedImage(scene,texture_path.C_Str());
      }
      model.materials.push_back(material);
    }
  }

  //========= ログの補助関数=========
  // 読み込んだデータの概要を、Infoのログへ出す。画像のパスも、材質ごとに出す。
  void LogSummary(const std::wstring& filePath,const SKINNED_MODEL_DATA& model)
  {
    Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,
                  L"FBXを読み込みました: " + filePath
                  + L" | 頂点 " + std::to_wstring(model.vertices.size())
                  + L" / Index " + std::to_wstring(model.indices.size())
                  + L" / 描画範囲 " + std::to_wstring(model.submeshes.size())
                  + L" / ノード " + std::to_wstring(model.nodes.size())
                  + L" / ボーン " + std::to_wstring(model.bones.size())
                  + L" / 材質 " + std::to_wstring(model.materials.size())
                  + L" / 埋込画像 " + std::to_wstring(model.embeddedTextureCount)
                  + L" / 重みなし頂点 " + std::to_wstring(model.unweightedVertexCount));

    for(std::size_t material_index = 0; material_index < model.materials.size(); ++material_index){
      Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,
                    L"  材質 " + std::to_wstring(material_index) + L" の画像: " + ToWide(model.materials[material_index].diffuseTexturePath)
                    + L" | 取り出した埋込画像 " + std::to_wstring(model.materials[material_index].diffuseImageData.size()) + L" バイト");
    }
  }

  // FBXに埋め込まれた全ての画像の、名前・形式・大きさを、Infoのログへ出す。形式は、圧縮された形式なら、拡張子の文字。
  void LogEmbeddedTextures(const aiScene& scene)
  {
    for(unsigned int texture_index = 0; texture_index < scene.mNumTextures; ++texture_index){
      const aiTexture& texture = *scene.mTextures[texture_index];
      Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,
                    L"  埋込画像 " + std::to_wstring(texture_index) + L": " + ToWide(texture.mFilename.C_Str())
                    + L" | 形式 " + ToWide(texture.achFormatHint) + L" | 幅 " + std::to_wstring(texture.mWidth) + L" / 高さ " + std::to_wstring(texture.mHeight));
    }
  }
  // 読込の失敗を、Errorのログへ出す。
  void LogFailure(const std::wstring& filePath,const std::wstring& reason)
  {
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,L"FBXの読込に失敗しました: " + filePath + L" | " + reason);
  }
}

//========= 読込関数=========
// FBXを読み込み、スキニング付きモデルのデータへ変換する。
bool SkinnedModelLoader::Load(const std::wstring& filePath,SKINNED_MODEL_DATA& outModel) const
{
  outModel = SKINNED_MODEL_DATA{};

  // FBXを読み込む。軸の補助ノード（Pivot）は、保持しない。保持しないと、アニメーションを1つのノードにまとめられ、階層が単純になる。
  Assimp::Importer importer;
  importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS,false);
  const aiScene* scene = importer.ReadFile(ToUtf8(filePath).c_str(),ns_IMPORT_FLAGS);
  if(scene == nullptr || scene->mRootNode == nullptr){
    LogFailure(filePath,ToWide(importer.GetErrorString()));
    return false;
  }

  // 階層と、根の変換の逆行列を取り出す。
  AppendNode(*scene->mRootNode,-1,outModel);
  aiMatrix4x4 root_inverse = scene->mRootNode->mTransformation;
  root_inverse.Inverse();
  outModel.globalInverseTransform = ConvertMatrix(root_inverse);

  // 全てのメッシュを、順に追加する。ボーンが上限を超える場合や、ノードがない場合は、失敗として、空のデータを返す。
  std::unordered_map<std::string,int> bone_lookup;
  for(unsigned int mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index){
    if(!AppendMesh(*scene->mMeshes[mesh_index],bone_lookup,outModel)){
      LogFailure(filePath,L"ボーンの数が上限(128)を超えたか、ボーンに対応するノードがありません。");
      outModel = SKINNED_MODEL_DATA{};
      return false;
    }
  }

  // 重みをそろえ、材質と、埋込画像の数を記録して、概要を出す。
  NormalizeWeights(outModel);
  AppendMaterials(*scene,outModel);
  outModel.embeddedTextureCount = scene->mNumTextures;
  LogSummary(filePath,outModel);
  LogEmbeddedTextures(*scene);
  return true;
}
