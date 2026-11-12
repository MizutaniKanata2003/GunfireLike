#pragma once

// Window生成とゲーム時間の更新で共有する、コンパイル時に確定した設定値を定義する。
// 実行時の設定変更やファイル読込は担当しない。既存の公開名と値は互換性のため維持する。

//========= C++標準ライブラリ インクルード=========
#include <cstdint>

// アプリケーション共通の固定設定をまとめる。
// Configと各定数の名前は既存参照を維持するため、namespace・ns_命名規則への移行対象として残す。
namespace Config
{
	//========= ウィンドウ設定定数=========
	// 起動時に指定するクライアント領域と描画先の基準幅。単位はpixel。
	constexpr std::uint32_t WINDOW_WIDTH = 1280;
	// 起動時に指定するクライアント領域と描画先の基準高さ。単位はpixel。
	constexpr std::uint32_t WINDOW_HEIGHT = 720;
	// Win32のWindow Class登録・Window生成・Class登録解除で共有する識別名。
	constexpr wchar_t WINDOW_CLASS_NAME[] = L"GunfireLikeWindowClass";
	// Window生成時に指定するタイトル文字列。
	constexpr wchar_t WINDOW_TITLE[] = L"GunfireLike DX11";

	//========= フレーム時間設定定数=========
	// GameTimerがTimeScaleを掛ける前に実時間へ適用する上限。単位は秒。
	// 長いフレーム停止による一度のゲーム時間の増加を制限する。最終DeltaTimeの上限ではない。
	constexpr float MAX_DELTA_TIME = 0.1f;
}