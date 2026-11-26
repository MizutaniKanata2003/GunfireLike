#pragma once

// 近接攻撃が届くかの判定を提供する。プレイヤーの視点と視線方向から、敵が射程と範囲の内にいるかを調べる。
// 状態を持たない静的関数だけで、ダメージの適用・動作の開始は担当しない。数値はCombatTableにまとめている。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class Enemy;

// 近接攻撃の届く範囲に関する判定をまとめた静的関数群。
class CombatResolver final
{
public:
  //========= 判定関数=========
  // 敵が、近接攻撃の射程と範囲の内にいるかを返す。
  // eyePosition: プレイヤーの視点のワールド座標。
  // forward: 視線方向の単位ベクトル。
  // enemy: 調べる敵。死亡している敵は、範囲の外として扱う。
  // 戻り値: 視点から敵の中心までの距離が射程以内で、視線方向と敵の方向のなす角が範囲以内ならtrue。
  [[nodiscard]] static bool IsEnemyInMeleeReach(const XMFLOAT3& eyePosition,const XMFLOAT3& forward,const Enemy& enemy);

private:
  //========= 生成禁止関数=========
  // 状態を持たず、静的関数だけを使用するため構築を禁止する。
  CombatResolver() = delete;
};