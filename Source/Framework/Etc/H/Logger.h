#pragma once

// 重要度と機能分類を付けたワイド文字列を、Windowsのデバッグ出力へ渡す共通Logger。
// ファイル保存・重要度による出力抑制・ログの蓄積は担当しない。

//========= C++標準ライブラリ インクルード=========
#include <string_view>

//========= 列挙型=========
// ログの重要度を表す。既存の公開型名と列挙子の順序を互換性のため維持する。
enum class e_LogLevel
{
	e_INFO,    // 通常の情報。
	e_WARNING, // 処理継続時の警告。
	e_ERROR    // 失敗や異常の報告。
};

// ログを出力した機能を分類する。既存の公開型名と列挙子の順序を維持する。
enum class e_LogCategory
{
	e_ASSET,    // Assetの読込。
	e_GRAPHICS, // 描画基盤とGPU Resource。
	e_SCENE,    // Sceneの管理と遷移。
	e_AUDIO,    // 音声。
	e_INPUT,    // 入力。
	e_GAME      // ゲーム処理。
};

// 状態を保持せず、静的メソッドで分類付きのデバッグ出力を提供する。
class Logger final
{
public:
	//========= ログ出力関数=========
	// [重要度][分類] メッセージの形式へ末尾改行を付け、OutputDebugStringWへ渡す。
	// logLevel: ログの重要度。未定義値はUnknownという表示へ変換する。
	// logCategory: 出力元の機能分類。未定義値はUnknownという表示へ変換する。
	// message: 呼出中に読み取れるワイド文字列。終端付きでなくてもよく、参照自体は保持しない。
	// 空文字列でも接頭辞と改行を出力する。文字列生成などの例外は捕捉しない。
	static void Write(e_LogLevel logLevel,e_LogCategory logCategory,std::wstring_view message);

private:
	//========= 生成禁止関数=========
	// 静的メソッドだけを使用するため、インスタンス生成を禁止する。
	Logger() = delete;
};