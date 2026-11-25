#include "../H/EffectController.h"

// 演出の粒の生成・更新・描画と、マズルフラッシュ・命中の火花・撃破の破片・ジャスト回避の広がりの生成を実装する。

//========= C++標準ライブラリ インクルード=========
#include <cmath> // std::sqrtで、正面方向から、右方向の向きを求めるために、水平方向の長さを求める。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/3D/H/CameraMatrices.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/Enemy.h"
#include "Scene/Common/Enemy/H/EnemyManager.h"
#include "Scene/Common/Object/H/ModelRegistry.h"
#include "Scene/Common/Object/H/Transform.h"
#include "Scene/Common/Weapon/H/WeaponSystem.h"

// 場面ごとの演出の数値をまとめる。企画書で未確定の数値は、プレイ確認後に調整する仮値。
namespace
{
  //========= マズルフラッシュ=========
  // 銃口の位置の、視点からのずれ。右・下・前の順。単位はワールド距離単位。Collision可視化の弾道の始点（右0.25・下0.3）と、同じ側にそろえる。
  constexpr float ns_MUZZLE_OFFSET_RIGHT = 0.25f;
  constexpr float ns_MUZZLE_OFFSET_DOWN = 0.3f;
  constexpr float ns_MUZZLE_OFFSET_FORWARD = 0.5f;
  // マズルフラッシュの粒の1辺の長さ。単位はワールド距離単位。
  constexpr float ns_MUZZLE_FLASH_SIZE = 0.15f;
  // マズルフラッシュが残る時間。単位は秒。
  constexpr float ns_MUZZLE_FLASH_SECONDS = 0.05f;
  // マズルフラッシュの色。黄。
  constexpr XMFLOAT4 ns_MUZZLE_FLASH_COLOR{1.0f,0.9f,0.3f,1.0f};
  // 視線がほぼ真上・真下を向く時の、水平方向の長さの下限。この値より小さい場合は、右方向をワールドのX軸にする。
  constexpr float ns_MIN_HORIZONTAL_LENGTH = 0.0001f;

  //========= 命中の火花=========
  // 火花の粒の数。
  constexpr std::size_t ns_SPARK_COUNT = 4;
  // 火花の飛ぶ方向。斜め上へ、4方向に散らす。長さは正規化せず、速さの係数を掛けて、速度にする。
  constexpr XMFLOAT3 ns_SPARK_DIRECTIONS[ns_SPARK_COUNT]{{1.0f,1.0f,1.0f},{-1.0f,1.0f,1.0f},{1.0f,1.0f,-1.0f},{-1.0f,1.0f,-1.0f}};
  // 火花の速さの係数。方向に掛けて、速度（ワールド距離単位毎秒）にする。
  constexpr float ns_SPARK_SPEED = 1.2f;
  // 火花の粒の1辺の長さ。単位はワールド距離単位。
  constexpr float ns_SPARK_SIZE = 0.08f;
  // 火花が残る時間。単位は秒。
  constexpr float ns_SPARK_SECONDS = 0.15f;
  // 火花の色。橙。
  constexpr XMFLOAT4 ns_SPARK_COLOR{1.0f,0.6f,0.1f,1.0f};

  //========= 撃破の破片=========
  // 撃破の破片の粒の数。
  constexpr std::size_t ns_DEFEAT_COUNT = 8;
  // 撃破の破片の飛ぶ方向。上へ向かって、水平に8方向へ散らす。長さは正規化せず、速さの係数を掛けて、速度にする。
  constexpr XMFLOAT3 ns_DEFEAT_DIRECTIONS[ns_DEFEAT_COUNT]{{1.0f,2.0f,0.0f},{-1.0f,2.0f,0.0f},{0.0f,2.0f,1.0f},{0.0f,2.0f,-1.0f},
                                                           {0.7f,3.0f,0.7f},{-0.7f,3.0f,0.7f},{0.7f,3.0f,-0.7f},{-0.7f,3.0f,-0.7f}};
  // 撃破の破片の速さの係数。方向に掛けて、速度（ワールド距離単位毎秒）にする。
  constexpr float ns_DEFEAT_SPEED = 1.0f;
  // 撃破の破片の粒の1辺の長さ。単位はワールド距離単位。
  constexpr float ns_DEFEAT_SIZE = 0.15f;
  // 撃破の破片が残る時間。単位は秒。
  constexpr float ns_DEFEAT_SECONDS = 0.4f;
  // 撃破の破片の色。赤み。
  constexpr XMFLOAT4 ns_DEFEAT_COLOR{0.9f,0.2f,0.2f,1.0f};

