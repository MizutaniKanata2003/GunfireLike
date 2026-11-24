#include "../H/ShopFlow.h"

// ショップの流れ（購入・売却・強化の反映）と、武器の持ち替え、HUDへ渡す状態の書込みを実装する。

//========= C++標準ライブラリ インクルード=========
#include <string> // 購入・売却・強化・持ち替えをログへ記録する際に、名前を連結する。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

//========= Scene インクルード=========
#include "Scene/Common/Player/H/Wallet.h"
#include "Scene/Common/Weapon/H/ShotController.h"

// 装備中の武器の判別に使う補助関数をまとめる。
namespace
{
  // 装備中の武器の性能値から、武器の種類の値（表の添字）を求める。表示名の文字列の指す先が、表の同じ武器の表示名と一致するかで判別する。
  // 武器の性能値は、表の値をコピーし、強化でも表示名は変わらないため、表示名のポインターも、表のものと同じになる。
  // weaponData: 装備中の武器の性能値。
  // 戻り値: 武器の種類の値。表にない場合は-1。
  int FindWeaponIndex(const WEAPON_DATA& weaponData)
  {
    // 表の全ての武器を先頭から調べ、表示名のポインターが一致する武器の添字を返す。
    for(int weapon_index = 0; weapon_index < WEAPON_TABLE::ns_WEAPON_COUNT; ++weapon_index){
      // 表示名の指す先が同じなら、この武器が装備中の武器。
      if(WEAPON_TABLE::ns_WEAPONS[weapon_index].displayName == weaponData.displayName){
        return weapon_index;
      }
    }

    // どの武器とも一致しない場合は、判別できない。
    return -1;
  }
}

//========= 初期化・終了関数=========
// 武器の所持・強化・保存した弾薬を初期状態へ戻し、ショップを閉じる。
void ShopFlow::Initialize()
{
  m_inventory.Initialize();
  m_shop.Initialize();
}

// 参照先を登録する。
bool ShopFlow::Link(const SHOP_FLOW_LINKS& links)
{
  // 参照先が1つでも欠けている場合は、以後の更新で空の参照を使ってしまうため、何も登録せず失敗を返す。
  if(links.wallet == nullptr || links.shotController == nullptr){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"ShopFlowの参照先の登録に失敗しました。");
    return false;
  }

  m_wallet = links.wallet;
  m_shotController = links.shotController;
  return true;
}

// 参照先を手放す。
void ShopFlow::Unlink()
{
  m_wallet = nullptr;
  m_shotController = nullptr;
}

//========= 更新関数=========
// 開いている間、操作を判定し、購入・売却・強化を反映する。
bool ShopFlow::Update(const SHOP_FLOW_INPUT& input)
{
  // 閉じている間と、参照先が未登録の間は、何もしない。
  if(!m_shop.IsOpen() || m_wallet == nullptr || m_shotController == nullptr)return false;

  // カーソルの位置（基準Canvas上）、左クリック、ボタンに対応するキーを渡す。
  // 購入できるかの判定には、通貨・装備中の武器・武器ごとの予備弾薬を渡す。
  const WeaponSystem& weapon = m_shotController->GetWeaponSystem();
  SHOP_INPUT shop_input{};
  shop_input.cursorCanvasPosition = input.cursorCanvasPosition;
  shop_input.isClickTriggered = input.isClickTriggered;
  shop_input.keyChoiceIndex = ResolveKeyChoice(input);
  SHOP_CONTEXT shop_context{};
  shop_context.balance = m_wallet->GetBalance();
  shop_context.equippedWeaponIndex = FindEquippedWeaponIndex();

  // 予備弾薬は、装備中の武器なら現在の値、装備していない武器なら保存した値を渡す。全ての武器を、先頭から順に設定する。
  for(int weapon_index = 0; weapon_index < WEAPON_TABLE::ns_WEAPON_COUNT; ++weapon_index){
    shop_context.reserveAmmo[weapon_index] = (weapon_index == shop_context.equippedWeaponIndex) ? weapon.GetReserveAmmo()
      : m_inventory.GetSavedAmmo(weapon_index).reserveAmmo;
  }
  const SHOP_UPDATE_RESULT shop_result = m_shop.Update(shop_input,shop_context,m_inventory);

  // 武器の購入・売却、弾薬の購入、強化が求められた場合は、反映する。閉じる操作は、戻り値で呼出側へ伝える。何も求められなかった場合は、何もしない。
  switch(shop_result){
  case SHOP_UPDATE_RESULT::e_PURCHASE_REQUESTED:      ApplyPurchase();     break; // 武器の購入を反映する。
  case SHOP_UPDATE_RESULT::e_SELL_REQUESTED:          ApplySale();         break; // 武器の売却を反映する。
  case SHOP_UPDATE_RESULT::e_AMMO_PURCHASE_REQUESTED: ApplyAmmoPurchase(); break; // 弾薬の購入を反映する。
  case SHOP_UPDATE_RESULT::e_UPGRADE_REQUESTED:       ApplyUpgrade();      break; // 強化の購入を反映する。
  case SHOP_UPDATE_RESULT::e_CLOSED:                  return true; // 閉じる操作があったことを、呼出側へ伝える。
  case SHOP_UPDATE_RESULT::e_NONE:                    break; // 何も求められていない。
  }
  return false;
}

