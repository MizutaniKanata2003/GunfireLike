#include "../H/InputSystem.h"

// キー状態の保存、マウスメッセージの取込、表示カウンターとカーソル座標・領域制限を実装する。
// Raw Inputへの切替、API失敗の復旧、差分計算法の変更は行わず、既存の処理順と副作用を説明する。

//========= C++標準ライブラリ インクルード=========
#include <cstring> // std::memcpyで前回キーを保存し、std::memsetで終了時の両キー配列を0にする。

//========= 初期化・更新関数=========
// Windowとキー比較の初期状態を登録し、マウス制御に使う中央座標を計算する。
void InputSystem::Initialize(HWND windowHandle)
{
	// 現在値と前回値を一致させ、登録時点の押下を新規トリガーとしない。sizeofは要素数ではなく総バイト数。
	// APIの戻り値は判定しない。再呼出時にCapture・差分を初期化する処理も追加しない。
	m_windowHandle = windowHandle;
	GetKeyboardState(m_currentKeys);
	std::memcpy(m_previousKeys,m_currentKeys,sizeof(m_currentKeys));

	// カーソル位置ではなく、Windowのクライアント領域中央を変換して保持する。
	UpdateMouseCenter();
}

// 新しいキー状態を取得し、FPS操作中だけカーソルの領域と位置を更新する。
void InputSystem::Update()
{
	// 取得順を逆にすると前回値との比較が崩れるため、旧現在値をコピーしてから新現在値を読む。
	std::memcpy(m_previousKeys,m_currentKeys,sizeof(m_currentKeys));
	GetKeyboardState(m_currentKeys);

	// Capture解除中も上のキー更新は行うが、以後の中央固定・領域制限は実行しない。
	if(!m_isMouseCaptureEnabled)return;

	// Window移動やResize後の中央・領域を取り直してからカーソルを中央へ移す。蓄積差分は保持する。
	UpdateMouseCenter();
	ClipMouseCursor();
	ResetMousePosition();
}

// カーソル設定の解除を試みた後に、参照先と保存済み入力状態をクリアする。
void InputSystem::Uninit()
{
	// ハンドルを消す前にモードを解除する。既にfalseの場合は同値判定によりOS操作を繰り返さない。
	SetMouseCaptureEnabled(false);

	// Windowを所有していないため破棄せず、登録と座標・ホイール状態だけを空にする。
	m_windowHandle = nullptr;
	m_mouseDelta = {};
	m_mouseWheelDelta = 0;
	m_screenCenter = {};

	// 保存した押下状態を終了後へ持ち越さないよう、各配列の全バイトを0にする。
	std::memset(m_currentKeys,0,sizeof(m_currentKeys));
	std::memset(m_previousKeys,0,sizeof(m_previousKeys));
}

// WindowProcedureが転送したメッセージのうち、FPS操作用の移動・ホイール回転とフォーカス喪失だけを処理する。
void InputSystem::OnWindowMessage(UINT message,WPARAM wParam,LPARAM lParam)
{
	// 自由カーソル操作中は移動差分とホイール回転量を蓄積しない。他のメッセージもここでは扱わない。
	if(!m_isMouseCaptureEnabled)return;

	// 移動通知とホイール通知は蓄積へ取り込み、フォーカス喪失は操作解除へ振り分ける。
	switch(message){
		// マウス移動は符号付きクライアント座標からスクリーン座標へ揃えて扱う。
	case WM_MOUSEMOVE:
	{
		// LPARAMのX・Yを符号付きで抽出する。クライアント範囲外の負座標も、符号を失わず変換へ渡す。
		POINT mouse_position{};
		mouse_position.x = GET_X_LPARAM(lParam);
		mouse_position.y = GET_Y_LPARAM(lParam);
		ClientToScreen(m_windowHandle,&mouse_position);

		// 中央からの差を各通知ごとに加算する。右向きX・下向きYで、直前通知との差分を取る方式ではない。
		// 中央へ戻す操作に伴う通知も特別扱いせず、FpsCameraによる消費まで累積する既存方式を維持する。
		m_mouseDelta.x += mouse_position.x - m_screenCenter.x;
		m_mouseDelta.y += mouse_position.y - m_screenCenter.y;
		break;
	}

	// ホイール回転は、WPARAMの上位ワードに入った符号付き回転量を累積へ加算する。
	case WM_MOUSEWHEEL:
	// GET_WHEEL_DELTA_WPARAMで上位16bitを符号付きの値として取り出す。標準のマウスでは1ノッチが120で、正は奥への回転。
	m_mouseWheelDelta += GET_WHEEL_DELTA_WPARAM(wParam);
	break;

	// フォーカスを失った場合は表示・領域制限を解除し、蓄積差分とホイール回転量も破棄する。
	case WM_KILLFOCUS:
	SetMouseCaptureEnabled(false);
	break;

	// このSystemが担当しない通知では、保持状態を変更せず分岐を終える。
	default:
	break;
	}
}

