#pragma once

// 1ウェーブ分の敵とColliderの登録・解除を管理する。敵の攻撃と部屋の遷移は担当しない。
// 敵どうしの重なりを、互いに押し離して解消する。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで敵と出現枠の対応を固定する。
#include <cmath> // std::sqrtで、敵どうしの水平距離を求める。
#include <cstddef> // std::size_tで出現枠を走査する。
#include <cstdint> // std::uint32_tで命中判定用の敵IDを扱う。

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/CombatResolver.h"
#include "Scene/Common/Enemy/H/Enemy.h"
#include "Scene/Common/Enemy/H/EnemyWaveTable.h"

// 敵どうしの押し離しに使う数値をまとめる。
namespace ENEMY_SEPARATION
{
  //========= 数値=========
  // 2体の中心の水平距離が、この値より小さい場合は、押す向きを決められないため、X方向へ押す。単位はワールド距離単位。中心がほぼ一致した場合の、0で割る除算を避けるための値。
  inline constexpr float ns_MIN_DIRECTION_DISTANCE = 0.0001f;
}

// ウェーブ内の敵を所有する。終了時はRegistryとCollisionWorldの解放前にClearを呼ぶ。
class EnemyManager final
{
public:
  //========= 生成・終了関数=========
  // 旧ウェーブを解除し、編成表から指定ウェーブを、種類と位置のとおりに、編成の敵の数だけ登録する。使わない枠は、未登録のまま残す。
  // roomNumber: 1始まりの部屋番号。
  // waveNumber: 1始まりのウェーブ番号。
  // registry: 初期化済みの共有Rendererの所有者。Clearまで有効であること。
  // collisionWorld: Colliderの登録先。旧ウェーブがある場合は登録に使ったものと同じ実体。
  // 戻り値: 全ての敵の登録が成功した場合にtrue。番号が範囲外、または編成の敵の数が枠の数を超える場合は旧状態を維持してfalse、生成失敗では空にしてfalse。
  [[nodiscard]] bool SpawnWave(int roomNumber,int waveNumber,ModelRegistry& registry,CollisionWorld& collisionWorld)
  {
    // 番号が正でない場合は、0始まりへの変換や旧ウェーブの解除を行わない。
    if(roomNumber < 1 || waveNumber < 1)return false;

    // 表の添字に直し、部屋の範囲を確認してから行を参照する。
    const auto room_index = static_cast<std::size_t>(roomNumber - 1);
    const auto wave_index = static_cast<std::size_t>(waveNumber - 1);
    // 短絡評価で、部屋の添字が有効な場合だけ対応するウェーブ行の要素数を調べる。
    if(room_index >= ENEMY_WAVE_TABLE::ns_ROOMS.size() || wave_index >= ENEMY_WAVE_TABLE::ns_ROOMS[room_index].size())return false;

    // 対象のウェーブの編成を参照する。
    const ENEMY_WAVE_TABLE::WAVE_DEFINITION& wave = ENEMY_WAVE_TABLE::ns_ROOMS[room_index][wave_index];
    // 編成の敵の数が、枠の数を超える場合は、配列の外を読まないよう、旧ウェーブを解除せずに失敗とする。
    if(wave.enemyCount > wave.spawns.size() || wave.enemyCount > m_enemies.size())return false;

    // 同じIDを再利用する前に、旧ウェーブのColliderを解除する。
    Clear(collisionWorld);
    // 編成の先頭から順に、敵の数だけ登録し、枠番号に1を足した値をウェーブ内で一意のIDにする。
    for(std::size_t slot_index = 0; slot_index < wave.enemyCount; ++slot_index){
      // 種類・位置・IDを、1つの構造体にまとめて、敵を初期化する。
      const ENEMY_INIT_PARAMS init_params{wave.spawns[slot_index].kind,wave.spawns[slot_index].position,static_cast<std::uint32_t>(slot_index + 1)};
      // 敵の初期化がfalseなら、先に成功した枠も解除し、空の状態を返す。例外は捕捉しない。
      if(!m_enemies[slot_index].Initialize(registry,collisionWorld,init_params)){
        Clear(collisionWorld);
        return false;
      }
    }
    return true;
  }

