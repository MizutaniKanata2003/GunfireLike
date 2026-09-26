#pragma once

//========= Windows インクルード=========
#include <windows.h>
#include <windowsx.h>

// キーボードとマウスの入力状態を管理する。
class InputSystem final
{
public:
	//========= 初期化・更新関数=========
	// 入力対象のウィンドウを登録し、現在と前フレームのキー状態を初期化する。
	void Initialize( HWND windowHandle );
	// キー状態、マウスキャプチャ、カーソル位置をフレームごとに更新する。
	void Update();
	// InputSystemが設定したカーソル制限とマウスキャプチャ状態を解除する。
	void Uninit();
	// WindowProcedureから受け取ったWin32メッセージを処理する。
	void OnWindowMessage( UINT message, WPARAM wParam, LPARAM lParam );

	//========= キー入力Getter関数=========
	// 指定したキーが現在押され続けているかを返す。
	[[nodiscard]] bool IsKeyPressed( unsigned char keyCode ) const;
	// 指定したキーがこのフレームで押された瞬間かを返す。
	[[nodiscard]] bool IsKeyTriggered( unsigned char keyCode ) const;

	//========= マウス入力Getter関数=========
	// WndProcで蓄積したマウス移動量を返し、内部の差分をリセットする。
	[[nodiscard]] POINT ConsumeMouseDelta();
	// FPS操作モードが有効かを返す。
	[[nodiscard]] bool IsMouseCaptureEnabled() const
	{
		return m_IsMouseCaptureEnabled;
	}

	//========= マウス操作設定関数=========
	// FPS操作モードの有効・無効を切り替える。
	void SetMouseCaptureEnabled( bool isEnabled );

private:
	//========= 補助関数=========
	// クライアント領域の中央をスクリーン座標へ変換して保持する。
	void UpdateMouseCenter();
	// FPS操作中にカーソルをクライアント領域中央へ戻す。
	void ResetMousePosition();
	// FPS操作中にカーソルがウィンドウ外へ出ないよう制限する。
	void ClipMouseCursor();

	//========= ウィンドウ情報=========
	// 入力対象として登録したWin32ウィンドウ。
	HWND m_WindowHandle{};

	//========= キーボード状態=========
	// 現フレームのキーボード状態。
	BYTE m_CurrentKeys[ 256 ]{};
	// 前フレームのキーボード状態。
	BYTE m_PreviousKeys[ 256 ]{};

	//========= マウス状態=========
	// WndProcから蓄積した中央座標基準のマウス移動量。
	POINT m_MouseDelta{};
	// クライアント領域中央のスクリーン座標。
	POINT m_ScreenCenter{};
	// FPS操作としてカーソルを非表示・固定するかを示すフラグ。
	bool m_IsMouseCaptureEnabled{};
};