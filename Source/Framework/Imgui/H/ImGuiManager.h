#pragma once

// Dear ImGui本体ContextとWin32・DirectX 11バックエンドの初期化・フレーム制御・明示終了を提供する。
// UI内容、Windowメッセージ転送、Render Targetの準備、Presentは担当しない。Windowと描画基盤を所有しない。
// Contextのポインターを保存せず現在のContextを操作する。複数Managerや外部Context切替との併用は前提にしない。

//========= 前方宣言=========
class GraphicsSystem;

// アプリケーションのDear ImGuiライフサイクルを管理する。破棄時の自動Uninitは定義せず、利用側で明示終了する。
class ImGuiManager final
{
public:
  //========= 初期化・終了関数=========
  // 本体ContextとDark Styleを準備し、Win32→DX11の順に初期化する。
  // windowHandle: 有効なHWNDをvoid*として受け取る。Windowは所有せず、バックエンド使用中は有効に保つ。
  // graphics: 有効なDevice・即時Contextの取得先。借用し、このManagerは参照自体を保持しない。
  // 戻り値: 初期化済み、または両バックエンドの成功でtrue。検出したバックエンド失敗でfalse。
  // 初期化済みなら新引数を検証・反映しない。DeviceやWindowの切替はUninit後に別途初期化する。
  // 失敗時はその時点で成功済みの段階を終了する。Context生成結果の独自判定、例外捕捉、ログは追加しない。
  bool Init(void* windowHandle,GraphicsSystem& graphics);
  // DX11→Win32→本体Contextの順に終了し、完了フラグを解除する。
  // 初期化前・検出した初期化失敗後・終了済みなら何もしない。GraphicsSystem終了前に呼ぶ。
  // 本体Contextを使うフレーム区間を終えてから呼ぶ。GPU完了待機や外部Contextへの切替は行わない。
  void Uninit();

  //========= フレーム制御関数=========
  // DX11・Win32・ImGui本体の順にフレーム準備を行う。初期化していなければ何もしない。
  // 利用側はBeginFrame→UI構築→EndFrameを対応させる。多重開始を検出する専用フラグは持たない。
  void BeginFrame();
  // UI構築を描画データへ確定し、DX11バックエンドへ描画を要求する。未初期化なら何もしない。
  // BeginFrame後に呼ぶ。描画先を利用側で準備し、描画後のPresentも利用側で行う。
  // このManager独自の描画State保存・復元、描画成否の返却、未開始Endの防止は行わない。
  void EndFrame();

private:
  //========= 初期化状態=========
  // 既定falseは通常フレーム・終了を許可しない状態。両バックエンドの成功後だけtrueへ移す。
  // 部分初期化の段階やFrame開始済み状態を表すフラグではない。Uninitでfalseに戻す。
  bool m_isInitialized{};
};