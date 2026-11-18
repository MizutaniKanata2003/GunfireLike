#include "../H/RoomController.h"

// 部屋とウェーブの切替を実装する。描画やCollisionの状態は変更しない。

// α版計画の部屋数と各部屋のウェーブ数をまとめる。
namespace
{
  //========= 攻略構成=========
  // 最終部屋の番号。部屋番号は1始まりのため、部屋数に等しい。
  constexpr int ns_ROOM_COUNT = 3;
  // 各部屋の最終ウェーブ番号。このウェーブの全滅で出口を開く。
  constexpr int ns_WAVES_PER_ROOM = 2;
}

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

  // 次のウェーブがある場合は番号を進め、生成完了の通知を待つ。
  if(m_waveNumber < ns_WAVES_PER_ROOM){
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

  // 最終部屋の出口なら、部屋番号を増やさず終了する。
  if(m_roomNumber == ns_ROOM_COUNT){
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
