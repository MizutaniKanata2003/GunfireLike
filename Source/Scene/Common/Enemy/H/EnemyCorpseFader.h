#pragma once

// 倒された敵の死体を預かり、敵の枠が解除された後も、モデルの死亡の動きの最後のポーズを、徐々に消えるまで、描き続ける。
// Waveが切り替わって、敵の枠が一括で解除されても、死体が突然消えないようにするためのクラス。
// 敵の状態（HP・Collider）は預からない。死体の描画に必要な、モデルの動きの状態（EnemyModelPlayback）と、位置・向き（ENEMY_PLAYBACK_STATE）だけを、コピーして持つ。
// 共有の読込（EnemyModelLibrary）は、コピーした動きの状態が、借用したまま持つため、このクラスより長く生存させる。

//========= C++標準ライブラリ インクルード=========
#include <array> // 預かる死体を、固定長で保持する。
#include <cstddef> // std::size_tで、預かれる数を表す。

//========= Framework インクルード=========
#include "Framework/3D/H/CameraMatrices.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyModelPlayback.h"

//========= 前方宣言=========
class GraphicsSystem;

// 預かれる死体の数をまとめる。
namespace ENEMY_CORPSE
{
  // 同時に預かれる死体の数。1つのWaveの敵の数より多くする。仮値。
  inline constexpr std::size_t ns_MAX_COUNT = 16;
}

// 死体を預かり、描き、消えたものを手放すクラス。
class EnemyCorpseFader final
{
public:
  //========= 預かり・解放関数=========
  // 死体を1つ預かる。死亡後で、モデルで描かれ、まだ消えていない死体だけを預かる。
  // playback: 死んだ敵のモデルの動きの状態。コピーして持つ。
  // state: 死んだ敵の状態。位置・向き・色を使う。生きている状態（isAlive）は、預からない。
  // 戻り値: 預かればtrue。生きている、モデルで描けない、すでに消えている、または、空きがない場合は、預からずfalse。
  bool Add(const EnemyModelPlayback& playback,const ENEMY_PLAYBACK_STATE& state);
  // 預かっている死体を、全て手放す。やり直し（F4）と、終了時に呼ぶ。
  void Clear();

  //========= 更新・描画関数=========
  // 預かっている死体の時間を進め、完全に消えたものを手放す。
  // deltaTime: この更新で進める経過時間。単位は秒。0以下なら何もしない。
  void Update(float deltaTime);
  // 預かっている死体を、全て描く。消えかけの死体は、市松状に画素を抜いて描かれる。
  // graphicsSystem: Rendererが使う描画基盤。
  // camera: View行列とProjection行列。
  void Draw(GraphicsSystem& graphicsSystem,const CAMERA_MATRICES& camera) const;

  //========= Getter関数=========
  // 預かっている死体の数を返す。
  [[nodiscard]] std::size_t GetCount() const;

private:
  // 預かっている死体1つ。
  struct CORPSE
  {
    // 死んだ敵のモデルの動きの状態のコピー。死亡の動きの時刻を、進めて、消えるまでを数える。
    EnemyModelPlayback playback{};
    // 死んだ敵の状態のコピー。位置と向きは、死んだ時のまま変えない。
    ENEMY_PLAYBACK_STATE state{};
    // 預かっているか。falseの要素は、空き。
    bool isActive{};
  };

  //========= 預かっている死体=========
  // 預かっている死体。空きは、isActiveがfalse。
  std::array<CORPSE,ENEMY_CORPSE::ns_MAX_COUNT> m_corpses{};
};
