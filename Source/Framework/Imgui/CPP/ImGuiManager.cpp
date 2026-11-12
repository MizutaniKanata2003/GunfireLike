#include "../H/ImGuiManager.h"

// Dear ImGuiの初期化失敗時の巻き戻し、フレーム開始・描画、明示的な終了処理を実装する。

//========= 外部ライブラリ インクルード=========
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

//========= 初期化・終了関数=========
// 初期化済みなら再生成せず、未初期化時だけ本体と各バックエンドを準備する。
bool ImGuiManager::Init(void* windowHandle,GraphicsSystem& graphics)
{
	// 既存Contextの二重生成を避ける。新たに渡された引数への切替は行わない。
	if(m_isInitialized)return true;

	// 使用中のImGuiとの整合を確認し、バックエンドが参照するContextと共通Styleを準備する。
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// Win32初期化に失敗した場合、この段階で生成済みのContextだけを破棄する。
	if(!ImGui_ImplWin32_Init(windowHandle)){
		ImGui::DestroyContext();
		return false;
	}

	// 描画側の初期化に失敗した場合、成功済みのWin32側とContextを逆順に終了する。
	// 完了フラグはまだfalseなので、Uninitに巻き戻しを任せず、この経路で明示的に処理する。
	if(!ImGui_ImplDX11_Init(graphics.GetDevice(),graphics.GetContext())){
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	// 両バックエンドの成功後にだけ、フレーム処理と通常終了を許可する。
	m_isInitialized = true;
	return true;
}

// 初期化に成功した状態だけを対象に、本体と両バックエンドを終了する。
void ImGuiManager::Uninit()
{
	// 初期化前・失敗後・二重終了では、未初期化のバックエンドへShutdownを送らない。
	if(!m_isInitialized)return;

	// バックエンドがContextを参照できる間に終了し、最後にContextを破棄する。
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// 終了後のフレーム処理と二重終了を抑止するため、完了状態を解除する。
	m_isInitialized = false;
}

//========= フレーム制御関数=========
// バックエンドの情報を更新してから、ImGui本体のフレームを開始する。
void ImGuiManager::BeginFrame()
{
	// 初期化前や終了後は、Contextを必要とするフレーム処理を行わない。
	if(!m_isInitialized)return;

	// 描画側、Win32側、本体の既存順序を維持して、呼出側のUI構築を受け付ける。
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

// 呼出側が構築したUIを描画データへ確定し、DirectX 11へ出力する。
void ImGuiManager::EndFrame()
{
	// 初期化前や終了後は、描画データの確定と描画を行わない。
	if(!m_isInitialized)return;

	// ImGuiのRenderは描画データを確定し、実際のGPU描画はバックエンドへ任せる。
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}