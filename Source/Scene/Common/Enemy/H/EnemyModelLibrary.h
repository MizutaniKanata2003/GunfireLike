#pragma once

// 敵の見た目（スキニング付きのFBXモデルと、動きのFBX）を、見た目ごとに1組だけ読み込み、全ての敵で共有するクラス。
// 見た目ごとに、モデルのデータ・アニメーションのデータ・動きごとの再生部（SkinnedAnimationPlayer）・描画部（SkinnedModelRenderer）を所有する。
// 歩きの動きの、足のボーンの動きから測った、地面に対する速度の表（位相ごとの速度と、平均）も、見た目ごとに持つ。足が滑らない移動速度を決めるために使う。
// 敵の状態から動きを選ぶこと、再生の時刻の管理、描画の呼出しは担当しない（Enemy側が行う）。読込は、Scene初期化中にだけ行い、プレイ中には行わない。
// SkinnedAnimationPlayerは、モデルとアニメーションの参照を保持するため、このクラスは、コピー・移動を禁止し、生成後にアドレスを変えない。

//========= C++標準ライブラリ インクルード=========
#include <array> // 見た目ごと・動きごとの所有物を、固定長で保持する。
#include <cstddef> // std::size_tで、見た目と動きの数・添字を表す。
#include <cstdint> // std::uint32_tで、敵の識別値を受け取る。

//========= Framework インクルード=========
#include "Framework/3D/H/SkinnedAnimationData.h"
#include "Framework/3D/H/SkinnedAnimationPlayer.h"
#include "Framework/3D/H/SkinnedModelData.h"
#include "Framework/3D/H/SkinnedModelRenderer.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyWaveTable.h"

//========= 前方宣言=========
class GraphicsSystem;

// 敵の見た目。モデルのFBXと、動きのFBXのフォルダーの組を表す。
enum class ENEMY_APPEARANCE
{
  e_MELEE_A, // 接近型のモデルA。
  e_MELEE_B  // 接近型のモデルB。Aと同じ動きを使い、見た目だけが違う。
};

// 敵の動き。FBXのファイル名と対応する。
enum class ENEMY_CLIP
{
  e_IDLE,      // 待機。
  e_WALK,      // 移動。
  e_TELEGRAPH, // 攻撃の予告。任意で、ファイルがない見た目では、呼出側が待機で代用する。
  e_ATTACK,    // 攻撃。
  e_HIT,       // 怯み。
  e_DOWN,      // 転倒。
  e_GET_UP,    // 起き上がり。
  e_DEATH      // 死亡。
};

// 見た目と動きの数をまとめる。
namespace ENEMY_MODEL
{
  // 見た目の数。ENEMY_APPEARANCEの要素数と同じにする。
  inline constexpr std::size_t ns_APPEARANCE_COUNT = 2;
  // 動きの数。ENEMY_CLIPの要素数と同じにする。
  inline constexpr std::size_t ns_CLIP_COUNT = 8;
  // 歩きの速度の表の、サンプル数。歩きの動きの1周を、この数に等分して、速度を測る。
  inline constexpr std::size_t ns_WALK_SPEED_SAMPLE_COUNT = 64;
}

// 見た目1つ分の、読込元と、描画の補正値。
struct ENEMY_APPEARANCE_PARAMS
{
  // モデルのFBXのパス。作業ディレクトリ基準。終端付きの静的な文字列。
  const wchar_t* modelPath;
  // 動きのFBXを置くフォルダー。末尾は「/」。中のファイル名は、動きごとに決まっている（Idle.fbxなど）。終端付きの静的な文字列。
  const wchar_t* animationFolder;
  // モデルの空間（Mixamoはセンチメートル）から、ワールドへの倍率。0.01で、1センチメートルが0.01になる。
  float modelScale;
  // 向きの補正。単位はラジアン。モデルの正面が-Zのため、敵の向き（+Zが正面）に合わせて、半回転を足す。
  float yawOffsetRadians;
  // 球の中心から、足元までの高さの差。下向きは負。床の上面（Y=-0.5）に足を付けるために、球の中心の高さから引く。
  float footOffsetY;
};

