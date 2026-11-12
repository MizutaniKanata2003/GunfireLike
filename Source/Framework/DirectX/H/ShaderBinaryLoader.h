#pragma once

// コンパイル済みShaderファイルをCPU側のバイト列へ読み込む共通Loaderを宣言する。
// Shaderのコンパイル・内容検証・GPU Resource生成は担当しない。

//========= C++標準ライブラリ インクルード=========
#include <vector>

// インスタンスを持たず、ファイル読込と失敗ログ出力を提供する。
class ShaderBinaryLoader final
{
public:
	//========= Shader読込関数=========
	// 指定ファイルの全バイトを読み込む。ファイル内容が有効なCSOかどうかは検証しない。
	// filePath: 読込先の終端付きパス。相対パスは作業ディレクトリ基準。nullptr・空文字列は失敗。
	// binaryData: 呼出側が所有する出力先。開始時に内容を消去し、成功時は読込データで置き換える。
	// 戻り値: 読込成功ならtrue。パス不正・Open失敗・サイズ0以下・Read失敗ならfalse。
	// falseを返す経路では出力先は空。検出した失敗はAssetカテゴリへログ出力する。
	// メモリ確保などの例外は捕捉しない。例外発生時の状態をfalse返却時の契約には含めない。
	static bool Load(const char* filePath,std::vector<char>& binaryData);

private:
	//========= 生成禁止関数=========
	// 静的メソッドだけを使用するため、インスタンス生成を禁止する。
	ShaderBinaryLoader() = delete;
};