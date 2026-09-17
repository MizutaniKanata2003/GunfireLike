#pragma once

#include <cstdint>

namespace Config
{
	// ウィンドウの横幅。
	constexpr std::uint32_t WINDOW_WIDTH = 1280;

	// ウィンドウの縦幅。
	constexpr std::uint32_t WINDOW_HEIGHT = 720;

	// Win32ウィンドウクラス名。
	constexpr wchar_t WINDOW_CLASS_NAME[] = L"GunfireLikeWindowClass";

	// ウィンドウのタイトル文字列。
	constexpr wchar_t WINDOW_TITLE[] = L"GunfireLike DX11";

	// フレーム停止時にゲーム進行が跳ねないよう制限する最大DeltaTime。
	constexpr float MAX_DELTA_TIME = 0.1f;
}