#pragma once

// 1ウェーブ分の敵とColliderの登録・解除を管理する。敵の攻撃と部屋の遷移は担当しない。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで敵と出現枠の対応を固定する。
#include <cstddef> // std::size_tで出現枠を走査する。
#include <cstdint> // std::uint32_tで命中判定用の敵IDを扱う。

//========= Scene インクルード=========
#include "Scene/GameScene/H/Enemy.h"
#include "Scene/GameScene/H/EnemyWaveTable.h"

// ウェーブ内の敵を所有する。終了時はRegistryとCollisionWorldの解放前にClearを呼ぶ。
class EnemyManager final
{
public:
	//========= 生成・終了関数=========
	// 旧ウェーブを解除し、編成表から指定ウェーブを登録する。
	// roomNumber: 1始まりの部屋番号。
	// waveNumber: 1始まりのウェーブ番号。
	// registry: 初期化済みの共有Rendererの所有者。Clearまで有効であること。
	// collisionWorld: Colliderの登録先。旧ウェーブがある場合は登録に使ったものと同じ実体。
	// 戻り値: 全枠の登録が成功した場合にtrue。範囲外では旧状態を維持し、生成失敗では空にする。
	[[nodiscard]] bool SpawnWave(int roomNumber,int waveNumber,ModelRegistry& registry,CollisionWorld& collisionWorld)
	{
		// 番号が正でない場合は、0始まりへの変換や旧ウェーブの解除を行わない。
		if(roomNumber < 1 || waveNumber < 1)return false;

		// 表の添字に直し、部屋の範囲を確認してから行を参照する。
		const auto room_index = static_cast<std::size_t>(roomNumber - 1);
		const auto wave_index = static_cast<std::size_t>(waveNumber - 1);
		// 短絡評価で、部屋の添字が有効な場合だけ対応するウェーブ行の要素数を調べる。
		if(room_index >= ENEMY_WAVE_TABLE::ns_ROOMS.size() || wave_index >= ENEMY_WAVE_TABLE::ns_ROOMS[room_index].size())return false;

		// 同じIDを再利用する前に、旧ウェーブのColliderを解除する。
		Clear(collisionWorld);
		const auto& spawns = ENEMY_WAVE_TABLE::ns_ROOMS[room_index][wave_index];
		// 編成表の先頭から順に全枠を登録し、枠番号に1を足した値をウェーブ内で一意のIDにする。
		for(std::size_t slot_index = 0; slot_index < spawns.size(); ++slot_index){
			const auto enemy_ID = static_cast<std::uint32_t>(slot_index + 1);
			// 敵の初期化がfalseなら、先に成功した枠も解除し、空の状態を返す。例外は捕捉しない。
			if(!m_enemies[slot_index].Initialize(registry,collisionWorld,spawns[slot_index].position,enemy_ID)){
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

	// 固定枠にある敵を出現順に読み取る。死亡中の枠も含む。
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
	//========= 所有状態=========
	// 編成表と同じ順序の敵。初期状態では全枠が未登録。
	std::array<Enemy,ENEMY_WAVE_TABLE::ns_SLOT_COUNT> m_enemies{};
};
