#include "../H/WeaponView.h"

// 4種の銃のモデルとアニメーションの読込、待機の姿勢と視点の計算、カメラの前への配置と描画、調整の画面を実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minとstd::maxで、頂点の位置の範囲（最小と最大）を更新する。
#include <cfloat> // FLT_MAXで、表示範囲の最小値の初期値を作る。
#include <cstddef> // std::size_tで、アニメーションと頂点の添字を表す。
#include <string> // std::stringで、アニメーションの名前の末尾を調べ、std::wstringで、ログの文字列を組み立てる。
#include <vector> // 視点と注視点のノードの番号と、位置の計算結果を受け渡す。

//========= 外部ライブラリ インクルード=========
#include "imgui.h"

//========= Framework インクルード=========
#include "Framework/3D/H/CameraMatrices.h"
#include "Framework/3D/H/SkinnedAnimationLoader.h"
#include "Framework/3D/H/SkinnedModelLoader.h"
#include "Framework/Etc/H/Logger.h"

// 銃のモデルの読込元と、動きの名前をまとめる。
namespace
{
  //========= 読込元=========
  // 武器ごとのFBXのパス。添字は、WEAPON_TYPEの値（拳銃・散弾銃・連射銃・単発高威力銃）。作業ディレクトリ基準。
  constexpr const wchar_t* ns_WEAPON_MODEL_PATHS[]{L"Assets/Models/Gun/Pistol/Model/MyPistol.fbx",L"Assets/Models/Gun/Shotgun/Model/AnimatedShotgun.fbx",
    L"Assets/Models/Gun/Scar/Model/SCAR.fbx",L"Assets/Models/Gun/Magnum/Model/AnimatedMagnum.fbx"};
  // 待機の動きの名前。FBXの動きの名前は、「Armature|Idle」のように、「|」の後ろに付くため、末尾の一致で探す。
  constexpr char ns_IDLE_CLIP_NAME[] = "Idle";
  // 待機の動きがない武器（散弾銃）の、代わりの動きの名前。持ち替えで出す動きの、最後の姿勢を、待機の姿勢として使う。
  constexpr char ns_FALLBACK_POSE_CLIP_NAME[] = "Take";
  // 待機の動きがある場合に、待機の姿勢を計算する時刻。単位は秒。待機の動きは、1コマだけのため、先頭を使う。
  constexpr float ns_IDLE_POSE_SECONDS = 0.0f;

  //========= 視点=========
  // モデルの視点のノードの名前。モデルの作者が、一人称の見え方の基準として置いたノード。末尾の一致で探す。
  constexpr char ns_EYE_NODE_NAME[] = "camera";
  // 視点から見る先のノードの名前。視点からこのノードへの向きを、画面の正面にする。末尾の一致で探す。
  constexpr char ns_EYE_TARGET_NODE_NAME[] = "camera_target";
  // モデルの空間で、画面の上にする向き。調整の画面で、X（Pitch）を90度にした時に、腕が立って見えたため、モデルの-Zを上とする。
  // 元のFBXが、Z軸が上（Blenderの標準）で、読込の左手系への変換で、Zの符号が反転した、という見立てだが、FBXの設定は、確認していない。
  constexpr XMFLOAT3 ns_MODEL_UP_DIRECTION{0.0f,0.0f,-1.0f};
  // 見る向きと上の向きの外積の長さの2乗が、この値以下なら、2つの向きが平行に近く、画面の向きが決まらないとみなす。
  constexpr float ns_MIN_EYE_CROSS_LENGTH_SQ = 1.0e-6f;

