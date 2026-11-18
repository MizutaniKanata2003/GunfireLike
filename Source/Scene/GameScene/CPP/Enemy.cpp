#include "../H/Enemy.h"

// 敵1体のHP・怯み・転倒・Collider登録・Cube表示を実装する。
// 種類ごとのHP・当たり・大きさ・色はEnemyTableから読み、怯みと転倒の時間と状態の色は、このファイルの定数にまとめた仮値。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/Collision/H/CollisionResolver.h"
#include "Framework/Collision/H/CollisionWorld.h"

//========= Scene インクルード=========
#include "Scene/Common/H/ModelRegistry.h"
#include "Scene/GameScene/H/EnemyTable.h"

// 検証用の敵の状態の数値と色をまとめる。企画書で未確定の数値は仮値。
namespace
{
	//========= 戦闘の仮値=========
	// 被弾後に怯む時間。単位は秒。企画書では「命中時に敵によって怯む」とだけ定められており、具体値は未確定の仮値。
	constexpr float ns_FLINCH_SECONDS = 0.3f;
	// 転倒している時間。単位は秒。企画書で未確定の仮値。
	constexpr float ns_KNOCKDOWN_SECONDS = 1.5f;

	//========= 形状=========
	// 転倒中のCubeの高さの倍率。1辺に掛けて低く潰し、倒れたことを示す。足元の位置は変えず、中心を下げる。仮値。
	constexpr float ns_KNOCKDOWN_HEIGHT_SCALE = 0.4f;

	//========= 状態の色=========
	// 怯み中の色。橙で、Object画像の色へ乗算して色味を変える。種類の色より優先する。
	constexpr XMFLOAT4 ns_FLINCH_COLOR{1.0f,0.5f,0.2f,1.0f};
	// 転倒中の色。青みがかった灰色で、Object画像の色へ乗算して色味を変える。怯みの色より優先する。
	constexpr XMFLOAT4 ns_KNOCKDOWN_COLOR{0.5f,0.6f,1.0f,1.0f};
}

//========= 初期化・終了関数=========
// Cube Rendererを借用し、種類の表からHPと大きさを設定して、球のColliderを登録する。
bool Enemy::Initialize(ModelRegistry& registry,CollisionWorld& collisionWorld,ENEMY_KIND kind,const XMFLOAT3& position,std::uint32_t enemyId)
{
	// 共有のCube Rendererを借用する。
	m_cubeRenderer = registry.GetCubeRenderer();
	// Registryが未初期化ならnullptrが返るため、Colliderを登録せず失敗とする。
	if(m_cubeRenderer == nullptr)return false;

	// 種類・位置・識別値・大きさを設定する。Cubeは1辺1のため、倍率がそのまま1辺の長さになる。
	const ENEMY_PARAMS& params = ENEMY_TABLE::GetParams(kind);
	m_kind = kind;
	m_position = position;
	m_enemyId = enemyId;
	m_transform.SetPosition(position);
	m_transform.SetRotation(XMFLOAT3{});
	m_transform.SetScale(XMFLOAT3{params.cubeSize,params.cubeSize,params.cubeSize});

	// HPを満タンにして生きている状態にし、球のColliderを敵レイヤーで登録する。
	m_currentHP = params.maxHP;
	m_flinchRemainingSeconds = 0.0f;
	m_knockdownRemainingSeconds = 0.0f;
	m_isAlive = true;
	m_colliderHandle = collisionWorld.AddSphere(COLLISION_SPHERE{position,params.colliderRadius},COLLISION_LAYER::ns_ENEMY,enemyId);
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
	m_knockdownRemainingSeconds = 0.0f;
}

//========= 更新・描画関数=========
// 怯みと転倒の残り時間を減らす。
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

	// 転倒中の場合だけ時間を進め、0未満にならないようにする。転倒でなければ何もしない。
	if(m_knockdownRemainingSeconds > 0.0f){
		m_knockdownRemainingSeconds -= deltaTime;
		// 残り時間が0以下になったら、転倒を終えて0にそろえる。
		if(m_knockdownRemainingSeconds < 0.0f){
			m_knockdownRemainingSeconds = 0.0f;
		}
	}
}

