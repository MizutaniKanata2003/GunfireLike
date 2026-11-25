#pragma once

// 敵1体の、モデルの動き（状態から動きを選ぶ・時刻を進める・ボーン行列を求めて描く）を担当する。
// 共有の読込（EnemyModelLibrary）は、借用するだけで、所有しない。敵の状態（HP・怯み・転倒など）は持たず、毎回の更新と描画で、呼出側（Enemy）が渡す。
// 動きの補間（H8e-3b-2）、歩きの速度（H8e-3c）、死体のフェード（H8e-3d）は、後の工程で追加する。

//========= C++標準ライブラリ インクルード=========
#include <cstdint> // std::uint32_tで、敵の識別値を受け取る。

//========= Framework インクルード=========
#include "Framework/3D/H/CameraMatrices.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyModelLibrary.h"
#include "Scene/Common/Enemy/H/EnemyWaveTable.h"

//========= 前方宣言=========
class GraphicsSystem;

// 動きの選択と描画に使う、敵1体の状態。更新と描画の引数の数を減らすための値の束で、Enemyが毎回作って渡す。
struct ENEMY_PLAYBACK_STATE
{
  // 敵の種類。見た目（モデル）を選ぶために使う。
  ENEMY_KIND kind{ENEMY_KIND::e_MELEE};
  // 敵の識別値。見た目（AかB）を選ぶために使う。
  std::uint32_t enemyId{};
  // 球の中心のワールド座標。移動の判定と、描画の位置に使う。
  XMFLOAT3 position{};
  // 向き。単位はラジアン。0が+Z、πが-Z。Enemy::SetFacingと同じ定義。描画だけに使う。
  float yawRadians{};
  // 生きているか。
  bool isAlive{};
  // 怯み中か。
  bool isFlinching{};
  // 転倒中か。
  bool isKnockedDown{};
  // 攻撃の予告中か。
  bool isTelegraphing{};
  // モデルの画像に乗算する色。描画だけに使う。
  XMFLOAT4 color{1.0f,1.0f,1.0f,1.0f};
};

// 敵1体のモデルの動きを選び、時刻を進め、描くクラス。
class EnemyModelPlayback final
{
public:
  //========= 設定・初期化関数=========
  // 全ての敵が共有する、モデルと動きの読込を設定する。参照だけを保持し、所有しない。
  // library: 共有の読込。非所有で、このクラスが使い終わるまで生存させる。nullptrなら、描画はfalseを返す。
  void SetLibrary(EnemyModelLibrary* library) { m_library = library; }
  // 動きの状態を、待機の最初へ戻す。共有の読込への参照は、変えない。
  // position: 敵の位置。移動の判定の、基準の位置にする。
  // idleStartSeconds: 待機の動きの開始位置。単位は秒。
  void Reset(const XMFLOAT3& position,float idleStartSeconds);
  // 死亡の動きを、最初から始める。倒された最初の描画から、死亡の動きになる。
  void StartDeath();

  //========= 更新・描画関数=========
  // 状態から動きを選び、変わった時は時刻を0へ戻し、変わらない時は進める。共有の読込がない場合や、時間が進まない場合は、何もしない。
  // state: 敵の現在の状態。
  // deltaTime: この更新で進める経過時間。単位は秒。0以下なら何もしない。
  void Update(const ENEMY_PLAYBACK_STATE& state,float deltaTime);
  // モデルで描く。読込済みの見た目と動きがある場合だけ描く。死亡後は、動きが終わり、保持の時間が過ぎたら、描かずに、trueを返す。
  // graphicsSystem: Rendererが使う描画基盤。
  // camera: View行列とProjection行列。
  // state: 敵の現在の状態。位置・向き・色を使う。
  // 戻り値: モデルで処理した（描いた、または、死亡後に描かないと決めた）場合はtrue。モデルを使えず、呼出側がCubeで描く必要がある場合はfalse。
  [[nodiscard]] bool Draw(GraphicsSystem& graphicsSystem,const CAMERA_MATRICES& camera,const ENEMY_PLAYBACK_STATE& state) const;

private:
  //========= 更新の補助関数=========
  // 前回の更新からの水平の移動の速さで、移動中かを更新する。開始と終了の閾値を変え、判定が、毎フレーム入れ替わらないようにする。
  // position: 敵の現在の位置。
  // deltaTime: 前回の更新からの経過時間。単位は秒。正の値であること。
  void UpdateMovingState(const XMFLOAT3& position,float deltaTime);
  // 状態から、再生する動きを選ぶ。優先順は、死亡、転倒、怯み、起き上がり、攻撃、移動、待機。
  // state: 敵の現在の状態。
  // isClipFinished: 現在の動きを、最後まで再生したか。
  // 戻り値: 再生する動き。
  [[nodiscard]] ENEMY_CLIP ChooseClip(const ENEMY_PLAYBACK_STATE& state,bool isClipFinished) const;
  // 待機と移動の切替が、直前の切替から短い間に起きないよう、現在の動きを保つ。それ以外の切替は、そのまま通す。
  // desiredClip: 選ばれた動き。
  // 戻り値: 実際に再生する動き。
  [[nodiscard]] ENEMY_CLIP ApplyLocomotionHold(ENEMY_CLIP desiredClip) const;

  //========= 参照先=========
  // 全ての敵が共有する、モデルと動きの読込。非所有で、EnemyControllerが所有する。未設定はnullptr。
  EnemyModelLibrary* m_library{};

  //========= 再生状態=========
  // 再生中の動き。
  ENEMY_CLIP m_clip{ENEMY_CLIP::e_IDLE};
  // 再生中の動きの時刻。単位は秒。動きごとの速さの倍率を掛けて進める。
  float m_clipTimeSeconds{};
  // 現在の動きに切り替わってからの経過時間。単位は秒。待機と移動の最短の保持に使う。
  float m_clipHoldSeconds{};

  //========= 判定用の履歴=========
  // 前回のUpdateでの位置。移動中かの判定に使う。
  XMFLOAT3 m_lastPosition{};
  // 移動中か。開始の閾値を超えるとtrueになり、終了の閾値を下回るとfalseへ戻る。
  bool m_isMoving{};
  // 前回のUpdateで、攻撃の予告中だったか。予告の開始を検出するために使う。
  bool m_wasTelegraphing{};
};