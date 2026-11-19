#pragma once

// 敵の攻撃が当たる瞬間に、プレイヤーが無敵だったか、さらにその回避が攻撃の直前だったか（ジャスト回避）を判定する。
// 攻撃の発生・ダメージの適用・スローの演出は担当しない。呼出側が攻撃の予告を登録し、結果に応じた処理を行う。

// 攻撃が当たる瞬間の判定結果。
enum class DODGE_JUDGE_RESULT
{
  e_NONE,        // 判定の結果がまだない、または攻撃が当たる瞬間ではない。
  e_HIT,         // プレイヤーが無敵でなく、攻撃が命中した。
  e_DODGED,      // 無敵で攻撃を避けたが、回避を始めたのが攻撃の直前ではなかった。
  e_JUST_DODGED  // 無敵で攻撃を避け、回避を始めたのが攻撃の直前だった（ジャスト回避）。
};

// 攻撃の予告から命中の瞬間までを数え、命中の瞬間に判定する。
class JustDodgeJudge final
{
public:
  //========= 初期化・登録関数=========
  // 予告中の攻撃を取り消す。
  void Initialize();
  // 攻撃の予告を登録する。予告中に再び呼ぶと、後の呼出で置き換える。
  // secondsUntilHit: 攻撃が当たるまでの時間。単位は秒。0以下では、次のUpdateで判定する。
  void StartIncomingAttack(float secondsUntilHit);

  //========= 更新関数=========
  // 攻撃が当たるまでの時間を進め、当たる瞬間にだけ判定の結果を返す。
  // deltaTime: この更新で進める経過時間。単位は秒。スローの間は、スローを反映した値を渡す。
  // isInvincible: 当たる瞬間のプレイヤーが無敵か。
  // secondsSinceDodgeStart: 最後に回避を始めてからの経過時間。単位は秒。ジャスト回避の判定に使う。
  // 戻り値: 攻撃が当たる瞬間の判定結果。予告がない、またはまだ当たらない間はe_NONE。
  [[nodiscard]] DODGE_JUDGE_RESULT Update(float deltaTime,bool isInvincible,float secondsSinceDodgeStart);

  //========= Getter関数=========
  // 攻撃を予告中かを返す。
  [[nodiscard]] bool HasIncomingAttack() const { return m_hasIncomingAttack; }
  // 攻撃が当たるまでの残り時間を返す。単位は秒。予告中でなければ0。
  [[nodiscard]] float GetSecondsUntilHit() const { return m_hasIncomingAttack ? m_secondsUntilHit : 0.0f; }

private:
  //========= 予告の状態=========
  // 攻撃を予告中か。
  bool m_hasIncomingAttack{};
  // 攻撃が当たるまでの残り時間。単位は秒。予告中だけ有効。
  float m_secondsUntilHit{};
};
