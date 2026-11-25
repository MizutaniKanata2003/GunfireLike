#pragma once

// FBXから読み込んだ、スキニング付きモデルのCPU側のデータ（頂点・Index・描画範囲・ノードの階層・ボーン・画像の情報）を定義する。
// 値を持つだけで、読込・GPU Buffer・アニメーション・描画は担当しない。Assimpのヘッダーには依存しない。
// 座標系は、読込側で左手系へ変換済みの値を入れる。行列は、DirectXMathと同じ向き（行ベクトル、移動は4行目）で保持する。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、頂点1個が持つ4本のボーン番号と重みを固定長で保持する。
#include <cstddef> // std::size_tで、頂点が持つボーンの影響の数を表す。
#include <cstdint> // std::uint8_tで、ボーン番号を1バイトで保持する。
#include <string> // std::stringで、ノードの名前と画像のパスを保持する。
#include <vector> // 頂点・Index・ノード・ボーンなどを、可変長配列で保持する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// スキニング付きモデルの数の上限をまとめる。
namespace SKINNED_MODEL
{
  // 1つの頂点が持つ、ボーンの影響の最大数。Assimpの重みの制限（aiProcess_LimitBoneWeights）の既定値と同じ。
  inline constexpr std::size_t ns_BONES_PER_VERTEX = 4;
  // 1つのモデルが持てる、ボーンの最大数。ボーン行列の定数Bufferの配列の大きさと同じにする。Mixamoのキャラクターは、約70本。
  inline constexpr int ns_MAX_BONE_COUNT = 128;
}

// スキニング付きの頂点1個。ボーン番号と重みは、同じ添字どうしが対になる。
struct SKINNED_VERTEX
{
  // モデルの空間での位置。
  XMFLOAT3 position{};
  // 法線。単位ベクトル。
  XMFLOAT3 normal{};
  // 画像のUV。左手系へ変換済みで、そのまま使える向き。
  XMFLOAT2 uv{};
  // 影響を与えるボーンの番号。SKINNED_MODEL_DATAのbonesの添字。重みが0の枠の番号は、無視する。
  std::array<std::uint8_t,SKINNED_MODEL::ns_BONES_PER_VERTEX> boneIndices{};
  // ボーンごとの重み。合計が1。重みが全て0の頂点は、どのボーンにも属さない。
  std::array<float,SKINNED_MODEL::ns_BONES_PER_VERTEX> boneWeights{};
};

// 同じ画像で描く、Indexの範囲。モデルが複数のメッシュを持つ場合に、メッシュごとに1つできる。
struct SKINNED_SUBMESH
{
  // 範囲の先頭の、Indexの添字。
  unsigned int indexStart{};
  // 範囲に含まれる、Indexの数。3の倍数。
  unsigned int indexCount{};
  // 使う材質の番号。SKINNED_MODEL_DATAのmaterialsの添字。
  unsigned int materialIndex{};
};

// モデルの階層（ボーンを含む全てのノード）の1つ。親が、必ず子より前に並ぶ。
struct SKINNED_NODE
{
  // ノードの名前。アニメーションのチャンネルが、この名前で対応するノードを探す。
  std::string name{};
  // 親ノードの番号。根は-1。
  int parentIndex{-1};
  // 親ノードから見た、基準の姿勢の変換。
  XMFLOAT4X4 localTransform{};
};

// ボーン1本。頂点の重みが参照する。
struct SKINNED_BONE
{
  // このボーンに対応する、ノードの番号。SKINNED_MODEL_DATAのnodesの添字。
  int nodeIndex{-1};
  // モデルの空間から、ボーンの空間へ移す変換（逆バインド行列）。
  XMFLOAT4X4 offsetMatrix{};
};

// 材質1つ。画像の情報だけを持つ。
struct SKINNED_MATERIAL
{
  // 拡散色の画像のパス。FBXに書かれた元のパス。埋め込まれた画像は、このパスの名前で、探せる。画像がなければ空。
  std::string diffuseTexturePath{};
  // FBXに埋め込まれた、拡散色の画像のファイルの中身。PNG・JPEGなどの、圧縮された形式のまま。埋め込まれていなければ空。
  std::vector<std::uint8_t> diffuseImageData{};
};

// FBX 1ファイルから読み込んだ、スキニング付きモデルのデータ。
struct SKINNED_MODEL_DATA
{
  // 全てのメッシュを、つなげた頂点。
  std::vector<SKINNED_VERTEX> vertices{};
  // 三角形のIndex。頂点の添字で、全てのメッシュを通した番号。
  std::vector<unsigned int> indices{};
  // 画像ごとのIndexの範囲。
  std::vector<SKINNED_SUBMESH> submeshes{};
  // 階層の全てのノード。先頭が根。
  std::vector<SKINNED_NODE> nodes{};
  // ボーン。名前が同じものは、1本にまとめる。
  std::vector<SKINNED_BONE> bones{};
  // 材質。
  std::vector<SKINNED_MATERIAL> materials{};
  // 根のノードの変換の逆行列。アニメーションの最終的な行列の計算で使う。
  XMFLOAT4X4 globalInverseTransform{};
  // FBXに埋め込まれた画像の数。
  unsigned int embeddedTextureCount{};
  // 重みが全て0の頂点の数。0でなければ、一部の頂点が、ボーンで動かない。
  unsigned int unweightedVertexCount{};
};
