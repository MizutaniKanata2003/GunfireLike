#pragma once

// 敵1体の、モデルの動き（状態から動きを選ぶ・時刻を進める・動きの切替を補間する・ボーン行列を求めて描く）を担当する。
// 共有の読込（EnemyModelLibrary）は、借用するだけで、所有しない。敵の状態（HP・怯み・転倒など）は持たず、毎回の更新と描画で、呼出側（Enemy）が渡す。
// 待機と移動の選択は、プレイヤーとの距離（発覚しているか）で決める。発覚している間は移動、発覚していない間は待機を再生する。

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

// 動きと描画に使う、敵1体の状態。更新と描画の引数の数を減らすための値の束で、Enemyが毎回作って渡す。
struct ENEMY_PLAYBACK_STATE
{
  // 敵の種類。見た目（モデル）を選ぶために使う。
  ENEMY_KIND kind{ENEMY_KIND::e_MELEE};
  // 敵の識別値。見た目（AかB）を選ぶために使う。
  std::uint32_t enemyId{};
  // 球の中心のワールド座標。描画の位置に使う。
  XMFLOAT3 position{};
  // 向き。単位はラジアン。0が+Z、πが-Z。Enemy::SetFacingと同じ定義。描画だけに使う。
  float yawRadians{};
  // プレイヤーとの水平距離。単位はワールド距離単位。発覚しているかの判定に使う。向きの目標が設定されるまでは0（発覚している）。
  float playerDistance{};
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

// 動きの切替の補間の状態。切替の直前の動きと、その時刻を保持し、短い時間で、今の動きへ混ぜていく。
struct ENEMY_CLIP_BLEND
{
  // 補間中か。falseの間は、他の値を使わない。
  bool isActive{};
  // 切替の直前の動き。
  ENEMY_CLIP previousClip{ENEMY_CLIP::e_IDLE};
  // 直前の動きの時刻。単位は秒。補間の間も、進め続ける。
  float previousTimeSeconds{};
  // 補間を始めてからの経過時間。単位は秒。
  float elapsedSeconds{};
  // 補間にかける時間。単位は秒。
  float durationSeconds{};
};

// 敵1体のモデルの動きを選び、時刻を進め、描くクラス。
class EnemyModelPlayback final
{
public:
  //========= 設定・初期化関数=========
  // 全ての敵が共有する、モデルと動きの読込を設定する。参照だけを保持し、所有しない。
  // library: 共有の読込。非所有で、このクラスが使い終わるまで生存させる。nullptrなら、描画はfalseを返す。
  void SetLibrary(EnemyModelLibrary* library) { m_library = library; }
  // 動きの状態を、発覚している状態の移動の動きの最初へ戻し、補間を止める。出現した敵は、待機せず、すぐに歩く。共有の読込への参照は、変えない。
  // startSeconds: 移動の動きの開始位置。単位は秒。同時に生成した敵が、揃って動かないよう、敵ごとにずらす。
  void Reset(float startSeconds);
  // 死亡の動きを、最初から始める。直前の動きからは、短い補間でつなぐ。
  void StartDeath();