  //========= 調整の画面=========
  // 大きさの倍率を動かせる範囲。対数の目盛りで動かす。
  constexpr float ns_TUNING_SCALE_MIN = 0.0001f;
  constexpr float ns_TUNING_SCALE_MAX = 10.0f;
  // 向きの角度を動かせる範囲。単位は度。
  constexpr float ns_TUNING_ROTATION_LIMIT_DEGREES = 180.0f;
  // 位置を動かせる範囲。単位はワールド距離単位。
  constexpr float ns_TUNING_OFFSET_LIMIT = 3.0f;
  // 向きの早見のボタンに使う角度。単位は度。
  constexpr float ns_QUARTER_TURN_DEGREES = 90.0f;
  constexpr float ns_HALF_TURN_DEGREES = 180.0f;

  //========= 補助関数=========
  // 名前の末尾が、「|」＋指定した名前、または、指定した名前そのものと一致するアニメーションの添字を返す。
  // clips: 探す対象のアニメーション。
  // name: 探す動きの名前。
  // 戻り値: 最初に見つかった添字。なければ、-1。
  int FindClipIndex(const std::vector<SKINNED_ANIMATION_CLIP>& clips,const char* name)
  {
    const std::string target{name};
    const std::string target_with_separator = "|" + target;
    for(std::size_t clip_index = 0; clip_index < clips.size(); ++clip_index){
      const std::string& clip_name = clips[clip_index].name;
      const bool is_exact = clip_name == target;
      const bool is_suffix = clip_name.size() > target_with_separator.size()
        && clip_name.compare(clip_name.size() - target_with_separator.size(),target_with_separator.size(),target_with_separator) == 0;
      if(is_exact || is_suffix)return static_cast<int>(clip_index);
    }
    return -1;
  }

