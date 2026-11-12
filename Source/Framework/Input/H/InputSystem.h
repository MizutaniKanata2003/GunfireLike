#pragma once

// Win32のキー状態と、FPS操作用のマウス差分・カーソル表示・移動制限を管理する。
// Windowは所有せず、メッセージ転送とフレームごとのUpdateは呼出側が担当する。

//========= Windows インクルード=========
#include <windows.h>
#include <windowsx.h>

// 現在・前回のキー状態による押下判定と、中央座標基準のマウス入力を提供する。
class InputSystem final
{
public:
	//========= 初期化・更新関数=========
	// Windowと現在のキー状態を登録し、前回状態へ同じ値をコピーして中央座標を計算する。
	// windowHandle: 有効な入力対象Window。所有しない。入力処理中は有効であること。
	// FPS操作フラグや蓄積済み差分をリセットする処理は行わない。
	void Initialize(HWND windowHandle);
	// 前回・現在のキー状態を更新し、FPS操作中だけ中央座標・移動制限・カーソル位置を更新する。
	// Sceneでキーのトリガー判定を行う前に、毎フレーム呼び出す。
	void Update();
	// FPS操作を解除し、Window参照・マウス差分・中央座標・両キー配列をクリアする。
	// Windowの破棄やSystem自身の破棄は行わない。
	void Uninit();
	// FPS操作中のマウス移動を蓄積し、フォーカス喪失時にはFPS操作を解除する。
	// message: WindowProcedureから渡すWin32メッセージ番号。
	// wParam: メッセージ固有情報。現行実装では参照しないが公開引数を維持する。
	// lParam: WM_MOUSEMOVEではクライアント座標のX・Yを取り出す。
	// FPS操作解除中はすべてのメッセージを処理せず戻る。
	void OnWindowMessage(UINT message,WPARAM wParam,LPARAM lParam);

	//========= キー入力Getter関数=========
	// 現在のキー状態の上位bitが立っている場合にtrueを返す。
	// keyCode: 0〜255の仮想キーコード。Updateで取得した状態を参照する。
	[[nodiscard]] bool IsKeyPressed(unsigned char keyCode) const { return (m_currentKeys[keyCode] & 0x80) != 0; }
	// 現在は押下中で、前回は押下されていなかった場合にtrueを返す。
	// keyCode: 0〜255の仮想キーコード。判定によってキー状態を消費しない。
	[[nodiscard]] bool IsKeyTriggered(unsigned char keyCode) const
	{
		// 上位bitだけを比較し、押下の立ち上がりを検出する。
		return (m_currentKeys[keyCode] & 0x80) != 0 && (m_previousKeys[keyCode] & 0x80) == 0;
	}

	//========= マウス入力Getter関数=========
	// メッセージ処理で蓄積した差分を返し、その場で両成分を0へ戻す。
	// 戻り値: スクリーン座標の中央基準の累積差分。消費後に再度呼べば新規入力がない限り0。
	[[nodiscard]] POINT ConsumeMouseDelta();
	// FPS操作としてカーソル非表示・中央固定・領域制限を行う設定ならtrueを返す。
	[[nodiscard]] bool IsMouseCaptureEnabled() const { return m_isMouseCaptureEnabled; }

	//========= マウス操作設定関数=========
	// FPS操作と自由なカーソル操作を切り替える。同じ設定値なら何もしない。
	// isEnabled: trueで非表示・中央固定・領域制限、falseで表示と領域制限解除を行う。
	// 切替時は蓄積差分を破棄する。OSのSetCapture・ReleaseCaptureは呼ばない。
	void SetMouseCaptureEnabled(bool isEnabled);

private:
	//========= 補助関数=========
	// クライアント領域中央を求め、スクリーン座標へ変換して保持する。有効なWindowが前提。
	void UpdateMouseCenter();
	// 計算済みのスクリーン座標へカーソルを移動する。中央座標の再計算は行わない。
	void ResetMousePosition() { SetCursorPos(m_screenCenter.x,m_screenCenter.y); }
	// Windowのクライアント領域をスクリーン座標へ変換し、カーソルの移動可能領域に設定する。
	void ClipMouseCursor();

	//========= ウィンドウ情報=========
	// 入力対象Windowへの非所有ハンドル。Initializeで登録し、Uninitでnullptrへ戻す。
	HWND m_windowHandle{};

	//========= キーボード状態=========
	// 最後に取得した256キー分の状態。上位bitを押下判定へ使用する。
	BYTE m_currentKeys[256]{};
	// Update直前のキー状態。現在値との比較によりトリガーを判定する。
	BYTE m_previousKeys[256]{};

	//========= マウス状態=========
	// 中央座標からの移動差分の累積値。ConsumeMouseDeltaまたはモード切替でクリアする。
	POINT m_mouseDelta{};
	// FPS操作の基準となるクライアント領域中央。スクリーン座標で保持する。
	POINT m_screenCenter{};
	// FPS操作を有効にする設定。OSのWindow Capture取得状態そのものではない。
	bool m_isMouseCaptureEnabled{};
};