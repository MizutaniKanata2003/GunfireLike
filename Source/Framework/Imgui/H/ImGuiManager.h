#pragma once

//========= 前方宣言=========
class GraphicsSystem;

// Dear ImGuiのWin32入力処理とDirectX 11描画処理を管理する。
class ImGuiManager final
{
public:
	//========= 初期化・終了関数=========
	// Dear ImGui本体とWin32・DirectX 11バックエンドを初期化する。
	bool Init( void* windowHandle, GraphicsSystem& graphics );
	// Dear ImGui本体と各バックエンドを終了する。
	void Uninit();

	//========= フレーム制御関数=========
	// Dear ImGuiの新しい描画フレームを開始する。
	void BeginFrame();
	// Dear ImGuiの描画データをDirectX 11へ出力する。
	void EndFrame();

private:
	//========= 初期化状態=========
	// Dear ImGui本体とWin32・DirectX 11バックエンドの初期化が完了しているかを保持する。
	bool m_IsInitialized{};
};