#pragma once

class GraphicsSystem;

// Dear ImGuiのWin32入力処理とDirectX 11描画処理を管理する。
class ImGuiManager final
{
public:
	// Dear ImGui本体とWin32・DirectX 11バックエンドを初期化する。
	bool Init( void* windowHandle, GraphicsSystem& graphics );

	// Dear ImGuiの新しい描画フレームを開始する。
	void BeginFrame();

	// Dear ImGuiの描画データをDirectX 11へ出力する。
	void EndFrame();

	// Dear ImGui本体と各バックエンドを終了する。
	void Uninit();
};