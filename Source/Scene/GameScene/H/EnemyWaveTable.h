#pragma once

// 部屋ごとの敵の生成位置と種類を定義する。敵の行動やColliderの登録は担当しない。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで部屋・ウェーブ・出現枠の数を固定する。
#include <cstddef> // std::size_tで出現枠数を表す。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// 出現する敵の行動種別。
enum class ENEMY_KIND
{
  e_MELEE,  // プレイヤーへ接近して攻撃する敵。
  e_RANGED, // 離れた位置から弾を撃つ敵。
  e_AREA    // 範囲を予告して攻撃する敵。
};

// 敵1体の初期配置。
struct ENEMY_SPAWN
{
  // 行動種別。
  ENEMY_KIND kind;
  // 球Colliderの中心。Xは左右、Yは床からの高さ、Zは奥行きのワールド座標。
  XMFLOAT3 position;
};

// D7のウェーブ編成をまとめる。
namespace ENEMY_WAVE_TABLE
{
  //========= 編成設定=========
  // 出現する敵の固定枠数。1ウェーブで両方の枠を使う。
  inline constexpr std::size_t ns_SLOT_COUNT = 2;
  // 3部屋それぞれの2ウェーブ。手前と奥や左右に分ける試遊用の仮配置。
  inline const std::array<std::array<std::array<ENEMY_SPAWN,ns_SLOT_COUNT>,2>,3> ns_ROOMS{{
    {{{{{ENEMY_KIND::e_MELEE,{-2.0f,1.0f,6.0f}},{ENEMY_KIND::e_MELEE,{2.0f,1.0f,8.0f}}}},
      {{{ENEMY_KIND::e_MELEE,{-2.0f,1.0f,7.0f}},{ENEMY_KIND::e_RANGED,{2.0f,1.0f,11.0f}}}}}},
    {{{{{ENEMY_KIND::e_MELEE,{-2.0f,1.0f,7.0f}},{ENEMY_KIND::e_RANGED,{2.0f,1.0f,11.0f}}}},
      {{{ENEMY_KIND::e_RANGED,{-3.0f,1.0f,10.0f}},{ENEMY_KIND::e_AREA,{3.0f,1.0f,12.0f}}}}}},
    {{{{{ENEMY_KIND::e_MELEE,{-2.0f,1.0f,6.0f}},{ENEMY_KIND::e_AREA,{2.0f,1.0f,10.0f}}}},
      {{{ENEMY_KIND::e_AREA,{-3.0f,1.0f,10.0f}},{ENEMY_KIND::e_RANGED,{3.0f,1.0f,12.0f}}}}}}
  }};
}
