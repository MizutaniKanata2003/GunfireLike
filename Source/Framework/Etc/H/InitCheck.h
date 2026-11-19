#pragma once

// 初期化の成否を判定し、失敗した場合だけ、エラーログへ記録する汎用の補助を宣言する。
// 初期化関数の結果を、そのまま呼出側へ返せる形にして、「失敗を検出したらログへ記録して失敗を返す」処理を1か所へまとめる。
// 失敗後の後始末（生成済みResourceの解放）・再試行・復旧は担当しない。呼出側が、返された結果を見て行う。HRESULTの失敗は対象外。

//========= C++標準ライブラリ インクルード=========
#include <string_view> // std::wstring_viewで、失敗時にログへ記録するメッセージを、所有せずに受け取る。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

// 初期化の成否の判定と、失敗の記録を行う。
class InitCheck final
{
public:
  //========= 判定関数=========
  // 初期化の結果を判定し、失敗していた場合だけ、エラーとしてログへ記録する。成功した場合は何も出力しない。
  // 複数の初期化を、&&で順につなぐ使い方を想定する。&&は短絡評価のため、最初に失敗した時点で後続の初期化は実行されず、失敗した対象のメッセージだけが記録される。
  // isSuccess: 初期化関数の結果。成功ならtrue。
  // logCategory: ログへ記録する機能分類。
  // failureMessage: 失敗した対象を示すメッセージ。呼出側が、所属名と失敗内容を含めて指定する。ログへ書き込む間だけ有効であればよい。
  // 戻り値: isSuccessをそのまま返す。
  [[nodiscard]] static bool Succeeded(bool isSuccess,e_LogCategory logCategory,std::wstring_view failureMessage)
  {
    // 初期化に失敗していた場合だけ、失敗した対象のメッセージをエラーとして記録する。成功時は記録しない。
    if(!isSuccess){
      Logger::Write(e_LogLevel::e_ERROR,logCategory,failureMessage);
    }
    return isSuccess;
  }

private:
  //========= 生成禁止関数=========
  // InitCheckのインスタンス生成を禁止する。
  InitCheck() = delete;
};
