#pragma once

// 部屋のクリア後に、報酬のカードを選ぶ状態（報酬選択）へ入り、カードを選ぶと抜ける流れと、報酬（特殊強化）の取得数を管理する。
// カードの当たりとホバーはUiButtonで判定する。時間の停止・入力の切替・画面の表示は、呼出側が行う。
// 取得数から、強化の効果の大きさ（制圧射撃の怯み時間の倍率、回避反撃と殴打追撃のダメージの倍率）を返す。効果を使う側へ値を渡すのは、呼出側が行う。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、カードごとのボタンと取得数を固定長で保持する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Stage/H/RoomController.h" // 部屋の進行状態（ROOM_PHASE）から、部屋のクリアの瞬間を検出するために使用する。
#include "Scene/Common/UI/H/UiButton.h"
#include "Scene/Common/UI/H/UiLayout.h"

// 報酬の表をまとめる。カードの見出しは、HUDのFontで描ける英字とし、説明は日本語にする。説明に使う文字は、Fontへ含めること（Tools/MakeGunfireFont.bat）。文言と数値は仮で、プレイ確認後に調整する。
namespace REWARD_TABLE
{
  //========= 報酬の数と種類=========
  // 報酬の種類の数。画面に並べるカードの数と同じで、毎回、全ての種類を並べる。
  inline constexpr int ns_CARD_COUNT = 3;
  // 報酬の種類。値は、報酬の表・取得数・カードの並びの添字と同じ。
  enum class REWARD_TYPE
  {
    e_SUPPRESSIVE_FIRE, // 制圧射撃。射撃で敵に与える怯みの時間を伸ばす。
    e_COUNTER_DODGE,    // 回避反撃。ジャスト回避の後、一定時間、射撃のダメージを増やす。
    e_FOLLOW_UP_STRIKE  // 殴打追撃。殴打を当てた敵が、一定時間、射撃で受けるダメージを増やす。
  };

  //========= レベル=========
  // 各報酬のレベルの上限。企画書では上限は未確定のため、仮値。上限に達した報酬を選んだ場合は、強化の代わりに通貨を得る。
  inline constexpr int ns_MAX_LEVEL = 3;
  // 上限に達した報酬を選んだ場合に得る通貨。企画書の「全種類最大なら通貨報酬に置換」に当たる仮値。
  inline constexpr int ns_CAPPED_CURRENCY_REWARD = 50;

  //========= 制圧射撃の効果=========
  // 制圧射撃のレベルごとの、射撃で敵に与える怯み時間の倍率。添字はレベル（0が未取得）。企画書の「怯み蓄積増加」を、現在の敵の仕組み（被弾で一定時間怯む）に合わせた仮の効果。
  inline constexpr float ns_SUPPRESSIVE_FLINCH_SCALES[ns_MAX_LEVEL + 1]{1.0f,1.5f,2.0f,2.5f};

  //========= 回避反撃の効果=========
  // 回避反撃のレベルごとの、ジャスト回避の後の射撃ダメージの倍率。添字はレベル（0が未取得）。企画書の「ジャスト回避後、一定時間の射撃ダメージ増加」に当たる仮値。
  inline constexpr float ns_COUNTER_DODGE_DAMAGE_SCALES[ns_MAX_LEVEL + 1]{1.0f,1.3f,1.6f,2.0f};
  // ジャスト回避の後に、射撃ダメージが増える時間。単位は秒。ジャスト回避のスローの影響を受けない、プレイヤー側の時間で数える。企画書で未確定の仮値。
  inline constexpr float ns_COUNTER_DODGE_SECONDS = 4.0f;

  //========= 殴打追撃の効果=========
  // 殴打追撃のレベルごとの、印が付いた敵が射撃で受けるダメージの倍率。添字はレベル（0が未取得）。企画書の「殴打命中敵が一定時間、射撃で受けるダメージ増加」に当たる仮値。
  inline constexpr float ns_FOLLOW_UP_DAMAGE_SCALES[ns_MAX_LEVEL + 1]{1.0f,1.25f,1.5f,2.0f};
  // 殴打を当てた敵に、印が続く時間。単位は秒。敵の側の時間（スローの影響を受ける時間）で数える。企画書で未確定の仮値。
  inline constexpr float ns_FOLLOW_UP_SECONDS = 5.0f;

