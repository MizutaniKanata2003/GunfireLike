#include "../H/ImGuiManager.h"

// Context準備、各バックエンド初期化、検出失敗時の段階別終了、フレーム処理、明示終了を実装する。
// 本体とバックエンドの呼出順・判定・完了フラグを維持し、例外対応や新しい状態管理は追加しない。

//========= 外部ライブラリ インクルード=========
#include "imgui.h" // バージョン整合確認、Context・Style・Frame・描画データ確定のAPIを使用する。
#include "imgui_impl_dx11.h" // Device/Contextを使う描画バックエンドの初期化・フレーム準備・描画・終了を使用する。
#include "imgui_impl_win32.h" // Windowを使う入力・プラットフォーム側バックエンドの初期化・更新・終了を使用する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

//========= 初期化・終了関数=========
// 本体Contextを先に準備し、両バックエンドの成功後にだけ通常フレームと終了を許可する。
bool ImGuiManager::Init(void* windowHandle,GraphicsSystem& graphics)
{
  // 既に完了していれば再生成しない。新しい引数への切替や有効性検証もせずtrueを返す。
  if(m_isInitialized)return true;

  // ヘッダーとライブラリ側の整合を確認し、後続バックエンドが使うContextを作ってDark Styleを適用する。
  // CreateContextの返却値は独自判定しない。Darkは既存の外観選択で、選定理由は推測しない。
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  // Win32側初期化がfalseなら、まだDX11側は開始していないため生成済み本体Contextだけを破棄して戻る。
  // 完了フラグはfalseのまま。失敗したバックエンドへ追加Shutdownを送る処理は導入しない。
  if(!ImGui_ImplWin32_Init(windowHandle)){
    ImGui::DestroyContext();
    return false;
  }

  // DX11側がfalseなら、成功済みWin32側を終了してから本体Contextを破棄する。
  // Uninitは完了フラグfalseで何もしないため、この部分初期化の経路で明示的に後始末する。
  if(!ImGui_ImplDX11_Init(graphics.GetDevice(),graphics.GetContext())){
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    return false;
  }

  // 両段階の成功後にだけtrueへ移し、以後のNewFrame・描画・通常Shutdownを許可する。
  m_isInitialized = true;
  return true;
}

// 完了フラグがtrueの場合だけ、バックエンドと本体を順に終了する。
void ImGuiManager::Uninit()
{
  // 未初期化・検出した失敗後・二重終了では、未準備のバックエンドにShutdownを送らない。
  if(!m_isInitialized)return;

  // 描画側→Window側→本体の逆順で終了する。バックエンドが使うContextを先に破棄しない。
  // GraphicsSystemの終了は行わない。利用側はDevice/Contextをまだ有効に保った状態でここへ到達させる。
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  // 以後のFrame処理と二重Shutdownを抑止する。呼出前の外部Contextを復元する処理は追加しない。
  m_isInitialized = false;
}

//========= フレーム制御関数=========
// バックエンドのフレーム情報を準備した後に本体へUI構築開始を通知する。
void ImGuiManager::BeginFrame()
{
  // 初期化前・終了後はContextやバックエンドを必要とするフレーム処理を省く。
  if(!m_isInitialized)return;

  // DX11→Win32→本体の既存順を維持する。この後、呼出側がSceneのUIやフェードを構築する。
  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
}

// 構築済みUIを描画データにし、準備済みのRender TargetへDX11バックエンドで描画する。
void ImGuiManager::EndFrame()
{
  // 未初期化なら確定と描画を両方省く。BeginFrame済みかの判定はこのフラグでは行わない。
  if(!m_isInitialized)return;

  // Renderは描画データの確定で、GPU描画そのものではない。確定後のデータを次のバックエンド呼出へ渡す。
  // SwapChain表示や独自のState復元はここでは行わず、利用側のGraphicsSystem::EndFrame等へ任せる。
  ImGui::Render();
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}