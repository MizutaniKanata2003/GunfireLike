#pragma once

// Game画面の固定ステージ（床と4面の壁）を、OBJECT_CONFIGの表として定義する。
// 以前のStageFieldが持っていた配置・倍率・色・UV倍率・描画順を、値を変えずに表へ移したもの。Resource生成や描画は担当しない。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // std::size_tで表の行数を表す。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"

//========= Scene インクルード=========
#include "Scene/Common/Object/H/ObjectConfig.h"

// Game画面の固定ステージの配置表をまとめる。
namespace STAGE_LAYOUT
{
  //========= ステージObject表=========
  // 描画順は床→左壁→右壁→手前壁→奥壁。StageFieldの既存順序を維持する。
  // 各行の並び: 形状、Cube画像、OBJパス、OBJ画像、位置(X,Y,Z)、回転(ラジアン)、倍率(X,Y,Z)、RGBA色、UV倍率(U,V)、Collisionレイヤー。
  // 座標はワールド空間。Cubeは1辺1のため、倍率が各軸の寸法になる。全行で回転なし、色は不透明な白。
  // 全行が動かない地形のため、STATICレイヤーのAABB Colliderとして登録する。
  inline constexpr OBJECT_CONFIG ns_OBJECTS[]
  {
    // 床: 中心(0,-0.6,8)、X・Zとも20の広さでY方向の厚みは0.2。Floor画像をUV10倍で繰り返す。
    {OBJECT_MESH_KIND::e_CUBE,BasicMeshRenderer::TextureType::Floor,nullptr,nullptr,
    {0.0f,-0.6f,8.0f},{0.0f,0.0f,0.0f},{20.0f,0.2f,20.0f},{1.0f,1.0f,1.0f,1.0f},{10.0f,10.0f},
    COLLISION_LAYER::ns_STATIC},

    // 左壁: -X側。X方向が厚み0.2、Y方向の高さ3.2、Z方向の長さ20。Wall画像をUV1倍で貼る。
    {OBJECT_MESH_KIND::e_CUBE,BasicMeshRenderer::TextureType::Wall,nullptr,nullptr,
    {-10.0f,1.1f,8.0f},{0.0f,0.0f,0.0f},{0.2f,3.2f,20.0f},{1.0f,1.0f,1.0f,1.0f},{1.0f,1.0f},
    COLLISION_LAYER::ns_STATIC},

    // 右壁: +X側。左壁と同じ倍率・色・UV倍率・画像を使う。
    {OBJECT_MESH_KIND::e_CUBE,BasicMeshRenderer::TextureType::Wall,nullptr,nullptr,
    {10.0f,1.1f,8.0f},{0.0f,0.0f,0.0f},{0.2f,3.2f,20.0f},{1.0f,1.0f,1.0f,1.0f},{1.0f,1.0f},
    COLLISION_LAYER::ns_STATIC},

    // 手前壁: -Z側。Z方向が厚み0.2、X方向の長さ20、Y方向の高さ3.2。
    {OBJECT_MESH_KIND::e_CUBE,BasicMeshRenderer::TextureType::Wall,nullptr,nullptr,
    {0.0f,1.1f,-2.0f},{0.0f,0.0f,0.0f},{20.0f,3.2f,0.2f},{1.0f,1.0f,1.0f,1.0f},{1.0f,1.0f},
    COLLISION_LAYER::ns_STATIC},

    // 奥壁: +Z側。手前壁と同じ倍率・色・UV倍率・画像を使う。
    {OBJECT_MESH_KIND::e_CUBE,BasicMeshRenderer::TextureType::Wall,nullptr,nullptr,
    {0.0f,1.1f,18.0f},{0.0f,0.0f,0.0f},{20.0f,3.2f,0.2f},{1.0f,1.0f,1.0f,1.0f},{1.0f,1.0f},
    COLLISION_LAYER::ns_STATIC}
  };

  //========= ステージObject表の行数=========
  // ns_OBJECTSの全要素数。SceneObjectFactory::Buildへ渡す。
  inline constexpr std::size_t ns_OBJECT_COUNT = sizeof(ns_OBJECTS) / sizeof(ns_OBJECTS[0]);
}