#include "../H/ImGuiManager.h"

//========= 外部ライブラリ インクルード=========
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

// Dear ImGui本体とWin32・DirectX 11バックエンドを初期化する。
bool ImGuiManager::Init( void* windowHandle, GraphicsSystem& graphics )
{
	// すでに初期化済みの場合は、二重初期化しない。
	if ( m_IsInitialized ) return true;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// Win32バックエンドの初期化に失敗した場合、作成済みContextを破棄する。
	if ( !ImGui_ImplWin32_Init( windowHandle ) )
	{
		ImGui::DestroyContext();
		return false;
	}

	// DirectX 11バックエンドの初期化に失敗した場合、Win32 backendとContextを終了する。
	if ( !ImGui_ImplDX11_Init( graphics.GetDevice(), graphics.GetContext() ) )
	{
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		return false;
	}

	m_IsInitialized = true;

	return true;
}

// Dear ImGuiの新しい描画フレームを開始する。
void ImGuiManager::BeginFrame()
{
	if ( !m_IsInitialized ) return;

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

// Dear ImGuiの描画データをDirectX 11へ出力する。
void ImGuiManager::EndFrame()
{
	if ( !m_IsInitialized ) return;

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );
}

// Dear ImGui本体と各バックエンドを終了する。
void ImGuiManager::Uninit()
{
	// 初期化前、初期化失敗後、二重終了時はBackendを終了しない。
	if ( !m_IsInitialized ) return;

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	m_IsInitialized = false;
}