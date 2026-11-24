#include "../H/EnemyProjectile.h"

// 敵の弾の生成・直進・壁での消滅・プレイヤーへの命中判定・描画を実装する。
// 壁との判定は、進む距離を小分けにしたRayで行い、命中の判定は、弾の球とプレイヤーの球の重なりで行う。無敵・回避の結果は、JustDodgeJudgeの判定を再利用する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで、1回に進める距離を残りの距離と1ステップの上限の小さい方に制限する。
#include <cmath> // std::isfiniteで経過時間を検証し、std::sqrtで水平方向の向きの長さを求める。

//========= Framework インクルード=========
#include "Framework/3D/H/BasicMeshRenderer.h"
#include "Framework/3D/H/CameraMatrices.h"
#include "Framework/Collision/H/CollisionMath.h"
#include "Framework/Collision/H/CollisionWorld.h"

//========= Scene インクルード=========
#include "Scene/Common/Object/H/ModelRegistry.h"
#include "Scene/Common/Object/H/Transform.h"

//========= 初期化・終了関数=========
// Cube Rendererを借用し、弾を空にする。
bool EnemyProjectileManager::Initialize(ModelRegistry& registry)
{
	// 共有のCube Rendererを借用する。
	m_cubeRenderer = registry.GetCubeRenderer();
	// Registryが未初期化ならnullptrが返るため、借用できず、失敗とする。
	if(m_cubeRenderer == nullptr)return false;

	// 前回の弾を持ち越さないよう、空にする。
	m_projectiles.clear();
	return true;
}

// 弾を全て消し、借用した参照を手放す。
void EnemyProjectileManager::Uninit()
{
	m_projectiles.clear();
	m_cubeRenderer = nullptr;
}

// 飛行中の全ての弾を消す。
void EnemyProjectileManager::Clear()
{
	m_projectiles.clear();
}

//========= 生成・更新・描画関数=========
// 水平な向きを求めて、上限に達していなければ弾を追加する。
bool EnemyProjectileManager::Spawn(const XMFLOAT3& origin,const XMFLOAT3& targetPosition)
{
	// 上限に達している間は、新しい弾を生成しない。
	if(m_projectiles.size() >= ENEMY_PROJECTILE::ns_MAX_COUNT)return false;

	// 発射位置から目標への水平な差と距離を求める。高さは使わない。
	const float offset_x = targetPosition.x - origin.x;
	const float offset_z = targetPosition.z - origin.z;
	const float length = std::sqrt(offset_x * offset_x + offset_z * offset_z);
	// 水平位置が一致している場合は、向きを決められない（0で割れない）ため、生成しない。
	if(length <= 0.0f)return false;

	// 水平な単位ベクトルを向きにし、高さは弾の高さへ置き換えて、寿命を設定する。
	PROJECTILE_STATE projectile{};
	projectile.position = XMFLOAT3{origin.x,ENEMY_PROJECTILE::ns_HEIGHT,origin.z};
	projectile.direction = XMFLOAT3{offset_x / length,0.0f,offset_z / length};
	projectile.remainingSeconds = ENEMY_PROJECTILE::ns_LIFETIME_SECONDS;
	m_projectiles.push_back(projectile);
	return true;
}

// 全ての弾を進め、飛び続ける弾だけを残す。
void EnemyProjectileManager::Update(const ENEMY_PROJECTILE_UPDATE_INPUT& input,const CollisionWorld& collisionWorld,std::vector<DODGE_JUDGE_RESULT>& results)
{
	// 時間が進まない、または時間が不正な場合は、弾を進めない。
	if(!std::isfinite(input.deltaTime) || input.deltaTime <= 0.0f)return;

	// 飛び続ける弾の移し先を用意する。
	std::vector<PROJECTILE_STATE> survivors;
	survivors.reserve(m_projectiles.size());
	// 全ての弾を生成順に進め、飛び続ける弾だけを新しい配列へ移す。消える弾は、判定結果を追加して除く。
	for(PROJECTILE_STATE& projectile : m_projectiles){
		// 飛び続ける弾だけを残す。
		if(AdvanceProjectile(projectile,input,collisionWorld,results)){
			survivors.push_back(projectile);
		}
	}
	m_projectiles.swap(survivors);
}