  //========= ジャスト回避の広がり=========
  // ジャスト回避の粒の数。
  constexpr std::size_t ns_DODGE_COUNT = 4;
  // ジャスト回避の粒の飛ぶ方向。水平に、前後左右の4方向。長さは1。
  constexpr XMFLOAT3 ns_DODGE_DIRECTIONS[ns_DODGE_COUNT]{{1.0f,0.0f,0.0f},{-1.0f,0.0f,0.0f},{0.0f,0.0f,1.0f},{0.0f,0.0f,-1.0f}};
  // ジャスト回避の粒の速さ。単位は、ワールド距離単位毎秒。
  constexpr float ns_DODGE_SPEED = 3.0f;
  // ジャスト回避の粒を出す位置の、プレイヤーの位置からの水平のずれ。視点に重なって、近いClip面で切れないための距離。単位はワールド距離単位。
  constexpr float ns_DODGE_START_DISTANCE = 0.5f;
  // ジャスト回避の粒を出す位置の、プレイヤーの位置からの高さのずれ。負の値で、視点より下へずらす。単位はワールド距離単位。
  constexpr float ns_DODGE_OFFSET_Y = -0.6f;
  // ジャスト回避の粒の1辺の長さ。単位はワールド距離単位。
  constexpr float ns_DODGE_SIZE = 0.2f;
  // ジャスト回避の粒が残る時間。単位は秒。
  constexpr float ns_DODGE_SECONDS = 0.3f;
  // ジャスト回避の粒の色。水色。
  constexpr XMFLOAT4 ns_DODGE_COLOR{0.4f,0.8f,1.0f,1.0f};
}

//========= 初期化・終了関数=========
// Cube Rendererを借用し、全ての粒を消す。
bool EffectController::Initialize(ModelRegistry& registry)
{
  // 共有のCube Rendererを借用する。
  m_cubeRenderer = registry.GetCubeRenderer();
  // Registryが未初期化ならnullptrが返るため、借用できず、失敗とする。
  if(m_cubeRenderer == nullptr)return false;

  // 前回の粒を持ち越さないよう、全ての粒を消す。
  Clear();
  return true;
}

// 全ての粒を消し、借用した参照を手放す。
void EffectController::Uninit()
{
  Clear();
  m_cubeRenderer = nullptr;
}

// 全ての粒を消し、生成の位置を先頭へ戻す。
void EffectController::Clear()
{
  // 全ての粒を先頭から、使われていない枠へ戻す。
  for(EFFECT_STATE& effect : m_effects){
    effect = EFFECT_STATE{};
  }
  m_nextIndex = 0;
}

//========= 更新・描画関数=========
// 粒を1個生成する。
void EffectController::Spawn(const EFFECT_SPAWN_PARAMS& params)
{
  // 残り時間が0以下の粒は、使われていない枠と区別できず、割合の計算で0による除算にもなるため、生成しない。
  if(params.lifetimeSeconds <= 0.0f)return;

  // 生成の位置から順に、使われていない枠を探す。見つからなければ、生成の位置の粒（最も古い）を置き換える。
  std::size_t target_index = m_nextIndex;
  for(std::size_t offset = 0; offset < m_effects.size(); ++offset){
    const std::size_t candidate_index = (m_nextIndex + offset) % m_effects.size();
    // 使われていない枠が見つかったら、探すのを止める。
    if(m_effects[candidate_index].remainingSeconds <= 0.0f){
      target_index = candidate_index;
      break;
    }
  }

  // 選んだ枠へ粒を設定し、次の探索の開始位置を、選んだ枠の次へ進める。
  m_effects[target_index] = EFFECT_STATE{params.position,params.velocity,params.color,params.size,params.lifetimeSeconds,params.lifetimeSeconds};
  m_nextIndex = (target_index + 1) % m_effects.size();
}

