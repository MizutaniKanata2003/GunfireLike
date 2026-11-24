#pragma once

// コンパイル済みShaderファイルをCPU側の連続したバイト列へ読み込み、検出した失敗をログへ記録する。
// HLSLのコンパイル、CSOの内容検証、GPU Shader生成・Bindは担当しない。ファイルの内容は加工しない。

//========= C++標準ライブラリ インクルード=========
#include <vector> // 読込結果を呼出側が所有するstd::vector<char>へ格納し、先頭とサイズをGPU生成へ渡せるようにする。

// 保存状態を持たず、静的なLoadだけを提供するファイルLoader。
class ShaderBinaryLoader final
{
public:
  //========= Shader読込関数=========
  // ファイルサイズ分を先頭から読み込む。有効なShaderかどうかは、利用側のShader生成などで別途扱う。
  // filePath: 呼出中に有効な終端付きcharパス。相対パスは作業ディレクトリ基準。nullptr・空文字列はfalse。
  // binaryData: 呼出側が所有する出力先。引数検証より前にclearし、成功時は読込バイトで内容を置き換える。
  // 戻り値: 読込成功ならtrue。パス不正・Open失敗・取得サイズ0以下・Read失敗ならfalse。
  // false返却時の出力先は空。clearは要素を消す操作で、確保容量の解放まで保証するものではない。
  // ログのパス表示はcharを1文字ずつwchar_tへキャストする既存方式。UTF-8等のデコードは行わない。
  // メモリ確保・ログ生成等の例外は捕捉しない。例外経路ではfalse返却時と同じ出力状態を保証しない。
  // 巨大ファイルの型変換上限や、サイズ取得後のファイル変化に対する追加の整合検証・再試行は行わない。
  static bool Load(const char* filePath,std::vector<char>& binaryData);

private:
  //========= 生成禁止関数=========
  // インスタンスに保存する状態がないため構築を禁止し、静的Loadを使う。
  ShaderBinaryLoader() = delete;
};