// 飛行中の弾を、小さなCubeで描く。
void EnemyProjectileManager::Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const
{
	// Rendererを借用していない場合は、描画しない。
	if(m_cubeRenderer == nullptr)return;

	// 全ての弾に共通の、回転と大きさを設定する。
	Transform transform;
	transform.SetRotation(XMFLOAT3{});
	transform.SetScale(XMFLOAT3{ENEMY_PROJECTILE::ns_DRAW_SIZE,ENEMY_PROJECTILE::ns_DRAW_SIZE,ENEMY_PROJECTILE::ns_DRAW_SIZE});

	// 全ての弾に共通の、視点の行列と、色・UV倍率・Texture種別を、1つずつの構造体にまとめる。
	const CAMERA_MATRICES camera{viewMatrix,projectionMatrix};
	const BasicMeshRenderer::CUBE_APPEARANCE appearance{ENEMY_PROJECTILE::ns_COLOR,XMFLOAT2{1.0f,1.0f},BasicMeshRenderer::TextureType::Object};

	// 全ての弾を、位置だけ変えて、同じ大きさと色で描く。
	for(const PROJECTILE_STATE& projectile : m_projectiles){
		transform.SetPosition(projectile.position);
		m_cubeRenderer->DrawCube(graphicsSystem,transform.GetWorldMatrix(),camera,appearance);
	}
}

//========= 補助関数=========
// 寿命を減らし、進む距離を小分けにして、壁とプレイヤーへの命中を順に調べる。
bool EnemyProjectileManager::AdvanceProjectile(PROJECTILE_STATE& projectile,const ENEMY_PROJECTILE_UPDATE_INPUT& input,const CollisionWorld& collisionWorld,
																							 std::vector<DODGE_JUDGE_RESULT>& results)
{
	// 寿命を減らす。
	projectile.remainingSeconds -= input.deltaTime;
	// 寿命が尽きた弾は、壁やプレイヤーの判定を行わずに消す。
	if(projectile.remainingSeconds <= 0.0f)return false;

	// 速度と時間から、この更新で進む距離を求める。
	float remaining_distance = ENEMY_PROJECTILE::ns_SPEED * input.deltaTime;
	// 進む距離を小分けにして、進み終えるまで、壁とプレイヤーへの命中を順に調べる。
	while(remaining_distance > 0.0f){
		// 残りの距離と1ステップの上限の小さい方だけ進める。壁の判定は、現在位置から進む向きへの線分で行い、薄い壁をすり抜けない。
		const float step_distance = std::min(remaining_distance,ENEMY_PROJECTILE::ns_MAX_STEP_DISTANCE);
		const COLLISION_RAY ray{projectile.position,projectile.direction};
		const COLLISION_RAY_QUERY wall_query{ray,step_distance,COLLISION_LAYER::ns_STATIC};
		COLLISION_RAY_HIT ray_hit{};
		COLLISION_HIT_INFO hit_info{};
		// 進む線分の途中に壁・出口があれば、弾を消す。プレイヤーへの判定は行わない。
		if(collisionWorld.Raycast(wall_query,ray_hit,hit_info))return false;

		// 壁がなければ進め、弾の球がプレイヤーの球と重なったかを調べる。
		projectile.position.x += projectile.direction.x * step_distance;
		projectile.position.y += projectile.direction.y * step_distance;
		projectile.position.z += projectile.direction.z * step_distance;
		// 重なった場合は、無敵・回避の状態から命中・回避・ジャスト回避を判定し、結果を追加して弾を消す。
		if(CollisionMath::IntersectSphereSphere(COLLISION_SPHERE{projectile.position,ENEMY_PROJECTILE::ns_RADIUS},input.playerBody)){
			// 回避の動作中だけ回避の開始からの経過時間を使い、それ以外の無敵（蹴り・殴打）は、ジャスト回避にならない大きな値にする。
			const float dodge_age_seconds = input.isDodging ? input.secondsSinceDodgeStart : ENEMY_PROJECTILE::ns_NON_DODGE_AGE_SECONDS;
			// 予告時間0の使い捨ての判定で、この瞬間の結果だけを得る。
			JustDodgeJudge judge;
			judge.Initialize();
			judge.StartIncomingAttack(0.0f);
			results.push_back(judge.Update(0.0f,input.isInvincible,dodge_age_seconds));
			return false;
		}
		remaining_distance -= step_distance;
	}
	return true;
}