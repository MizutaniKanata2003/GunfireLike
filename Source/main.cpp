#include <windows.h>
#include <objbase.h>

#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/AppConfig.h"
#include "Framework/Etc/H/GameTimer.h"
#include "Framework/Imgui/H/ImGuiManager.h"
#include "Framework/Input/H/InputSystem.h"

#include "Scene/Common/H/SceneManager.h"

#include "imgui.h"
#include "imgui_impl_win32.h"

// Win32のウィンドウメッセージをDear ImGuiへ渡す関数。
// Dear ImGui公式のWin32サンプルと同じく、明示的に前方宣言する。
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam );

// Win32ウィンドウのメッセージを処理する。
// InputSystemへの入力転送、ImGuiへの入力転送、終了要求の処理だけを担当する。
LRESULT CALLBACK WindowProcedure( HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam )
{
	InputSystem* inputSystem = reinterpret_cast<InputSystem*>( GetWindowLongPtrW( windowHandle, GWLP_USERDATA ) );

	if ( inputSystem != nullptr )inputSystem->OnWindowMessage( message, wParam, lParam );

	// FPS操作モードではゲーム側がマウスを使う。
	// ImGui操作モードだけDear ImGuiへ入力を渡す。
	if ( inputSystem == nullptr || !inputSystem->IsMouseCaptureEnabled() )
	{
		if ( ImGui_ImplWin32_WndProcHandler( windowHandle, message, wParam, lParam ) )
		{
			return 1;
		}
	}

	switch ( message )
	{
		case WM_DESTROY:
		PostQuitMessage( 0 );
		return 0;

		default:
		return DefWindowProcW( windowHandle, message, wParam, lParam );
	}
}

// Windowsアプリケーションのエントリーポイント。
int WINAPI WinMain( HINSTANCE instance, HINSTANCE, LPSTR, int showCommand )
{
	// AudioEngineが使用するMTAモデルと統一する。
	// COINIT_APARTMENTTHREADED (STA) は使用しない。
	const HRESULT comResult = CoInitializeEx( nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE );

	if ( FAILED( comResult ) )return -100;

	WNDCLASSEXW windowClass{};
	windowClass.cbSize = sizeof( windowClass );
	windowClass.hInstance = instance;
	windowClass.lpfnWndProc = WindowProcedure;
	windowClass.lpszClassName = Config::WINDOW_CLASS_NAME;
	windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );

	if ( RegisterClassExW( &windowClass ) == 0 )
	{
		CoUninitialize();
		return -1;
	}

	RECT windowRect
	{
		0,
		0,
		static_cast<LONG>( Config::WINDOW_WIDTH ),
		static_cast<LONG>( Config::WINDOW_HEIGHT )
	};

	AdjustWindowRect( &windowRect, WS_OVERLAPPEDWINDOW, FALSE );

	HWND windowHandle =
		CreateWindowExW(
			0,
			Config::WINDOW_CLASS_NAME,
			Config::WINDOW_TITLE,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			windowRect.right - windowRect.left,
			windowRect.bottom - windowRect.top,
			nullptr,
			nullptr,
			instance,
			nullptr );

	if ( windowHandle == nullptr )
	{
		UnregisterClassW( Config::WINDOW_CLASS_NAME, instance );

		CoUninitialize();
		return -2;
	}

	GraphicsSystem graphicsSystem;

	if ( !graphicsSystem.Init( windowHandle, Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT ) )
	{
		DestroyWindow( windowHandle );

		UnregisterClassW( Config::WINDOW_CLASS_NAME, instance );

		CoUninitialize();
		return -3;
	}

	InputSystem inputSystem;
	inputSystem.Initialize( windowHandle );

	AudioSystem audioSystem;

	if ( !audioSystem.Initialize() )
	{
		graphicsSystem.Uninit();

		DestroyWindow( windowHandle );

		UnregisterClassW( Config::WINDOW_CLASS_NAME, instance );

		CoUninitialize();
		return -4;
	}

	SetWindowLongPtrW( windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( &inputSystem ) );

	GameTimer gameTimer;
	gameTimer.Initialize();

	ImGuiManager imguiManager;

	if ( !imguiManager.Init( windowHandle, graphicsSystem ) )
	{
		audioSystem.Uninit();
		graphicsSystem.Uninit();

		DestroyWindow( windowHandle );

		UnregisterClassW( Config::WINDOW_CLASS_NAME, instance );

		CoUninitialize();
		return -5;
	}

	ShowWindow( windowHandle, showCommand );

	UpdateWindow( windowHandle );

	SceneManager sceneManager;

	sceneManager.Initialize( inputSystem, graphicsSystem, audioSystem );

	bool isRunning = true;

	while ( isRunning )
	{
		MSG message{};

		while ( PeekMessage( &message, nullptr, 0, 0, PM_REMOVE ) )
		{
			if ( message.message == WM_QUIT )
			{
				isRunning = false;
			}

			TranslateMessage( &message );
			DispatchMessage( &message );
		}

		if ( !isRunning )break;

		gameTimer.Update();
		inputSystem.Update();
		audioSystem.Update();

		if ( inputSystem.IsKeyTriggered( VK_ESCAPE ) )
		{
			PostQuitMessage( 0 );
			continue;
		}

		sceneManager.Update( gameTimer.GetDeltaTime() );

		const float clearColor[ 4 ]
		{
			0.06f,
			0.09f,
			0.14f,
			1.0f
		};

		graphicsSystem.BeginFrame( clearColor );
		imguiManager.BeginFrame();

		sceneManager.Draw();

		imguiManager.EndFrame();
		graphicsSystem.EndFrame();
	}

	sceneManager.Finalize();
	imguiManager.Uninit();

	SetWindowLongPtrW( windowHandle, GWLP_USERDATA, 0 );

	inputSystem.Uninit();
	audioSystem.Uninit();
	graphicsSystem.Uninit();

	if ( IsWindow( windowHandle ) )DestroyWindow( windowHandle );

	UnregisterClassW( Config::WINDOW_CLASS_NAME, instance );

	CoUninitialize();

	return 0;
}