  // 待機の姿勢で、全ての頂点が、ボーンで動いた後の位置の範囲（最小と最大）を求め、ログへ出す。銃の大きさ・向き・位置を決める手がかりにする。
  // weaponIndex: 武器の種類の値。ログの表示に使う。
  // model: 頂点とボーンの重みを持つモデル。
  // boneMatrices: 待機の姿勢の、全ボーンの最終の行列（行ベクトルの向き）。
  void LogPoseBounds(int weaponIndex,const SKINNED_MODEL_DATA& model,const BoneMatrixArray& boneMatrices)
  {
    XMFLOAT3 minimum{FLT_MAX,FLT_MAX,FLT_MAX};
    XMFLOAT3 maximum{-FLT_MAX,-FLT_MAX,-FLT_MAX};
    for(const SKINNED_VERTEX& vertex : model.vertices){
      // 頂点を、重みの付いたボーンの行列で動かし、重みで混ぜる。重みがない頂点は、動かさない。
      const DirectX::XMVECTOR source = DirectX::XMLoadFloat3(&vertex.position);
      DirectX::XMVECTOR blended = DirectX::XMVectorZero();
      float total_weight = 0.0f;
      for(std::size_t slot = 0; slot < SKINNED_MODEL::ns_BONES_PER_VERTEX; ++slot){
        if(vertex.boneWeights[slot] <= 0.0f)continue;
        const DirectX::XMVECTOR moved = DirectX::XMVector3Transform(source,DirectX::XMLoadFloat4x4(&boneMatrices[vertex.boneIndices[slot]]));
        blended = DirectX::XMVectorAdd(blended,DirectX::XMVectorScale(moved,vertex.boneWeights[slot]));
        total_weight += vertex.boneWeights[slot];
      }
      if(total_weight <= 0.0f)blended = source;

      // 最小と最大を更新する。
      XMFLOAT3 position{};
      DirectX::XMStoreFloat3(&position,blended);
      minimum = XMFLOAT3{std::min(minimum.x,position.x),std::min(minimum.y,position.y),std::min(minimum.z,position.z)};
      maximum = XMFLOAT3{std::max(maximum.x,position.x),std::max(maximum.y,position.y),std::max(maximum.z,position.z)};
    }

    Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,
                  L"銃の表示範囲（待機の姿勢） 武器 " + std::to_wstring(weaponIndex)
                  + L" | 最小 X " + std::to_wstring(minimum.x) + L" Y " + std::to_wstring(minimum.y) + L" Z " + std::to_wstring(minimum.z)
                  + L" | 最大 X " + std::to_wstring(maximum.x) + L" Y " + std::to_wstring(maximum.y) + L" Z " + std::to_wstring(maximum.z));
  }

  // 待機の姿勢で、視点（cameraノード）から、注視点（camera_targetノード）を見る行列を作る。2つの位置と距離を、ログへ出す。
  // weaponIndex: 武器の種類の値。ログの表示に使う。
  // player: 待機の動きの再生部。初期化済みであること。ノードの位置の計算で、作業領域を書き換える。
  // poseSeconds: 待機の姿勢の時刻。単位は秒。
  // 戻り値: モデルの空間から、視点の空間（視点が原点で、+Zが注視点の向き）への変換行列。ノードがない、または、向きが決まらない場合は、単位行列。
  XMMATRIX BuildEyeMatrix(int weaponIndex,SkinnedAnimationPlayer& player,float poseSeconds)
  {
    // 視点と注視点のノードを探す。どちらかがなければ、視点を使わず、表示の位置だけで置く。
    const int eye_node_index = player.FindNodeIndexBySuffix(ns_EYE_NODE_NAME);
    const int target_node_index = player.FindNodeIndexBySuffix(ns_EYE_TARGET_NODE_NAME);
    if(eye_node_index < 0 || target_node_index < 0){
      Logger::Write(e_LogLevel::e_WARNING,e_LogCategory::e_ASSET,L"銃の視点のノードが見つかりません。視点を使わずに置きます: 武器 " + std::to_wstring(weaponIndex));
      return XMMatrixIdentity();
    }

    // 2つのノードの位置を、スキニング後の頂点と同じ空間（モデル空間）で求める。
    std::vector<XMFLOAT3> positions;
    player.EvaluateNodePositions(poseSeconds,std::vector<int>{eye_node_index,target_node_index},positions);
    const XMVECTOR eye_position = XMLoadFloat3(&positions[0]);
    const XMVECTOR forward = DirectX::XMVectorSubtract(XMLoadFloat3(&positions[1]),eye_position);
    Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_ASSET,
                  L"銃の視点（待機の姿勢） 武器 " + std::to_wstring(weaponIndex)
                  + L" | 視点 X " + std::to_wstring(positions[0].x) + L" Y " + std::to_wstring(positions[0].y) + L" Z " + std::to_wstring(positions[0].z)
                  + L" | 注視点 X " + std::to_wstring(positions[1].x) + L" Y " + std::to_wstring(positions[1].y) + L" Z " + std::to_wstring(positions[1].z)
                  + L" | 距離 " + std::to_wstring(XMVectorGetX(DirectX::XMVector3Length(forward))));

    // 見る向きが長さ0、または、上の向きと平行に近いと、画面の向きが決まらないため、視点を使わない。
    const XMVECTOR up_direction = XMLoadFloat3(&ns_MODEL_UP_DIRECTION);
    if(XMVectorGetX(DirectX::XMVector3LengthSq(XMVector3Cross(forward,up_direction))) <= ns_MIN_EYE_CROSS_LENGTH_SQ){
      Logger::Write(e_LogLevel::e_WARNING,e_LogCategory::e_ASSET,L"銃の視点の向きが決まりません。視点を使わずに置きます: 武器 " + std::to_wstring(weaponIndex));
      return XMMatrixIdentity();
    }
    return XMMatrixLookToLH(eye_position,forward,up_direction);
  }
}

// 武器の読込元の数が、武器の種類の数と一致していることを確認する。
static_assert(sizeof(ns_WEAPON_MODEL_PATHS) / sizeof(ns_WEAPON_MODEL_PATHS[0]) == WEAPON_TABLE::ns_WEAPON_COUNT,"銃のモデルのパスの数と、武器の種類の数を一致させること。");

