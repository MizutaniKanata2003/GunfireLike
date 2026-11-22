#pragma once

// ショップ（武器と弾薬の購入・武器の売却・武器の強化の画面）の流れと、武器の所持・強化・持ち替えを、まとめて担当する。
// 武器の所持の記録（WeaponInventory）とショップの画面の状態（ShopController）を所有し、画面の操作の結果（購入・売却・強化）を、通貨（Wallet）と射撃（ShotController）へ反映し、数字キーによる武器の持ち替えを行う。
// 通貨の所有・時間の停止・Captureの切替・入力の取得・画面の描画は担当しない。入力は、呼出側（GameSession）から値として受け取り、HUDへ渡す値は、FillHudStateで書き込む。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Shop/H/ShopController.h"
#include "Scene/Common/Weapon/H/WeaponInventory.h"
#include "Scene/GameScene/H/GameHud.h" // HUDへ渡す状態（GAME_HUD_STATE）を使用する。

//========= 前方宣言=========
class ShotController;
class Wallet;

// ShopFlowが借用する参照先をまとめる。参照先は、Unlinkまで有効であること。
struct SHOP_FLOW_LINKS
{
  // 購入・売却・強化の支払いと、売却の受取りに使う通貨。
  Wallet* wallet{};
  // 持ち替え・購入した武器の装備・弾薬の加算・強化の反映の先となる射撃。
  ShotController* shotController{};
};

// ショップの更新に渡す入力。取得は、呼出側が行う。
struct SHOP_FLOW_INPUT
{
  // カーソルの位置。基準Canvas上の座標。ボタンのホバーとクリックの判定に使う。
  XMFLOAT2 cursorCanvasPosition{};
  // クリックが、この更新で押されたか。
  bool isClickTriggered{};
  // この更新で押された数字キー（1から順）に対応する添字。押されていなければ-1。売買のページでは、武器と弾薬のボタン（6個）に、強化のページでは、強化の項目（5個）に対応する。
  int numberKeyIndex{-1};
  // 閉じるキー（Enter）が、この更新で押されたか。
  bool isCloseKeyTriggered{};
  // ページ切替キー（U）が、この更新で押されたか。
  bool isSwitchKeyTriggered{};
};

// ショップの流れと、武器の所持・強化・持ち替えを管理するクラス。
class ShopFlow final
{
public:
  //========= 初期化・終了関数=========
  // 武器の所持・強化・保存した弾薬を初期状態へ戻し、ショップを閉じる。ボタンの矩形も、設定し直す。参照先は変えない。新しいプレイの開始の時に呼ぶ。
  void Initialize();
  // 参照先を登録する。
  // links: 借用する参照先。1つでも欠けている場合は、何も登録せず失敗する。
  // 戻り値: 登録できたらtrue。
  [[nodiscard]] bool Link(const SHOP_FLOW_LINKS& links);
  // 参照先を手放す。未登録でも、繰り返しても呼出可能。
  void Unlink();

  //========= 更新関数=========
  // ショップを開く。売買のページから始める。
  void Open() { m_shop.Open(); }
  // ショップを閉じる。購入の記録は消さない。
  void Close() { m_shop.Close(); }
  // 開いている間、カーソルとキーから、購入・売却・強化・ページ切替・閉じる操作を判定し、購入・売却・強化は、通貨と射撃へ反映する。閉じている間は、何もしない。
  // input: カーソルの位置・クリック・キーの入力。
  // 戻り値: この更新で、ショップを閉じる操作があったらtrue。
  [[nodiscard]] bool Update(const SHOP_FLOW_INPUT& input);
  // 数字キーに対応する武器へ持ち替える。所持していない武器や、装備中の武器の場合は、何もしない。報酬選択・ショップが閉じていて、Capture中の間だけ呼ぶ。
  // numberKeyIndex: 押された数字キー（1から順）に対応する武器の添字。押されていなければ-1。
  void UpdateWeaponSwitch(int numberKeyIndex);

  //========= HUD=========
  // HUDへ渡す状態のうち、ショップと武器の所持に関する項目を書き込む。他の項目は変えない。
  // hudState: 書き込み先。
  void FillHudState(GAME_HUD_STATE& hudState) const;

  //========= Getter関数=========
  // ショップが開いているかを返す。開いている間は、時間とプレイヤーの操作が止まる。
  [[nodiscard]] bool IsOpen() const { return m_shop.IsOpen(); }

private:
  //========= 補助関数=========
  // この更新の入力から、押されたボタンの添字を求める。Enterは閉じるボタン、Uはページ切替ボタン、数字キーは、現在のページのボタンに対応する。
  // input: キーの入力。
  // 戻り値: 押されたボタンの添字。どれも押されていなければ-1。
  [[nodiscard]] int ResolveKeyChoice(const SHOP_FLOW_INPUT& input) const;
  // 武器の購入を反映する。通貨を支払い、武器を所持に加え、その武器へ持ち替える。どちらかに失敗した場合は、何も変えない。
  void ApplyPurchase();
  // 武器の売却を反映する。武器を所持から外し、売却で得る通貨（強化に使った費用の回収を含む）を通貨へ加える。手放した武器を装備していた場合は、所持している別の武器を装備する。外せなかった場合は、何も変えない。
  void ApplySale();
  // 弾薬の購入を反映する。装備中の武器なら予備弾薬を、装備していない武器なら保存した予備弾薬を、上限内で増やし、通貨を支払う。1発も増やせなかった場合は、何も変えない。
  void ApplyAmmoPurchase();
  // 強化の購入を反映する。装備中の武器の、選ばれた項目の段階を1つ上げ、通貨を支払い、使用中の武器の性能値を強化後の値へ差し替える。装備中の武器が判別できない場合や、上げられなかった場合は、何も変えない。
  void ApplyUpgrade();
  // 装備中の武器の弾薬を保存してから、別の武器へ持ち替える。装備中の武器と同じ種類が指定された場合は、何もしない。
  // weaponIndex: 持ち替える先の武器の種類の値（表の添字）。表の範囲内で、所持していること。
  void SwitchToWeapon(int weaponIndex);
  // 所持している武器を、強化を反映した性能値で装備する。保存した弾薬があれば戻し、なければ、強化後の装弾数で満タンになる。
  // weaponIndex: 装備する武器の種類の値（表の添字）。表の範囲内であること。
  void EquipOwnedWeapon(int weaponIndex);
  // 装備中の武器の種類の値（表の添字）を返す。
  // 戻り値: 武器の種類の値。射撃が未登録、または表にない場合は-1。
  [[nodiscard]] int FindEquippedWeaponIndex() const;

  //========= 参照先=========
  // 支払いと受取りに使う通貨。非所有。未登録はnullptr。
  Wallet* m_wallet{};
  // 持ち替え・装備・弾薬・強化の反映先となる射撃。非所有。未登録はnullptr。
  ShotController* m_shotController{};

  //========= 所有状態=========
  // 所持している武器と、武器ごとの強化・切替で保存した弾薬の記録。購入・売却・強化と、持ち替えで変わる。
  WeaponInventory m_inventory{};
  // 武器と弾薬の購入、武器の売却、武器の強化の画面の状態。
  ShopController m_shop{};
};
