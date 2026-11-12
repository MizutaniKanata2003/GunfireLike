#include "../H/Logger.h"

// ログの重要度・分類を表示文字列へ変換し、接頭辞と改行を付けてデバッグ出力へ渡す。

//========= C++標準ライブラリ インクルード=========
#include <string>

//========= Windows インクルード=========
#include <Windows.h>

// Logger内部だけで使用する、列挙値から表示文字列への変換処理をまとめる。
namespace
{
	//========= 表示文字列変換関数=========
	// ログ重要度に対応する固定文字列を返す。返却先は呼出側で解放しない。
	// logLevel: 表示する重要度。未定義値ではUnknownを返す。
	const wchar_t* GetLogLevelText(e_LogLevel logLevel)
	{
		// 定義済みの重要度を既存の英字表記へ対応付け、範囲外はUnknownへまとめる。
		switch(logLevel){
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

	// ログ分類に対応する固定文字列を返す。返却先は呼出側で解放しない。
	// logCategory: 表示する機能分類。未定義値ではUnknownを返す。
	const wchar_t* GetLogCategoryText(e_LogCategory logCategory)
	{
		// 分類ごとの既存表記を選び、未定義の値が渡されても表示用の文字列を返す。
		switch(logCategory){
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

//========= ログ出力関数=========
// 指定した重要度、分類、メッセージをデバッグ出力へ書き込む。
void Logger::Write(e_LogLevel logLevel,e_LogCategory logCategory,std::wstring_view message)
{
	// viewの内容をローカル文字列へ連結し、分類付きの終端文字列と末尾改行を用意する。
	std::wstring log_message{L"["};
	log_message += GetLogLevelText(logLevel);
	log_message += L"][";
	log_message += GetLogCategoryText(logCategory);
	log_message += L"] ";
	log_message += message;
	log_message += L"\n";

	// 完成した文字列をWindowsのデバッグ出力へ渡す。保存先や出力抑制の処理は追加しない。
	OutputDebugStringW(log_message.c_str());
}