  //========= 報酬の定義=========
  // 報酬1種類分の表示内容。
  struct REWARD_DEFINITION
  {
    // カードの見出し。英字だけ。静的な文字列定数を指し、所有しない。
    const wchar_t* title;
    // カードの説明。日本語。改行（\n）で区切った複数行で、1行は、カードの幅に収まる、全角11文字以内にする。静的な文字列定数を指し、所有しない。
    const wchar_t* description;
  };

  // 報酬の表。並びは、REWARD_TYPEと同じ。
  inline constexpr REWARD_DEFINITION ns_REWARDS[ns_CARD_COUNT]{
    {L"SUPPRESSIVE FIRE",L"射撃で当てた敵の\n怯み時間が\n長くなる。"},
    {L"COUNTER DODGE",L"ジャスト回避の後、\n一定時間、射撃の\nダメージが増える。"},
    {L"FOLLOW-UP STRIKE",L"殴打を当てた敵が\n射撃で受ける\nダメージが増える。"}
  };
}

// 報酬選択の更新で起きた出来事。
enum class REWARD_UPDATE_RESULT
{
  e_NONE,    // 状態の変化はない。
  e_STARTED, // この更新で、報酬選択へ入った。
  e_FINISHED // この更新で、カードが選ばれて、報酬選択を抜けた。選ばれたカードは、GetLastChosenIndexで取得する。
};

// 部屋のクリアで報酬選択へ入り、カードを選ぶと抜ける状態と、報酬の取得数を持つ。
class RewardController final
{
public:
  //========= 初期化関数=========
  // 報酬の取得数を0へ戻し、カードのボタンの矩形を配置表から設定して、報酬選択を抜ける。新しいプレイの開始の時に呼ぶ。
  void Initialize()
  {
    m_acquiredCounts.fill(0);
    m_lastChosenIndex = -1;
    m_wasLastChoiceCapped = false;

    // 配置表のカードの位置と大きさを、ボタンへ設定する。
    for(int card_index = 0; card_index < REWARD_TABLE::ns_CARD_COUNT; ++card_index){
      m_buttons[card_index].Initialize(REWARD_LAYOUT::ns_CARD_POSITIONS[card_index],REWARD_LAYOUT::ns_CARD_SIZE);
    }
    Reset();
  }
  // 報酬選択を抜け、部屋の進行状態の記録を、初期の状態へ戻す。報酬の取得数は変えない。ステージを最初から再開する時に呼ぶ。
  void Reset()
  {
    m_isSelecting = false;
    m_previousPhase = ROOM_PHASE::e_NEEDS_SPAWN;
    m_isInputGuardActive = false;
    m_isGuardFirstFrame = false;
    ClearHover();
  }

