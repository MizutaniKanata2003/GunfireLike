#include "../H/RoomController.h"

// 部屋とウェーブの切替を実装する。描画やCollisionの状態は変更しない。

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyWaveTable.h" // 部屋の数（ns_ROOM_COUNT）と、部屋ごとのウェーブの数（GetWaveCount）を、編成表から読む。

//========= 進行関数=========
// 第1部屋の第1ウェーブを、未生成の状態から開始する。
void RoomController::Initialize()
{
	m_roomNumber = 1;
	m_waveNumber = 1;
	m_phase = ROOM_PHASE::e_NEEDS_SPAWN;
}

// 生成完了したウェーブだけを全滅判定の対象にする。
void RoomController::NotifyWaveSpawned()
{
	// 生成待ちの通知だけを受理し、戦闘中や出口開放中の重複通知は無視する。
	if(m_phase == ROOM_PHASE::e_NEEDS_SPAWN){
		m_phase = ROOM_PHASE::e_FIGHTING;
	}
}

// 生存中の敵がいなくなったら、次の進行状態へ切り替える。
void RoomController::Update(bool hasLivingEnemies)
{
	// 生成待ちの空リストを全滅と誤認しないよう、戦闘中かつ生存者なしの場合だけ進める。
	if(m_phase != ROOM_PHASE::e_FIGHTING || hasLivingEnemies)return;

	// 現在の部屋に次のウェーブがある場合は番号を進め、生成完了の通知を待つ。部屋ごとのウェーブの数は、編成表が決める。
	if(m_waveNumber < ENEMY_WAVE_TABLE::GetWaveCount(m_roomNumber)){
		++m_waveNumber;
		m_phase = ROOM_PHASE::e_NEEDS_SPAWN;
		return;
	}
	m_phase = ROOM_PHASE::e_EXIT_OPEN;
}

// 出口を通過した部屋を閉じ、次の部屋または攻略完了へ移る。
bool RoomController::TryAdvanceRoom()
{
	// 出口が開いていない場合は、早すぎる通過や重複通知で部屋を進めない。
	if(m_phase != ROOM_PHASE::e_EXIT_OPEN)return false;

	// 最終部屋（編成表の最後の部屋）の出口なら、部屋番号を増やさず終了する。
	if(m_roomNumber == static_cast<int>(ENEMY_WAVE_TABLE::ns_ROOM_COUNT)){
		m_phase = ROOM_PHASE::e_COMPLETE;
		return true;
	}

	// 次の部屋の第1ウェーブの生成を要求する。
	++m_roomNumber;
	m_waveNumber = 1;
	m_phase = ROOM_PHASE::e_NEEDS_SPAWN;
	return true;
}

//========= Getter関数=========
// 1始まりの部屋番号を返す。
int RoomController::GetRoomNumber() const
{
	return m_roomNumber;
}

// 1始まりのウェーブ番号を返す。
int RoomController::GetWaveNumber() const
{
	return m_waveNumber;
}

// 生成・戦闘・出口・完了の状態を返す。
ROOM_PHASE RoomController::GetPhase() const
{
	return m_phase;
}