#pragma once

// 射撃型の敵が撃つ弾を管理する。弾の生成・直進・壁での消滅・プレイヤーへの命中判定と、命中時の無敵・回避の判定結果を返す。
// 弾を撃つタイミングの判断、ダメージとHPの反映、ヒットストップ、スローは担当せず、呼出側が結果に応じて行う。
// 弾は生成時のプレイヤーの水平位置へ向かって直進し、その後は追尾しない。数値は企画書で未確定の試遊用の仮値。距離の単位はワールド距離単位、時間の単位は秒。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // std::size_tで、現在の弾の数と上限を表す。
#include <vector> // 飛行中の弾を可変長配列で保持し、1回の更新で出た判定結果を返す。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/JustDodgeJudge.h"

//========= 前方宣言=========
class BasicMeshRenderer;
class CollisionWorld;
class GraphicsSystem;
class ModelRegistry;

// 敵の弾の数値をまとめる。
namespace ENEMY_PROJECTILE
{
	//========= 弾の形状=========
	// 当たりの球の半径。単位はワールド距離単位。
	inline constexpr float ns_RADIUS = 0.15f;
	// 弾が飛ぶ高さ。単位はワールド距離単位。立っている時の視点の高さ（1.6）の球には当たり、しゃがみ（1.05）・スライディング（0.55）の球には当たらない高さの仮値。
	inline constexpr float ns_HEIGHT = 1.7f;
	// 表示するCubeの1辺の長さ。単位はワールド距離単位。
	inline constexpr float ns_DRAW_SIZE = 0.3f;

	//========= 弾の動き=========
	// 速度。単位はワールド距離単位/秒。仮値。
	inline constexpr float ns_SPEED = 12.0f;
	// 生成してから消えるまでの時間。単位は秒。壁に当たらなかった場合の上限。仮値。
	inline constexpr float ns_LIFETIME_SECONDS = 3.0f;
	// 壁との判定を1回行うまでに進める最大距離。単位はワールド距離単位。壁の厚み（0.2）より長くても、判定は線分で行うため、すり抜けない。
	inline constexpr float ns_MAX_STEP_DISTANCE = 0.3f;
	// 同時に存在できる弾の上限。上限に達している間は、新しい弾を生成しない。仮値。
	inline constexpr std::size_t ns_MAX_COUNT = 16;

	//========= 戦闘=========
	// 命中した場合にプレイヤーへ与えるダメージ。仮値。
	inline constexpr float ns_DAMAGE = 12.0f;

	//========= 判定=========
	// 回避の動作中ではない無敵（蹴り・殴打）を、ジャスト回避にしないために、回避の開始からの経過時間として渡す値。単位は秒。判定の閾値より十分に大きい。
	inline constexpr float ns_NON_DODGE_AGE_SECONDS = 1000.0f;

	//========= 色=========
	// 弾の色。黄みのある橙で、Object画像の色へ乗算して、敵の予告の色と区別しやすくする。
	inline constexpr XMFLOAT4 ns_COLOR{1.0f,0.85f,0.2f,1.0f};
}

//========= 更新の入力=========
// 弾の更新に必要な、経過時間とプレイヤーの状態をまとめた入力。EnemyProjectileManager::Updateの引数の数を減らすための値の束で、更新は変更しない。
struct ENEMY_PROJECTILE_UPDATE_INPUT
{
	// 敵側の経過時間。単位は秒。0以下、または有限でない値なら、弾を進めない。
	float deltaTime{};
	// プレイヤーの体の球。現在の位置と半径。
	COLLISION_SPHERE playerBody{};
	// プレイヤーが無敵か。回避・蹴り・殴打の無敵を含む。
	bool isInvincible{};
	// プレイヤーが回避の動作中か。ジャスト回避は、回避の動作中の無敵だけが対象になる。
	bool isDodging{};
	// 最後に回避を始めてからの経過時間。単位は秒。ジャスト回避の判定に使う。
	float secondsSinceDodgeStart{};
};