// 全ての粒の位置を進め、残り時間を減らし、終わった粒を消す。
void EffectController::Update(float deltaTime)
{
  // 全ての粒を先頭から更新する。
  for(EFFECT_STATE& effect : m_effects){
    // 使われていない枠は、更新せずに次へ進む。
    if(effect.remainingSeconds <= 0.0f)continue;

    // 残り時間を減らし、0以下になった粒は、使われていない枠へ戻して、次へ進む。
    effect.remainingSeconds -= deltaTime;
    if(effect.remainingSeconds <= 0.0f){
      effect = EFFECT_STATE{};
      continue;
    }

    // 残った粒は、速度の分だけ位置を進める。
    effect.position.x += effect.velocity.x * deltaTime;
    effect.position.y += effect.velocity.y * deltaTime;
    effect.position.z += effect.velocity.z * deltaTime;
  }
}

// 残っている全ての粒を、残り時間の割合に応じた大きさで描く。
void EffectController::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
  // Rendererを借用していない場合は、描画しない。
  if(m_cubeRenderer == nullptr)return;

  // 全ての粒に共通の、視点の行列を、1つの構造体にまとめる。回転はなし。
  const CAMERA_MATRICES camera{viewMatrix,projectionMatrix};
  Transform transform;
  transform.SetRotation(XMFLOAT3{});

  // 全ての粒を先頭から調べ、残っている粒だけ描く。
  for(const EFFECT_STATE& effect : m_effects){
    // 使われていない枠は、描かずに次へ進む。
    if(effect.remainingSeconds <= 0.0f)continue;

    // 生成した時の大きさに、残り時間の割合を掛け、時間とともに小さくする。Cubeは1辺1のため、倍率がそのまま1辺の長さになる。
    const float current_size = effect.size * (effect.remainingSeconds / effect.lifetimeSeconds);
    transform.SetPosition(effect.position);
    transform.SetScale(XMFLOAT3{current_size,current_size,current_size});

    // 画像を使わない単色（Color）で、粒の色・UV倍率1を指定して描く。
    const BasicMeshRenderer::CUBE_APPEARANCE appearance{effect.color,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Color};
    m_cubeRenderer->DrawCube(graphicsSystem,transform.GetWorldMatrix(),camera,appearance);
  }
}

//========= 場面の生成関数=========
// マズルフラッシュを、銃口の位置へ出す。
void EffectController::SpawnMuzzleFlash(const XMFLOAT3& eyePosition,const XMFLOAT3& forward)
{
  // 視線の右方向を、ワールドの上方向と正面方向の外積から求める。左手系のため、右 = 上 × 正面。
  // 上方向は(0,1,0)のため、外積は(正面のZ,0,正面のX × -1)になり、水平方向の長さで割って、長さ1にする。
  const float horizontal_length = std::sqrt(forward.x * forward.x + forward.z * forward.z);
  XMFLOAT3 right{1.0f,0.0f,0.0f};
  // ほぼ真上・真下を向いている場合は、水平方向が定まらないため、右方向をワールドのX軸のままにする。
  if(horizontal_length > ns_MIN_HORIZONTAL_LENGTH){
    right = XMFLOAT3{forward.z / horizontal_length,0.0f,-forward.x / horizontal_length};
  }

  // 視線の上方向を、正面方向と右方向の外積から求める。右方向のY成分は0のため、式が簡単になる。
  const XMFLOAT3 up{forward.y * right.z,forward.z * right.x - forward.x * right.z,-forward.y * right.x};

  // 視点から、右・下（上方向の逆）・前へずらした位置へ、動かない粒を1個出す。
  EFFECT_SPAWN_PARAMS params{};
  params.position = XMFLOAT3{eyePosition.x + right.x * ns_MUZZLE_OFFSET_RIGHT - up.x * ns_MUZZLE_OFFSET_DOWN + forward.x * ns_MUZZLE_OFFSET_FORWARD,
    eyePosition.y + right.y * ns_MUZZLE_OFFSET_RIGHT - up.y * ns_MUZZLE_OFFSET_DOWN + forward.y * ns_MUZZLE_OFFSET_FORWARD,
    eyePosition.z + right.z * ns_MUZZLE_OFFSET_RIGHT - up.z * ns_MUZZLE_OFFSET_DOWN + forward.z * ns_MUZZLE_OFFSET_FORWARD};
  params.color = ns_MUZZLE_FLASH_COLOR;
  params.size = ns_MUZZLE_FLASH_SIZE;
  params.lifetimeSeconds = ns_MUZZLE_FLASH_SECONDS;
  Spawn(params);
}

