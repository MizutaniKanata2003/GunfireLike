#include "../H/ShaderBinaryLoader.h"

//========= C++標準ライブラリ インクルード=========
#include <fstream>
#include <string>

//========= Framework インクルード=========
#include "../../Etc/H/Logger.h"

namespace
{
	// Shaderファイル読込失敗のログメッセージを作る。
	void WriteShaderAssetError( const wchar_t* errorText, const char* filePath )
	{
		std::wstring message{ errorText };
		message += L": ";

		while ( *filePath != '\0' )
		{
			message += static_cast<wchar_t>( *filePath );
			++filePath;
		}

		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_ASSET, message );
	}
}

// 指定したCSOファイルを読み込み、Shaderバイナリを返す。
bool ShaderBinaryLoader::Load( const char* filePath, std::vector<char>& binaryData )
{
	binaryData.clear();

	if ( filePath == nullptr || filePath[ 0 ] == '\0' )
	{
		Logger::Write( e_LogLevel::e_ERROR, e_LogCategory::e_ASSET, L"Shaderファイルパスが空です。" );
		return false;
	}

	std::ifstream file( filePath, std::ios::binary | std::ios::ate );

	if ( !file )
	{
		WriteShaderAssetError( L"Shaderファイルを開けません", filePath );
		return false;
	}

	const std::streamsize fileSize = file.tellg();

	if ( fileSize <= 0 )
	{
		WriteShaderAssetError( L"Shaderファイルが空です", filePath );
		return false;
	}

	binaryData.resize( static_cast<size_t>( fileSize ) );
	file.seekg( 0, std::ios::beg );

	if ( !file.read( binaryData.data(), fileSize ) )
	{
		binaryData.clear();
		WriteShaderAssetError( L"Shaderファイルの読込に失敗しました", filePath );
		return false;
	}

	return true;
}