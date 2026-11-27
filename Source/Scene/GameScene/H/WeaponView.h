#pragma once

// 一人称の銃（腕を含む）を表示する。4種の武器のモデルと、待機（Idle）の動きを、武器ごとに読み込み、現在の武器を、カメラの前に置いて描く。
// 視点は、モデルに入っている「camera」ノードの位置から、「camera_target」ノードの位置を見る向きにする。モデルの作者が決めた見え方に合わせるため。
// 表示の位置・大きさ・向きは、武器ごとに持ち、調整の画面（ImGui）で変えられる。調整した値は、WeaponView::Prepareの配置へ書き込む。
// 動きの再生（発砲・リロード・持ち替え）は、担当しない。今は、待機の姿勢を、止めたまま描く。武器の切替と、ゲームの状態は、呼出側が渡す。
// 描画先・Render Pass・深度の準備は、呼出側が行う。モデルの読込は、Resourceを生成するため、Scene初期化中にだけ呼ぶ。

//========= C++標準ライブラリ インクルード=========
#include <array> // 武器ごとの所有物と、表示の位置を、固定長で保持する。
#include <vector> // 1つのFBXに入っている全てのアニメーションを、可変長で保持する。

//========= Framework インクルード=========
#include "Framework/3D/H/SkinnedAnimationData.h"
#include "Framework/3D/H/SkinnedAnimationPlayer.h"
#include "Framework/3D/H/SkinnedModelData.h"
#include "Framework/3D/H/SkinnedModelRenderer.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Weapon/H/WeaponTable.h"

//========= 前方宣言=========
class GraphicsSystem;

// 銃を、カメラの前に置く位置・大きさ・向き。モデルの視点（cameraノード）から見た後に、追加で掛ける。
struct WEAPON_VIEW_PLACEMENT
{
  // モデルの大きさの倍率。視点を原点として拡大するため、画面上の見え方は変わらず、近距離Clip面で欠ける範囲だけが変わる。
  float scale{0.01f};
  // 向きの角度。単位は度。X（Pitch）・Y（Yaw）・Z（Roll）の順。カメラの空間（カメラが原点で、+Zが正面、+Xが右、+Yが上）で回す。
  XMFLOAT3 rotationDegrees{};
  // 位置。単位はワールド距離単位。カメラの空間（カメラが原点で、+Zが正面、+Xが右、+Yが上）での位置。
  XMFLOAT3 offset{};
};

// 一人称の銃を描くクラス。
class WeaponView final
{
public:
  //========= 初期化・終了関数=========
  // 旧Resourceを解放し、武器ごとの表示の位置を、確認済みの値へ設定する。GPU Resourceは作らない。モデルの読込は、LoadWeaponを武器ごとに呼んで行う。
  void Prepare();
  // 全てのResourceを解放する。未初期化・終了済みでも、繰り返しても呼出可能。
  void Uninit();

  //========= 更新・描画関数=========
  // 描く武器を、性能値から選ぶ。表示名の指す先が、表の同じ武器と一致する武器を選ぶ。どの武器とも一致しない場合は、選択を変えない。
  // weaponData: 装備中の武器の性能値。
  void SelectWeapon(const WEAPON_DATA& weaponData);
  // 選んだ武器を、待機の姿勢で描く。カメラは原点に固定し、モデルの視点の行列と、武器ごとの表示の位置から、配置を作る。
  // 選んだ武器を読み込めていない場合は、何もしない。
  // graphicsSystem: 描画基盤。Render Passと、深度の準備は、呼出側が済ませておくこと。
  // projectionMatrix: View空間からClip空間への変換行列。ワールドの描画と同じ値。
  void Draw(GraphicsSystem& graphicsSystem,const XMMATRIX& projectionMatrix);
  // 選んだ武器の、位置・大きさ・向きを変える調整の画面を、ImGuiに出す。ImGuiのフレームの中で呼ぶ。調整の間だけ使い、値が決まったら、呼出しを削除する。
  void DrawTuningUi();

  //========= 読込関数=========
  // 武器1種類の、モデル・全てのアニメーション・待機の姿勢・描画部を読み込む。Prepareの後に、読み込む武器ごとに呼ぶ。
  // 同じ武器をもう一度呼んだ場合は、読込済みの内容を、そのまま上書きする。
  // graphicsSystem: 描画部の生成に使う描画基盤。Uninitまで有効であること。
  // weaponIndex: 武器の種類の値（表の添字）。0以上、WEAPON_TABLE::ns_WEAPON_COUNT未満。範囲外ならfalse。
  // 戻り値: モデル・待機の動き・描画部が、全て揃ったらtrue。失敗した武器は、描かず、失敗をログへ記録する（残りの武器は、使える）。
  [[nodiscard]] bool LoadWeapon(GraphicsSystem& graphicsSystem,int weaponIndex);

private:
  //========= 内部の型=========
  // 武器1種類の所有物。再生部が、モデルとアニメーションを借用するため、同じ構造体の中に置く。
  struct WEAPON_VIEW_ASSETS
  {
    // FBXから読み込んだモデルのデータ。
    SKINNED_MODEL_DATA model{};
    // FBXに入っている全てのアニメーション。名前は、「Armature|Idle」のように、「|」の後ろに動きの名前が付く。
    std::vector<SKINNED_ANIMATION_CLIP> clips{};
    // 待機の動きの再生部。待機の姿勢を、計算するために使う。
    SkinnedAnimationPlayer idlePlayer{};
    // 描画部。
    SkinnedModelRenderer renderer{};
    // 待機の姿勢の、全ボーンの最終の行列。LoadWeaponで、1回だけ計算する。
    BoneMatrixArray idleBoneMatrices{};
    // モデルの空間から、モデルの視点（cameraノード）の空間への変換行列。待機の姿勢で、LoadWeaponで、1回だけ計算する。視点のノードがなければ、単位行列。
    XMFLOAT4X4 eyeMatrix{};
    // モデルと待機の動きを読み込み、描画部まで生成できたか。
    bool isLoaded{};
  };

  //========= 補助関数=========
  // 選んだ武器の視点の行列と表示の位置から、モデルの空間からカメラの空間への変換行列を作る。
  // 戻り値: 視点・大きさ・向き・位置を、この順に合成した行列。
  [[nodiscard]] XMMATRIX BuildWorldMatrix() const;

  //========= 所有状態=========
  // 武器ごとの所有物。添字は、WEAPON_TYPEの値。
  std::array<WEAPON_VIEW_ASSETS,WEAPON_TABLE::ns_WEAPON_COUNT> m_assets{};
  // 武器ごとの表示の位置。添字は、WEAPON_TYPEの値。
  std::array<WEAPON_VIEW_PLACEMENT,WEAPON_TABLE::ns_WEAPON_COUNT> m_placements{};
  // 選んでいる武器の種類の値。表の範囲内。
  int m_selectedWeaponIndex{};
};