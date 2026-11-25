#include "../H/EnemyModelLibrary.h"

// 敵の見た目ごとに、FBXのモデルと、動きを読み込み、再生部と描画部を準備する。読込元の表、見た目の選択、ワールド行列の作成も、このファイルで実装する。

//========= C++標準ライブラリ インクルード=========
#include <filesystem> // std::filesystem::existsで、任意の動きのファイルの有無を調べる。
#include <string> // std::wstringで、動きのファイルのパスとログの文字列を組み立てる。
#include <system_error> // std::error_codeで、ファイルの有無の確認で例外を出さない。

//========= Framework インクルード=========
#include "Framework/3D/H/SkinnedAnimationLoader.h"
#include "Framework/3D/H/SkinnedModelLoader.h"
#include "Framework/Etc/H/Logger.h"

// 見た目の読込元の表と、読込の補助関数をまとめる。
namespace
{
  //========= 見た目の表=========
  // 見た目ごとの読込元と補正値。添字は、ENEMY_APPEARANCEの値。数値は、見た目を確認して、調整する仮値。
  // 倍率0.01は、Mixamoのセンチメートルを、メートルへ直し、高さ約2.0にする。足元の補正-1.5は、球の中心の高さ1.0から、床の上面-0.5までの差。
  constexpr ENEMY_APPEARANCE_PARAMS ns_APPEARANCE_TABLE[ENEMY_MODEL::ns_APPEARANCE_COUNT]
  {
    {L"Assets/Models/Enemy/Melee/Model/MeleeA.fbx",L"Assets/Models/Enemy/Melee/Animation/",0.01f,DirectX::XM_PI,-1.5f},
    {L"Assets/Models/Enemy/Melee/Model/MeleeB.fbx",L"Assets/Models/Enemy/Melee/Animation/",0.01f,DirectX::XM_PI,-1.5f}
  };

  //========= 動きのファイル名=========
  // 動きごとのFBXのファイル名。添字は、ENEMY_CLIPの値。
  constexpr const wchar_t* ns_CLIP_FILE_NAMES[ENEMY_MODEL::ns_CLIP_COUNT]
  {
    L"Idle.fbx",L"Walk.fbx",L"Telegraph.fbx",L"Attack.fbx",L"Hit.fbx",L"Down.fbx",L"GetUp.fbx",L"Death.fbx"
  };

  //========= 読込の補助関数=========
  // 動きが、ファイルがなくても、読込を続けてよい任意の動きかを返す。
  // clipIndex: 動きの添字。
  bool IsOptionalClip(std::size_t clipIndex)
  {
    return clipIndex == static_cast<std::size_t>(ENEMY_CLIP::e_TELEGRAPH);
  }

  // 動き1つを読み込み、再生部を初期化する。
  // filePath: 動きのFBXのパス。
  // clipIndex: 動きの添字。
  // assets: 読込先。モデルは、読込済みであること。
  // 戻り値: 両方成功したらtrue。失敗の理由は、各クラスがログへ記録する。
  bool LoadClip(const std::wstring& filePath,std::size_t clipIndex,ENEMY_APPEARANCE_ASSETS& assets)
  {
    SkinnedAnimationLoader animation_loader;
    if(!animation_loader.Load(filePath,assets.clips[clipIndex]))return false;
    if(!assets.players[clipIndex].Initialize(assets.model,assets.clips[clipIndex]))return false;

    assets.hasClip[clipIndex] = true;
    return true;
  }

  // 見た目1つ分の、全ての動きを読み込む。任意の動きは、ファイルがなければ、ログだけを残して、読み飛ばす。
  // params: 見た目の読込元。
  // assets: 読込先。モデルは、読込済みであること。
  // 戻り値: 必須の動きを全て読み込めたらtrue。
  bool LoadClips(const ENEMY_APPEARANCE_PARAMS& params,ENEMY_APPEARANCE_ASSETS& assets)
  {
    for(std::size_t clip_index = 0; clip_index < ENEMY_MODEL::ns_CLIP_COUNT; ++clip_index){
      const std::wstring file_path = std::wstring{params.animationFolder} + ns_CLIP_FILE_NAMES[clip_index];

      // 任意の動きは、ファイルがなければ、読み飛ばす。呼出側が、別の動きで代用する。
      std::error_code error{};
      if(IsOptionalClip(clip_index) && !std::filesystem::exists(std::filesystem::path{file_path},error)){
        Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,L"任意のアニメーションがないため、読み飛ばします: " + file_path);
        continue;
      }
      if(!LoadClip(file_path,clip_index,assets))return false;
    }
    return true;
  }

  // 読み込んだ動きの数を数える。
  // assets: 数える対象。
  std::size_t CountLoadedClips(const ENEMY_APPEARANCE_ASSETS& assets)
  {
    std::size_t count = 0;
    for(const bool has_clip : assets.hasClip){
      if(has_clip)++count;
    }
    return count;
  }
}

//========= 初期化・終了関数=========
// 全ての見た目を読み込む。
bool EnemyModelLibrary::Initialize(GraphicsSystem& graphicsSystem)
{
  // 旧所有物を先に解放する。以後の失敗でも、旧状態へは復帰しない。
  Uninit();

  // 全ての見た目を順に読み込む。最初に失敗した時点で、全て解放して失敗を返す。
  for(std::size_t appearance_index = 0; appearance_index < ENEMY_MODEL::ns_APPEARANCE_COUNT; ++appearance_index){
    if(!LoadAppearance(graphicsSystem,appearance_index)){
      Uninit();
      return false;
    }
  }
  return true;
}