// 見た目1つ分の所有物。SkinnedAnimationPlayerが、modelとclipsを借用するため、同じ構造体の中に置く。
struct ENEMY_APPEARANCE_ASSETS
{
  // FBXから読み込んだモデルのデータ。
  SKINNED_MODEL_DATA model{};
  // 動きごとのアニメーションのデータ。添字は、ENEMY_CLIPの値。読み込まなかった動きは空。
  std::array<SKINNED_ANIMATION_CLIP,ENEMY_MODEL::ns_CLIP_COUNT> clips{};
  // 動きごとの再生部。添字は、ENEMY_CLIPの値。hasClipがfalseの動きは、未初期化。
  std::array<SkinnedAnimationPlayer,ENEMY_MODEL::ns_CLIP_COUNT> players{};
  // 動きを読み込んだか。添字は、ENEMY_CLIPの値。
  std::array<bool,ENEMY_MODEL::ns_CLIP_COUNT> hasClip{};
  // スキニング付きモデルの描画部。
  SkinnedModelRenderer renderer{};
  // 歩きの動きの1周を等分した位相ごとの、地面に対する速度。単位はワールド距離単位/秒。hasWalkSpeedsがfalseの間は使わない。
  std::array<float,ENEMY_MODEL::ns_WALK_SPEED_SAMPLE_COUNT> walkSpeeds{};
  // 歩きの速度の平均。単位はワールド距離単位/秒。hasWalkSpeedsがfalseの間は使わない。
  float walkSpeedAverage{};
  // 歩きの速度の表を作れたか。足のボーンが見つからない場合は、false。
  bool hasWalkSpeeds{};
  // モデルと必須の動きを全て読み込み、描画部まで初期化できたか。
  bool isLoaded{};
};

// 全ての見た目を、読み込んで共有するクラス。
class EnemyModelLibrary final
{
public:
  //========= 生成・コピー設定=========
  // 未初期化の状態で生成する。コピー禁止の宣言により暗黙の既定コンストラクターが消えるため、明示する。
  EnemyModelLibrary() = default;
  // 再生部が、モデルとアニメーションの参照を保持するため、コピーを禁止する。
  EnemyModelLibrary(const EnemyModelLibrary&) = delete;
  // 再生部が、モデルとアニメーションの参照を保持するため、コピーを禁止する。
  EnemyModelLibrary& operator=(const EnemyModelLibrary&) = delete;

  //========= 初期化・終了関数=========
  // 旧Resourceを先にUninitし、全ての見た目のモデルと動きを読み込み、描画部を初期化し、歩きの速度の表を作る。Scene初期化中にだけ呼ぶ。
  // graphicsSystem: 描画部の生成に使うDeviceの取得元。参照は保持しない。
  // 戻り値: 全ての見た目を読み込めたらtrue。1つでも失敗したら、全て解放してfalse。失敗の理由は、各Loaderがログへ記録する。歩きの速度の表を作れなくても、失敗にしない。
  [[nodiscard]] bool Initialize(GraphicsSystem& graphicsSystem);
  // 全ての所有物を解放する。未初期化・終了済みでも呼出可能。ContextのBind解除・GPU完了待機は行わない。
  void Uninit();

