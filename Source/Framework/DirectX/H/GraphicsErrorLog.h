#pragma once

// Direct3D・DXGIのAPI呼出しが失敗した時に、API名と失敗値（HRESULT）を、Graphics分類のエラーとしてログへ記録する補助を宣言する。
// 失敗後のResourceの解放・再試行・復旧は担当しない。呼出側が、結果を見て行う。

//========= Windows インクルード=========
#include <windows.h> // HRESULTを受け取る。

// 描画APIの失敗を、ログへ記録する。
class GraphicsErrorLog final
{
public:
  //========= 記録関数=========
  // API名と失敗値を、「API名 に失敗しました。HRESULT: 0x00000000」の形で、Graphics分類のエラーとして記録する。
  // functionName: 失敗したAPIの名前。有効な終端付きの文字列であること。nullptrは不可。ログへ書き込む間だけ有効であればよい。
  // result: 記録するHRESULT。0x付きの大文字16進数8桁で表示する。
  static void Write(const wchar_t* functionName,HRESULT result);

private:
  //========= 生成禁止関数=========
  // GraphicsErrorLogのインスタンス生成を禁止する。
  GraphicsErrorLog() = delete;
};
