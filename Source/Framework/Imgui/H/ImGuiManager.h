#pragma once

// Dear ImGui本体とWin32・DirectX 11バックエンドの初期化、フレーム制御、終了を提供する。
// UIの内容やWin32メッセージ転送は呼出側が担当する。Contextとバックエンドの終了はUninitで明示的に行う。

//========= 前方宣言=========
class GraphicsSystem;

// アプリケーションで使用するDear ImGuiのライフサイクルを管理する。
// 現在のContextを対象に操作するため、複数Managerや外部のContext切替を併用する設計ではない。
class ImGuiManager final
{
public:
	//========= 初期化・終了関数=========
	// Contextを生成してDark Styleを設定し、Win32、DirectX 11の順に初期化する。
	// windowHandle: Win32バックエンドへ渡す有効なHWNDをvoid*で受け取る。ウィンドウは所有しない。
	// graphics: 初期化済みDeviceとContextを提供する描画基盤。ImGui終了までResourceを有効に保つ。
	// 戻り値: 初期化済み、または両バックエンドの初期化成功ならtrue。バックエンド初期化失敗ならfalse。
	// 失敗時は、それまでに成功した初期化を取り消す。初期化済みなら引数の変更は反映しない。
	bool Init(void* windowHandle,GraphicsSystem& graphics);
	// 描画バックエンド、Win32バックエンド、Contextの順で終了する。
	// 初期化前・初期化失敗後・終了済みの場合は何もしない。GraphicsSystemの終了前に呼ぶ。
	void Uninit();

	//========= フレーム制御関数=========
	// バックエンドのフレーム情報を更新し、UI構築を開始する。未初期化なら何もしない。
	// 正常な呼出順はBeginFrame、呼出側のUI構築、EndFrame。途中で再度BeginFrameを呼ばない。
	void BeginFrame();
	// UI構築結果を確定し、DirectX 11バックエンドで描画する。未初期化なら何もしない。
	// BeginFrame後に呼ぶ。Render Targetなどの描画先準備とPresentは呼出側の責務。
	void EndFrame();

private:
	//========= 初期化状態=========
	// 本体と両バックエンドの初期化がすべて完了したか。部分初期化の状態は保持しない。
	bool m_isInitialized{};
};