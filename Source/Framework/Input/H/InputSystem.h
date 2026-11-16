#pragma once

// Win32のキー状態を保存し、FPS操作用の中央基準マウス差分とカーソルの表示・位置・領域制限を管理する。
// Windowは所有しない。メッセージ転送とフレーム更新は呼出側の責務で、Win32 APIの成否は検証しない。

//========= Windows インクルード=========
#include <windows.h> // HWND・POINT・BYTE等の型と、キー取得・座標変換・カーソル操作のWin32 APIを使用する。
#include <windowsx.h> // cppのGET_X_LPARAM・GET_Y_LPARAMで、マウスメッセージから符号付き座標を取り出す。

// キーの押下・立ち上がり判定と、FPSカメラへ渡すマウス差分の消費を提供する。
class InputSystem final
{
public:
	//========= 初期化・更新関数=========
	// 入力先を登録し、現在キーを前回キーにもコピーして中央位置を計算する。差分やCapture設定はリセットしない。
	// windowHandle: 入力対象の有効なWindow。所有しない。入力・カーソル操作中は呼出側が有効に保つ。
	void Initialize(HWND windowHandle);
	// 前回値を保存して新しいキー状態を取得する。Capture中は中央・制限領域・カーソル位置も更新する。
	// Sceneがトリガーを判定する前に毎フレーム呼ぶ。蓄積したマウス差分はここではクリアしない。
	void Update();
	// Captureを解除し、Window登録・差分・中央・両キー配列をクリアする。Window自体は破棄しない。
	// Captureが既にfalseならカーソル制御は再実行しない。OS全体の設定を無条件に復元する処理ではない。
	void Uninit();
	// Capture中のマウス移動を蓄積し、フォーカス喪失で設定を解除する。解除中は全メッセージを無視する。
	// message: WindowProcedureから転送するWin32メッセージ番号。
	// wParam: 公開契約を維持するメッセージ追加情報。現行処理では使用しない。
	// lParam: WM_MOUSEMOVEでクライアント座標のX・Yを抽出する値。ここでは他メッセージの追加情報に使わない。
	void OnWindowMessage(UINT message,WPARAM wParam,LPARAM lParam);

	//========= キー入力Getter関数=========
	// 保存済み現在値の上位bit(0x80)が立っていれば押下中。下位のトグル状態は判定しない。
	// keyCode: 256要素の配列へ対応する0〜255の仮想キーコード。状態を消費しない。
	[[nodiscard]] bool IsKeyPressed(unsigned char keyCode) const { return (m_currentKeys[keyCode] & 0x80) != 0; }
	// 現在は押下中かつ前回は非押下ならtrue。繰り返し呼んでも判定結果を消費しない。
	// keyCode: 比較する0〜255の仮想キーコード。Update前後の保存値を使用する。
	[[nodiscard]] bool IsKeyTriggered(unsigned char keyCode) const
	{
		// 0x80のマスクで上位bitだけを取り出し、現在の押下と前回の非押下を組み合わせる。
		return (m_currentKeys[keyCode] & 0x80) != 0 && (m_previousKeys[keyCode] & 0x80) == 0;
	}

	//========= マウス入力Getter関数=========
	// 中央基準で蓄積した差分を値で返し、内部差分を0へ戻す。新規入力がなければ次の取得は(0,0)。
	// 戻り値: スクリーン座標へ変換したマウス位置と中央との差の累積値。右向きX・下向きY、単位はpixel。
	[[nodiscard]] POINT ConsumeMouseDelta();
	// FPS操作用の表示・中央固定・領域制限を要求している設定かを返す。OS操作の成功を保証する値ではない。
	[[nodiscard]] bool IsMouseCaptureEnabled() const { return m_isMouseCaptureEnabled; }

	//========= マウス操作設定関数=========
	// 設定変更時だけ差分を破棄し、カーソル表示・中央固定・領域制限を切り替える。
	// isEnabled: trueで非表示・中央合わせ・領域制限、falseで表示と領域制限解除。同値なら何もしない。
	// OSのSetCapture・ReleaseCaptureは使用しない。Win32 API失敗時の設定巻き戻しも行わない。
	void SetMouseCaptureEnabled(bool isEnabled);

private:
	//========= 補助関数=========
	// クライアント領域の整数中央を計算し、スクリーン座標に変換して保持する。有効なWindowが前提。
	void UpdateMouseCenter();
	// 保持済みスクリーン座標へ移動を要求する。中央の再計算・成否判定・マウス差分のクリアは行わない。
	void ResetMousePosition() { SetCursorPos(m_screenCenter.x,m_screenCenter.y); }
	// クライアント矩形の両端をスクリーン座標へ変換し、ClipCursorへ渡す。以前の制限矩形は保存しない。
	void ClipMouseCursor();

	//========= ウィンドウ情報=========
	// Initializeで借用するWindowハンドル。既定は未登録、Uninitでnullptrへ戻す。
	HWND m_windowHandle{};

	//========= キーボード状態=========
	// 仮想キー0〜255に対応する256バイトの最新状態。既定は全0。押下判定では各要素の上位bitだけを使う。
	BYTE m_currentKeys[256]{};
	// 新しい状態を取得する直前のコピー。初期登録時は現在値と同一、Uninitでは全0にする。
	BYTE m_previousKeys[256]{};

	//========= マウス状態=========
	// 各WM_MOUSEMOVEの中央基準差分を加算する。既定は(0,0)、消費・設定変更・Uninitでクリアする。
	POINT m_mouseDelta{};
	// クライアント領域中央をスクリーン座標のpixelで保持する。既定は(0,0)、生成や表示サイズを表す値ではない。
	POINT m_screenCenter{};
	// 既定falseは自由カーソル操作。Window Captureの取得状態やOS操作の成否を表すものではない。
	bool m_isMouseCaptureEnabled{};
};