#include "../H/ImGuiManager.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "Framework/DirectX/H/GraphicsSystem.h"

// Dear ImGui本体とWin32・DirectX 11バックエンドを初期化する。
bool ImGuiManager::Init( void* windowHandle, GraphicsSystem& graphics )
{
	IMGUI_CHECKVERSION();

	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	return ImGui_ImplWin32_Init( windowHandle ) &&
		ImGui_ImplDX11_Init( graphics.GetDevice(), graphics.GetContext() );
}

// Dear ImGuiの新しい描画フレームを開始する。
void ImGuiManager::BeginFrame()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

// Dear ImGuiの描画データをDirectX 11へ出力する。
void ImGuiManager::EndFrame()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );
}

// Dear ImGui本体と各バックエンドを終了する。
void ImGuiManager::Uninit()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}