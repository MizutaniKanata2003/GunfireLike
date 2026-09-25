#pragma once

//========= C++標準ライブラリ インクルード=========
#include <vector>

// ShaderのCSOファイルを読み込む共通Loader。
class ShaderBinaryLoader final
{
public:
	//========= Shader読込関数=========
	// 指定したCSOファイルを読み込み、Shaderバイナリを返す。
	static bool Load( const char* filePath, std::vector<char>& binaryData );

private:
	//========= 生成禁止関数=========
	// ShaderBinaryLoaderのインスタンス生成を禁止する。
	ShaderBinaryLoader() = delete;
};