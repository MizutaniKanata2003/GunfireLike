#include "Framework/Input/H/InputSystem.h"

//========= C++標準ライブラリ インクルード=========
#include <cstring>

// 入力対象のウィンドウを登録し、現在と前フレームのキー状態を初期化する。
void InputSystem::Initialize(HWND windowHandle)
{
	// 入力対象のウィンドウとキー状態を初期化する。
	m_windowHandle = windowHandle;
	GetKeyboardState(m_currentKeys);
	std::memcpy(m_previousKeys,m_currentKeys,sizeof(m_currentKeys));

	// FPS操作で使用するカーソルの中央座標を計算する。
	UpdateMouseCenter();
}

// キー状態、マウスキャプチャ、カーソル位置をフレームごとに更新する。
void InputSystem::Update()
{
	// 前フレーム状態を保存してから現在のキーボード状態を取得する。
	std::memcpy(m_previousKeys,m_currentKeys,sizeof(m_currentKeys));
	GetKeyboardState(m_currentKeys);

	if(!m_isMouseCaptureEnabled)return;

	// FPS操作中はカーソルを中央へ戻し、画面端で視点回転が止まらないようにする。
	UpdateMouseCenter();
	ClipMouseCursor();
	ResetMousePosition();
}

// InputSystemが設定したカーソル制限とマウスキャプチャ状態を解除する。
void InputSystem::Uninit()
{
	SetMouseCaptureEnabled(false);

	m_windowHandle = nullptr;
	m_mouseDelta = {};
	m_screenCenter = {};

	std::memset(m_currentKeys,0,sizeof(m_currentKeys));
	std::memset(m_previousKeys,0,sizeof(m_previousKeys));
}

// WindowProcedureから受け取ったWin32メッセージを処理する。
void InputSystem::OnWindowMessage(UINT message,WPARAM wParam,LPARAM lParam)
{
	if(!m_isMouseCaptureEnabled)return;

	switch(message){
	case WM_MOUSEMOVE:
	{
		// クライアント座標のマウス位置をスクリーン座標へ変換して差分を計算する。
		POINT mouse_position{};
		mouse_position.x = GET_X_LPARAM(lParam);
		mouse_position.y = GET_Y_LPARAM(lParam);

		ClientToScreen(m_windowHandle,&mouse_position);

		m_mouseDelta.x += mouse_position.x - m_screenCenter.x;
		m_mouseDelta.y += mouse_position.y - m_screenCenter.y;
		break;
	}

	case WM_KILLFOCUS:
	// フォーカスを失った場合はカーソルを解放し、意図しない視点回転を防ぐ。
	SetMouseCaptureEnabled(false);
	break;

	default:
	break;
	}
}

// WndProcで蓄積したマウス移動量を返し、内部の差分をリセットする。
POINT InputSystem::ConsumeMouseDelta()
{
	// 現フレームのマウス差分を返し、次フレームへ持ち越さないようリセットする。
	const POINT mouse_delta = m_mouseDelta;
	m_mouseDelta = {};

	return mouse_delta;
}

// FPS操作モードの有効・無効を切り替える。
void InputSystem::SetMouseCaptureEnabled(bool isEnabled)
{
	if(m_isMouseCaptureEnabled == isEnabled)return;

	// モード切替時に前モードのマウス差分を破棄する。
	m_isMouseCaptureEnabled = isEnabled;
	m_mouseDelta = {};

	if(m_isMouseCaptureEnabled){
		// FPS操作ではカーソルを非表示にし、中央固定で無限に視点回転できるようにする。
		while(ShowCursor(FALSE) >= 0){}

		UpdateMouseCenter();
		ClipMouseCursor();
		ResetMousePosition();

		return;
	}

	// ImGui操作ではカーソルを表示し、ウィンドウ外も自由に移動できるようにする。
	while(ShowCursor(TRUE) < 0){}

	ClipCursor(nullptr);
}

// クライアント領域の中央をスクリーン座標へ変換して保持する。
void InputSystem::UpdateMouseCenter()
{
	// クライアント領域の中央をクライアント座標で計算する。
	RECT client_rect{};
	GetClientRect(m_windowHandle,&client_rect);

	POINT client_center{};
	client_center.x = (client_rect.right - client_rect.left) / 2;
	client_center.y = (client_rect.bottom - client_rect.top) / 2;

	// SetCursorPosと比較できるよう、中央座標をスクリーン座標へ変換する。
	ClientToScreen(m_windowHandle,&client_center);
	m_screenCenter = client_center;
}

// FPS操作中にカーソルがウィンドウ外へ出ないよう制限する。
void InputSystem::ClipMouseCursor()
{
	// クライアント領域の左上と右下をスクリーン座標へ変換する。
	RECT client_rect{};
	GetClientRect(m_windowHandle,&client_rect);

	POINT clip_top_left{client_rect.left,client_rect.top};
	POINT clip_bottom_right{client_rect.right,client_rect.bottom};

	ClientToScreen(m_windowHandle,&clip_top_left);
	ClientToScreen(m_windowHandle,&clip_bottom_right);

	const RECT clip_rect{clip_top_left.x,clip_top_left.y,clip_bottom_right.x,clip_bottom_right.y};

	ClipCursor(&clip_rect);
}