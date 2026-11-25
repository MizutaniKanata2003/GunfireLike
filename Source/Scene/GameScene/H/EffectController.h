#pragma once

// 演出（小さなCubeの粒）を、固定長の配列で保持し、生成・更新・描画を担当する。
// 粒は、位置・速度・色・大きさ・残り時間を持ち、時間とともに小さくなって消える。画像は使わず、単色のCubeで描く。配列が満杯の場合は、最も古い粒を置き換える。
// 発砲のマズルフラッシュと、命中の火花は、場面の位置と方向を受け取って、粒の組合せとして生成する。どの場面で出すか（発砲・命中の検出）は担当せず、呼出側が、生成を求める。Cube Rendererは、共有Registryから借用し、所有しない。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、演出の粒を、固定長で保持する。
#include <cstddef> // std::size_tで、粒の最大数と、配列の位置を表す。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class BasicMeshRenderer;
class GraphicsSystem;
class ModelRegistry;

// 演出の数値をまとめる。
namespace EFFECT_CONTROLLER
{
  //========= 上限=========
  // 同時に保持できる粒の最大数。満杯の時の生成は、最も古い粒を置き換える。仮値。
  inline constexpr std::size_t ns_MAX_COUNT = 64;
}

// 粒1個を生成するための値の束。EffectController::Spawnの引数の数を減らす。
struct EFFECT_SPAWN_PARAMS
{
  // 粒の中心のワールド座標。
  XMFLOAT3 position{};
  // 粒の速度。単位は、ワールド距離単位毎秒。0なら、その場で動かない。
  XMFLOAT3 velocity{};
  // 粒の色。RGBA。単色で描く。
  XMFLOAT4 color{1.0f,1.0f,1.0f,1.0f};
  // 生成した時の1辺の長さ。単位はワールド距離単位。時間とともに0へ近づく。
  float size{0.1f};
  // 粒が残る時間。単位は秒。0より大きいこと。
  float lifetimeSeconds{0.2f};
};

// 演出の粒を保持し、更新と描画を行うクラス。
class EffectController final
{
public:
  //========= 初期化・終了関数=========
  // Cube Rendererを借用し、全ての粒を消す。
  // registry: Cube Rendererの借用元。Initialize済みであること。このEffectControllerより長く生存させる。
  // 戻り値: Cube Rendererを借用できたらtrue。Registryが未初期化ならfalse。
  [[nodiscard]] bool Initialize(ModelRegistry& registry);
  // 全ての粒を消し、借用した参照を手放す。未初期化でも、繰り返しても呼出可能。
  void Uninit();
  // 全ての粒を消す。ウェーブの切替や、プレイのやり直しで、旧い演出を持ち越さないために使う。
  void Clear();

  //========= 更新・描画関数=========
  // 粒を1個生成する。空きがなければ、最も古い粒を置き換える。
  // params: 生成する粒の値。lifetimeSecondsが0以下の場合は、何も生成しない。
  void Spawn(const EFFECT_SPAWN_PARAMS& params);
  // 全ての粒の位置を速度で進め、残り時間を減らし、0以下になった粒を消す。
  // deltaTime: この更新で進める経過時間。単位は秒。0なら、何も変わらない。
  void Update(float deltaTime);
  // 残っている全ての粒を、残り時間の割合に応じた大きさのCubeで描く。Rendererを借用していない場合は、何もしない。
  // graphicsSystem: Rendererが使う描画基盤。
  // viewMatrix: World空間からView空間への変換行列。
  // projectionMatrix: View空間からClip空間への変換行列。
  void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

  //========= 場面の生成関数=========
  // 発砲のマズルフラッシュを、銃口の位置へ、黄色の粒1個で出す。銃口は、視点から、右・下・前へずらした位置（Collision可視化の弾道の始点と同じ側）。
  // eyePosition: 発砲した時の視点のワールド座標。
  // forward: 発砲の方向。長さ1に正規化済みであること。
  void SpawnMuzzleFlash(const XMFLOAT3& eyePosition,const XMFLOAT3& forward);
  // 命中の火花を、命中点から、斜め上へ散る橙色の粒として、複数個出す。命中した相手が敵か地形かは問わない。
  // hitPoint: 命中点のワールド座標。
  void SpawnHitSparks(const XMFLOAT3& hitPoint);

private:
  //========= 粒の状態=========
  // 粒1個の状態。残り時間が0以下なら、使われていない枠として扱う。
  struct EFFECT_STATE
  {
    // 中心のワールド座標。
    XMFLOAT3 position{};
    // 速度。単位は、ワールド距離単位毎秒。
    XMFLOAT3 velocity{};
    // 色。RGBA。
    XMFLOAT4 color{1.0f,1.0f,1.0f,1.0f};
    // 生成した時の1辺の長さ。
    float size{};
    // 生成した時の残り時間。単位は秒。残り時間の割合を求める基準。
    float lifetimeSeconds{};
    // 残り時間。単位は秒。0以下なら使われていない枠。
    float remainingSeconds{};
  };

  //========= 参照先=========
  // Cube描画に使う共有Renderer。非所有で、ModelRegistryが所有する。未取得はnullptr。
  BasicMeshRenderer* m_cubeRenderer{};

  //========= 所有状態=========
  // 粒の固定長の配列。
  std::array<EFFECT_STATE,EFFECT_CONTROLLER::ns_MAX_COUNT> m_effects{};
  // 次に生成する粒を探し始める位置。空きがなければ、この位置の粒（最も古い）を置き換える。
  std::size_t m_nextIndex{};
};