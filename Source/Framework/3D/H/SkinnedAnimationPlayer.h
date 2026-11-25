#pragma once

// スキニング付きモデル1つと、アニメーション1本の組合せについて、指定した時刻の、全ボーンの最終の行列を計算するクラス。
// 時刻から、ノードごとの位置・回転・拡大のキーを補間し、階層の親から順に変換を掛けて、逆バインド行列と合成する。
// 同じモデルの別の動きと、ノードごとの位置・回転・拡大の段階で混ぜる（クロスフェード）計算も行う。ボーン行列そのものを混ぜると、体が潰れるため、行わない。
// 再生の進行（時刻を進める・切り替える・混ぜる割合を決める）、GPUへの転送、描画は担当しない。時刻は、呼出側が持つ。
// モデルとアニメーションは借用し、所有しない。呼出側が、このクラスより長く生存させる。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、ボーン行列を固定長で出力する。
#include <vector> // ノードごとの、チャンネルの番号と、全体の変換の作業領域を保持する。

//========= Framework インクルード=========
#include "Framework/3D/H/SkinnedAnimationData.h"
#include "Framework/3D/H/SkinnedModelData.h"

// 全ボーンの最終の行列。添字は、SKINNED_MODEL_DATAのbonesの添字。使わない分は、単位行列。DirectXMathの向き（行ベクトル）のまま、転置していない。
using BoneMatrixArray = std::array<XMFLOAT4X4,SKINNED_MODEL::ns_MAX_BONE_COUNT>;

// ノード1つの、親から見た姿勢（拡大・回転・位置）。2つの動きを混ぜるために、行列へ合成する前の形で扱う。
struct ANIMATION_NODE_POSE
{
  // 拡大。
  XMFLOAT3 scale{1.0f,1.0f,1.0f};
  // 回転。正規化したクォータニオン。
  XMFLOAT4 rotation{0.0f,0.0f,0.0f,1.0f};
  // 位置。
  XMFLOAT3 translation{};
};

//========= 前方宣言=========
class SkinnedAnimationPlayer;

// 2つの動きを混ぜる時の、混ぜる相手（直前の動き）と、割合。EvaluateBlendedの引数の数を減らすための値の束。
struct ANIMATION_BLEND_INPUT
{
  // 直前の動きの再生部。借用で、同じモデルのものであること。
  const SkinnedAnimationPlayer* previousPlayer{};
  // 直前の動きの時刻。単位は秒。範囲を整えた値を渡す。
  float previousTimeSeconds{};
  // 今の動きの割合。0で、直前の動きだけ、1で、今の動きだけ。
  float currentWeight{1.0f};
};

// モデルとアニメーションの組合せから、ボーン行列を計算するクラス。
class SkinnedAnimationPlayer final
{
public:
  //========= 初期化・終了関数=========
  // モデルとアニメーションを借用し、ノードの名前とチャンネルの名前を対応づけた表を作る。対応した数を、Infoのログへ出す。
  // model: 借用するモデル。読込済みであること。このPlayerより長く生存させ、内容を変えない。
  // clip: 借用するアニメーション。読込済みであること。このPlayerより長く生存させ、内容を変えない。
  // 戻り値: ノードが1つ以上あり、1つ以上のチャンネルが、ノードに対応したらtrue。失敗した場合は、Warningのログへ出し、借用しない。
  [[nodiscard]] bool Initialize(const SKINNED_MODEL_DATA& model,const SKINNED_ANIMATION_CLIP& clip);
  // 借用した参照と、作業領域を手放す。未初期化・終了済みでも呼出可能。
  void Uninit();

