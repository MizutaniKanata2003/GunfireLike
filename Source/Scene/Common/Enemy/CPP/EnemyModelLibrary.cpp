#include "../H/EnemyModelLibrary.h"

// 敵の見た目ごとに、FBXのモデルと、動きを読み込み、再生部と描画部を準備する。読込元の表、見た目の選択、ワールド行列の作成、歩きの速度の表の作成も、このファイルで実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minmax_elementで、速度の最小と最大を求める。
#include <cmath> // std::floorで、位相を0以上1未満へ戻す。
#include <filesystem> // std::filesystem::existsで、任意の動きのファイルの有無を調べる。
#include <numeric> // std::accumulateで、速度の合計を求める。
#include <string> // std::wstringで、動きのファイルのパスとログの文字列を組み立てる。
#include <system_error> // std::error_codeで、ファイルの有無の確認で例外を出さない。
#include <vector> // 歩きの動きの測定値を、可変長配列で保持する。

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

  //========= 歩きの速度の測定=========
  // 歩きの速度を測る足と腰のノードの、名前の末尾。Mixamoの「mixamorig:Hips」「mixamorig:LeftFoot」「mixamorig:RightFoot」の、接頭辞を除いた部分。
  constexpr char ns_HIPS_NODE_SUFFIX[] = "Hips";
  constexpr char ns_LEFT_FOOT_NODE_SUFFIX[] = "LeftFoot";
  constexpr char ns_RIGHT_FOOT_NODE_SUFFIX[] = "RightFoot";
  // モデル空間での、モデルの正面の向きのZ成分。モデルは-Zを向く（H8dの実測）。足が体に対して後ろへ動く速さを、前への速度へ直すために使う。
  constexpr float ns_MODEL_FORWARD_Z = -1.0f;
  // 速度の表を滑らかにする、移動平均の、片側の幅（サンプル数）。足を持ち替える瞬間の、速度の飛びをならす。仮値。
  constexpr int ns_SMOOTH_HALF_WIDTH = 2;

  // 歩きの動きの、1つの時刻の測定値。足が体に対して前後のどこにあるかと、足の高さ。
  struct WALK_SAMPLE
  {
    // 左足の、腰から見た、モデルの正面方向の位置。単位はモデルの空間の単位（センチメートル）。
    float leftForward{};
    // 右足の、腰から見た、モデルの正面方向の位置。
    float rightForward{};
    // 左足の高さ。低い方の足を、床に着いている足とみなす。
    float leftHeight{};
    // 右足の高さ。
    float rightHeight{};
  };

  //========= 読込の補助関数=========
  // 動きが、ファイルがなくても、読込を続けてよい任意の動きかを返す。
  // clipIndex: 動きの添字。
  bool IsOptionalClip(std::size_t clipIndex)
  {
    return clipIndex == static_cast<std::size_t>(ENEMY_CLIP::e_TELEGRAPH);
  }

  // 動き1つを読み込み、再生部を初期化する。全ての動きで、腰の水平の位置を固定し、動きの切替で、体が横へずれて見えないようにする。
  // filePath: 動きのFBXのパス。
  // clipIndex: 動きの添字。
  // assets: 読込先。モデルは、読込済みであること。
  // 戻り値: 両方成功したらtrue。失敗の理由は、各クラスがログへ記録する。
  bool LoadClip(const std::wstring& filePath,std::size_t clipIndex,ENEMY_APPEARANCE_ASSETS& assets)
  {
    SkinnedAnimationLoader animation_loader;
    if(!animation_loader.Load(filePath,assets.clips[clipIndex]))return false;
    if(!assets.players[clipIndex].Initialize(assets.model,assets.clips[clipIndex]))return false;

    // 腰が見つからない場合は、-1が渡り、固定されない。歩きの速度の表は、腰から見た足の位置で測るため、固定しても変わらない。
    assets.players[clipIndex].LockHorizontalMotion(assets.players[clipIndex].FindNodeIndexBySuffix(ns_HIPS_NODE_SUFFIX));

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

  //========= 歩きの速度の補助関数=========
  // 歩きの動きの1周を等分した時刻ごとに、腰と両足の位置を測り、足が体に対して前後のどこにあるかと、足の高さを、記録する。
  // player: 歩きの動きの再生部。
  // outSamples: 出力先。サンプル数+1個（最後は1周の終わり）。
  // 戻り値: 腰と両足のノードが見つかれば、true。見つからない場合は、ログへ記録して、false。
  bool MeasureWalkSamples(SkinnedAnimationPlayer& player,std::vector<WALK_SAMPLE>& outSamples)
  {
    // 腰・左足・右足のノードを、名前の末尾から探す。
    const std::vector<int> node_indices{player.FindNodeIndexBySuffix(ns_HIPS_NODE_SUFFIX),
      player.FindNodeIndexBySuffix(ns_LEFT_FOOT_NODE_SUFFIX),player.FindNodeIndexBySuffix(ns_RIGHT_FOOT_NODE_SUFFIX)};
    if(node_indices[0] < 0 || node_indices[1] < 0 || node_indices[2] < 0){
      Logger::Write(e_LogLevel::e_WARNING,e_LogCategory::e_ASSET,L"歩きの速度の表を作れません。腰・左足・右足のノード（末尾が Hips・LeftFoot・RightFoot）が見つかりません。");
      return false;
    }

    // 1周を等分した時刻ごとに、位置を求め、腰から見た足の前後の位置と、足の高さを記録する。
    const float interval_seconds = player.GetDurationSeconds() / static_cast<float>(ENEMY_MODEL::ns_WALK_SPEED_SAMPLE_COUNT);
    std::vector<XMFLOAT3> positions{};
    outSamples.assign(ENEMY_MODEL::ns_WALK_SPEED_SAMPLE_COUNT + 1,WALK_SAMPLE{});
    for(std::size_t sample_index = 0; sample_index < outSamples.size(); ++sample_index){
      player.EvaluateNodePositions(interval_seconds * static_cast<float>(sample_index),node_indices,positions);
      outSamples[sample_index] = WALK_SAMPLE{(positions[1].z - positions[0].z) * ns_MODEL_FORWARD_Z,(positions[2].z - positions[0].z) * ns_MODEL_FORWARD_Z,
        positions[1].y,positions[2].y};
    }
    return true;
  }

  // 測定値から、区間ごとの、床に着いている足の動きに基づく、前への速度を求める。足が体に対して後ろへ動く速さが、体が地面に対して前へ進む速さになる。
  // samples: 測定値。サンプル数+1個。
  // intervalSeconds: 区間の長さ。単位は秒。
  // 戻り値: 区間ごとの速度。サンプル数個。単位はモデルの空間の単位/秒。符号は、まだ整えていない。
  std::vector<float> CalculateStanceSpeeds(const std::vector<WALK_SAMPLE>& samples,float intervalSeconds)
  {
    std::vector<float> speeds(ENEMY_MODEL::ns_WALK_SPEED_SAMPLE_COUNT,0.0f);
    for(std::size_t interval_index = 0; interval_index < speeds.size(); ++interval_index){
      const WALK_SAMPLE& start = samples[interval_index];
      const WALK_SAMPLE& end = samples[interval_index + 1];

      // 区間の平均の高さが低い足を、床に着いている足とし、その足の、前後の位置の変化から、速度を求める。
      const bool is_left_stance = start.leftHeight + end.leftHeight <= start.rightHeight + end.rightHeight;
      const float forward_change = is_left_stance ? end.leftForward - start.leftForward : end.rightForward - start.rightForward;
      speeds[interval_index] = -forward_change / intervalSeconds;
    }
    return speeds;
  }

  // 速度の並びを、1周の繰り返しとして、前後の数サンプルの平均で滑らかにする。
  // speeds: 滑らかにする速度の並び。書き換える。
  void SmoothSpeedsCircular(std::vector<float>& speeds)
  {
    const std::vector<float> source = speeds;
    const int count = static_cast<int>(source.size());
    for(int index = 0; index < count; ++index){
      float sum = 0.0f;
      for(int offset = -ns_SMOOTH_HALF_WIDTH; offset <= ns_SMOOTH_HALF_WIDTH; ++offset){
        sum += source[static_cast<std::size_t>((index + offset + count) % count)];
      }
      speeds[static_cast<std::size_t>(index)] = sum / static_cast<float>(2 * ns_SMOOTH_HALF_WIDTH + 1);
    }
  }

  // 歩きの動きから、地面に対する速度の表を作り、ログへ平均・最小・最大を出す。
  // modelScale: モデルの空間から、ワールドへの倍率。
  // assets: 歩きの動きを読み込み済みの所有物。表と平均を書き込む。
  // 戻り値: 表を作れたらtrue。歩きの動きがない、または、足のノードが見つからない場合は、false。
  bool BuildWalkSpeedTable(float modelScale,ENEMY_APPEARANCE_ASSETS& assets)
  {
    SkinnedAnimationPlayer& player = assets.players[static_cast<std::size_t>(ENEMY_CLIP::e_WALK)];
    const float interval_seconds = player.GetDurationSeconds() / static_cast<float>(ENEMY_MODEL::ns_WALK_SPEED_SAMPLE_COUNT);
    std::vector<WALK_SAMPLE> samples{};
    if(!assets.hasClip[static_cast<std::size_t>(ENEMY_CLIP::e_WALK)] || interval_seconds <= 0.0f || !MeasureWalkSamples(player,samples))return false;

    // 区間ごとの速度を求め、平均が負なら、モデルの正面の向きの仮定が逆のため、符号を反転する。負の速度は、0にして、滑らかにする。
    std::vector<float> speeds = CalculateStanceSpeeds(samples,interval_seconds);
    const float sign = std::accumulate(speeds.begin(),speeds.end(),0.0f) < 0.0f ? -1.0f : 1.0f;
    for(float& speed : speeds){
      speed = std::max(0.0f,speed * sign);
    }
    SmoothSpeedsCircular(speeds);

    // ワールドの単位へ直して、表へ書き、平均・最小・最大をログへ出す。
    for(std::size_t index = 0; index < speeds.size(); ++index){
      assets.walkSpeeds[index] = speeds[index] * modelScale;
    }
    assets.walkSpeedAverage = std::accumulate(assets.walkSpeeds.begin(),assets.walkSpeeds.end(),0.0f) / static_cast<float>(assets.walkSpeeds.size());
    assets.hasWalkSpeeds = true;
    const auto min_max = std::minmax_element(assets.walkSpeeds.begin(),assets.walkSpeeds.end());
    Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,
                  L"歩きの速度の表を作りました: 平均 " + std::to_wstring(assets.walkSpeedAverage) + L" / 最小 " + std::to_wstring(*min_max.first)
                  + L" / 最大 " + std::to_wstring(*min_max.second) + L"（ワールド距離単位/秒）");
    return true;
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

// 見た目を1つだけ読み込む。
bool EnemyModelLibrary::LoadAppearanceStep(GraphicsSystem& graphicsSystem,std::size_t appearanceIndex)
{
  // 範囲外はfalse。読み込み済みの見た目は、二重に読み込まず、成功として扱う。
  if(appearanceIndex >= ENEMY_MODEL::ns_APPEARANCE_COUNT)return false;
  if(m_assets[appearanceIndex].isLoaded)return true;

  return LoadAppearance(graphicsSystem,appearanceIndex);
}

// 全ての見た目を、描画に使えるかを返す。
bool EnemyModelLibrary::IsAllLoaded() const
{
  return std::all_of(m_assets.begin(),m_assets.end(),[](const ENEMY_APPEARANCE_ASSETS& assets){return assets.isLoaded; });
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

// 歩きの動きの位相から、地面に対する速度を返す。
bool EnemyModelLibrary::TryGetWalkSpeed(ENEMY_APPEARANCE appearance,float phase,float& outSpeed) const
{
  const std::size_t appearance_index = static_cast<std::size_t>(appearance);
  if(appearance_index >= ENEMY_MODEL::ns_APPEARANCE_COUNT || !m_assets[appearance_index].hasWalkSpeeds)return false;

  // 位相を、0以上1未満へ戻し、表の位置（小数を含む）へ直す。隣り合う2つのサンプルの間は、直線で補間し、最後のサンプルの次は、最初へ戻る。
  const std::array<float,ENEMY_MODEL::ns_WALK_SPEED_SAMPLE_COUNT>& speeds = m_assets[appearance_index].walkSpeeds;
  const float wrapped_phase = phase - std::floor(phase);
  const float table_position = wrapped_phase * static_cast<float>(speeds.size());
  const std::size_t first_index = static_cast<std::size_t>(table_position) % speeds.size();
  const std::size_t second_index = (first_index + 1) % speeds.size();
  const float ratio = table_position - std::floor(table_position);
  outSpeed = speeds[first_index] + (speeds[second_index] - speeds[first_index]) * ratio;
  return true;
}

// 歩きの速度の平均を返す。
bool EnemyModelLibrary::TryGetWalkSpeedAverage(ENEMY_APPEARANCE appearance,float& outSpeed) const
{
  const std::size_t appearance_index = static_cast<std::size_t>(appearance);
  if(appearance_index >= ENEMY_MODEL::ns_APPEARANCE_COUNT || !m_assets[appearance_index].hasWalkSpeeds)return false;

  outSpeed = m_assets[appearance_index].walkSpeedAverage;
  return true;
}

//========= 読込の補助関数=========
// 見た目1つ分の、モデル・動き・描画部を読み込み、歩きの速度の表を作る。
bool EnemyModelLibrary::LoadAppearance(GraphicsSystem& graphicsSystem,std::size_t appearanceIndex)
{
  // モデルのFBXを読み込む。失敗の理由は、SkinnedModelLoaderがログへ記録する。
  const ENEMY_APPEARANCE_PARAMS& params = ns_APPEARANCE_TABLE[appearanceIndex];
  ENEMY_APPEARANCE_ASSETS& assets = m_assets[appearanceIndex];
  const SkinnedModelLoader model_loader{};
  if(!model_loader.Load(params.modelPath,assets.model))return false;

  // 動きを全て読み込み、再生部を初期化してから、描画部を初期化する。
  if(!LoadClips(params,assets) || !assets.renderer.Initialize(graphicsSystem,assets.model))return false;

  // 歩きの速度の表を作る。作れなくても、読込は成功とし、呼出側が、既定の速度を使う。
  static_cast<void>(BuildWalkSpeedTable(params.modelScale,assets));

  assets.isLoaded = true;
  Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,
                L"敵の見た目を読み込みました: " + std::wstring{params.modelPath}
  + L" | 動き " + std::to_wstring(CountLoadedClips(assets)) + L" / " + std::to_wstring(ENEMY_MODEL::ns_CLIP_COUNT));
  return true;
}