  //========= 取得関数=========
  // 敵の種類と識別値から、使う見た目を決める。接近型は、識別値が偶数ならA、奇数ならB。
  // kind: 敵の種類。
  // enemyId: 敵の識別値。
  // outAppearance: 見た目の出力先。戻り値がtrueの時だけ書き換える。
  // 戻り値: 種類に、モデルの見た目があればtrue。まだ見た目がない種類は、falseで、呼出側が、Cubeで描く。
  [[nodiscard]] static bool TryGetAppearance(ENEMY_KIND kind,std::uint32_t enemyId,ENEMY_APPEARANCE& outAppearance);
  // 見た目の読込元と、描画の補正値を返す。範囲外の値は、先頭の見た目として扱う。
  // appearance: 見た目。
  // 戻り値: 静的な表への参照。寿命の制約はない。
  [[nodiscard]] static const ENEMY_APPEARANCE_PARAMS& GetParams(ENEMY_APPEARANCE appearance);
  // 敵の位置と向きから、モデルの空間からワールドへの変換行列を作る。「倍率 × 回転（向き＋補正） × 移動（位置＋足元の補正）」の順。
  // appearance: 見た目。倍率・向きの補正・足元の補正に使う。
  // position: 敵の位置。球の中心のワールド座標。
  // yawRadians: 敵の向き。単位はラジアン。Enemy::SetFacingと同じ定義で、0が+Z、πが-Z。
  // 戻り値: 転置前の、ワールド行列。
  [[nodiscard]] static XMFLOAT4X4 BuildWorldMatrix(ENEMY_APPEARANCE appearance,const XMFLOAT3& position,float yawRadians);
  // 見た目を、描画に使えるかを返す。
  // appearance: 見た目。
  // 戻り値: モデルと必須の動きを読み込み済みならtrue。
  [[nodiscard]] bool IsLoaded(ENEMY_APPEARANCE appearance) const;
  // 見た目と動きに対応する再生部を返す。再生部は、計算の途中の値を持つため、呼出ごとにEvaluateを呼んだ直後に、結果を使うこと。
  // appearance: 見た目。
  // clip: 動き。
  // 戻り値: 再生部への借用ポインター。未読込・範囲外・ファイルがない動きはnullptr。所有権は移さない。
  [[nodiscard]] SkinnedAnimationPlayer* FindPlayer(ENEMY_APPEARANCE appearance,ENEMY_CLIP clip);
  // 見た目に対応する描画部を返す。
  // appearance: 見た目。
  // 戻り値: 描画部への借用ポインター。未読込・範囲外はnullptr。所有権は移さない。
  [[nodiscard]] SkinnedModelRenderer* FindRenderer(ENEMY_APPEARANCE appearance);
  // 歩きの動きの位相から、地面に対する速度を返す。位相は、動きの1周を0〜1で表し、範囲外の値は、1周ごとに繰り返す。表の隣り合うサンプルの間は、直線で補間する。
  // appearance: 見た目。
  // phase: 歩きの動きの位相。単位は周期の数。
  // outSpeed: 速度の出力先。単位はワールド距離単位/秒。戻り値がtrueの時だけ書き換える。
  // 戻り値: 速度の表があればtrue。未読込・範囲外・表がない場合はfalse。
  [[nodiscard]] bool TryGetWalkSpeed(ENEMY_APPEARANCE appearance,float phase,float& outSpeed) const;
  // 歩きの速度の平均を返す。
  // appearance: 見た目。
  // outSpeed: 平均の出力先。単位はワールド距離単位/秒。戻り値がtrueの時だけ書き換える。
  // 戻り値: 速度の表があればtrue。未読込・範囲外・表がない場合はfalse。
  [[nodiscard]] bool TryGetWalkSpeedAverage(ENEMY_APPEARANCE appearance,float& outSpeed) const;

private:
  //========= 読込の補助関数=========
  // 見た目1つ分の、モデル・動き・描画部を読み込み、歩きの速度の表を作る。
  // graphicsSystem: 描画部の生成に使う描画基盤。
  // appearanceIndex: 見た目の添字。範囲内であること。
  // 戻り値: 全て成功したらtrue。歩きの速度の表を作れなくても、失敗にしない。
  [[nodiscard]] bool LoadAppearance(GraphicsSystem& graphicsSystem,std::size_t appearanceIndex);

  //========= 所有物=========
  // 見た目ごとの所有物。添字は、ENEMY_APPEARANCEの値。配列の要素のアドレスは、再生部が参照するため、変えない。
  std::array<ENEMY_APPEARANCE_ASSETS,ENEMY_MODEL::ns_APPEARANCE_COUNT> m_assets{};
};