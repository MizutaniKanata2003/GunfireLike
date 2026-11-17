#include "../H/Enemy.h"

// 敵1体のHP・怯み・Collider登録・Cube表示を実装する。
// ColliderのHPや怯みの数値は、このファイルの定数にまとめた仮値。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/Collision/H/CollisionWorld.h"

//========= Scene インクルード=========
#include "Scene/Common/H/ModelRegistry.h"

// 検証用の敵の数値と見た目をまとめる。企画書で未確定の数値は仮値。
namespace
{
  //========= 戦闘の仮値=========
  // 最大HP。企画書で敵のHPは未確定のため、検証用の仮値。
  constexpr float ns_MAX_HP = 100.0f;
  // 被弾後に怯む時間。単位は秒。企画書では「命中時に敵によって怯む」とだけ定められており、具体値は未確定の仮値。
  constexpr float ns_FLINCH_SECONDS = 0.3f;

  //========= 形状=========
  // 被弾判定の球の半径。単位はワールド距離単位。
  constexpr float ns_COLLIDER_RADIUS = 0.8f;
  // 表示するCubeの1辺の長さ。単位はワールド距離単位。球の内側に収まる大きさを目安にした仮の大きさ。
  constexpr float ns_CUBE_SIZE = 1.0f;

  //========= 色=========
  // 通常時の色。白で、Object画像の色をそのまま出す。
  constexpr XMFLOAT4 ns_NORMAL_COLOR{1.0f,1.0f,1.0f,1.0f};
  // 怯み中の色。橙で、Object画像の色へ乗算して色味を変える。
  constexpr XMFLOAT4 ns_FLINCH_COLOR{1.0f,0.5f,0.2f,1.0f};
}

//========= 初期化・終了関数=========
// Cube Rendererを借用し、HP・位置を設定して、球のColliderを登録する。
bool Enemy::Initialize(ModelRegistry& registry,CollisionWorld& collisionWorld,const XMFLOAT3& position,std::uint32_t enemyId)
{
  // 共有のCube Rendererを借用する。
  m_cubeRenderer = registry.GetCubeRenderer();
  // Registryが未初期化ならnullptrが返るため、Colliderを登録せず失敗とする。
  if(m_cubeRenderer == nullptr)return false;

  // 位置・識別値・大きさを設定する。Cubeは1辺1のため、倍率がそのまま1辺の長さになる。
  m_position = position;
  m_enemyId = enemyId;
  m_transform.SetPosition(position);
  m_transform.SetRotation(XMFLOAT3{});
  m_transform.SetScale(XMFLOAT3{ns_CUBE_SIZE,ns_CUBE_SIZE,ns_CUBE_SIZE});

  // HPを満タンにして生きている状態にし、球のColliderを敵レイヤーで登録する。
  m_currentHP = ns_MAX_HP;
  m_flinchRemainingSeconds = 0.0f;
  m_isAlive = true;
  m_colliderHandle = collisionWorld.AddSphere(COLLISION_SPHERE{position,ns_COLLIDER_RADIUS},COLLISION_LAYER::ns_ENEMY,enemyId);
  return true;
}

// Colliderを解除し、状態を未初期化へ戻す。
void Enemy::Uninit(CollisionWorld& collisionWorld)
{
  // 登録中のColliderがある場合だけ解除する。死亡中や未登録の場合は、解除済みのため何もしない。
  if(m_colliderHandle != COLLIDER_HANDLE::ns_INVALID){
    collisionWorld.Remove(m_colliderHandle);
  }

  // 借用した参照と状態を手放す。
  m_colliderHandle = COLLIDER_HANDLE::ns_INVALID;
  m_cubeRenderer = nullptr;
  m_isAlive = false;
  m_currentHP = 0.0f;
  m_flinchRemainingSeconds = 0.0f;
}

//========= 更新・描画関数=========
// 怯みの残り時間を減らす。
void Enemy::Update(float deltaTime)
{
  // 怯み中の場合だけ時間を進め、0未満にならないようにする。怯みでなければ何もしない。
  if(m_flinchRemainingSeconds > 0.0f){
    m_flinchRemainingSeconds -= deltaTime;
    // 残り時間が0以下になったら、怯みを終えて0にそろえる。
    if(m_flinchRemainingSeconds < 0.0f){
      m_flinchRemainingSeconds = 0.0f;
    }
  }
}

// 生きている間だけ、Cubeを描く。
void Enemy::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
  // 死亡している、またはRendererを借用していない場合は、描画しない。
  if(!m_isAlive || m_cubeRenderer == nullptr)return;

  // 怯み中は橙色、そうでなければ白で、Object画像のCubeを描く。UV倍率は等倍。
  const XMFLOAT4& color = IsFlinching() ? ns_FLINCH_COLOR : ns_NORMAL_COLOR;
  m_cubeRenderer->DrawCube(graphicsSystem,m_transform.GetWorldMatrix(),viewMatrix,projectionMatrix,color,XMFLOAT2{1.0f,1.0f},
                           BasicMeshRenderer::TextureType::Object);
}

//========= 戦闘関数=========
// ダメージを受けてHPを減らし、怯ませ、0以下なら死亡させる。
bool Enemy::ApplyDamage(float damage,CollisionWorld& collisionWorld)
{
  // すでに死亡している場合は、ダメージも怯みも与えずfalseを返す。
  if(!m_isAlive)return false;

  // HPを減らし、怯みの時間を最初から数え直す。
  m_currentHP -= damage;
  m_flinchRemainingSeconds = ns_FLINCH_SECONDS;

  // HPが残っていれば、死亡せずに戻る。
  if(m_currentHP > 0.0f)return false;

  // HPが0以下になったため、Colliderを解除して死亡にする。以後の弾は、この敵を通り抜ける。
  m_currentHP = 0.0f;
  m_isAlive = false;
  collisionWorld.Remove(m_colliderHandle);
  m_colliderHandle = COLLIDER_HANDLE::ns_INVALID;
  return true;
}

// HPと怯みを戻し、死亡中ならColliderを登録し直す。
void Enemy::Respawn(CollisionWorld& collisionWorld)
{
  // 未初期化（Rendererを借用していない）の場合は、生き返らせる対象がないため戻る。
  if(m_cubeRenderer == nullptr)return;

  // HPを満タンにして、怯みを解除する。
  m_currentHP = ns_MAX_HP;
  m_flinchRemainingSeconds = 0.0f;

  // 死亡中だった場合だけ、Colliderを同じ位置へ登録し直して生きている状態にする。生きている場合は二重登録しない。
  if(!m_isAlive){
    m_colliderHandle = collisionWorld.AddSphere(COLLISION_SPHERE{m_position,ns_COLLIDER_RADIUS},COLLISION_LAYER::ns_ENEMY,m_enemyId);
    m_isAlive = true;
  }
}

//========= Getter関数=========
// 最大HPを返す。
float Enemy::GetMaxHP() const
{
  return ns_MAX_HP;
}