// 全ての所有物を解放する。
void EnemyModelLibrary::Uninit()
{
  for(ENEMY_APPEARANCE_ASSETS& assets : m_assets){
    // 描画部と再生部を先に解放し、その後、モデルと動きのデータごと、初期状態へ戻す。
    assets.renderer.Uninit();
    for(SkinnedAnimationPlayer& player : assets.players){
      player.Uninit();
    }
    assets = ENEMY_APPEARANCE_ASSETS{};
  }
}

//========= 取得関数=========
// 敵の種類と識別値から、使う見た目を決める。
bool EnemyModelLibrary::TryGetAppearance(ENEMY_KIND kind,std::uint32_t enemyId,ENEMY_APPEARANCE& outAppearance)
{
  // 接近型だけが、モデルの見た目を持つ。識別値の偶奇で、AとBを分ける。ほかの種類は、素材が揃った後に足す。
  if(kind != ENEMY_KIND::e_MELEE)return false;

  outAppearance = (enemyId % 2 == 0) ? ENEMY_APPEARANCE::e_MELEE_A : ENEMY_APPEARANCE::e_MELEE_B;
  return true;
}

// 見た目の読込元と、描画の補正値を返す。
const ENEMY_APPEARANCE_PARAMS& EnemyModelLibrary::GetParams(ENEMY_APPEARANCE appearance)
{
  // 範囲外の値は、配列の外を読まないよう、先頭の見た目として扱う。
  const std::size_t appearance_index = static_cast<std::size_t>(appearance);
  return ns_APPEARANCE_TABLE[appearance_index < ENEMY_MODEL::ns_APPEARANCE_COUNT ? appearance_index : 0];
}

// 敵の位置と向きから、モデルの空間からワールドへの変換行列を作る。
XMFLOAT4X4 EnemyModelLibrary::BuildWorldMatrix(ENEMY_APPEARANCE appearance,const XMFLOAT3& position,float yawRadians)
{
  // 倍率→回転（向きに補正を足す）→移動（足元を、床へ合わせる）の順に、行ベクトルの向きで合成する。
  const ENEMY_APPEARANCE_PARAMS& params = GetParams(appearance);
  const XMMATRIX scale_matrix = XMMatrixScaling(params.modelScale,params.modelScale,params.modelScale);
  const XMMATRIX rotation_matrix = XMMatrixRotationRollPitchYaw(0.0f,yawRadians + params.yawOffsetRadians,0.0f);
  const XMMATRIX translation_matrix = XMMatrixTranslation(position.x,position.y + params.footOffsetY,position.z);

  XMFLOAT4X4 world_matrix{};
  XMStoreFloat4x4(&world_matrix,XMMatrixMultiply(XMMatrixMultiply(scale_matrix,rotation_matrix),translation_matrix));
  return world_matrix;
}

// 見た目を、描画に使えるかを返す。
bool EnemyModelLibrary::IsLoaded(ENEMY_APPEARANCE appearance) const
{
  const std::size_t appearance_index = static_cast<std::size_t>(appearance);
  return appearance_index < ENEMY_MODEL::ns_APPEARANCE_COUNT && m_assets[appearance_index].isLoaded;
}

// 見た目と動きに対応する再生部を返す。
SkinnedAnimationPlayer* EnemyModelLibrary::FindPlayer(ENEMY_APPEARANCE appearance,ENEMY_CLIP clip)
{
  // 範囲外・未読込・ファイルがない動きは、nullptrを返す。
  const std::size_t appearance_index = static_cast<std::size_t>(appearance);
  const std::size_t clip_index = static_cast<std::size_t>(clip);
  if(appearance_index >= ENEMY_MODEL::ns_APPEARANCE_COUNT || clip_index >= ENEMY_MODEL::ns_CLIP_COUNT)return nullptr;

  ENEMY_APPEARANCE_ASSETS& assets = m_assets[appearance_index];
  return assets.hasClip[clip_index] ? &assets.players[clip_index] : nullptr;
}

// 見た目に対応する描画部を返す。
SkinnedModelRenderer* EnemyModelLibrary::FindRenderer(ENEMY_APPEARANCE appearance)
{
  const std::size_t appearance_index = static_cast<std::size_t>(appearance);
  if(appearance_index >= ENEMY_MODEL::ns_APPEARANCE_COUNT || !m_assets[appearance_index].isLoaded)return nullptr;

  return &m_assets[appearance_index].renderer;
}

//========= 読込の補助関数=========
// 見た目1つ分の、モデル・動き・描画部を読み込む。
bool EnemyModelLibrary::LoadAppearance(GraphicsSystem& graphicsSystem,std::size_t appearanceIndex)
{
  // モデルのFBXを読み込む。失敗の理由は、SkinnedModelLoaderがログへ記録する。
  const ENEMY_APPEARANCE_PARAMS& params = ns_APPEARANCE_TABLE[appearanceIndex];
  ENEMY_APPEARANCE_ASSETS& assets = m_assets[appearanceIndex];
  const SkinnedModelLoader model_loader{};
  if(!model_loader.Load(params.modelPath,assets.model))return false;

  // 動きを全て読み込み、再生部を初期化してから、描画部を初期化する。
  if(!LoadClips(params,assets) || !assets.renderer.Initialize(graphicsSystem,assets.model))return false;

  assets.isLoaded = true;
  Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,
                L"敵の見た目を読み込みました: " + std::wstring{params.modelPath}
  + L" | 動き " + std::to_wstring(CountLoadedClips(assets)) + L" / " + std::to_wstring(ENEMY_MODEL::ns_CLIP_COUNT));
  return true;
}