// 飛行中の敵の弾を保持し、更新・描画する。
class EnemyProjectileManager final
{
public:
	//========= 初期化・終了関数=========
	// 共有のCube Rendererを借用し、弾を全て空にする。
	// registry: Cube Rendererの借用元。Initialize済みであること。このManagerより長く生存させる。
	// 戻り値: Cube Rendererを借用できればtrue。Registryが未初期化ならfalse。
	[[nodiscard]] bool Initialize(ModelRegistry& registry);
	// 全ての弾を消し、借用した参照を手放す。未初期化でも呼出可能。
	void Uninit();
	// 飛行中の全ての弾を消す。借用した参照は変えない。ウェーブの切替や再開で、旧ウェーブの弾を持ち越さないために使う。
	void Clear();

	//========= 生成・更新・描画関数=========
	// 弾を1つ生成する。弾は水平に、目標の位置へ向かって飛び、以後は追尾しない。
	// origin: 発射位置のワールド座標。XとZだけを使い、高さは弾の高さに置き換える。
	// targetPosition: 狙う位置のワールド座標。XとZだけを使う。
	// 戻り値: 生成できたらtrue。上限に達している、または発射位置と目標の水平位置が一致して向きを決められない場合はfalse。
	bool Spawn(const XMFLOAT3& origin,const XMFLOAT3& targetPosition);
	// 全ての弾を進め、壁・プレイヤーへの命中、寿命による消滅を処理する。命中した弾は、判定結果を1つずつ結果へ追加して消える。
	// input: 経過時間と、プレイヤーの体の球・無敵・回避の状態。経過時間が0以下なら何も進めない。
	// collisionWorld: 壁（STATICレイヤー）の判定に使うCollisionWorld。読み取りだけを行う。
	// results: 判定結果の出力先。この関数は、先に空にしない。命中した弾ごとに、e_HIT・e_DODGED・e_JUST_DODGEDのいずれかを追加する。
	void Update(const ENEMY_PROJECTILE_UPDATE_INPUT& input,const CollisionWorld& collisionWorld,std::vector<DODGE_JUDGE_RESULT>& results);
	// 飛行中の全ての弾を、Cubeで描く。未初期化なら何も描かない。
	// graphicsSystem: Rendererが使う描画基盤。
	// viewMatrix: World空間からView空間への変換行列。
	// projectionMatrix: View空間からClip空間への変換行列。
	void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

	//========= Getter関数=========
	// 飛行中の弾の数を返す。
	[[nodiscard]] std::size_t GetCount() const { return m_projectiles.size(); }

private:
	//========= 内部の型=========
	// 飛行中の弾1つの状態。
	struct PROJECTILE_STATE
	{
		// 現在の中心のワールド座標。
		XMFLOAT3 position{};
		// 進む向き。水平な長さ1のベクトル。生成時に決め、以後変えない。
		XMFLOAT3 direction{};
		// 消えるまでの残り時間。単位は秒。
		float remainingSeconds{};
	};

	//========= 補助関数=========
	// 1つの弾を進める。寿命・壁・プレイヤーへの命中を、進む距離を小分けにして順に調べる。
	// projectile: 進める弾。
	// input: 経過時間と、プレイヤーの体の球・無敵・回避の状態。
	// collisionWorld: 壁の判定に使うCollisionWorld。
	// results: 判定結果の追加先。
	// 戻り値: 弾が飛び続けるならtrue。壁・プレイヤーへの命中、寿命で消えるならfalse。
	[[nodiscard]] static bool AdvanceProjectile(PROJECTILE_STATE& projectile,const ENEMY_PROJECTILE_UPDATE_INPUT& input,const CollisionWorld& collisionWorld,
																							std::vector<DODGE_JUDGE_RESULT>& results);

	//========= 状態=========
	// Cube描画に使う共有Renderer。非所有で、ModelRegistryが所有する。未取得はnullptr。
	BasicMeshRenderer* m_cubeRenderer{};
	// 飛行中の弾。生成順に並ぶ。
	std::vector<PROJECTILE_STATE> m_projectiles{};
};