//========= 初期化・終了関数=========
// 旧Resourceを解放し、武器ごとの表示の位置を、確認済みの値へ設定する。
void WeaponView::Prepare()
{
  // 旧Resourceを先に解放する。以後の読込の失敗でも、旧状態へは復帰しない。
  Uninit();

  // 武器ごとに確認した配置を設定する。構造体の順は、大きさ・回転（度）・カメラ空間の位置。銃口の位置は、構造体の既定値のまま。
  // 4種とも、モデル内のカメラを基準に、Y軸を-180度回して銃口を奥へ向ける。
  m_placements[static_cast<int>(WEAPON_TYPE::e_PISTOL)] = WEAPON_VIEW_PLACEMENT{0.01f,XMFLOAT3{0.0f,-180.0f,0.0f},XMFLOAT3{0.0f,-33.0f,3.0f}};
  m_placements[static_cast<int>(WEAPON_TYPE::e_SHOTGUN)] = WEAPON_VIEW_PLACEMENT{0.01f,XMFLOAT3{0.0f,-180.0f,0.0f},XMFLOAT3{0.0f,-34.0f,0.0f}};
  m_placements[static_cast<int>(WEAPON_TYPE::e_SCAR)] = WEAPON_VIEW_PLACEMENT{0.01f,XMFLOAT3{0.0f,-180.0f,0.0f},XMFLOAT3{0.0f,-32.0f,1.0f}};
  m_placements[static_cast<int>(WEAPON_TYPE::e_MAGNUM)] = WEAPON_VIEW_PLACEMENT{0.01f,XMFLOAT3{0.0f,-180.0f,0.0f},XMFLOAT3{0.0f,-32.5f,1.0f}};
}

// 全てのResourceを解放し、選択を先頭へ戻す。
void WeaponView::Uninit()
{
  for(WEAPON_VIEW_ASSETS& assets : m_assets){
    // 描画部と再生部を先に解放し、その後、モデルとアニメーションのデータごと、初期状態へ戻す。
    assets.renderer.Uninit();
    assets.idlePlayer.Uninit();
    assets = WEAPON_VIEW_ASSETS{};
  }
  m_selectedWeaponIndex = 0;
}

//========= 更新・描画関数=========
// 表示名の指す先で、描く武器を選ぶ。
void WeaponView::SelectWeapon(const WEAPON_DATA& weaponData)
{
  // 表の全ての武器を先頭から調べ、表示名の指す先が同じ武器を選ぶ。武器の性能値は、表の値のコピーで、強化でも、表示名の指す先は変わらない。
  for(int weapon_index = 0; weapon_index < WEAPON_TABLE::ns_WEAPON_COUNT; ++weapon_index){
    if(WEAPON_TABLE::ns_WEAPONS[weapon_index].displayName == weaponData.displayName){
      m_selectedWeaponIndex = weapon_index;
      return;
    }
  }
}

// 選んだ武器を、待機の姿勢で描く。
void WeaponView::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& projectionMatrix)
{
  // 選んだ武器を読み込めていない場合は、描かない。
  WEAPON_VIEW_ASSETS& assets = m_assets[m_selectedWeaponIndex];
  if(!assets.isLoaded)return;

  // カメラは原点に固定する。銃は、カメラの空間に置くため、ワールドの視点の動きに、そのまま、付いてくる。
  SKINNED_DRAW_PARAMS draw_params{};
  XMStoreFloat4x4(&draw_params.worldMatrix,BuildWorldMatrix());
  draw_params.boneMatrices = &assets.idleBoneMatrices;
  const CAMERA_MATRICES camera{DirectX::XMMatrixIdentity(),projectionMatrix};
  assets.renderer.Draw(graphicsSystem,camera,draw_params);
}

