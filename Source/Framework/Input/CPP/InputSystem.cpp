#include "../H/InputSystem.h"

// キー状態の更新、Win32メッセージからのマウス差分蓄積、カーソル制御と終了を実装する。
// Win32 APIの成否確認や入力方式の変更は追加せず、既存の処理順を維持する。

//========= C++標準ライブラリ インクルード=========
#include <cstring>

//========= 初期化・更新関数=========
// 参照先Windowとキー判定の基準状態を登録する。
void InputSystem::Initialize(HWND windowHandle)
{
	// 初回の現在値と前回値を一致させ、登録時点の押下を差分なしの状態にする。
	m_windowHandle = windowHandle;
	GetKeyboardState(m_currentKeys);
	std::memcpy(m_previousKeys,m_currentKeys,sizeof(m_currentKeys));

	// マウス座標変換に使う中央位置を、Windowの現在のクライアント領域から求める。
	UpdateMouseCenter();
}

// キー状態を更新し、FPS操作中はカーソル制御を更新する。
void InputSystem::Update()
{
	// 新しいキー状態を取得する前に現在値を保存し、押下開始の比較元を残す。
	std::memcpy(m_previousKeys,m_currentKeys,sizeof(m_currentKeys));
	GetKeyboardState(m_currentKeys);

	// 自由なカーソル操作中もキー取得は行うが、中央固定と移動制限は適用しない。
	if(!m_isMouseCaptureEnabled)return;

	// Window移動やResizeを反映した領域へ制限し、画面端で視点回転が止まらないよう中央へ戻す。
	UpdateMouseCenter();
	ClipMouseCursor();
	ResetMousePosition();
}

// 入力制限を解除してから、登録先と保持状態をクリアする。
void InputSystem::Uninit()
{
	// Window参照を消す前に、現在のFPS操作設定を解除する。
	SetMouseCaptureEnabled(false);

	// 外部Windowへの非所有参照と、座標に関する保持値を解除する。
	m_windowHandle = nullptr;
	m_mouseDelta = {};
	m_screenCenter = {};

	// 両方の配列を0へ戻し、終了後に以前の押下状態を残さない。
	std::memset(m_currentKeys,0,sizeof(m_currentKeys));
	std::memset(m_previousKeys,0,sizeof(m_previousKeys));
}

// FPS操作中に必要なWindowメッセージだけを処理する。
void InputSystem::OnWindowMessage(UINT message,WPARAM wParam,LPARAM lParam)
{
	// 解除中のマウス操作は差分へ取り込まない。wParamは現行処理では使用しない。
	if(!m_isMouseCaptureEnabled)return;

	// WM_MOUSEMOVEで差分を蓄積し、フォーカス喪失時は操作設定を解除する。
	switch(message){
	case WM_MOUSEMOVE:
	{
		// 符号付きのクライアント座標を取り出し、保持する中央座標と同じスクリーン座標へ揃える。
		POINT mouse_position{};
		mouse_position.x = GET_X_LPARAM(lParam);
		mouse_position.y = GET_Y_LPARAM(lParam);
		ClientToScreen(m_windowHandle,&mouse_position);

		// 各メッセージの中央基準差分を加算する。直前のマウス位置との差分へは変更しない。
		m_mouseDelta.x += mouse_position.x - m_screenCenter.x;
		m_mouseDelta.y += mouse_position.y - m_screenCenter.y;
		break;
	}

	case WM_KILLFOCUS:
	// 操作対象から外れた場合は表示と移動制限を戻し、蓄積差分を破棄する。
	SetMouseCaptureEnabled(false);
	break;

	default:
	// このSystemが担当しないメッセージでは状態を変更しない。
	break;
	}
}

//========= マウス入力Getter関数=========
// 保持している差分を一度取り出し、次の消費へ持ち越さないようにする。
POINT InputSystem::ConsumeMouseDelta()
{
	// 返却用のコピーを作ってから内部値をクリアし、同じ差分の重複使用を避ける。
	const POINT mouse_delta = m_mouseDelta;
	m_mouseDelta = {};
	return mouse_delta;
}

//========= マウス操作設定関数=========
// モード切替に伴う差分リセットと、カーソルの表示・位置・移動制限を設定する。
void InputSystem::SetMouseCaptureEnabled(bool isEnabled)
{
	// 同じ設定ではShowCursorのカウンター操作や差分リセットを繰り返さない。
	if(m_isMouseCaptureEnabled == isEnabled)return;

	// 前モードで蓄積した入力が、新モードで視点更新へ混入しないよう破棄する。
	m_isMouseCaptureEnabled = isEnabled;
	m_mouseDelta = {};

	// FPS操作を開始する場合は、表示カウンターを非表示側へ進めてから領域と位置を設定する。
	if(m_isMouseCaptureEnabled){
		// ShowCursorは表示カウンターを変えるため、非表示を示す負値になるまで繰り返す。
		while(ShowCursor(FALSE) >= 0){}

		// 非表示化後に中央座標と制限領域を更新し、開始位置を中央へ合わせる。
		UpdateMouseCenter();
		ClipMouseCursor();
		ResetMousePosition();
		return;
	}

	// 解除時は表示カウンターが0以上になるまで進め、領域制限も解除する。
	while(ShowCursor(TRUE) < 0){}
	ClipCursor(nullptr);
}

//========= 補助関数=========
// Windowのクライアント領域から、FPS操作の中心となるスクリーン座標を計算する。
void InputSystem::UpdateMouseCenter()
{
	// クライアント領域の幅・高さから、整数座標の中央を求める。
	RECT client_rect{};
	GetClientRect(m_windowHandle,&client_rect);
	POINT client_center{};
	client_center.x = (client_rect.right - client_rect.left) / 2;
	client_center.y = (client_rect.bottom - client_rect.top) / 2;

	// SetCursorPosやマウス差分計算と同じ座標系へ変換して保持する。
	ClientToScreen(m_windowHandle,&client_center);
	m_screenCenter = client_center;
}

// クライアント領域をスクリーン座標の移動可能領域として登録する。
void InputSystem::ClipMouseCursor()
{
	// Window内の左上・右下の座標を用意する。
	RECT client_rect{};
	GetClientRect(m_windowHandle,&client_rect);
	POINT clip_top_left{client_rect.left,client_rect.top};
	POINT clip_bottom_right{client_rect.right,client_rect.bottom};

	// ClipCursorが受け取るスクリーン座標へ両端点を変換する。
	ClientToScreen(m_windowHandle,&clip_top_left);
	ClientToScreen(m_windowHandle,&clip_bottom_right);

	// 変換した端点から矩形を作り、カーソルの移動範囲に設定する。
	const RECT clip_rect{clip_top_left.x,clip_top_left.y,clip_bottom_right.x,clip_bottom_right.y};
	ClipCursor(&clip_rect);
}