  //========= 更新・描画関数=========
  // 状態から動きを選び、変わった時は補間を始めて時刻を0へ戻し、変わらない時は進める。共有の読込がない場合や、時間が進まない場合は、何もしない。
  // state: 敵の現在の状態。
  // deltaTime: この更新で進める経過時間。単位は秒。0以下なら何もしない。
  void Update(const ENEMY_PLAYBACK_STATE& state,float deltaTime);
  // モデルで描く。読込済みの見た目と動きがある場合だけ描く。補間中は、直前の動きと混ぜる。死亡後は、保持の時間が過ぎてから、徐々に消え、消えたら、描かずに、trueを返す。
  // graphicsSystem: Rendererが使う描画基盤。
  // camera: View行列とProjection行列。
  // state: 敵の現在の状態。位置・向き・色を使う。
  // 戻り値: モデルで処理した（描いた、または、死亡後に描かないと決めた）場合はtrue。モデルを使えず、呼出側がCubeで描く必要がある場合はfalse。
  [[nodiscard]] bool Draw(GraphicsSystem& graphicsSystem,const CAMERA_MATRICES& camera,const ENEMY_PLAYBACK_STATE& state) const;
  // 足が地面を滑らない、追従の移動速度を返す。歩きの動きの再生中は、動きの位相に対応する速度、それ以外は、歩きの速度の平均を使い、歩きの再生の倍率を掛ける。
  // state: 敵の現在の状態。種類と識別値から、見た目を決める。
  // fallbackSpeed: 速度の表がない（モデルがない・足のノードが見つからなかった）時に返す、既定の速度。単位はワールド距離単位/秒。
  // 戻り値: 移動速度。単位はワールド距離単位/秒。
  [[nodiscard]] float GetMoveSpeed(const ENEMY_PLAYBACK_STATE& state,float fallbackSpeed) const;
  // 死体が、完全に消えたか（または、モデルで描けず、預かる必要がないか）を返す。死亡後の保持と、フェードの時間が過ぎると、trueになる。
  // state: 敵の現在の状態。種類と識別値から、見た目を決める。
  // 戻り値: 生きている場合はfalse。死亡後で、完全に透明になったか、モデルで描けない場合はtrue。
  [[nodiscard]] bool IsCorpseGone(const ENEMY_PLAYBACK_STATE& state) const;
  // 起き上がりの動きを再生中かを返す。起き上がりが終わるまで、呼出側は、敵の移動と攻撃を止める。モデルを使えない場合は、常にfalse。
  [[nodiscard]] bool IsGettingUp() const { return m_clip == ENEMY_CLIP::e_GET_UP; }
private:
  //========= 更新の補助関数=========
  // プレイヤーとの距離から、発覚しているかを更新する。発覚する距離と、発覚が解除される距離を変え、境目で判定が入れ替わらないようにする。
  // playerDistance: プレイヤーとの水平距離。単位はワールド距離単位。
  void UpdateAlertState(float playerDistance);
  // 状態から、再生する動きを選ぶ。優先順は、死亡、転倒、怯み、起き上がり、攻撃、発覚中の移動、発覚していない間の待機。
  // state: 敵の現在の状態。
  // isClipFinished: 現在の動きを、最後まで再生したか。
  // 戻り値: 再生する動き。
  [[nodiscard]] ENEMY_CLIP ChooseClip(const ENEMY_PLAYBACK_STATE& state,bool isClipFinished) const;
  // 待機と移動の切替が、直前の切替から短い間に起きないよう、現在の動きを保つ。それ以外の切替は、そのまま通す。
  // desiredClip: 選ばれた動き。
  // 戻り値: 実際に再生する動き。
  [[nodiscard]] ENEMY_CLIP ApplyLocomotionHold(ENEMY_CLIP desiredClip) const;
  // 動きを切り替える。直前の動きと、その時刻を、補間の元として残し、今の動きの時刻を0へ戻す。
  // nextClip: 切り替える先の動き。
  void SwitchClip(ENEMY_CLIP nextClip);
  // 補間の経過時間と、直前の動きの時刻を進め、補間の時間が過ぎたら終える。
  // deltaTime: この更新で進める経過時間。単位は秒。
  void AdvanceBlend(float deltaTime);

  //========= 描画の補助関数=========
  // 全ボーンの最終の行列を求める。補間中で、直前の動きがあれば、混ぜた結果を、なければ、今の動きだけの結果を返す。
  // appearance: 見た目。直前の動きの再生部を探すために使う。
  // player: 今の動きの再生部。
  // outBoneMatrices: 出力先。
  void EvaluateBoneMatrices(ENEMY_APPEARANCE appearance,SkinnedAnimationPlayer& player,BoneMatrixArray& outBoneMatrices) const;

  //========= 参照先=========
  // 全ての敵が共有する、モデルと動きの読込。非所有で、EnemyControllerが所有する。未設定はnullptr。
  EnemyModelLibrary* m_library{};

  //========= 再生状態=========
  // 再生中の動き。出現した敵は、すぐに歩くため、移動から始める。
  ENEMY_CLIP m_clip{ENEMY_CLIP::e_WALK};
  // 再生中の動きの時刻。単位は秒。動きごとの速さの倍率を掛けて進める。
  float m_clipTimeSeconds{};
  // 現在の動きに切り替わってからの経過時間。単位は秒。待機と移動の最短の保持に使う。
  float m_clipHoldSeconds{};
  // 動きの切替の補間の状態。
  ENEMY_CLIP_BLEND m_blend{};

  //========= 判定用の履歴=========
  // 発覚しているか。発覚する距離の内に入るとtrueになり、発覚が解除される距離の外に出るとfalseへ戻る。出現した直後は、発覚している。
  bool m_isAlert{true};
  // 前回のUpdateで、攻撃の予告中だったか。予告の開始を検出するために使う。
  bool m_wasTelegraphing{};
};