  // 全枠のColliderを解除し、敵を未初期化に戻す。
  // collisionWorld: SpawnWaveで使った登録先。
  void Clear(CollisionWorld& collisionWorld)
  {
    // 全枠を先頭から解除する。Enemy::Uninitは死亡済み・未初期化の枠にも呼べる。
    for(Enemy& enemy : m_enemies){
      enemy.Uninit(collisionWorld);
    }
  }

  // 全枠の怯み・転倒の残り時間を進める。死亡枠や未登録枠も、Enemy側の条件に従う。
  // deltaTime: 敵側の経過時間。単位は秒。
  void Update(float deltaTime)
  {
    // 全枠を先頭から更新し、第1枠だけでなく全ての敵の怯み・転倒を同じ時間で進める。
    for(Enemy& enemy : m_enemies){
      enemy.Update(deltaTime);
    }
  }

  // 生きている敵どうしの、体の球が水平に重なっている組を、重なりが無くなるまで、互いに押し離す。体の大きい（半径の大きい）敵ほど、動かされにくい。
  // 地形への押し込みは、各敵の移動の処理が補正する。毎フレーム呼ぶと、敵どうしが重なったままにならない。
  // collisionWorld: 敵のColliderを登録したCollisionWorld。
  void SeparateEnemies(CollisionWorld& collisionWorld)
  {
    // 全ての組を、1回ずつ調べる。
    for(std::size_t first_index = 0; first_index < m_enemies.size(); ++first_index){
      for(std::size_t second_index = first_index + 1; second_index < m_enemies.size(); ++second_index){
        SeparatePair(m_enemies[first_index],m_enemies[second_index],collisionWorld);
      }
    }
  }

  // 固定枠にある敵を出現順に変更可能な形で受け取る。死亡中・未登録の枠も含む。
  // 戻り値: 管理中の固定配列への参照。Managerの寿命を超えて保持しない。
  [[nodiscard]] std::array<Enemy,ENEMY_WAVE_TABLE::ns_SLOT_COUNT>& GetEnemies()
  {
    return m_enemies;
  }

  // 視点と視線から近接攻撃の射程と範囲に入る、最も近い生存中の敵を探す。
  // eyePosition: プレイヤーの視点のワールド座標。
  // forward: 視線方向の単位ベクトル。
  // requiresKickable: trueなら蹴れる状態（怯み中で転倒していない敵）に限る。
  // 戻り値: 対象の敵への非所有ポインター。対象なしはnullptr。次のClearやSpawnWaveをまたいで保持しない。
  [[nodiscard]] Enemy* FindMeleeTarget(const XMFLOAT3& eyePosition,const XMFLOAT3& forward,bool requiresKickable)
  {
    Enemy* nearest_enemy = nullptr;
    float nearest_distance_squared = 0.0f;

    // 全枠を先頭から調べ、条件を満たす敵の中で視点からの距離が最も短いものを残す。
    for(Enemy& enemy : m_enemies){
      const bool is_target = requiresKickable ? CombatResolver::CanKickEnemy(eyePosition,forward,enemy) :
        CombatResolver::IsEnemyInMeleeReach(eyePosition,forward,enemy);
      // 範囲外・死亡・蹴れない敵は候補に入れず、次の枠へ進む。
      if(!is_target)continue;

      // 比較だけに使うため、平方根を取らず距離の二乗で近さを決める。
      const XMFLOAT3 center = enemy.GetBodySphere().center;
      const float offset_x = center.x - eyePosition.x;
      const float offset_y = center.y - eyePosition.y;
      const float offset_z = center.z - eyePosition.z;
      const float distance_squared = offset_x * offset_x + offset_y * offset_y + offset_z * offset_z;
      // 最初の候補、またはより近い候補だけを保持する。同じ距離なら先に調べた枠を残す。
      if(nearest_enemy == nullptr || distance_squared < nearest_distance_squared){
        nearest_enemy = &enemy;
        nearest_distance_squared = distance_squared;
      }
    }
    return nearest_enemy;
  }

  //========= 問い合わせ関数=========
  // 生存中の敵が1体でもいるかを返す。
  [[nodiscard]] bool HasLivingEnemies() const
  {
    // 全枠を先頭から調べ、生存中の敵を見つけた時点で走査を終える。
    for(const Enemy& enemy : m_enemies){
      // 生存枠があれば、ウェーブは全滅していない。
      if(enemy.IsAlive())return true;
    }
    return false;
  }

