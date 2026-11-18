#pragma once

// 回復アイテムの所持数と、連続して使う間隔を管理する。使った効果（HPの回復）は担当せず、使えたかどうかだけを返す。
// アイテムの入手・購入・ドロップは担当しない。所持数は0以上で上限以下に保つ。

// 回復アイテムの所持状態。
class HealItems final
{
public:
  //========= 初期化関数=========
  // 所持数と上限を設定し、連続使用の待ち時間を解除する。
  // initialCount: 開始時の所持数。上限を超える場合は上限へ丸める。
  // maxCount: 所持できる上限。
  void Initialize(int initialCount,int maxCount)
  {
    m_maxCount = maxCount;
    m_count = initialCount < maxCount ? initialCount : maxCount;
    m_cooldownRemainingSeconds = 0.0f;
  }

  //========= 更新関数=========
  // 連続使用の待ち時間を進める。
  // deltaTime: この更新で進める経過時間。単位は秒。
  void Update(float deltaTime)
  {
    // 待ち時間が残っている場合だけ減らし、0未満にならないようにする。
    if(m_cooldownRemainingSeconds > 0.0f){
      m_cooldownRemainingSeconds -= deltaTime;
      // 残り時間が0以下になったら、0にそろえる。
      if(m_cooldownRemainingSeconds < 0.0f){
        m_cooldownRemainingSeconds = 0.0f;
      }
    }
  }
  // 1個使う。所持数がない、または連続使用の待ち時間が残っている場合は使えない。
  // cooldownSeconds: 使えた場合に設定する、次に使えるまでの待ち時間。単位は秒。
  // 戻り値: 使えたらtrue。使えなかった場合は、所持数も待ち時間も変えずfalse。
  bool TryUse(float cooldownSeconds)
  {
    // 所持数がない、または待ち時間が残っている場合は、使えない。
    if(m_count <= 0 || m_cooldownRemainingSeconds > 0.0f)return false;

    // 1個減らし、次に使えるまでの待ち時間を設定する。
    --m_count;
    m_cooldownRemainingSeconds = cooldownSeconds;
    return true;
  }
  // 所持数を増やす。上限を超えない。
  // count: 増やす個数。正の値。負の値では何もしない。
  void Add(int count)
  {
    // 負の個数は所持数を減らしてしまうため、何もしない。
    if(count < 0)return;

    // 個数を足す。
    m_count += count;
    // 足した結果が上限を超えたら、上限へ丸める。
    if(m_count > m_maxCount){
      m_count = m_maxCount;
    }
  }

  //========= Getter関数=========
  // 現在の所持数を返す。
  [[nodiscard]] int GetCount() const { return m_count; }
  // 所持できる上限を返す。
  [[nodiscard]] int GetMaxCount() const { return m_maxCount; }

private:
  //========= 所持状態=========
  // 現在の所持数。0以上で上限以下。
  int m_count{};
  // 所持できる上限。
  int m_maxCount{};
  // 次に使えるまでの残り時間。単位は秒。0なら使える。
  float m_cooldownRemainingSeconds{};
};
