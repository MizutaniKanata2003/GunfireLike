#include "../H/GraphicsErrorLog.h"

// 描画APIの失敗を、API名とHRESULTを連結して、Graphics分類のエラーとして記録する処理を実装する。

//========= C++標準ライブラリ インクルード=========
#include <iomanip> // setfill・setwでHRESULTを0埋め8桁に整える。
#include <sstream> // wostringstreamへ数値と書式指定を渡し、失敗値の表示文字列を生成する。
#include <string> // wstringでAPI名・HRESULTを連結してLoggerへ渡す。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

// HRESULTの表示文字列を作る補助関数をまとめる。
namespace
{
  // HRESULTを0x付き大文字16進数8桁へ変換する。
  // result: 表示するHRESULT。unsigned表現へ変換して既存の桁表記を維持する。
  std::wstring GetHRESULTText(HRESULT result)
  {
    // 16進・大文字・0埋めを設定する。setw(8)は後続の数値部分の最小幅で、0xは別に付加する。
    std::wostringstream stream{};
    stream << L"0x" << std::uppercase << std::hex << std::setfill(L'0') << std::setw(8) << static_cast<unsigned long>(result);
    return stream.str();
  }
}

//========= 記録関数=========
// API名と失敗値をGraphics分類へ出力する。Resourceの後始末は呼出元が行う。
void GraphicsErrorLog::Write(const wchar_t* functionName,HRESULT result)
{
  // API名と結果を同じメッセージへ連結する。ログ生成の例外をここで捕捉する処理はない。
  std::wstring message{functionName};
  message += L" に失敗しました。HRESULT: ";
  message += GetHRESULTText(result);
  Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_GRAPHICS,message);
}