  // 固定の試遊編成で、既存の単体敵処理を第1枠へ接続する。
  // 戻り値: 生死を問わず第1枠への非所有参照。ウェーブの切替をまたいで保持しない。
  [[nodiscard]] Enemy& GetFirstEnemy()
  {
    return m_enemies.front();
  }

  // 固定枠にある敵を出現順に読み取る。死亡中・未登録の枠も含む。
  // 戻り値: 管理中の固定配列の読み取り専用参照。Managerの寿命を超えて保持しない。
  [[nodiscard]] const std::array<Enemy,ENEMY_WAVE_TABLE::ns_SLOT_COUNT>& GetEnemies() const
  {
    return m_enemies;
  }

  // 命中先のIDを生存中の敵に対応付ける。
  // enemyID: ColliderのuserData。現在のウェーブの問い合わせ結果だけを渡す。
  // 戻り値: 生存中の一致する敵への非所有ポインター。対象なしはnullptr。次のClearやSpawnWaveをまたいで保持しない。
  [[nodiscard]] Enemy* FindLivingEnemy(std::uint32_t enemyID)
  {
    // 全枠を先頭から調べ、IDが一致する生存枠だけを返す。
    for(Enemy& enemy : m_enemies){
      // 死亡枠には以前のIDが残りうるため、生存とID一致の両方を確認する。
      if(enemy.IsAlive() && enemy.GetEnemyId() == enemyID)return &enemy;
    }
    return nullptr;
  }

private:
  //========= 補助関数=========
  // 2体の敵の体の球が、水平に重なっている場合だけ、重なりの分を、互いに反対の向きへ押し離す。押す量は、半径の二乗（大きさの目安）に反比例して配分する。高さは変えない。
  // first: 1体目の敵。死亡中なら何もしない。
  // second: 2体目の敵。死亡中なら何もしない。
  // collisionWorld: 敵のColliderを登録したCollisionWorld。移動の押し戻しに使う。
  static void SeparatePair(Enemy& first,Enemy& second,CollisionWorld& collisionWorld)
  {
    // どちらかが死亡している組は、重なっても押し離さない。
    if(!first.IsAlive() || !second.IsAlive())return;

    // 水平の距離と、重なりが生じる距離（半径の和）を求める。重なっていなければ戻る。
    const COLLISION_SPHERE first_sphere = first.GetBodySphere();
    const COLLISION_SPHERE second_sphere = second.GetBodySphere();
    const float offset_x = second_sphere.center.x - first_sphere.center.x;
    const float offset_z = second_sphere.center.z - first_sphere.center.z;
    const float distance = std::sqrt(offset_x * offset_x + offset_z * offset_z);
    const float contact_distance = first_sphere.radius + second_sphere.radius;
    if(distance >= contact_distance)return;

    // 1体目から2体目へ向かう単位ベクトルを求める。中心がほぼ一致している場合は、向きが決まらないため、X方向とする。
    const bool has_direction = distance > ENEMY_SEPARATION::ns_MIN_DIRECTION_DISTANCE;
    const float direction_x = has_direction ? offset_x / distance : 1.0f;
    const float direction_z = has_direction ? offset_z / distance : 0.0f;

    // 重なりの分を、半径の二乗の逆比で配分する。大きい敵ほど、動く量が小さい。
    const float overlap = contact_distance - distance;
    const float first_weight = first_sphere.radius * first_sphere.radius;
    const float second_weight = second_sphere.radius * second_sphere.radius;
    const float first_push = overlap * second_weight / (first_weight + second_weight);
    const float second_push = overlap * first_weight / (first_weight + second_weight);

    // 互いに反対の向きへ移動させる。地形との重なりは、TryMoveToが押し戻す。
    static_cast<void>(first.TryMoveTo(XMFLOAT3{first_sphere.center.x - direction_x * first_push,first_sphere.center.y,
                      first_sphere.center.z - direction_z * first_push},collisionWorld));
    static_cast<void>(second.TryMoveTo(XMFLOAT3{second_sphere.center.x + direction_x * second_push,second_sphere.center.y,
                      second_sphere.center.z + direction_z * second_push},collisionWorld));
  }

  //========= 所有状態=========
  // 編成表と同じ順序の敵。初期状態では全枠が未登録。
  std::array<Enemy,ENEMY_WAVE_TABLE::ns_SLOT_COUNT> m_enemies{};
};