//========= マウス入力Getter関数=========
// 累積した差分を一度取り出し、次の消費へ同じ値を持ち越さない。
POINT InputSystem::ConsumeMouseDelta()
{
	// 返却用コピーを先に作る。先に0へ戻すと今回の入力を失うため、この順序を維持する。
	const POINT mouse_delta = m_mouseDelta;
	m_mouseDelta = {};
	return mouse_delta;
}

// 累積したホイール回転量を一度取り出し、次の消費へ同じ値を持ち越さない。
int InputSystem::ConsumeMouseWheelDelta()
{
	// 返却用コピーを先に作る。先に0へ戻すと今回の入力を失うため、マウス差分と同じ順序にする。
	const int mouse_wheel_delta = m_mouseWheelDelta;
	m_mouseWheelDelta = 0;
	return mouse_wheel_delta;
}

//========= マウス操作設定関数=========
// 表示カウンターを調整し、FPS操作用の制限・中央合わせと自由操作を切り替える。
void InputSystem::SetMouseCaptureEnabled(bool isEnabled)
{
	// 同じ設定なら差分も消去しない。表示カウンターへの重複操作や領域の再設定を行わず戻る。
	if(m_isMouseCaptureEnabled == isEnabled)return;

	// 設定を先に反映して、前モードの累積差分とホイール回転量を破棄する。後続OS操作の成否による巻き戻しはしない。
	m_isMouseCaptureEnabled = isEnabled;
	m_mouseDelta = {};
	m_mouseWheelDelta = 0;

	// 有効化時は非表示にしてから中央・制限領域を更新する。この経路は最後にreturnし、解除処理へ進まない。
	if(m_isMouseCaptureEnabled){
		// FALSEで表示カウンターを減らす。戻り値が0以上の間繰り返し、負値になったら非表示側への調整を終える。
		// 空のループ本体でも条件式に副作用がある。単純な状態確認や待機へ置き換えない。
		while(ShowCursor(FALSE) >= 0){}

		// 現在のWindow位置に合わせて中央と制限を作り直し、開始位置を中央へ移動する。
		UpdateMouseCenter();
		ClipMouseCursor();
		ResetMousePosition();
		return;
	}

	// 無効化時はTRUEでカウンターを増やし、戻り値が負の間繰り返す。0以上になれば表示側への調整を終える。
	while(ShowCursor(TRUE) < 0){}
	// nullptrはカーソルの領域制限解除。以前の矩形を保存して復元する方式ではない。
	ClipCursor(nullptr);
}

//========= 補助関数=========
// クライアント領域の中央をスクリーン座標へ変換して、移動と差分計算の共通基準にする。
void InputSystem::UpdateMouseCenter()
{
	// 矩形の幅・高さを2で整数除算する。奇数サイズの端数は保持せず、既存の整数中央を使う。
	RECT client_rect{};
	GetClientRect(m_windowHandle,&client_rect);
	POINT client_center{};
	client_center.x = (client_rect.right - client_rect.left) / 2;
	client_center.y = (client_rect.bottom - client_rect.top) / 2;

	// SetCursorPosと比較先のmouse_positionに合わせ、Window内の座標を画面全体の座標へ変換する。
	// API失敗の判定は追加せず、結果をそのまま保持する。
	ClientToScreen(m_windowHandle,&client_center);
	m_screenCenter = client_center;
}

// クライアント領域をClipCursorが受け取るスクリーン座標の矩形へ変換する。
void InputSystem::ClipMouseCursor()
{
	// Window内の左上・右下を個別の点へ取り出す。ハンドルの有効性やAPIの成否は呼出時に検証しない。
	RECT client_rect{};
	GetClientRect(m_windowHandle,&client_rect);
	POINT clip_top_left{client_rect.left,client_rect.top};
	POINT clip_bottom_right{client_rect.right,client_rect.bottom};

	// 両端を同じスクリーン座標系へ揃える。クライアント矩形のままClipCursorへ渡さない。
	ClientToScreen(m_windowHandle,&clip_top_left);
	ClientToScreen(m_windowHandle,&clip_bottom_right);

	// 変換後の端点を矩形へまとめ、移動可能領域の設定を要求する。失敗の戻り値は確認しない。
	const RECT clip_rect{clip_top_left.x,clip_top_left.y,clip_bottom_right.x,clip_bottom_right.y};
	ClipCursor(&clip_rect);
}