// 数字キーに対応する武器へ持ち替える。
void ShopFlow::UpdateWeaponSwitch(int numberKeyIndex)
{
  // 参照先が未登録の場合と、キーが押されていない・範囲外の場合は、持ち替える武器が決まらないため、何もしない。
  if(m_shotController == nullptr || numberKeyIndex < 0 || numberKeyIndex >= WEAPON_TABLE::ns_WEAPON_COUNT)return;

  // 所持していない武器へは、持ち替えない。
  if(!m_inventory.IsOwned(static_cast<WEAPON_TYPE>(numberKeyIndex)))return;

  // 装備中の武器と同じ場合は、持ち替えない。
  if(FindEquippedWeaponIndex() == numberKeyIndex)return;

  SwitchToWeapon(numberKeyIndex);
  Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,std::wstring{L"武器を持ち替えました: "} + SHOP_TABLE::ns_WEAPON_NAMES[numberKeyIndex]);
}

//========= HUD=========
// HUDへ渡す状態のうち、ショップと武器の所持に関する項目を書き込む。
void ShopFlow::FillHudState(GAME_HUD_STATE& hudState) const
{
  // 装備中の武器の種類を求め、強化の段階の取得に使う。
  const int equipped_weapon_index = FindEquippedWeaponIndex();

  // ショップの値を、HUDのショップの状態（shop）へまとめて書き込む。
  HUD_SHOP_STATE& shop = hudState.shop;
  shop.isOpen = m_shop.IsOpen();
  shop.page = m_shop.GetPage();
  shop.hoveredIndex = m_shop.GetHoveredIndex();
  shop.rowStates = m_shop.GetRowStates();
  shop.sellPrices = m_shop.GetSellPrices();
  shop.ammoStates = m_shop.GetAmmoStates();
  shop.upgradeStates = m_shop.GetUpgradeStates();
  shop.upgradeLevels = m_inventory.GetUpgradeLevels(equipped_weapon_index);
  shop.ownedWeaponCount = m_inventory.GetCount();
  shop.equippedWeaponIndex = equipped_weapon_index;
}

//========= 補助関数=========
// 押されたキーに対応するボタンの添字を求める。
int ShopFlow::ResolveKeyChoice(const SHOP_FLOW_INPUT& input) const
{
  // Enterは、閉じるボタンの代わりにする。数字キーより優先する。
  if(input.isCloseKeyTriggered)return SHOP_TABLE::ns_CLOSE_INDEX;

  // Uは、ページ切替ボタンの代わりにする。数字キーより優先する。
  if(input.isSwitchKeyTriggered)return SHOP_TABLE::ns_SWITCH_INDEX;

  // 数字キーが押されていない場合は、対応するボタンがない。
  if(input.numberKeyIndex < 0)return -1;

  // 強化のページは、強化の項目の数まで、数字キーを強化のボタンの添字へ変換する。範囲外のキーは、対応するボタンがない。
  if(m_shop.GetPage() == SHOP_PAGE::e_UPGRADE){
    return input.numberKeyIndex < SHOP_TABLE::ns_UPGRADE_COUNT ? SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET + input.numberKeyIndex : -1;
  }

  // 売買のページは、武器と弾薬のボタンの数（強化のボタンの手前まで）まで、数字キーをそのままボタンの添字にする。
  return input.numberKeyIndex < SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET ? input.numberKeyIndex : -1;
}

