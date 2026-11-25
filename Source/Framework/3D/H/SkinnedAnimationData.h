#pragma once

// FBXから読み込んだ、スキニングのアニメーション（ノードごとの、位置・回転・拡大のキーフレーム）のCPU側のデータを定義する。
// 値を持つだけで、読込・補間・ボーン行列の計算・再生の進行は担当しない。Assimpのヘッダーには依存しない。
// 時刻は、全て秒。回転は、左手系へ変換済みのクォータニオン。ノードは、名前で対応づける（SKINNED_NODEのname）。

//========= C++標準ライブラリ インクルード=========
#include <string> // std::stringで、アニメーションの名前とノードの名前を保持する。
#include <vector> // キーフレームとチャンネルを、可変長配列で保持する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// 3成分（位置または拡大）のキーフレーム1つ。
struct ANIMATION_VECTOR_KEY
{
  // このキーの時刻。単位は秒。
  float timeSeconds{};
  // このキーの値。
  XMFLOAT3 value{};
};

// 回転のキーフレーム1つ。
struct ANIMATION_ROTATION_KEY
{
  // このキーの時刻。単位は秒。
  float timeSeconds{};
  // このキーの回転。クォータニオン。x・y・z・wの順で、単位の長さ。
  XMFLOAT4 value{};
};

// ノード1つ分の、アニメーション。キーのない種類は、空になる（その種類は、ノードの基準の姿勢のままにする）。
struct ANIMATION_CHANNEL
{
  // 動かすノードの名前。
  std::string nodeName{};
  // 位置のキー。時刻の昇順。
  std::vector<ANIMATION_VECTOR_KEY> positionKeys{};
  // 回転のキー。時刻の昇順。
  std::vector<ANIMATION_ROTATION_KEY> rotationKeys{};
  // 拡大のキー。時刻の昇順。
  std::vector<ANIMATION_VECTOR_KEY> scaleKeys{};
};

// アニメーション1本。
struct SKINNED_ANIMATION_CLIP
{
  // FBXの中の、アニメーションの名前。
  std::string name{};
  // アニメーションの長さ。単位は秒。
  float durationSeconds{};
  // ノードごとのアニメーション。
  std::vector<ANIMATION_CHANNEL> channels{};
};