  //========= 計算関数=========
  // 時刻を、アニメーションの範囲へ整える。繰り返す場合は、長さで割った余り、繰り返さない場合は、0〜長さへ制限する。
  // timeSeconds: 整える前の時刻。単位は秒。
  // isLooping: 繰り返すか。
  // 戻り値: 整えた時刻。単位は秒。借用していない場合は0。
  [[nodiscard]] float NormalizeTime(float timeSeconds,bool isLooping) const;
  // 指定した時刻の、全ボーンの最終の行列を計算する。式は「逆バインド行列 × ノードの全体の変換 × 根の逆行列」（行ベクトルの向き）。
  // 借用していない場合は、全て単位行列にする。キーのないノードは、基準の姿勢のまま動かない。
  // timeSeconds: 計算する時刻。単位は秒。範囲の外の値は、最初か最後のキーに止まる。繰り返しは、NormalizeTimeで整えてから渡す。
  // outBoneMatrices: 出力先。全てを書き換える。
  void Evaluate(float timeSeconds,BoneMatrixArray& outBoneMatrices);
  // この動きと、直前の動き（同じモデル）を、ノードごとに、位置・拡大は直線補間、回転は球面補間で混ぜた、全ボーンの最終の行列を計算する。
  // 混ぜる相手がない、または、別のモデルの場合は、Evaluateと同じ結果にする。
  // timeSeconds: この動きの時刻。単位は秒。範囲を整えた値を渡す。
  // blendInput: 直前の動きと、その時刻、今の動きの割合。
  // outBoneMatrices: 出力先。全てを書き換える。
  void EvaluateBlended(float timeSeconds,const ANIMATION_BLEND_INPUT& blendInput,BoneMatrixArray& outBoneMatrices);
  // アニメーションの長さを返す。単位は秒。借用していない場合は0。
  [[nodiscard]] float GetDurationSeconds() const;

private:
  //========= 計算の補助関数=========
  // 全てのノードの、親を含めた全体の変換を、親が先の順で計算し、m_globalTransformsへ書く。
  // timeSeconds: 計算する時刻。単位は秒。
  // blend: 混ぜる相手。nullptrなら、この動きだけで計算する。
  void ComputeGlobalTransforms(float timeSeconds,const ANIMATION_BLEND_INPUT* blend);
  // m_globalTransformsから、ボーンごとの最終の行列を合成して出力する。
  // outBoneMatrices: 出力先。使わない分は、単位行列のまま。
  void ComposeBoneMatrices(BoneMatrixArray& outBoneMatrices) const;
  // ノード1つの、親から見た変換を作る。どちらの動きにもチャンネルがなければ基準の姿勢。キーの種類がなければ、その種類は基準の姿勢の値を使う。
  // nodeIndex: ノードの番号。
  // timeSeconds: 計算する時刻。単位は秒。
  // blend: 混ぜる相手。nullptrなら、この動きだけで計算する。
  // 戻り値: 親から見た変換。
  [[nodiscard]] XMMATRIX BuildLocalTransform(int nodeIndex,float timeSeconds,const ANIMATION_BLEND_INPUT* blend) const;
  // ノード1つの、親から見た姿勢（拡大・回転・位置）を、この動きだけで求める。チャンネルがなければ、基準の姿勢を分解した値。
  // nodeIndex: ノードの番号。
  // timeSeconds: 計算する時刻。単位は秒。
  // 戻り値: 親から見た姿勢。
  [[nodiscard]] ANIMATION_NODE_POSE SampleNodePose(int nodeIndex,float timeSeconds) const;

  //========= 参照先=========
  // 借用したモデル。未登録はnullptr。
  const SKINNED_MODEL_DATA* m_model{};
  // 借用したアニメーション。未登録はnullptr。
  const SKINNED_ANIMATION_CLIP* m_clip{};

  //========= 作業用の値=========
  // ノードごとの、対応するチャンネルの番号。対応がなければ-1。添字は、ノードの番号。
  std::vector<int> m_channelIndexByNode{};
  // ノードごとの、全体の変換。計算のたびに、書き換える作業領域。添字は、ノードの番号。
  std::vector<XMFLOAT4X4> m_globalTransforms{};
};