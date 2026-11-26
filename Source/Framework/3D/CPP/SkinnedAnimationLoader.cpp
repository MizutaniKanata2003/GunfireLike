#include "../H/SkinnedAnimationLoader.h"

// アニメーションのFBXの読込を実装する。Assimpの読込と、キーフレームの時刻の秒への変換、概要のログを行う。先頭の1本だけを読む関数と、全てのアニメーションを読む関数が、同じ変換を使う。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで、キーの数の最大を求める。
#include <string> // std::wstringとstd::stringで、パスと名前を扱い、ログの文字列を組み立てる。

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
  // 読込後に行う処理。左手系への変換だけ。モデルの読込（SkinnedModelLoader）と同じ向きにそろえる。
  constexpr unsigned int ns_IMPORT_FLAGS = aiProcess_ConvertToLeftHanded;
  // FBXが、1秒当たりの目盛りの数を持たない場合に使う値。
  constexpr double ns_DEFAULT_TICKS_PER_SECOND = 25.0;

  //========= 変換の補助関数=========
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

  //========= チャンネルの補助関数=========
  // ノード1つ分のアニメーションを、チャンネルへ変換して、clipへ追加する。時刻は、目盛りから秒へ変える。
  void AppendChannel(const aiNodeAnim& source,double ticksPerSecond,SKINNED_ANIMATION_CLIP& clip)
  {
    ANIMATION_CHANNEL channel{};
    channel.nodeName = source.mNodeName.C_Str();

    // 位置・回転・拡大のキーを、それぞれ、時刻を秒にして、順に追加する。
    for(unsigned int key_index = 0; key_index < source.mNumPositionKeys; ++key_index){
      const aiVectorKey& key = source.mPositionKeys[key_index];
      channel.positionKeys.push_back(ANIMATION_VECTOR_KEY{static_cast<float>(key.mTime / ticksPerSecond),XMFLOAT3{key.mValue.x,key.mValue.y,key.mValue.z}});
    }
    for(unsigned int key_index = 0; key_index < source.mNumRotationKeys; ++key_index){
      const aiQuatKey& key = source.mRotationKeys[key_index];
      channel.rotationKeys.push_back(ANIMATION_ROTATION_KEY{static_cast<float>(key.mTime / ticksPerSecond),XMFLOAT4{key.mValue.x,key.mValue.y,key.mValue.z,key.mValue.w}});
    }
    for(unsigned int key_index = 0; key_index < source.mNumScalingKeys; ++key_index){
      const aiVectorKey& key = source.mScalingKeys[key_index];
      channel.scaleKeys.push_back(ANIMATION_VECTOR_KEY{static_cast<float>(key.mTime / ticksPerSecond),XMFLOAT3{key.mValue.x,key.mValue.y,key.mValue.z}});
    }
    clip.channels.push_back(channel);
  }

  // アニメーション1つを、名前・長さ（目盛りから秒）・ノードごとのチャンネルへ変換して、clipへ書き込む。
  void ConvertAnimation(const aiAnimation& animation,SKINNED_ANIMATION_CLIP& outClip)
  {
    const double ticks_per_second = animation.mTicksPerSecond > 0.0 ? animation.mTicksPerSecond : ns_DEFAULT_TICKS_PER_SECOND;
    outClip = SKINNED_ANIMATION_CLIP{};
    outClip.name = animation.mName.C_Str();
    outClip.durationSeconds = static_cast<float>(animation.mDuration / ticks_per_second);

    // ノードごとのアニメーションを、順にチャンネルへ変換する。
    for(unsigned int channel_index = 0; channel_index < animation.mNumChannels; ++channel_index){
      AppendChannel(*animation.mChannels[channel_index],ticks_per_second,outClip);
    }
  }

  //========= ログの補助関数=========
  // 読み込んだデータの概要を、Infoのログへ出す。キーの数は、チャンネルの中の最大を出し、時刻の範囲の確認に、最後のキーの時刻も出す。
  void LogSummary(const std::wstring& filePath,const SKINNED_ANIMATION_CLIP& clip)
  {
    std::size_t max_position_keys = 0;
    std::size_t max_rotation_keys = 0;
    float last_key_seconds = 0.0f;
    for(const ANIMATION_CHANNEL& channel : clip.channels){
      max_position_keys = std::max(max_position_keys,channel.positionKeys.size());
      max_rotation_keys = std::max(max_rotation_keys,channel.rotationKeys.size());
      if(!channel.rotationKeys.empty()){
        last_key_seconds = std::max(last_key_seconds,channel.rotationKeys.back().timeSeconds);
      }
    }

    Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,
                  L"アニメーションを読み込みました: " + filePath
                  + L" | 名前 " + ToWide(clip.name)
                  + L" / 長さ(秒) " + std::to_wstring(clip.durationSeconds)
                  + L" / チャンネル " + std::to_wstring(clip.channels.size())
                  + L" / 位置キー最大 " + std::to_wstring(max_position_keys)
                  + L" / 回転キー最大 " + std::to_wstring(max_rotation_keys)
                  + L" / 最後のキー(秒) " + std::to_wstring(last_key_seconds));
  }

  // 読込の失敗を、Errorのログへ出す。
  void LogFailure(const std::wstring& filePath,const std::wstring& reason)
  {
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,L"アニメーションの読込に失敗しました: " + filePath + L" | " + reason);
  }

  //========= 読込の補助関数=========
  // FBXを読み込み、アニメーションが1つ以上あるシーンを返す。Pivotは、モデルの読込と同じく、保持しない。
  // importer: 読込の結果（シーン）を所有する読込器。戻り値のシーンは、この読込器が生きている間だけ有効。
  // filePath: FBXのパス。
  // 戻り値: アニメーションを持つシーン。読み込めない、またはアニメーションがない場合は、失敗をログへ記録して、nullptr。
  const aiScene* ReadSceneWithAnimations(Assimp::Importer& importer,const std::wstring& filePath)
  {
    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS,false);
    const aiScene* scene = importer.ReadFile(ToUtf8(filePath).c_str(),ns_IMPORT_FLAGS);
    if(scene == nullptr){
      LogFailure(filePath,ToWide(importer.GetErrorString()));
      return nullptr;
    }
    if(!scene->HasAnimations()){
      LogFailure(filePath,L"アニメーションがありません。");
      return nullptr;
    }
    return scene;
  }
}

