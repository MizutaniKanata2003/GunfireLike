#include "../H/CombatResolver.h"

// 近接攻撃の射程と範囲の判定を実装する。

//========= C++標準ライブラリ インクルード=========
#include <cmath> // std::sqrtで、視点から敵の中心までの距離を求める。

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/CombatTable.h"
#include "Scene/Common/Enemy/H/Enemy.h"

//========= 判定関数=========
// 敵の中心への向きと距離を求め、射程と、視線方向との角度の範囲で判定する。
bool CombatResolver::IsEnemyInMeleeReach(const XMFLOAT3& eyePosition,const XMFLOAT3& forward,const Enemy& enemy)
{
  // 死亡している敵は、近接攻撃の対象にならない。
  if(!enemy.IsAlive())return false;

  // 視点から敵の中心へのベクトルと、その長さを求める。
  const XMFLOAT3 enemy_center = enemy.GetBodySphere().center;
  const float to_enemy_x = enemy_center.x - eyePosition.x;
  const float to_enemy_y = enemy_center.y - eyePosition.y;
  const float to_enemy_z = enemy_center.z - eyePosition.z;
  const float distance = std::sqrt(to_enemy_x * to_enemy_x + to_enemy_y * to_enemy_y + to_enemy_z * to_enemy_z);

  // 距離が射程を超えている場合は、範囲外。
  if(distance > COMBAT_TABLE::ns_MELEE_REACH)return false;

  // 距離が0（視点と敵の中心が一致）の場合は、方向を求められない（0除算になる）ため、重なっているとして範囲内にする。
  if(distance <= 0.0f)return true;

  // 敵の方向の単位ベクトルと視線方向の内積を求める。内積は、2つの単位ベクトルのなす角の余弦に等しい。
  const float dot = (to_enemy_x * forward.x + to_enemy_y * forward.y + to_enemy_z * forward.z) / distance;

  // 余弦が下限以上（なす角が範囲以内）なら、範囲内。
  return dot >= COMBAT_TABLE::ns_MELEE_MIN_FORWARD_DOT;
}

// 範囲内にいて、怯んでいて、転倒していない敵だけを、蹴れるとする。
bool CombatResolver::CanKickEnemy(const XMFLOAT3& eyePosition,const XMFLOAT3& forward,const Enemy& enemy)
{
  return IsEnemyInMeleeReach(eyePosition,forward,enemy) && enemy.IsFlinching() && !enemy.IsKnockedDown();
}
