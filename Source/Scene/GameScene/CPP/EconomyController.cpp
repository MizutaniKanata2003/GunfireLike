#include "../H/EconomyController.h"

// 通貨・報酬選択・撃破報酬の初期化と更新を実装する。

//========= C++標準ライブラリ インクルード=========
#include <string> // 選ばれた報酬の見出しを、ログの文字列へ連結する。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/Enemy.h"
#include "Scene/Common/Enemy/H/EnemyManager.h"

//========= 初期化関数=========
// 通貨・報酬選択・敵の生死の記録を、初期状態へ戻す。
void EconomyController::Initialize()
{
  // 通貨を開始時の所持額へ戻し、敵の生死の記録を空にする。前回のプレイの敵を、倒された敵として扱わない。
  m_wallet.Initialize(ECONOMY_TABLE::ns_INITIAL_BALANCE);
  m_wasEnemyAlive.fill(false);

  // 報酬の取得数を0へ戻し、報酬選択を解除する。カードのボタンの矩形も、設定し直す。
  m_reward.Initialize();
}

//========= 更新関数=========
// 報酬選択へ入る・抜ける判定を行い、カードが選ばれた更新では、置換の通貨とログを処理する。
REWARD_UPDATE_RESULT EconomyController::UpdateReward(ROOM_PHASE currentPhase,const XMFLOAT2& cursorCanvasPosition,bool isClickTriggered,int keyChoiceIndex)
{
  const REWARD_UPDATE_RESULT reward_result = m_reward.Update(currentPhase,cursorCanvasPosition,isClickTriggered,keyChoiceIndex);

  // カードが選ばれた更新以外は、呼出側が結果を見て処理するため、ここでは何もしない。
  if(reward_result != REWARD_UPDATE_RESULT::e_FINISHED)return reward_result;

  // 上限に達した報酬を選んだ場合は、強化の代わりに通貨を与える。そうでなければ、選ばれた報酬を、ログへ記録する。
  if(m_reward.WasLastChoiceCapped()){
    m_wallet.Add(REWARD_TABLE::ns_CAPPED_CURRENCY_REWARD);
    Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,L"報酬が上限のため、通貨へ置き換えました。");
  }
  else{
    Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,
                  std::wstring{L"報酬を選びました: "} + REWARD_TABLE::ns_REWARDS[m_reward.GetLastChosenIndex()].title);
  }
  return reward_result;
}

// 新しく倒された敵の撃破報酬を、通貨へ加える。
void EconomyController::UpdateKillRewards(EnemyManager& enemyManager)
{
  // 全枠を先頭から調べ、前回は生きていて、今回は死亡になっている枠だけを、倒された敵として、種類に応じた報酬を通貨へ加える。
  const auto& enemies = enemyManager.GetEnemies();
  for(std::size_t slot_index = 0; slot_index < enemies.size(); ++slot_index){
    const Enemy& enemy = enemies[slot_index];
    const bool is_alive = enemy.IsAlive();
    // 前回は生きていて、今回は死亡の枠だけに、報酬を与える。死亡のまま残る枠には、繰り返し与えない。
    if(m_wasEnemyAlive[slot_index] && !is_alive){
      m_wallet.Add(ECONOMY_TABLE::GetKillReward(enemy.GetKind()));
    }

    // 次の確認のために、今回の生死を記録する。
    m_wasEnemyAlive[slot_index] = is_alive;
  }
}

// 敵の枠ごとの生死の記録を、現在の状態へ揃える。
void EconomyController::SyncEnemyAliveFlags(EnemyManager& enemyManager)
{
  // 全枠の現在の生死を記録し直す。報酬は与えない。
  const auto& enemies = enemyManager.GetEnemies();
  for(std::size_t slot_index = 0; slot_index < enemies.size(); ++slot_index){
    m_wasEnemyAlive[slot_index] = enemies[slot_index].IsAlive();
  }
}
