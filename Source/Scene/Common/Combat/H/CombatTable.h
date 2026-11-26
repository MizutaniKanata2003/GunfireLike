#pragma once

// 戦闘に関する数値（近接攻撃・被弾・回復・確認用の攻撃・スロー）を1か所へまとめる。
// 数値の定義だけを持ち、判定や状態は担当しない。単位は、特に記載がない限り、ワールド距離単位と秒。

// 戦闘に使う数値をまとめる。
namespace COMBAT_TABLE
{
  //========= プレイヤーのHP=========
  // プレイヤーの最大HP。
  inline constexpr float ns_PLAYER_MAX_HP = 100.0f;

  //========= 近接攻撃=========
  // 殴打の1回あたりのダメージ。
  inline constexpr float ns_PUNCH_DAMAGE = 20.0f;
  // 近接攻撃が届く距離。視点から敵の中心までの距離で測る。
  inline constexpr float ns_MELEE_REACH = 2.5f;
  // 近接攻撃が当たる範囲を決める、視線方向と敵の方向のなす角の余弦の下限。0.5は約60度以内。
  inline constexpr float ns_MELEE_MIN_FORWARD_DOT = 0.5f;

  //========= 接近型の攻撃=========
  // 予告開始と命中に使う水平中心間距離。追従の停止距離1.5より広くする。
  inline constexpr float ns_ENEMY_MELEE_REACH = 1.65f;
  // 予告開始から命中判定までの時間。敵側の時間で1.0秒。攻撃の動き（Attack、2.63秒）の、腕を振る瞬間に合わせる。
  inline constexpr float ns_ENEMY_MELEE_WINDUP_SECONDS = 1.0f;
  // 命中判定・空振り・予告中断の後、次の予告を始められるまでの時間。敵側の時間で2.0秒。攻撃の動きが終わってから、次の予告に入る長さ。
  inline constexpr float ns_ENEMY_MELEE_RECOVERY_SECONDS = 2.0f;
  // 接近型の攻撃が命中したときに減らすプレイヤーHP。
  inline constexpr float ns_ENEMY_MELEE_DAMAGE = 10.0f;

  //========= ヒットストップと怯み=========
  // 近接攻撃が当たった時の、ヒットストップの長さ。単位は秒。
  inline constexpr float ns_MELEE_HIT_STOP_SECONDS = 0.05f;
  // 攻撃による被弾の時の、ヒットストップの長さ。単位は秒。
  inline constexpr float ns_DAMAGE_HIT_STOP_SECONDS = 0.08f;
  // 敵への接触の時の、ヒットストップの長さ。単位は秒。
  inline constexpr float ns_CONTACT_HIT_STOP_SECONDS = 0.05f;
  // 被弾・接触の時の、プレイヤーの怯みの長さ。単位は秒。企画書では「短い怯み」とだけ定められている。
  inline constexpr float ns_PLAYER_FLINCH_SECONDS = 0.3f;
  // 敵への接触の反応が、連続して起きないようにする間隔。単位は秒。
  inline constexpr float ns_CONTACT_COOLDOWN_SECONDS = 0.6f;

  //========= 回復アイテム=========
  // 回復アイテム1個で回復するHP。
  inline constexpr float ns_HEAL_AMOUNT = 30.0f;
  // 回復アイテムを連続して使える間隔。単位は秒。
  inline constexpr float ns_HEAL_COOLDOWN_SECONDS = 0.5f;
  // 開始時に持つ回復アイテムの数。
  inline constexpr int ns_HEAL_INITIAL_COUNT = 2;
  // 所持できる回復アイテムの上限。
  inline constexpr int ns_HEAL_MAX_COUNT = 5;

  //========= 確認用の攻撃とスロー=========
  // F7で発生させる確認用の攻撃が、予告から当たるまでの時間。単位は秒。回避の入力を間に合わせられる長さ。
  inline constexpr float ns_SIMULATED_ATTACK_DELAY_SECONDS = 0.8f;
  // 確認用の攻撃が命中した場合に、プレイヤーへ与えるダメージ。
  inline constexpr float ns_SIMULATED_ATTACK_DAMAGE = 10.0f;
  // F5で与える確認用のダメージ量。HPバーと低HP警告の表示確認に使う。
  inline constexpr float ns_DEBUG_DAMAGE = 10.0f;
  // ジャスト回避で、スローを続ける時間。単位は実時間の秒。
  inline constexpr float ns_SLOW_MOTION_SECONDS = 1.5f;
  // スロー中の、プレイヤー以外の時間の進む速さ。1が通常で、小さいほど遅い。
  inline constexpr float ns_SLOW_MOTION_SCALE = 0.3f;
}