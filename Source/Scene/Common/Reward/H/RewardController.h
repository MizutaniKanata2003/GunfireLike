#pragma once

// 部屋のクリア後に、報酬を選ぶ状態（報酬選択）へ入り、選び終えた時に抜ける流れを管理する。
// 状態の切替だけを担当する。時間の停止・入力の切替・報酬の内容・画面の表示は、呼出側が行う。
// 報酬の内容と選択の画面は、E3以降で追加する。現時点では、Enterキーで選び終えたことにする暫定の操作で、状態の基盤を確認する。

//========= Scene インクルード=========
#include "Scene/Common/Stage/H/RoomController.h" // 部屋の進行状態（ROOM_PHASE）から、部屋のクリアの瞬間を検出するために使用する。

// 報酬選択の更新で起きた出来事。
enum class REWARD_UPDATE_RESULT
{
  e_NONE,    // 状態の変化はない。
  e_STARTED, // この更新で、報酬選択へ入った。
  e_FINISHED // この更新で、報酬を選び終えて、報酬選択を抜けた。
};

// 部屋のクリアで報酬選択へ入り、選び終えると抜ける状態を持つ。
class RewardController final
{
public:
  //========= 初期化関数=========
  // 報酬選択を抜け、部屋の進行状態の記録を、初期の状態へ戻す。プレイの開始・再開の時に呼ぶ。
  void Reset()
  {
    m_isSelecting = false;
    m_previousPhase = ROOM_PHASE::e_NEEDS_SPAWN;
  }

  //========= 更新関数=========
  // 部屋の進行状態を見て、報酬選択へ入る・抜ける判定を行う。毎回の更新で、部屋の進行の更新の後に呼ぶ。
  // 部屋が、戦闘中から出口の開放（部屋の全滅）へ変わった更新で、報酬選択へ入る。選択中は、選び終えた操作があった更新で抜ける。
  // currentPhase: 現在の部屋の進行状態。
  // isConfirmTriggered: 選び終える操作が、この更新で押されたか。報酬選択へ入った更新に押されていても、無視する。
  // 戻り値: この更新で起きた出来事。
  [[nodiscard]] REWARD_UPDATE_RESULT Update(ROOM_PHASE currentPhase,bool isConfirmTriggered)
  {
    // 前回の進行状態を取り出し、今回の状態を、次の更新のために記録する。
    const ROOM_PHASE previous_phase = m_previousPhase;
    m_previousPhase = currentPhase;

    // 選択中は、選び終えた操作があった更新だけ抜ける。それ以外は、何も変えない。
    if(m_isSelecting){
      if(!isConfirmTriggered)return REWARD_UPDATE_RESULT::e_NONE;

      m_isSelecting = false;
      return REWARD_UPDATE_RESULT::e_FINISHED;
    }

    // 出口の開放へ変わった更新だけ、報酬選択へ入る。出口が開いたままの間は、繰り返し入らない。
    if(previous_phase != ROOM_PHASE::e_EXIT_OPEN && currentPhase == ROOM_PHASE::e_EXIT_OPEN){
      m_isSelecting = true;
      return REWARD_UPDATE_RESULT::e_STARTED;
    }
    return REWARD_UPDATE_RESULT::e_NONE;
  }

  //========= Getter関数=========
  // 報酬選択の最中かを返す。最中は、時間とプレイヤーの操作が止まる。
  [[nodiscard]] bool IsSelecting() const { return m_isSelecting; }

private:
  //========= 状態=========
  // 報酬選択の最中か。
  bool m_isSelecting{};
  // 前回の更新での部屋の進行状態。部屋のクリアの瞬間を検出するために使う。
  ROOM_PHASE m_previousPhase{ROOM_PHASE::e_NEEDS_SPAWN};
};
