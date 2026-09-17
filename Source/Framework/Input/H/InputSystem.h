#pragma once

#include <windows.h>
#include <windowsx.h>

// キーボードとマウスの入力状態を管理する。
class InputSystem final
{
public:
	// 入力対象のウィンドウを登録し、現在と前フレームのキー状態を初期化する。
	void Initialize( HWND windowHandle );
	// キー状態、マウスキャプチャ、カーソル位置をフレームごとに更新する。
	void Update();
	// WindowProcedureから受け取ったWin32メッセージを処理する。
	void OnWindowMessage( UINT message, WPARAM wParam, LPARAM lParam );

	// 指定したキーが現在押され続けているかを返す。
	[[nodiscard]] bool IsKeyPressed( unsigned char keyCode ) const;
	// 指定したキーがこのフレームで押された瞬間かを返す。
	[[nodiscard]] bool IsKeyTriggered( unsigned char keyCode ) const;
	// 指定したマウスボタンが現在押され続けているかを返す。
	[[nodiscard]] bool IsMouseButtonPressed( int virtualKeyCode ) const;
	// 指定したマウスボタンがこのフレームで押された瞬間かを返す。
	[[nodiscard]] bool IsMouseButtonTriggered( int virtualKeyCode ) const;
	// WndProcで蓄積したマウス移動量を返し、内部の差分をリセットする。
	[[nodiscard]] POINT ConsumeMouseDelta();

	// FPS操作モードの有効・無効を切り替える。
	void SetMouseCaptureEnabled( bool isEnabled );
	// FPS操作モードが有効かを返す。
	[[nodiscard]] bool IsMouseCaptureEnabled() const
	{
		return m_IsMouseCaptureEnabled;
	}

private:
	// クライアント領域の中央をスクリーン座標へ変換して保持する。
	void UpdateMouseCenter();
	// FPS操作中にカーソルをクライアント領域中央へ戻す。
	void ResetMousePosition();
	// FPS操作中にカーソルがウィンドウ外へ出ないよう制限する。
	void ClipMouseCursor();

	// 入力対象として登録したWin32ウィンドウ。
	HWND m_WindowHandle{};

	// 現フレームのキーボード状態。
	BYTE m_CurrentKeys[ 256 ]{};
	// 前フレームのキーボード状態。
	BYTE m_PreviousKeys[ 256 ]{};

	// WndProcから蓄積した中央座標基準のマウス移動量。
	POINT m_MouseDelta{};
	// クライアント領域中央のスクリーン座標。
	POINT m_ScreenCenter{};

	// FPS操作としてカーソルを非表示・固定するかを示すフラグ。
	bool m_IsMouseCaptureEnabled{};

	// 現フレームのマウスボタン状態。
	bool m_IsMouseButtons[ 3 ]{};
	// 前フレームのマウスボタン状態。
	bool m_PreviousMouseButtons[ 3 ]{};
};