// 命中の火花を、命中点から斜め上へ散らして出す。
void EffectController::SpawnHitSparks(const XMFLOAT3& hitPoint)
{
  // 全ての火花に共通の、位置・色・大きさ・残り時間を設定する。
  EFFECT_SPAWN_PARAMS params{};
  params.position = hitPoint;
  params.color = ns_SPARK_COLOR;
  params.size = ns_SPARK_SIZE;
  params.lifetimeSeconds = ns_SPARK_SECONDS;

  // 4方向へ、方向ごとの速度を設定して、1個ずつ出す。
  for(const XMFLOAT3& direction : ns_SPARK_DIRECTIONS){
    params.velocity = XMFLOAT3{direction.x * ns_SPARK_SPEED,direction.y * ns_SPARK_SPEED,direction.z * ns_SPARK_SPEED};
    Spawn(params);
  }
}

// 発射の結果から、マズルフラッシュと、命中した弾ごとの火花を出す。
void EffectController::SpawnShotEffects(const WEAPON_UPDATE_RESULT& shotResult)
{
  // 発射していない結果と、弾が0個の結果は、出す対象がないため、何も出さない。
  if(!shotResult.didFire || shotResult.pelletCount <= 0)return;

  // マズルフラッシュは、1回の発射につき1個だけ、先頭の弾の始点（視点）と方向で出す。
  const SHOT_PELLET_RESULT& first_pellet = shotResult.pellets[0];
  SpawnMuzzleFlash(first_pellet.ray.origin,first_pellet.ray.direction);

  // 弾ごとに、命中した弾だけ、命中点へ火花を出す。命中しなかった弾は、次の弾へ進む。
  for(int pellet_index = 0; pellet_index < shotResult.pelletCount; ++pellet_index){
    const SHOT_PELLET_RESULT& pellet = shotResult.pellets[pellet_index];
    if(!pellet.didHit)continue;

    SpawnHitSparks(pellet.rayHit.point);
  }
}

// 倒された敵の演出を、まだ出していない敵ごとに、敵の位置から上へ散らして出す。
void EffectController::SpawnDefeatEffects(EnemyManager& enemyManager)
{
  // 全ての粒に共通の、色・大きさ・残り時間を設定する。
  EFFECT_SPAWN_PARAMS params{};
  params.color = ns_DEFEAT_COLOR;
  params.size = ns_DEFEAT_SIZE;
  params.lifetimeSeconds = ns_DEFEAT_SECONDS;

  // 全ての敵を先頭から調べ、倒された演出をまだ出していない敵だけ、敵の位置から、8方向へ破片を出す。
  for(Enemy& enemy : enemyManager.GetEnemies()){
    // 演出を出す対象でない敵は、次へ進む。出す対象の場合は、出した旨を、この呼出しの中で敵へ記録する。
    if(!enemy.TakeDefeatEffectNotice())continue;

    params.position = enemy.GetBodySphere().center;
    for(const XMFLOAT3& direction : ns_DEFEAT_DIRECTIONS){
      params.velocity = XMFLOAT3{direction.x * ns_DEFEAT_SPEED,direction.y * ns_DEFEAT_SPEED,direction.z * ns_DEFEAT_SPEED};
      Spawn(params);
    }
  }
}

// ジャスト回避の演出を、プレイヤーの付近から、水平に4方向へ広げて出す。
void EffectController::SpawnJustDodgeEffect(const XMFLOAT3& playerPosition)
{
  // 全ての粒に共通の、色・大きさ・残り時間を設定する。
  EFFECT_SPAWN_PARAMS params{};
  params.color = ns_DODGE_COLOR;
  params.size = ns_DODGE_SIZE;
  params.lifetimeSeconds = ns_DODGE_SECONDS;

  // 前後左右の4方向へ、プレイヤーから少し離れた位置から、方向ごとの速度で、1個ずつ出す。
  for(const XMFLOAT3& direction : ns_DODGE_DIRECTIONS){
    params.position = XMFLOAT3{playerPosition.x + direction.x * ns_DODGE_START_DISTANCE,playerPosition.y + ns_DODGE_OFFSET_Y,
      playerPosition.z + direction.z * ns_DODGE_START_DISTANCE};
    params.velocity = XMFLOAT3{direction.x * ns_DODGE_SPEED,0.0f,direction.z * ns_DODGE_SPEED};
    Spawn(params);
  }
}