  //========= 更新関数=========
  // 部屋の進行状態とカーソルを見て、報酬選択へ入る・抜ける判定を行う。毎回の更新で、部屋の進行の更新の後に呼ぶ。
  // 部屋が、戦闘中から出口の開放（部屋の全滅）へ変わった更新で、報酬選択へ入る。選択中は、カードがクリックされた、または、カードの番号のキーが押された更新で抜ける。
  // currentPhase: 現在の部屋の進行状態。
  // cursorCanvasPosition: カーソルの位置。基準Canvas上の座標。カードのホバーとクリックの判定に使う。
  // isClickTriggered: クリックが、この更新で押されたか。報酬選択へ入った更新に押されていても、無視する。
  // keyChoiceIndex: この更新で押された、カードの番号のキーに対応するカードの添字。押されていなければ-1。範囲外の値は無視する。
  // 戻り値: この更新で起きた出来事。
  [[nodiscard]] REWARD_UPDATE_RESULT Update(ROOM_PHASE currentPhase,const XMFLOAT2& cursorCanvasPosition,bool isClickTriggered,int keyChoiceIndex)
  {
    // 前回の進行状態を取り出し、今回の状態を、次の更新のために記録する。
    const ROOM_PHASE previous_phase = m_previousPhase;
    m_previousPhase = currentPhase;

    // 選択中は、カードのホバーとクリックを更新し、選ばれたカードがあれば、報酬選択を抜ける。
    if(m_isSelecting)return UpdateSelection(cursorCanvasPosition,isClickTriggered,keyChoiceIndex);

    // 出口の開放へ変わった更新だけ、報酬選択へ入る。出口が開いたままの間は、繰り返し入らない。
    if(previous_phase != ROOM_PHASE::e_EXIT_OPEN && currentPhase == ROOM_PHASE::e_EXIT_OPEN){
      m_isSelecting = true;
      return REWARD_UPDATE_RESULT::e_STARTED;
    }
    return REWARD_UPDATE_RESULT::e_NONE;
  }
  // 報酬を選んだ操作（クリックや数字キー）が、ゲームの操作（射撃や武器の切替）として重複して反応しないよう、選び終えた直後の入力を止めるかを判定する。
  // カードを選んだ更新は、押されたキーとクリックを使わせないため、止める。その後は、左クリックが離されるまで止め、離された更新で解除する。
  // isLeftButtonPressed: 左クリックが、押された状態か。
  // 戻り値: ゲームの操作の更新を止めるべきならtrue。選び終えた直後でなければfalse。
  [[nodiscard]] bool UpdateInputGuard(bool isLeftButtonPressed)
  {
    // 選び終えた直後でなければ、止めない。
    if(!m_isInputGuardActive)return false;

    // 選び終えた更新は、押されたキーを、他の操作へ使わせないため止める。
    if(m_isGuardFirstFrame){
      m_isGuardFirstFrame = false;
      return true;
    }

    // 左クリックが押されている間は、選んだクリックが射撃として反応しないよう止め、離された更新で解除する。
    if(isLeftButtonPressed)return true;
    m_isInputGuardActive = false;
    return false;
  }

  //========= Getter関数=========
  // 報酬選択の最中かを返す。最中は、時間とプレイヤーの操作が止まる。
  [[nodiscard]] bool IsSelecting() const { return m_isSelecting; }
  // カーソルが乗っているカードの添字を返す。どのカードにも乗っていなければ-1。
  [[nodiscard]] int GetHoveredIndex() const { return m_hoveredIndex; }
  // 直近に選ばれたカードの添字を返す。まだ選ばれていなければ-1。
  [[nodiscard]] int GetLastChosenIndex() const { return m_lastChosenIndex; }
  // 直近に選ばれたカードが、すでに上限に達していて、強化として取得されなかったかを返す。trueなら、呼出側が、代わりの通貨を与える。
  [[nodiscard]] bool WasLastChoiceCapped() const { return m_wasLastChoiceCapped; }
  // 報酬ごとの取得数（レベル）を返す。添字は、報酬の表の並びと同じ。戻り値の参照は、このRewardControllerの寿命を超えて保持しない。
  [[nodiscard]] const std::array<int,REWARD_TABLE::ns_CARD_COUNT>& GetAcquiredCounts() const { return m_acquiredCounts; }
  // 制圧射撃の現在のレベルに応じた、射撃で敵に与える怯み時間の倍率を返す。未取得なら1.0。
  [[nodiscard]] float GetSuppressiveFlinchScale() const
  {
    return REWARD_TABLE::ns_SUPPRESSIVE_FLINCH_SCALES[GetLevel(REWARD_TABLE::REWARD_TYPE::e_SUPPRESSIVE_FIRE)];
  }
  // 回避反撃の現在のレベルに応じた、ジャスト回避の後の射撃ダメージの倍率を返す。未取得なら1.0（増えない）。
  [[nodiscard]] float GetCounterDodgeDamageScale() const
  {
    return REWARD_TABLE::ns_COUNTER_DODGE_DAMAGE_SCALES[GetLevel(REWARD_TABLE::REWARD_TYPE::e_COUNTER_DODGE)];
  }
  // 殴打追撃の現在のレベルに応じた、印が付いた敵が射撃で受けるダメージの倍率を返す。未取得なら1.0（印を付けない）。
  [[nodiscard]] float GetFollowUpDamageScale() const
  {
    return REWARD_TABLE::ns_FOLLOW_UP_DAMAGE_SCALES[GetLevel(REWARD_TABLE::REWARD_TYPE::e_FOLLOW_UP_STRIKE)];
  }

private:
  //========= 補助関数=========
  // 報酬の現在のレベル（取得数）を返す。0以上で、上限以下。
  // rewardType: 調べる報酬の種類。
  [[nodiscard]] int GetLevel(REWARD_TABLE::REWARD_TYPE rewardType) const { return m_acquiredCounts[static_cast<int>(rewardType)]; }
  // カードのホバーとクリックを更新し、選ばれたカードがあれば、取得数を増やして報酬選択を抜ける。
  // 引数と戻り値は、Updateと同じ。
  [[nodiscard]] REWARD_UPDATE_RESULT UpdateSelection(const XMFLOAT2& cursorCanvasPosition,bool isClickTriggered,int keyChoiceIndex)
  {
    // 範囲内のキーの選択があれば、それを選ばれたカードとして始める。
    int chosen_index = (keyChoiceIndex >= 0 && keyChoiceIndex < REWARD_TABLE::ns_CARD_COUNT) ? keyChoiceIndex : -1;

    // 全てのカードのホバーを更新する。複数のカードがクリックされた場合や、キーとクリックが同時の場合は、先に決まった選択を残す。
    m_hoveredIndex = -1;
    for(int card_index = 0; card_index < REWARD_TABLE::ns_CARD_COUNT; ++card_index){
      const bool is_clicked = m_buttons[card_index].Update(cursorCanvasPosition,isClickTriggered);
      if(m_buttons[card_index].IsHovered()){
        m_hoveredIndex = card_index;
      }
      if(is_clicked && chosen_index < 0){
        chosen_index = card_index;
      }
    }

    // 選ばれたカードがなければ、選択を続ける。
    if(chosen_index < 0)return REWARD_UPDATE_RESULT::e_NONE;

    // 上限に達した報酬は、取得数を増やさず、呼出側が通貨へ置き換えられるよう記録する。上限未満なら、取得数を増やす。取得数は、上限を超えない。
    m_wasLastChoiceCapped = m_acquiredCounts[chosen_index] >= REWARD_TABLE::ns_MAX_LEVEL;
    if(!m_wasLastChoiceCapped){
      ++m_acquiredCounts[chosen_index];
    }

    // 選択を終え、選び終えた直後の入力を止める状態にする。
    m_lastChosenIndex = chosen_index;
    m_isSelecting = false;
    m_isInputGuardActive = true;
    m_isGuardFirstFrame = true;
    ClearHover();
    return REWARD_UPDATE_RESULT::e_FINISHED;
  }
  // 全てのカードのホバーを解除する。
  void ClearHover()
  {
    m_hoveredIndex = -1;
    for(UiButton& button : m_buttons){
      button.ClearHover();
    }
  }