// 配置を、範囲制限のない数値入力と、細かいドラッグ操作で調整する。
void WeaponView::DrawTuningUi()
{
  // 移動量はカメラ空間の値。ドラッグは1pixelあたり0.001、直接入力は小数点以下6桁で扱う。
  constexpr float OFFSET_DRAG_SPEED = 0.001f;
  constexpr float OFFSET_STEP = 0.01f;
  constexpr float OFFSET_FAST_STEP = 1.0f;
  WEAPON_VIEW_PLACEMENT& placement = m_placements[m_selectedWeaponIndex];
  ImGui::Begin("Weapon View (tuning)");
  ImGui::Text("Weapon index: %d / loaded: %s",m_selectedWeaponIndex,m_assets[m_selectedWeaponIndex].isLoaded ? "yes" : "NO");

  // 正の縮尺を数値で入力する。従来のスライダーの範囲には制限しない。
  ImGui::InputFloat("Scale exact",&placement.scale,0.0001f,0.01f,"%.6f");
  // 0以下の縮尺による消失や意図しない鏡映を防ぐ。
  if(placement.scale <= 0.0f)placement.scale = 0.000001f;

  // Rotationは追加回転。0へ戻した状態で、先にOffsetだけを調整できるようにする。
  ImGui::InputFloat3("Rotation exact (deg)",&placement.rotationDegrees.x,"%.3f");
  // 全体の向きを変えずに配置を探すため、追加回転だけを解除する。
  if(ImGui::Button("Reset rotation to 0"))placement.rotationDegrees = XMFLOAT3{};

  // 最小値と最大値を同じにして、ドラッグ操作の移動範囲を制限しない。
  ImGui::DragFloat3("Offset fine drag",&placement.offset.x,OFFSET_DRAG_SPEED,0.0f,0.0f,"%.6f");
  // 軸ごとに直接入力する。従来の-3から3の範囲を超えた値も入力できる。
  ImGui::InputFloat("Offset X (right)",&placement.offset.x,OFFSET_STEP,OFFSET_FAST_STEP,"%.6f");
  ImGui::InputFloat("Offset Y (up)",&placement.offset.y,OFFSET_STEP,OFFSET_FAST_STEP,"%.6f");
  ImGui::InputFloat("Offset Z (forward)",&placement.offset.z,OFFSET_STEP,OFFSET_FAST_STEP,"%.6f");
  // 移動量だけを解除し、回転と縮尺は維持する。
  if(ImGui::Button("Reset offset"))placement.offset = XMFLOAT3{};

  // マズルフラッシュを出す銃口の位置を、視点からの右・上・前のずれとして入力する。発砲するたびに、この位置へ出る。
  ImGui::InputFloat3("Muzzle flash (right up forward)",&placement.muzzleOffset.x,"%.3f");

  // 報告用の値を、入力と同じ精度で表示する。
  ImGui::Text("Scale %.6f / Rotation %.3f %.3f %.3f",placement.scale,
              placement.rotationDegrees.x,placement.rotationDegrees.y,placement.rotationDegrees.z);
  ImGui::Text("Offset %.6f %.6f %.6f",placement.offset.x,placement.offset.y,placement.offset.z);
  ImGui::Text("Muzzle %.3f %.3f %.3f",placement.muzzleOffset.x,placement.muzzleOffset.y,placement.muzzleOffset.z);
  ImGui::TextUnformatted("Direct input has no offset limits. Ctrl-click a drag field to type a value.");
  ImGui::End();
}

