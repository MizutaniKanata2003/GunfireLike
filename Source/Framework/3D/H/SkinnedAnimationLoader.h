#pragma once

// アニメーションを持つFBXを、Assimpで読み込み、アニメーションのCPU側のデータ（SKINNED_ANIMATION_CLIP）へ変換するクラス。
// 2通りの読み方がある。1つ目は、先頭のアニメーション1本だけを読む（MixamoのWithout Skinのように、1ファイルに1本の動きの場合）。
// 2つ目は、ファイルに入っている全てのアニメーションを、名前付きで読む（銃のように、1ファイルに複数の動きが入っている場合）。
// 読むのは、ノードごとの位置・回転・拡大のキーフレーム。モデルの読込と同じ設定（左手系への変換・Pivotの非保持）で読む。
// 補間・ボーン行列の計算・再生の進行は担当しない。Assimpのヘッダーは、実装側だけが使う。状態を持たない。

//========= C++標準ライブラリ インクルード=========
#include <string> // std::wstringで、FBXのパスを受け取る。
#include <vector> // 全てのアニメーションを、読み込んだ順に返す。

//========= Framework インクルード=========
#include "Framework/3D/H/SkinnedAnimationData.h"

// アニメーションのFBXを読み込むクラス。
class SkinnedAnimationLoader final
{
public:
  //========= 読込関数=========
  // アニメーションのFBXを読み込み、先頭のアニメーションを変換する。成功した場合は、概要（長さ・チャンネル数・キーの数）を、Infoのログへ出す。
  // filePath: FBXのパス。相対パスは、プロセスの作業ディレクトリ基準。参照は保持しない。
  // outClip: 読み込んだデータの出力先。呼出前の内容は、捨てる。失敗した場合は、空にする。
  // 戻り値: 読み込めて、アニメーションが1つ以上あればtrue。失敗の理由は、Errorのログへ記録する。
  [[nodiscard]] bool Load(const std::wstring& filePath,SKINNED_ANIMATION_CLIP& outClip) const;
  // アニメーションのFBXを読み込み、含まれる全てのアニメーションを、ファイルの中の並びのまま変換する。名前は、各アニメーション（FBXのTake）の名前。
  // 成功した場合は、アニメーションの数と、アニメーションごとの概要（名前・長さ・チャンネル数・キーの数）を、Infoのログへ出す。1つのファイルに、複数の動きが、名前付きで入っているか、1本の長い時間軸にまとまっているかは、このログで分かる。
  // filePath: FBXのパス。相対パスは、プロセスの作業ディレクトリ基準。参照は保持しない。
  // outClips: 読み込んだデータの出力先。呼出前の内容は、捨てる。失敗した場合は、空にする。
  // 戻り値: 読み込めて、アニメーションが1つ以上あればtrue。失敗の理由は、Errorのログへ記録する。
  [[nodiscard]] bool LoadAll(const std::wstring& filePath,std::vector<SKINNED_ANIMATION_CLIP>& outClips) const;
};