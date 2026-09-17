#include "Framework/Input/H/InputSystem.h"

#include <cstring>

// 入力対象のウィンドウを登録し、現在と前フレームのキー状態を初期化する。
void InputSystem::Initialize( HWND windowHandle )
{
	m_WindowHandle = windowHandle;

	// 起動直後に押下開始として判定されないよう、現在と前フレームの状態を一致させる。
	GetKeyboardState( m_CurrentKeys );
	std::memcpy( m_PreviousKeys, m_CurrentKeys, sizeof( m_CurrentKeys ) );

	// FPS操作で使用するカーソルの中央座標を計算する。
	UpdateMouseCenter();
}

// キー状態、マウスキャプチャ、カーソル位置をフレームごとに更新する。
void InputSystem::Update()
{
	// 前フレーム状態を保存してから現在のキーボード状態を取得する。
	std::memcpy( m_PreviousKeys, m_CurrentKeys, sizeof( m_CurrentKeys ) );
	GetKeyboardState( m_CurrentKeys );

	if ( !m_IsMouseCaptureEnabled ) return;

	// FPS操作中はカーソルを中央へ戻し、画面端で視点回転が止まらないようにする。
	UpdateMouseCenter();
	ClipMouseCursor();
	ResetMousePosition();
}

// WindowProcedureから受け取ったWin32メッセージを処理する。
void InputSystem::OnWindowMessage( UINT message, WPARAM wParam, LPARAM lParam )
{
	if ( !m_IsMouseCaptureEnabled ) return;

	switch ( message )
	{
		case WM_MOUSEMOVE:
		{
			// クライアント座標のマウス位置をスクリーン座標へ変換して差分を計算する。
			POINT mousePosition{};
			mousePosition.x = GET_X_LPARAM( lParam );
			mousePosition.y = GET_Y_LPARAM( lParam );

			ClientToScreen( m_WindowHandle, &mousePosition );

			m_MouseDelta.x += mousePosition.x - m_ScreenCenter.x;
			m_MouseDelta.y += mousePosition.y - m_ScreenCenter.y;
			break;
		}

		case WM_KILLFOCUS:
		// フォーカスを失った場合はカーソルを解放し、意図しない視点回転を防ぐ。
		SetMouseCaptureEnabled( false );
		break;

		default:
		break;
	}
}

// 指定したキーが現在押され続けているかを返す。
bool InputSystem::IsKeyPressed( unsigned char keyCode ) const
{
	return ( m_CurrentKeys[ keyCode ] & 0x80 ) != 0;
}

// 指定したキーがこのフレームで押された瞬間かを返す。
bool InputSystem::IsKeyTriggered( unsigned char keyCode ) const
{
	return ( m_CurrentKeys[ keyCode ] & 0x80 ) != 0 && ( m_PreviousKeys[ keyCode ] & 0x80 ) == 0;
}

// WndProcで蓄積したマウス移動量を返し、内部の差分をリセットする。
POINT InputSystem::ConsumeMouseDelta()
{
	const POINT mouseDelta = m_MouseDelta;
	m_MouseDelta = {};

	return mouseDelta;
}

// FPS操作モードの有効・無効を切り替える。
void InputSystem::SetMouseCaptureEnabled( bool isEnabled )
{
	if ( m_IsMouseCaptureEnabled == isEnabled ) return;

	m_IsMouseCaptureEnabled = isEnabled;
	m_MouseDelta = {};

	if ( m_IsMouseCaptureEnabled )
	{
		// FPS操作ではカーソルを非表示にし、中央固定で無限に視点回転できるようにする。
		while ( ShowCursor( FALSE ) >= 0 ) {}

		UpdateMouseCenter();
		ClipMouseCursor();
		ResetMousePosition();

		return;
	}

	// ImGui操作ではカーソルを表示し、ウィンドウ外も自由に移動できるようにする。
	while ( ShowCursor( TRUE ) < 0 ) {}

	ClipCursor( nullptr );
}

// クライアント領域の中央をスクリーン座標へ変換して保持する。
void InputSystem::UpdateMouseCenter()
{
	RECT clientRect{};
	GetClientRect( m_WindowHandle, &clientRect );

	POINT clientCenter{};
	clientCenter.x = ( clientRect.right - clientRect.left ) / 2;
	clientCenter.y = ( clientRect.bottom - clientRect.top ) / 2;

	ClientToScreen( m_WindowHandle, &clientCenter );
	m_ScreenCenter = clientCenter;
}

// FPS操作中にカーソルをクライアント領域中央へ戻す。
void InputSystem::ResetMousePosition()
{
	SetCursorPos( m_ScreenCenter.x, m_ScreenCenter.y );
}

// FPS操作中にカーソルがウィンドウ外へ出ないよう制限する。
void InputSystem::ClipMouseCursor()
{
	RECT clientRect{};
	GetClientRect( m_WindowHandle, &clientRect );

	POINT clipTopLeft{ clientRect.left, clientRect.top };
	POINT clipBottomRight{ clientRect.right, clientRect.bottom };

	ClientToScreen( m_WindowHandle, &clipTopLeft );
	ClientToScreen( m_WindowHandle, &clipBottomRight );

	// クライアント領域をスクリーン座標の制限矩形へ変換する。
	const RECT clipRect{
		clipTopLeft.x,
		clipTopLeft.y,
		clipBottomRight.x,
		clipBottomRight.y
	};

	ClipCursor( &clipRect );
}