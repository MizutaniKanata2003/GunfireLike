#pragma once

// FBXファイルを、Assimpで読み込み、スキニング付きモデルのCPU側のデータ（SKINNED_MODEL_DATA）へ変換するクラス。
// 読み込むのは、メッシュ（位置・法線・UV・ボーン番号・重み）、ノードの階層、ボーン、材質の画像のパス。座標系は、左手系へ変換する。
// アニメーション・GPU Buffer・画像の読込・描画は担当しない。Assimpのヘッダーは、実装側だけが使う。状態を持たない。

//========= C++標準ライブラリ インクルード=========
#include <string> // std::wstringで、FBXのパスを受け取る。

//========= Framework インクルード=========
#include "Framework/3D/H/SkinnedModelData.h"

// FBXからスキニング付きモデルを読み込むクラス。
class SkinnedModelLoader final
{
public:
  //========= 読込関数=========
  // FBXを読み込み、メッシュ・階層・ボーン・材質へ変換する。成功した場合は、概要（頂点数・ボーン数など）を、Infoのログへ出す。
  // filePath: FBXのパス。相対パスは、プロセスの作業ディレクトリ基準。参照は保持しない。
  // outModel: 読み込んだデータの出力先。呼出前の内容は、捨てる。失敗した場合は、空にする。
  // 戻り値: 読み込めて、ボーンの数が上限（SKINNED_MODEL::ns_MAX_BONE_COUNT）以内ならtrue。
  // 失敗の理由（ファイルがない・形式が不正・ボーンの上限超え・ノードが見つからない）は、Errorのログへ記録する。
  [[nodiscard]] bool Load(const std::wstring& filePath,SKINNED_MODEL_DATA& outModel) const;
};
