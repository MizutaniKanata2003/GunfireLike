#pragma once

//========= C++標準ライブラリ インクルード=========
#include <string_view>

// ログ出力の重要度を表す。
enum class e_LogLevel
{
	e_INFO,
	e_WARNING,
	e_ERROR
};

// ログを出力した機能分類を表す。
enum class e_LogCategory
{
	e_ASSET,
	e_GRAPHICS,
	e_SCENE,
	e_AUDIO,
	e_INPUT,
	e_GAME
};

// Visual Studioの出力ウィンドウへ分類付きログを出力する。
class Logger final
{
public:
	//========= ログ出力関数=========
	// 指定した重要度、分類、メッセージをデバッグ出力へ書き込む。
	static void Write(
		e_LogLevel logLevel,
		e_LogCategory logCategory,
		std::wstring_view message );

private:
	//========= 生成禁止関数=========
	// Loggerのインスタンス生成を禁止する。
	Logger() = delete;
};