// 武器の購入を反映する。
void ShopFlow::ApplyPurchase()
{
  // 購入する武器と価格を、ショップの表から取り出す。行の添字は、武器の種類の値と同じ。
  const int row_index = m_shop.GetLastPurchaseIndex();
  const WEAPON_TYPE weapon_type = static_cast<WEAPON_TYPE>(row_index);
  const int price = SHOP_TABLE::ns_WEAPON_PRICES[row_index];

  // 通貨を支払う。足りない場合は、何も変えずに戻る。
  if(!m_wallet->TrySpend(price))return;

  // 武器を所持に加える。加えられなかった場合は、支払った通貨を戻して、何も変えない。
  if(!m_inventory.TryAdd(weapon_type)){
    m_wallet->Add(price);
    return;
  }

  // 購入した武器へ持ち替える。持ち替える前の武器の弾薬は、保存される。購入した武器の弾薬は、その武器の初期値になる。
  SwitchToWeapon(row_index);
  Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,std::wstring{L"武器を購入しました: "} + SHOP_TABLE::ns_WEAPON_NAMES[row_index]);
}

// 武器の売却を反映する。
void ShopFlow::ApplySale()
{
  // 売却する武器を、ショップの表から取り出す。行の添字は、武器の種類の値と同じ。
  const int row_index = m_shop.GetLastPurchaseIndex();
  const WEAPON_TYPE weapon_type = static_cast<WEAPON_TYPE>(row_index);

  // 売却する武器を装備中かと、売却で得る通貨（強化に使った費用の回収を含む）を、手放す前に調べる。
  const bool was_equipped = FindEquippedWeaponIndex() == row_index;
  const int sell_price = SHOP_TABLE::GetSellPrice(row_index,m_inventory.GetUpgradeSpent(row_index));

  // 武器を手放す。外せなかった場合（最後の1丁など）は、何も変えずに戻る。
  if(!m_inventory.TryRemove(weapon_type))return;

  // 売却で得る通貨を、加える。
  m_wallet->Add(sell_price);

  // 装備中の武器を手放した場合は、所持している別の武器（表の並びで最も先）を、代わりに装備する。保存した弾薬があれば戻る。
  // 所持の下限があるため、必ず1丁は残る。
  if(was_equipped){
    const int next_weapon_index = m_inventory.FindFirstOwnedIndex();

    // 代わりの武器が見つかった場合だけ、装備する。
    if(next_weapon_index >= 0){
      EquipOwnedWeapon(next_weapon_index);
    }
  }
  Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,std::wstring{L"武器を売却しました: "} + SHOP_TABLE::ns_WEAPON_NAMES[row_index]);
}

// 弾薬の購入を反映する。
void ShopFlow::ApplyAmmoPurchase()
{
  // 購入する弾薬の行と、その価格・1回で増える弾数・上限を、ショップの表から取り出す。行の添字は、武器の種類の値と同じ。
  const int row_index = m_shop.GetLastPurchaseIndex();
  const int price = SHOP_TABLE::ns_AMMO_PRICES[row_index];
  const int pack_size = SHOP_TABLE::ns_AMMO_PACK_SIZES[row_index];
  const int reserve_cap = SHOP_TABLE::ns_AMMO_RESERVE_CAPS[row_index];

  // 通貨を支払う。足りない場合は、何も変えずに戻る。
  if(!m_wallet->TrySpend(price))return;

  // 装備中の武器なら、使用中の予備弾薬を、装備していない武器なら、保存した予備弾薬を、上限内で増やす。
  const bool is_equipped = FindEquippedWeaponIndex() == row_index;
  const int added_ammo = is_equipped ? m_shotController->AddReserveAmmo(pack_size,reserve_cap)
    : m_inventory.AddSavedReserve(row_index,pack_size,reserve_cap);

  // 1発も増やせなかった場合は、支払った通貨を戻して、何も変えない。
  if(added_ammo <= 0){
    m_wallet->Add(price);
    return;
  }
  Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,
                std::wstring{L"弾薬を購入しました: "} + SHOP_TABLE::ns_WEAPON_NAMES[row_index] + L" +" + std::to_wstring(added_ammo));
}

