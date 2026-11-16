#pragma once

// ワイド文字列に重要度・機能分類と末尾改行を付け、Windowsのデバッグ出力へ渡す共通Logger。
// ファイル保存、ログ蓄積、重要度による出力抑制は行わない。出力失敗の通知や例外捕捉も追加しない。

//========= C++標準ライブラリ インクルード=========
#include <string_view> // std::wstring_viewで呼出側の文字列を非所有のまま受け取り、長さ付きで連結する。

//========= 列挙型=========
// ログ重要度。公開型名e_LogLevelは既存参照との互換性のため維持し、列挙順・暗黙値も変更しない。
enum class e_LogLevel
{
	e_INFO,    // 通常の情報。表示文字列はInfo。重要度による出力抑制は行わない。
	e_WARNING, // 警告。表示文字列はWarning。処理を継続・停止するかは呼出側が決める。
	e_ERROR    // 失敗や異常。表示文字列はError。ログ出力自体では例外送出や終了を要求しない。
};

// ログ出力元の機能分類。公開型名e_LogCategoryと既存の列挙順・暗黙値を維持する。
enum class e_LogCategory
{
	e_ASSET,    // Assetの読込。表示文字列はAsset。
	e_GRAPHICS, // 描画基盤・GPU Resource。表示文字列はGraphics。
	e_SCENE,    // Sceneの管理・遷移。表示文字列はScene。
	e_AUDIO,    // 音声。表示文字列はAudio。
	e_INPUT,    // 入力。表示文字列はInput。
	e_GAME      // ゲーム処理。表示文字列はGame。
};

// 保存状態を持たず、静的メソッドで分類付きのデバッグ出力を提供する。
class Logger final
{
public:
	//========= ログ出力関数=========
	// [重要度][分類] メッセージの形式で終端付き文字列を作り、末尾に改行を一つ追加して出力する。
	// logLevel: 重要度。未定義の列挙値も受け付け、表示名をUnknownとする。
	// logCategory: 出力元の分類。未定義の列挙値では表示名をUnknownとする。
	// message: 呼出中に有効なワイド文字列view。元の文字列を所有せず、参照を保持しない。終端は不要。
	// 空文字列でも接頭辞と改行を出力する。渡された内容の改行除去や埋込NULLの検査は行わない。
	// 戻り値はなく、出力の到達を通知しない。ローカル文字列のメモリ確保などの例外は捕捉せず伝播する。
	static void Write(e_LogLevel logLevel,e_LogCategory logCategory,std::wstring_view message);

private:
	//========= 生成禁止関数=========
	// インスタンス用の状態を持たず、静的Writeだけを使用するため構築を禁止する。
	Logger() = delete;
};