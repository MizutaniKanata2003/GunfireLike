#pragma once

// 報酬選択とショップへの、入力の受渡しと、状態の変化に伴うCaptureの切替を担当する。GameSessionの更新から、報酬選択とショップの部分を切り出したもの。
// 通貨と報酬選択（EconomyController）・ショップ（ShopFlow）・射撃・プレイヤーの戦闘への参照を借用し、所有はしない。時間の停止・ショップを開く条件・部屋の進行は、呼出側（GameSession）が決める。

//========= Scene インクルード=========
#include "Scene/GameScene/H/EconomyController.h" // 部屋の進行状態（ROOM_PHASE）を引数に使い、EconomyControllerを借用する。

//========= 前方宣言=========
class GraphicsSystem;
class InputSystem;
class PlayerCombatController;
class ShopFlow;
class ShotController;

// SessionEconomyFlowが借用する参照先をまとめる。参照先は、Unlinkまで有効であること。
struct SESSION_ECONOMY_FLOW_LINKS
{
  // キー・クリックの取得元と、Captureの切替先。
  InputSystem* inputSystem{};
  // カーソルの位置を、基準Canvas上の座標へ変換するために使う描画基盤。
  GraphicsSystem* graphicsSystem{};
  // 部屋のクリア後の報酬選択と、報酬の取得数・効果の倍率を持つ。
  EconomyController* economyController{};
  // カードが選ばれた後に開くショップ。開いている間の入力の渡し先。
  ShopFlow* shopFlow{};
  // 制圧射撃（怯み時間の倍率）の反映先となる射撃。
  ShotController* shotController{};
  // 殴打追撃（印の倍率と時間）の反映先となるプレイヤーの戦闘。
  PlayerCombatController* playerCombat{};
};

// 報酬選択とショップの入力の受渡しを担当するクラス。
class SessionEconomyFlow final
{
public:
  //========= 初期化・終了関数=========
  // 参照先を登録する。
  // links: 借用する参照先。1つでも欠けている場合は、何も登録せず失敗する。
  // 戻り値: 登録できたらtrue。失敗した場合は、ログへ記録する。
  [[nodiscard]] bool Link(const SESSION_ECONOMY_FLOW_LINKS& links);
  // 参照先を手放す。未登録でも、繰り返しても呼出可能。
  void Unlink();

  //========= 更新関数=========
  // 部屋のクリアで報酬選択へ入る判定と、カードが選ばれた時に抜ける判定を行い、入る時はCaptureを解除する。カードが選ばれたら、続けてショップを開く。
  // 取得した報酬の効果のうち、常に効くもの（制圧射撃・殴打追撃）を、使う側へ渡す。Link成功後に呼ぶ。
  // currentPhase: 現在の部屋の進行状態。
  void UpdateReward(ROOM_PHASE currentPhase);
  // ショップが開いている間、カーソルとキーの入力をShopFlowへ渡す。閉じる操作があった場合は、Captureを再開する。閉じている間は何もしない。Link成功後に呼ぶ。
  void UpdateShop();

private:
  //========= 参照先=========
  // キー・クリックの取得元と、Captureの切替先。非所有。未登録はnullptr。
  InputSystem* m_inputSystem{};
  // カーソルの位置の変換に使う描画基盤。非所有。未登録はnullptr。
  GraphicsSystem* m_graphicsSystem{};
  // 報酬選択と、報酬の取得数・効果の倍率。非所有。未登録はnullptr。
  EconomyController* m_economyController{};
  // 報酬選択の後に開く、ショップ。非所有。未登録はnullptr。
  ShopFlow* m_shopFlow{};
  // 制圧射撃の反映先となる射撃。非所有。未登録はnullptr。
  ShotController* m_shotController{};
  // 殴打追撃の反映先となるプレイヤーの戦闘。非所有。未登録はnullptr。
  PlayerCombatController* m_playerCombat{};
};
