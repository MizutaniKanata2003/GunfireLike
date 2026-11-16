#include "../H/ShaderBinaryLoader.h"

// パスを検証して末尾位置からバイト数を取得し、領域確保・先頭移動・読込の順で出力先へ格納する。
// 既存の条件・戻り値・ログ・出力消去のタイミングを維持し、安全策や文字コード変換は追加しない。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // 取得したファイルサイズをvectorの要素数へ渡すstd::size_tに変換する。
#include <fstream> // std::ifstreamでバイナリOpen、末尾位置取得、先頭移動、指定バイト数の読込を行う。
#include <string> // std::wstringで失敗理由と表示用パスを連結し、Loggerへ渡す。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

// 読込失敗時に理由とパスを組み立てる内部関数を、この翻訳単位内へ限定する。
namespace
{
	//========= 失敗ログ関数=========
	// エラー説明とcharパスを一つのワイド文字列にまとめる。読込結果の消去は呼出元が行う。
	// errorText: 呼出中に有効な終端付き失敗説明。nullptr不可。
	// filePath: 呼出前に検証済みの終端付きcharパス。nullptr不可。ポインター自体の変更はローカルだけに反映する。
	void WriteShaderAssetError(const wchar_t* errorText,const char* filePath)
	{
		// 説明へ区切りを加え、後続のパスを同じメッセージへ連結する。
		std::wstring message{errorText};
		message += L": ";

		// 終端NULLに達するまで1文字ずつ進める。マルチバイト列のデコードではなく、既存の単純キャストを維持する。
		while(*filePath != '\0'){
			message += static_cast<wchar_t>(*filePath); // 現在のchar値をwchar_tへ変換して末尾へ加える。
			++filePath; // 次のcharへ進める。呼出元のポインター変数は変更しない。
		}

		// Asset分類のErrorとして記録する。ログ用のメモリ確保や出力に独自の例外捕捉は追加しない。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,message);
	}
}

//========= Shader読込関数=========
// 出力先を空にしてから読み込み、検出した失敗ではAssetログとfalseを返す。
bool ShaderBinaryLoader::Load(const char* filePath,std::vector<char>& binaryData)
{
	// 再利用された出力先の旧内容を、パスの有効性によらず先に消す。旧バイナリへの復帰は行わない。
	binaryData.clear();
	// nullptrを先に判定する短絡評価により、未指定パスの先頭を参照しない。空文字列も失敗として戻る。
	if(filePath == nullptr || filePath[0] == '\0'){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,L"Shaderファイルパスが空です。");
		return false;
	}

	// binaryでテキスト変換を避け、ateでOpen直後に末尾へ移動する。後段のtellgで読込量を取得するための設定。
	// ローカルストリームの寿命でファイルを閉じる。GPU ResourceやShaderのコンパイル処理は作らない。
	std::ifstream file(filePath,std::ios::binary | std::ios::ate);
	// Open後の状態が失敗なら読込へ進まず、出力先は空のまま理由と対象パスを記録する。
	if(!file){
		WriteShaderAssetError(L"Shaderファイルを開けません",filePath);
		return false;
	}

	// 末尾の位置を読込バイト数として取得する。既存のstreamsize型への代入を維持する。
	const std::streamsize file_size = file.tellg();
	// 0以下は空ファイルまたは有効な正サイズを得られない経路として失敗にする。既存ログ文言は変更しない。
	if(file_size <= 0){
		WriteShaderAssetError(L"Shaderファイルが空です",filePath);
		return false;
	}

	// char要素数は読込バイト数に対応する。正サイズをsize_tへ変換して確保し、ateで進めた位置を先頭へ戻す。
	// 型変換上限の検証やseekg単体の成否分岐は追加しない。確保失敗の例外は呼出側へ伝播する。
	binaryData.resize(static_cast<std::size_t>(file_size));
	file.seekg(0,std::ios::beg);
	// 指定バイト数を読み切れない等でReadが失敗した場合、部分データを消してからログを出しfalseを返す。
	if(!file.read(binaryData.data(),file_size)){
		binaryData.clear();
		WriteShaderAssetError(L"Shaderファイルの読込に失敗しました",filePath);
		return false;
	}
	// 指定サイズ分の読込が成功したことだけを通知する。CSO形式や内容の妥当性を保証するtrueではない。
	return true;
}