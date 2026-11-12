#include "../H/ShaderBinaryLoader.h"

// Shaderファイルのバイナリ読込と、失敗理由のログ出力を実装する。

//========= C++標準ライブラリ インクルード=========
#include <cstddef>
#include <fstream>
#include <string>

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

// Shader読込で使用する内部用のログ出力処理をまとめる。
namespace
{
	//========= 失敗ログ関数=========
	// 説明とファイルパスを連結し、Assetカテゴリへエラーログを出力する。
	// errorText: 失敗理由を表す終端付きのワイド文字列。nullptr不可。
	// filePath: 失敗したファイルの終端付きパス。呼出前に有効性を確認する。nullptr不可。
	// 既存の1バイト単位の拡張を維持し、文字コードのデコード処理は追加しない。
	void WriteShaderAssetError(const wchar_t* errorText,const char* filePath)
	{
		std::wstring message{errorText};
		message += L": ";

		while(*filePath != '\0'){
			message += static_cast<wchar_t>(*filePath);
			++filePath;
		}

		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,message);
	}
}

//========= Shader読込関数=========
// 出力先を初期化してからファイル全体を読み込み、検出した失敗はログへ記録する。
bool ShaderBinaryLoader::Load(const char* filePath,std::vector<char>& binaryData)
{
	binaryData.clear();
	if(filePath == nullptr || filePath[0] == '\0'){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_ASSET,L"Shaderファイルパスが空です。");
		return false;
	}

	// 末尾位置から読込サイズを取得するため、Open時に末尾へ移動する。
	std::ifstream file(filePath,std::ios::binary | std::ios::ate);
	if(!file){
		WriteShaderAssetError(L"Shaderファイルを開けません",filePath);
		return false;
	}

	const std::streamsize file_size = file.tellg();
	if(file_size <= 0){
		WriteShaderAssetError(L"Shaderファイルが空です",filePath);
		return false;
	}

	// 全体分の領域を確保して先頭から読み、Read失敗時には出力先を空へ戻す。
	binaryData.resize(static_cast<std::size_t>(file_size));
	file.seekg(0,std::ios::beg);
	if(!file.read(binaryData.data(),file_size)){
		binaryData.clear();
		WriteShaderAssetError(L"Shaderファイルの読込に失敗しました",filePath);
		return false;
	}
	return true;
}