//========= 読込関数=========
// 武器1種類のモデル・アニメーション・待機の姿勢・描画部を用意する。
bool WeaponView::LoadWeapon(GraphicsSystem& graphicsSystem,int weaponIndex)
{
  // 範囲外の武器の値では、配列の外を読み書きするため、何も読み込まず失敗を返す。
  if(weaponIndex < 0 || weaponIndex >= WEAPON_TABLE::ns_WEAPON_COUNT){
    Logger::Write(e_LogLevel::e_WARNING,e_LogCategory::e_ASSET,L"銃の武器の値が範囲外のため、読み込めません: " + std::to_wstring(weaponIndex));
    return false;
  }

  // 同じ武器を読み込み直す場合に備え、旧い描画部と再生部を先に解放する。
  WEAPON_VIEW_ASSETS& assets = m_assets[weaponIndex];
  assets.renderer.Uninit();
  assets.idlePlayer.Uninit();
  const wchar_t* model_path = ns_WEAPON_MODEL_PATHS[weaponIndex];

  // モデルと、全てのアニメーションを読み込む。失敗の理由は、各Loaderがログへ記録する。
  if(!SkinnedModelLoader{}.Load(model_path,assets.model) || !SkinnedAnimationLoader{}.LoadAll(model_path,assets.clips)){
    assets = WEAPON_VIEW_ASSETS{};
    return false;
  }

  // 待機の動きを探す。なければ、持ち替えで出す動きの、最後の姿勢を、待機の姿勢にする。
  int pose_clip_index = FindClipIndex(assets.clips,ns_IDLE_CLIP_NAME);
  float pose_seconds = ns_IDLE_POSE_SECONDS;
  if(pose_clip_index < 0){
    pose_clip_index = FindClipIndex(assets.clips,ns_FALLBACK_POSE_CLIP_NAME);
    if(pose_clip_index >= 0){
      pose_seconds = assets.clips[static_cast<std::size_t>(pose_clip_index)].durationSeconds;
    }
  }

  // 再生部と描画部を初期化する。
  if(pose_clip_index < 0 || !assets.idlePlayer.Initialize(assets.model,assets.clips[static_cast<std::size_t>(pose_clip_index)])
     || !assets.renderer.Initialize(graphicsSystem,assets.model)){
    Logger::Write(e_LogLevel::e_WARNING,e_LogCategory::e_ASSET,std::wstring{L"銃の待機の姿勢の動きまたは描画部を用意できませんでした: "} + model_path);
    assets.renderer.Uninit();
    assets.idlePlayer.Uninit();
    assets = WEAPON_VIEW_ASSETS{};
    return false;
  }

  // 待機の姿勢と視点を計算し、表示の範囲をログへ出す。
  assets.idlePlayer.Evaluate(pose_seconds,assets.idleBoneMatrices);
  XMStoreFloat4x4(&assets.eyeMatrix,BuildEyeMatrix(weaponIndex,assets.idlePlayer,pose_seconds));
  LogPoseBounds(weaponIndex,assets.model,assets.idleBoneMatrices);
  assets.isLoaded = true;
  return true;
}

//========= 補助関数=========
// 視点・大きさ・向き・位置を、この順に合成する。
XMMATRIX WeaponView::BuildWorldMatrix() const
{
  // 行ベクトルの向きで、「視点 → 大きさ → 回転 → 移動」の順に合成する。視点で、モデルの視点を原点へ移してから拡大するため、拡大で、画面上の見え方は変わらない。回転の角度は、度から、ラジアンへ直す。
  const WEAPON_VIEW_PLACEMENT& placement = m_placements[m_selectedWeaponIndex];
  const XMMATRIX scale_matrix = XMMatrixScaling(placement.scale,placement.scale,placement.scale);
  const XMMATRIX rotation_matrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(placement.rotationDegrees.x),XMConvertToRadians(placement.rotationDegrees.y),
                                                                XMConvertToRadians(placement.rotationDegrees.z));
  const XMMATRIX translation_matrix = XMMatrixTranslation(placement.offset.x,placement.offset.y,placement.offset.z);
  const XMMATRIX eye_scale_matrix = XMMatrixMultiply(XMLoadFloat4x4(&m_assets[m_selectedWeaponIndex].eyeMatrix),scale_matrix);
  return XMMatrixMultiply(XMMatrixMultiply(eye_scale_matrix,rotation_matrix),translation_matrix);
}