// 移動先の球を地形から押し戻し、Colliderの更新に成功した座標を描画にも使う。
bool Enemy::TryMoveTo(const XMFLOAT3& targetPosition,CollisionWorld& collisionWorld)
{
	// 死亡した敵には有効なColliderがないため、位置を変更しない。
	if(!m_isAlive)return false;

	// 現在の球から半径を引き継ぎ、移動先の中心を地形のAABBから押し戻す。
	COLLISION_SPHERE body_sphere = GetBodySphere();
	body_sphere.center = targetPosition;
	body_sphere.center = CollisionResolver::ResolveSphere(collisionWorld,body_sphere,COLLISION_LAYER::ns_STATIC);

	// 登録済みの球を更新できなければ、保持座標も描画用Transformも変更しない。
	if(!collisionWorld.UpdateSphere(m_colliderHandle,body_sphere))return false;

	// 判定に反映された中心を、敵の保持座標と描画の両方に反映する。
	m_position = body_sphere.center;
	m_transform.SetPosition(m_position);
	return true;
}

// 生きている間だけ、Cubeを描く。
void Enemy::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
	// 死亡している、またはRendererを借用していない場合は、描画しない。
	if(!m_isAlive || m_cubeRenderer == nullptr)return;

	// 描画用のTransformと色を決める。通常は保持しているTransformと、種類の色。
	const ENEMY_PARAMS& params = ENEMY_TABLE::GetParams(m_kind);
	Transform draw_transform = m_transform;
	XMFLOAT4 color = params.normalColor;

	// 転倒中は、高さを低く潰し、足元の位置が変わらないよう中心を下げて、色を変える。中心を下げる量は、潰した高さの分の半分。
	if(IsKnockedDown()){
		const float squashed_height = params.cubeSize * ns_KNOCKDOWN_HEIGHT_SCALE;
		draw_transform.SetScale(XMFLOAT3{params.cubeSize,squashed_height,params.cubeSize});
		draw_transform.SetPosition(XMFLOAT3{m_position.x,m_position.y - (params.cubeSize - squashed_height) * 0.5f,m_position.z});
		color = ns_KNOCKDOWN_COLOR;
	}
	// 怯み中は、橙色にする。転倒中の表示を優先する。
	else if(IsFlinching()){
		color = ns_FLINCH_COLOR;
	}

	m_cubeRenderer->DrawCube(graphicsSystem,draw_transform.GetWorldMatrix(),viewMatrix,projectionMatrix,color,XMFLOAT2{1.0f,1.0f},
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

// 転倒させ、怯みを解除する。
bool Enemy::ApplyKnockdown()
{
	// 死亡している、またはすでに転倒中の場合は、同じ転倒を重ねないため何もしない。
	if(!m_isAlive || IsKnockedDown())return false;

	// 転倒の時間を設定し、怯みは転倒へ置き換えるため解除する。
	m_knockdownRemainingSeconds = ns_KNOCKDOWN_SECONDS;
	m_flinchRemainingSeconds = 0.0f;
	return true;
}

// HPと怯み・転倒を戻し、死亡中ならColliderを登録し直す。
void Enemy::Respawn(CollisionWorld& collisionWorld)
{
	// 未初期化（Rendererを借用していない）の場合は、生き返らせる対象がないため戻る。
	if(m_cubeRenderer == nullptr)return;

	// HPを満タンにして、怯みと転倒を解除する。
	const ENEMY_PARAMS& params = ENEMY_TABLE::GetParams(m_kind);
	m_currentHP = params.maxHP;
	m_flinchRemainingSeconds = 0.0f;
	m_knockdownRemainingSeconds = 0.0f;

	// 死亡中だった場合だけ、Colliderを同じ位置へ登録し直して生きている状態にする。生きている場合は二重登録しない。
	if(!m_isAlive){
		m_colliderHandle = collisionWorld.AddSphere(COLLISION_SPHERE{m_position,params.colliderRadius},COLLISION_LAYER::ns_ENEMY,m_enemyId);
		m_isAlive = true;
	}
}

//========= Getter関数=========
// 最大HPを返す。
float Enemy::GetMaxHP() const
{
	return ENEMY_TABLE::GetParams(m_kind).maxHP;
}

// 敵の位置を中心にした、Colliderと同じ半径の球を返す。
COLLISION_SPHERE Enemy::GetBodySphere() const
{
	return COLLISION_SPHERE{m_position,ENEMY_TABLE::GetParams(m_kind).colliderRadius};
}