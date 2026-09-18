#pragma once

//========= C++標準ライブラリ インクルード=========
#include <cstdint>

// アプリケーション全体で使用する変更不可の設定値を定義する。
namespace Config
{
	//========= ウィンドウ設定定数=========
	// ウィンドウの横幅と縦幅。
	constexpr std::uint32_t WINDOW_WIDTH = 1280;
	constexpr std::uint32_t WINDOW_HEIGHT = 720;

	// Win32ウィンドウクラス名とタイトル文字列。
	constexpr wchar_t WINDOW_CLASS_NAME[] = L"GunfireLikeWindowClass";
	constexpr wchar_t WINDOW_TITLE[] = L"GunfireLike DX11";

	//========= フレーム時間設定定数=========
	// フレーム停止時にゲーム進行が跳ねないよう制限する最大DeltaTime。
	constexpr float MAX_DELTA_TIME = 0.1f;
}