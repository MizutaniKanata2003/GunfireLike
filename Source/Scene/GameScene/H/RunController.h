#pragma once

// 1回のプレイ（新規開始から、プレイヤーの死亡または最終部屋のクリアで終わるまで）の状態と、終了した後の待ち時間を管理する。
// 判定に使う値（プレイヤーの死亡・部屋の進行）は、呼出側から受け取る。終了した後の動作（やり直し・結果の表示・遷移）は担当せず、呼出側が状態を見て決める。
// 終了の状態は、一度なったら、新しいプレイを始める（Initialize）まで変わらない。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyWaveTable.h" // 最終部屋の番号（部屋の数）を、編成表から読む。
#include "Scene/Common/Stage/H/RoomController.h" // 部屋の進行状態（ROOM_PHASE）から、最終部屋のクリアを判定する。

// プレイの終わりに関する数値をまとめる。
namespace RUN_TABLE
{
  //========= 終了後の待ち時間=========
  // プレイが終了（死亡・クリア）してから、呼出側が次へ進むまでの待ち時間。単位は実時間の秒。この間は、最後の画面のまま止まる。仮値。
  inline constexpr float ns_END_DELAY_SECONDS = 2.0f;
}

// 1回のプレイの状態。
enum class RUN_STATE
{
  e_PLAYING, // プレイ中。
  e_DEAD,    // プレイヤーが死亡して終了した。
  e_CLEARED  // 最終部屋（ボスの部屋）のクリアで終了した。
};

// 1回のプレイの状態を管理する。
class RunController final
{
public:
  //========= 初期化関数=========
  // 新しいプレイを始める。プレイ中の状態にし、終了後の待ち時間を0へ戻す。
  void Initialize()
  {
    m_state = RUN_STATE::e_PLAYING;
    m_endElapsedSeconds = 0.0f;
  }

  //========= 更新関数=========
  // プレイ中なら、死亡と最終部屋のクリアを判定し、終了した更新で状態を変える。終了した後は、待ち時間を進める。毎回の更新で、他の更新より先に呼ぶ。
  // 死亡とクリアが同じ更新で起きた場合は、死亡を優先する。
  // deltaTime: 実時間の経過時間。単位は秒。終了後の待ち時間に使う。時間の停止やスローの影響を受けない。
  // isPlayerDead: プレイヤーのHPが0か。
  // roomNumber: 現在の部屋番号。1始まり。
  // roomPhase: 現在の部屋の進行状態。
  void Update(float deltaTime,bool isPlayerDead,int roomNumber,ROOM_PHASE roomPhase)
  {
    // 終了した後は、状態を変えず、待ち時間だけを進める。
    if(m_state != RUN_STATE::e_PLAYING){
      m_endElapsedSeconds += deltaTime;
      return;
    }

    // プレイヤーが死亡した場合は、死亡で終了する。同じ更新でクリアの条件を満たしていても、死亡を優先する。
    if(isPlayerDead){
      m_state = RUN_STATE::e_DEAD;
      Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,L"プレイヤーが死亡しました。");
      return;
    }

    // 最終部屋で、出口が開いた（ボスを倒した）か、通過済みの場合は、クリアで終了する。
    const bool is_final_room = roomNumber == static_cast<int>(ENEMY_WAVE_TABLE::ns_ROOM_COUNT);
    const bool is_room_cleared = roomPhase == ROOM_PHASE::e_EXIT_OPEN || roomPhase == ROOM_PHASE::e_COMPLETE;
    if(is_final_room && is_room_cleared){
      m_state = RUN_STATE::e_CLEARED;
      Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,L"最終部屋をクリアしました。");
    }
  }

  //========= Getter関数=========
  // 現在のプレイの状態を返す。
  [[nodiscard]] RUN_STATE GetState() const { return m_state; }
  // プレイ中かを返す。falseの間は、呼出側が、ゲームの更新を止める。
  [[nodiscard]] bool IsPlaying() const { return m_state == RUN_STATE::e_PLAYING; }
  // 終了してから、待ち時間が過ぎたかを返す。プレイ中はfalse。
  [[nodiscard]] bool HasEndDelayElapsed() const { return m_state != RUN_STATE::e_PLAYING && m_endElapsedSeconds >= RUN_TABLE::ns_END_DELAY_SECONDS; }

private:
  //========= 状態=========
  // プレイの状態。終了の状態は、Initializeまで変わらない。
  RUN_STATE m_state{RUN_STATE::e_PLAYING};
  // 終了してからの経過時間。単位は実時間の秒。プレイ中は0。
  float m_endElapsedSeconds{};
};
