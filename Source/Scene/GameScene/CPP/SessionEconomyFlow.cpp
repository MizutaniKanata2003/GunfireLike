#include "../H/SessionEconomyFlow.h"

// 報酬選択とショップの、入力の受渡しと、Captureの切替、取得した報酬の効果の受渡しを実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_LBUTTON・VK_RETURNの仮想キーコードで、クリック・ショップを閉じるキーを指定する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/PlayerCombatController.h"
#include "Scene/Common/Weapon/H/ShotController.h"
#include "Scene/GameScene/H/GameInputHelper.h"
#include "Scene/GameScene/H/ShopFlow.h"

//========= 初期化・終了関数=========
// 参照先を登録する。
bool SessionEconomyFlow::Link(const SESSION_ECONOMY_FLOW_LINKS& links)
{
  // 参照先が1つでも欠けている場合は、以後の更新で空の参照を使ってしまうため、何も登録せず失敗を返す。
  if(links.inputSystem == nullptr || links.graphicsSystem == nullptr || links.economyController == nullptr ||
     links.shopFlow == nullptr || links.shotController == nullptr || links.playerCombat == nullptr){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"SessionEconomyFlowの参照先の登録に失敗しました。");
    return false;
  }

  // 参照先を登録する。以後の更新は、この参照先を使う。
  m_inputSystem = links.inputSystem;
  m_graphicsSystem = links.graphicsSystem;
  m_economyController = links.economyController;
  m_shopFlow = links.shopFlow;
  m_shotController = links.shotController;
  m_playerCombat = links.playerCombat;
  return true;
}

// 参照先を手放す。
void SessionEconomyFlow::Unlink()
{
  m_inputSystem = nullptr;
  m_graphicsSystem = nullptr;
  m_economyController = nullptr;
  m_shopFlow = nullptr;
  m_shotController = nullptr;
  m_playerCombat = nullptr;
}

//========= 更新関数=========
// 部屋のクリアで報酬選択へ入る判定と、カードが選ばれた時に抜ける判定を行い、取得した報酬の効果を各クラスへ渡す。
void SessionEconomyFlow::UpdateReward(ROOM_PHASE currentPhase)
{
  // 部屋の進行状態と、カーソルの位置（基準Canvas上）、左クリック、カードの番号のキーを渡して、状態の変化を受け取る。
  // 上限に達した報酬の置換の通貨と、選ばれた報酬のログは、EconomyControllerが処理する。
  const XMFLOAT2 cursor_position = GAME_INPUT_HELPER::ConvertCursorToCanvas(*m_inputSystem,*m_graphicsSystem);
  const int key_choice_index = GAME_INPUT_HELPER::FindTriggeredNumberKey(*m_inputSystem,REWARD_TABLE::ns_CARD_COUNT);
  const REWARD_UPDATE_RESULT reward_result = m_economyController->UpdateReward(currentPhase,cursor_position,m_inputSystem->IsKeyTriggered(VK_LBUTTON),key_choice_index);

  // 入った更新では、Captureを解除して、マウスをカードの操作へ渡す。ゲームの操作と時間は、GameSessionが止める。
  if(reward_result == REWARD_UPDATE_RESULT::e_STARTED){
    m_inputSystem->SetMouseCaptureEnabled(false);
    Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,L"部屋のクリアで、報酬選択へ入りました。");
  }
  // 抜けた更新では、続けてショップを開く。Captureは、ショップを閉じるまで解除したままにする。出口は、すでに開いている。
  else if(reward_result == REWARD_UPDATE_RESULT::e_FINISHED){
    m_shopFlow->Open();
  }

  // 常に効く特殊強化の効果を、使う側へ渡す。毎回渡して、再開や初期化の後も、取得数と一致させる。
  // 制圧射撃は、射撃で敵へ与える怯み時間の倍率として、射撃の担当へ渡す。
  // 殴打追撃は、殴打が当たった敵へ付ける印の倍率と時間として、戦闘の担当へ渡す。
  // 回避反撃は、ジャスト回避の時だけ効くため、ここでは渡さない（GameSessionのStartJustDodgeEffectsが渡す）。
  const RewardController& reward = m_economyController->GetReward();
  m_shotController->SetFlinchScale(reward.GetSuppressiveFlinchScale());
  m_playerCombat->SetFollowUpStrike(reward.GetFollowUpDamageScale(),REWARD_TABLE::ns_FOLLOW_UP_SECONDS);
}

// ショップが開いている間、入力をShopFlowへ渡し、閉じる操作があればCaptureを再開する。
void SessionEconomyFlow::UpdateShop()
{
  // 閉じている間は、何もしない。
  if(!m_shopFlow->IsOpen())return;

  // カーソルの位置（基準Canvas上）、左クリック、数字キー、Enter、Uを渡す。
  // 数字キーは、売買のページの武器と弾薬のボタンの数だけ調べる（強化のページは、先頭の5個を使う）。
  SHOP_FLOW_INPUT shop_input{};
  shop_input.cursorCanvasPosition = GAME_INPUT_HELPER::ConvertCursorToCanvas(*m_inputSystem,*m_graphicsSystem);
  shop_input.isClickTriggered = m_inputSystem->IsKeyTriggered(VK_LBUTTON);
  shop_input.numberKeyIndex = GAME_INPUT_HELPER::FindTriggeredNumberKey(*m_inputSystem,SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET);
  shop_input.isCloseKeyTriggered = m_inputSystem->IsKeyTriggered(VK_RETURN);
  shop_input.isSwitchKeyTriggered = m_inputSystem->IsKeyTriggered(GAME_INPUT_HELPER::ns_SHOP_SWITCH_KEY);

  // 閉じた場合は、Captureを戻して、ゲームの操作へ戻る。選んだクリックが射撃にならないよう、入力の保護が、続けて働く。
  if(m_shopFlow->Update(shop_input)){
    m_inputSystem->SetMouseCaptureEnabled(true);
    Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,L"ショップを閉じました。");
  }
}
