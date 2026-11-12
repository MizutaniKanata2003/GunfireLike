#pragma once

//========= C++標準ライブラリ インクルード=========
#include <string_view>

//========= 列挙型=========
// ログ出力の重要度を表す。
enum class e_LogLevel
{
	e_INFO, // 通常情報。
	e_WARNING, // 警告。
	e_ERROR // エラー。
};
// ログを出力した機能分類を表す。
enum class e_LogCategory
{
	e_ASSET, // Asset読込。
	e_GRAPHICS, // 描画。
	e_SCENE, // Scene管理。
	e_AUDIO, // 音声。
	e_INPUT, // 入力。
	e_GAME // ゲーム処理。
};

// Visual Studioの出力ウィンドウへ分類付きログを出力する。
class Logger final
{
public:
	//========= ログ出力関数=========
	// 指定した重要度、分類、メッセージをデバッグ出力へ書き込む。
	// logLevel: ログの重要度。
	// logCategory: 出力元の機能分類。
	// message: この呼出中に読み取れる文字列。保持しない。
	static void Write(e_LogLevel logLevel,e_LogCategory logCategory,std::wstring_view message);

private:
	//========= 生成禁止関数=========
	// Loggerのインスタンス生成を禁止する。
	Logger() = delete;
};