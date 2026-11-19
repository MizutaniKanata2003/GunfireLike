#include "../H/Logger.h"

// 列挙値を固定の表示名へ変換し、長さ付きviewから組み立てた終端付き文字列をWin32へ渡す。
// 重要度・分類・出力文字列を維持し、整形とコメントだけを整理する。

//========= Windows インクルード=========
#include <Windows.h> // OutputDebugStringWで、組み立てたワイド文字列をデバッグ出力へ渡す。

//========= C++標準ライブラリ インクルード=========
#include <string> // std::wstringで接頭辞・メッセージ・改行を連結し、c_str()から終端付き文字列を取得する。

// 表示名への変換をこの翻訳単位内へ限定する。返す文字列リテラルは静的寿命を持ち、呼出側で解放しない。
namespace
{
	//========= 表示文字列変換関数=========
	// 重要度の固定表示名を返す。文字列の生成やメモリ所有権の移動は行わない。
	// logLevel: 表示する重要度。列挙にない値はUnknownへ対応させる。
	const wchar_t* GetLogLevelText(e_LogLevel logLevel)
	{
		// 定義済みの重要度を既存の英字表記へ分類し、未定義値でも表示用ポインターを返す。
		switch(logLevel){
			// 通常情報はInfoとして出力する。ここでは重要度による抑制や状態変更を行わない。
		case e_LogLevel::e_INFO:
		return L"Info";

		// 警告はWarningとして出力する。処理を継続するかはこの変換関数では決めない。
		case e_LogLevel::e_WARNING:
		return L"Warning";

		// 異常報告はErrorとして出力する。名前の変換だけで終了や例外を要求しない。
		case e_LogLevel::e_ERROR:
		return L"Error";

		// 定義されていない値でもnullptrを返さず、既存のUnknown表記を使用する。
		default:
		return L"Unknown";
		}
	}

	// 機能分類の固定表示名を返す。返却文字列への書込や解放は行わない。
	// logCategory: 表示する出力元分類。列挙にない値はUnknownへ対応させる。
	const wchar_t* GetLogCategoryText(e_LogCategory logCategory)
	{
		// 出力元の分類に対応する既存表記を選ぶ。分類による出力先の切替は行わない。
		switch(logCategory){
			// 読込対象やAsset処理のログをAssetと表示する。
		case e_LogCategory::e_ASSET:
		return L"Asset";

		// 描画基盤・GPU Resource関連のログをGraphicsと表示する。
		case e_LogCategory::e_GRAPHICS:
		return L"Graphics";

		// Scene管理・遷移関連のログをSceneと表示する。
		case e_LogCategory::e_SCENE:
		return L"Scene";

		// 音声関連のログをAudioと表示する。
		case e_LogCategory::e_AUDIO:
		return L"Audio";

		// 入力関連のログをInputと表示する。
		case e_LogCategory::e_INPUT:
		return L"Input";

		// ゲーム処理のログをGameと表示する。
		case e_LogCategory::e_GAME:
		return L"Game";

		// 定義されていない分類もUnknownとして表示し、メッセージ自体は出力対象とする。
		default:
		return L"Unknown";
		}
	}
}

//========= ログ出力関数=========
// 分類付きのローカル文字列を完成させてから、Windowsのデバッグ出力APIへ渡す。
void Logger::Write(e_LogLevel logLevel,e_LogCategory logCategory,std::wstring_view message)
{
	// [Level][Category] と半角空白、本文、改行の順で連結する。messageはviewの長さを使ってコピーする。
	// 元のviewは保持せず、終端を持つローカルwstringへまとめる。空本文の特別扱いや末尾改行の重複除去はしない。
	std::wstring log_message{L"["};
	log_message += GetLogLevelText(logLevel);
	log_message += L"][";
	log_message += GetLogCategoryText(logCategory);
	log_message += L"] ";
	log_message += message;
	log_message += L"";

	// 生成したwstringの終端付きポインターを呼出中に渡す。メモリ確保などの例外を捕捉する処理は追加しない。
	// APIには長さを渡さないため、本文に埋込NULLがあればその位置までの文字列が渡る。保存先や到達の保証はしない。
	OutputDebugStringW(log_message.c_str());
}