// 強化の購入を反映する。
void ShopFlow::ApplyUpgrade()
{
  // 強化する項目と、強化の対象（装備中の武器）を求める。装備中の武器が判別できない場合は、何も変えずに戻る。
  const int item_index = m_shop.GetLastPurchaseIndex();
  const int weapon_index = FindEquippedWeaponIndex();
  if(weapon_index < 0)return;

  // 次の段階へ上げる価格を求め、通貨を支払う。足りない場合は、何も変えずに戻る。
  const int price = WEAPON_UPGRADE_TABLE::GetPrice(item_index,m_inventory.GetUpgradeLevel(weapon_index,item_index));
  if(!m_wallet->TrySpend(price))return;

  // 段階を1つ上げる。上げられなかった場合（上限など）は、支払った通貨を戻して、何も変えない。
  if(!m_inventory.TryUpgrade(static_cast<WEAPON_TYPE>(weapon_index),item_index,price)){
    m_wallet->Add(price);
    return;
  }

  // 使用中の武器の性能値を、強化後の値へ差し替える。弾薬とリロードの進行は、引き継ぐ。
  m_shotController->ApplyUpgradedData(BuildUpgradedWeaponData(WEAPON_TABLE::ns_WEAPONS[weapon_index],m_inventory.GetUpgradeLevels(weapon_index)));
  Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,
                std::wstring{L"武器を強化しました: "} + SHOP_TABLE::ns_WEAPON_NAMES[weapon_index] + L" " + WEAPON_UPGRADE_TABLE::ns_ITEM_NAMES[item_index]);
}

// 装備中の武器の弾薬を保存してから、別の武器へ持ち替える。
void ShopFlow::SwitchToWeapon(int weaponIndex)
{
  // 装備中の武器を求める。同じ武器への持ち替えは、弾薬の保存と復元が不要なため、何もしない。
  const int current_weapon_index = FindEquippedWeaponIndex();
  if(current_weapon_index == weaponIndex)return;

  // 持ち替える前の武器が判別できる場合だけ、その弾薬を保存する。同じ武器へ戻った時に、この弾薬が戻る。
  if(current_weapon_index >= 0){
    const WeaponSystem& weapon = m_shotController->GetWeaponSystem();
    m_inventory.SaveAmmo(current_weapon_index,weapon.GetMagazineAmmo(),weapon.GetReserveAmmo());
  }
  EquipOwnedWeapon(weaponIndex);
}

// 所持している武器を、強化を反映した性能値で装備する。
void ShopFlow::EquipOwnedWeapon(int weaponIndex)
{
  // 表の基本の性能値へ、その武器の強化の段階を反映した値で、装備する。装備すると、弾薬は満タンの初期値になる。
  m_shotController->EquipWeapon(static_cast<WEAPON_TYPE>(weaponIndex),
                                BuildUpgradedWeaponData(WEAPON_TABLE::ns_WEAPONS[weaponIndex],m_inventory.GetUpgradeLevels(weaponIndex)));

  // 保存した弾薬がある場合は、持ち替える前の弾薬へ戻す。
  if(m_inventory.HasSavedAmmo(weaponIndex)){
    const WEAPON_AMMO_RECORD saved_ammo = m_inventory.GetSavedAmmo(weaponIndex);
    m_shotController->SetAmmo(saved_ammo.magazineAmmo,saved_ammo.reserveAmmo);
  }
}

// 装備中の武器の種類の値を返す。
int ShopFlow::FindEquippedWeaponIndex() const
{
  // 射撃が未登録の場合は、判別できない。
  if(m_shotController == nullptr)return -1;

  return FindWeaponIndex(m_shotController->GetWeaponSystem().GetWeaponData());
}