//========= 読込関数=========
// アニメーションのFBXを読み込み、先頭のアニメーションを変換する。
bool SkinnedAnimationLoader::Load(const std::wstring& filePath,SKINNED_ANIMATION_CLIP& outClip) const
{
  outClip = SKINNED_ANIMATION_CLIP{};

  // FBXを読み込み、先頭のアニメーションを変換して、概要を出す。
  Assimp::Importer importer;
  const aiScene* scene = ReadSceneWithAnimations(importer,filePath);
  if(scene == nullptr)return false;

  ConvertAnimation(*scene->mAnimations[0],outClip);
  LogSummary(filePath,outClip);
  return true;
}

// アニメーションのFBXを読み込み、全てのアニメーションを変換する。
bool SkinnedAnimationLoader::LoadAll(const std::wstring& filePath,std::vector<SKINNED_ANIMATION_CLIP>& outClips) const
{
  outClips.clear();

  // FBXを読み込み、含まれる全てのアニメーションの数を、ログへ出す。
  Assimp::Importer importer;
  const aiScene* scene = ReadSceneWithAnimations(importer,filePath);
  if(scene == nullptr)return false;
  Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,L"アニメーションの数: " + std::to_wstring(scene->mNumAnimations) + L" | " + filePath);

  // 全てのアニメーションを、ファイルの中の並びのまま変換し、1つずつ概要を出す。
  outClips.resize(scene->mNumAnimations);
  for(unsigned int animation_index = 0; animation_index < scene->mNumAnimations; ++animation_index){
    ConvertAnimation(*scene->mAnimations[animation_index],outClips[animation_index]);
    LogSummary(filePath,outClips[animation_index]);
  }
  return true;
}