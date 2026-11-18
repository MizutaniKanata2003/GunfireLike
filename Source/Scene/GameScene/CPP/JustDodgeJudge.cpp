#include "../H/JustDodgeJudge.h"

// 攻撃の予告の登録と、命中の瞬間の判定を実装する。
// ジャスト回避とみなす時間は、企画書で未確定のため、このファイルの仮値にまとめた。

// 判定の閾値をまとめる。
namespace
{
  //========= ジャスト回避の判定=========
  // 回避を始めてから攻撃が当たるまでの時間が、この値以下ならジャスト回避とする。単位は秒。
  // 企画書では「攻撃直前の回避成功」とだけ定められており、具体値は未確定の仮値。
  constexpr float ns_JUST_DODGE_WINDOW_SECONDS = 0.3f;
}

//========= 初期化・登録関数=========
// 予告を取り消す。
void JustDodgeJudge::Initialize()
{
  m_hasIncomingAttack = false;
  m_secondsUntilHit = 0.0f;
}

// 攻撃の予告を登録する。
void JustDodgeJudge::StartIncomingAttack(float secondsUntilHit)
{
  m_hasIncomingAttack = true;
  m_secondsUntilHit = secondsUntilHit;
}

//========= 更新関数=========
// 時間を進め、当たる瞬間に、無敵かどうかと回避の開始からの経過時間で判定する。
DODGE_JUDGE_RESULT JustDodgeJudge::Update(float deltaTime,bool isInvincible,float secondsSinceDodgeStart)
{
  // 予告がなければ、判定する攻撃がないため何もしない。
  if(!m_hasIncomingAttack)return DODGE_JUDGE_RESULT::e_NONE;

  // 当たるまでの残り時間を進める。
  m_secondsUntilHit -= deltaTime;
  // まだ残り時間が0を超えている間は、当たる瞬間ではないため、結果を返さない。
  if(m_secondsUntilHit > 0.0f)return DODGE_JUDGE_RESULT::e_NONE;

  // 当たる瞬間のため、予告を終える。以後の判定は、この結果で行う。
  m_hasIncomingAttack = false;

  // 無敵でなければ、攻撃が命中する。
  if(!isInvincible)return DODGE_JUDGE_RESULT::e_HIT;

  // 無敵で、回避を始めてからの時間がジャスト回避の時間以内なら、ジャスト回避。そうでなければ、通常の回避成功。
  return secondsSinceDodgeStart <= ns_JUST_DODGE_WINDOW_SECONDS ? DODGE_JUDGE_RESULT::e_JUST_DODGED : DODGE_JUDGE_RESULT::e_DODGED;
}