  //========= 状態=========
  // 報酬選択の最中か。
  bool m_isSelecting{};
  // 前回の更新での部屋の進行状態。部屋のクリアの瞬間を検出するために使う。
  ROOM_PHASE m_previousPhase{ROOM_PHASE::e_NEEDS_SPAWN};

  //========= 選び終えた直後の入力の保護=========
  // 選び終えた直後の入力を止めている最中か。左クリックが離されると解除する。
  bool m_isInputGuardActive{};
  // 選び終えた更新を、まだ止めていないか。最初の確認で、1度だけtrueを返すために使う。
  bool m_isGuardFirstFrame{};

  //========= カード=========
  // カードごとのボタン。並びは、報酬の表の並びと同じ。
  std::array<UiButton,REWARD_TABLE::ns_CARD_COUNT> m_buttons{};
  // カーソルが乗っているカードの添字。乗っていなければ-1。
  int m_hoveredIndex{-1};
  // 直近に選ばれたカードの添字。まだなければ-1。
  int m_lastChosenIndex{-1};
  // 直近に選ばれたカードが、上限に達していて、取得されなかったか。
  bool m_wasLastChoiceCapped{};

  //========= 取得数=========
  // 報酬ごとの取得数（レベル）。添字は、報酬の表の並びと同じ。0以上で、ns_MAX_LEVEL以下。
  std::array<int,REWARD_TABLE::ns_CARD_COUNT> m_acquiredCounts{};
};

// 配置表のカードの数と、報酬の表の数が、一致していることを確認する。
static_assert(REWARD_TABLE::ns_CARD_COUNT == REWARD_LAYOUT::ns_CARD_COUNT,"報酬の表の数と、カードの配置の数を一致させること。");