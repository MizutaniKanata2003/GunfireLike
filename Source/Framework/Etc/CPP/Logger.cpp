#include "../H/Logger.h"

//========= C++標準ライブラリ インクルード=========
#include <string>

//========= Windows インクルード=========
#include <Windows.h>

namespace
{
	// ログ重要度を表示用文字列へ変換する。
	const wchar_t* GetLogLevelText( e_LogLevel logLevel )
	{
		switch ( logLevel )
		{
			case e_LogLevel::e_INFO:
			return L"Info";

			case e_LogLevel::e_WARNING:
			return L"Warning";

			case e_LogLevel::e_ERROR:
			return L"Error";

			default:
			return L"Unknown";
		}
	}

	// ログ分類を表示用文字列へ変換する。
	const wchar_t* GetLogCategoryText( e_LogCategory logCategory )
	{
		switch ( logCategory )
		{
			case e_LogCategory::e_ASSET:
			return L"Asset";

			case e_LogCategory::e_GRAPHICS:
			return L"Graphics";

			case e_LogCategory::e_SCENE:
			return L"Scene";

			case e_LogCategory::e_AUDIO:
			return L"Audio";

			case e_LogCategory::e_INPUT:
			return L"Input";

			case e_LogCategory::e_GAME:
			return L"Game";

			default:
			return L"Unknown";
		}
	}
}

// 指定した重要度、分類、メッセージをデバッグ出力へ書き込む。
void Logger::Write(
	e_LogLevel logLevel,
	e_LogCategory logCategory,
	std::wstring_view message )
{
	std::wstring logMessage{ L"[" };
	logMessage += GetLogLevelText( logLevel );
	logMessage += L"][";
	logMessage += GetLogCategoryText( logCategory );
	logMessage += L"] ";
	logMessage += message;
	logMessage += L"\n";

	OutputDebugStringW( logMessage.c_str() );
}