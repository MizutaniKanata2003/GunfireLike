// Windowsアプリケーションの起動・メッセージ転送・フレーム処理・明示的な終了を担当する。
// 各SystemとSceneManagerをローカルに保持し、生成順・失敗分岐・終了順は既存実装を維持する。

//========= Windows インクルード=========
#include <windows.h>
#include <objbase.h>

//========= 外部ライブラリ インクルード=========
#include "imgui.h"
#include "imgui_impl_win32.h"

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/AppConfig.h"
#include "Framework/Etc/H/GameTimer.h"
#include "Framework/Imgui/H/ImGuiManager.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/H/SceneManager.h"

//========= 外部関数宣言=========
// Win32バックエンドへウィンドウメッセージを転送する。外部APIの名前と型は変更しない。
// windowHandle: メッセージの送信先となるWindow。
// message: Win32メッセージ番号。
// wParam: メッセージ固有の追加情報。
// lParam: メッセージ固有の追加情報。
// 戻り値: バックエンドの処理結果。呼出側では非0なら処理済みとして扱う。
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND windowHandle,UINT message,WPARAM wParam,LPARAM lParam);

//========= Window関連型=========
// WindowProcedureへ渡すSystem参照をまとめる。実体と各SystemはWinMainのローカルで保持する。
struct WINDOW_CONTEXT
{
	// 入力転送とCapture状態の取得に使う非所有ポインター。登録前はnullptrとして扱う。
	InputSystem* inputSystem{};
	// サイズ変更通知に使う非所有ポインター。登録前はnullptrとして扱う。
	GraphicsSystem* graphicsSystem{};
};

//========= Windowメッセージ関数=========
// 入力・ImGuiへの通知、Resize、終了要求を処理し、それ以外は既定処理へ渡す。
// windowHandle: メッセージを受け取ったWindow。GWLP_USERDATAから非所有参照を取得する。
// message: Win32メッセージ番号。
// wParam: メッセージ固有の追加情報。
// lParam: メッセージ固有の追加情報。WM_SIZEでは幅と高さを取り出す。
// 戻り値: ImGui処理済みなら1、明示処理したWM_SIZE・WM_DESTROYでは0、それ以外は既定処理の結果。
LRESULT CALLBACK WindowProcedure(HWND windowHandle,UINT message,WPARAM wParam,LPARAM lParam)
{
	// Window生成中や参照解除後も受信し得るため、未登録時はSystemへの転送を行わない。
	WINDOW_CONTEXT* window_context = reinterpret_cast<WINDOW_CONTEXT*>(GetWindowLongPtrW(windowHandle,GWLP_USERDATA));
	InputSystem* input_system = window_context != nullptr ? window_context->inputSystem : nullptr;
	GraphicsSystem* graphics_system = window_context != nullptr ? window_context->graphicsSystem : nullptr;

	// ImGuiより先にゲーム側へ通知し、入力Systemが扱うWindow状態を更新する。
	if(input_system != nullptr)input_system->OnWindowMessage(message,wParam,lParam);

	// FPS操作中はImGuiへ転送せず、Capture解除中またはSystem未登録時は既存どおり転送する。
	// バックエンドが処理済みを返した場合は、後続のメッセージ分岐へ進まない。
	if(input_system == nullptr || !input_system->IsMouseCaptureEnabled()){
		if(ImGui_ImplWin32_WndProcHandler(windowHandle,message,wParam,lParam)){
			return 1;
		}
	}

	// Resizeと終了通知だけをここで処理し、その他の挙動はWin32既定処理へ任せる。
	switch(message){
	case WM_SIZE:
	// サイズ0を含め、WM_SIZEから取得した値をそのままGraphicsSystemへ渡す。
	if(graphics_system != nullptr){
		const unsigned int width = static_cast<unsigned int>(LOWORD(lParam));
		const unsigned int height = static_cast<unsigned int>(HIWORD(lParam));
		graphics_system->Resize(width,height);
	}
	return 0;

	case WM_DESTROY:
	// メインループ側で通常終了へ進むよう、終了メッセージを登録する。
	PostQuitMessage(0);
	return 0;

	default:
	// この関数で扱わないWindow操作は、標準のWindow Procedureへ転送する。
	return DefWindowProcW(windowHandle,message,wParam,lParam);
	}
}

//========= エントリーポイント=========
// Systemを順に初期化し、メッセージとゲームフレームを処理してから既存順序で終了する。
// instance: このアプリケーションのインスタンス。Window Class登録とWindow生成に使用する。
// 第2引数: Win32エントリーポイントの旧インスタンス引数。未使用のため無名を維持する。
// 第3引数: コマンドライン文字列。未使用のため無名を維持する。
// showCommand: Windowの初回表示方法。
// 戻り値: 通常終了は0。COM失敗は-100、Class登録・Window生成・Graphics・Audio・ImGui失敗は順に-1〜-5。
// 例外捕捉や新しい失敗判定は追加せず、既存の戻り値と失敗時終了処理を維持する。
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int showCommand)
{
	// AudioEngineが使用するMTAに合わせてCOMを初期化する。失敗時は解除せず終了する。
	const HRESULT com_result = CoInitializeEx(nullptr,COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
	if(FAILED(com_result))return -100;

	// WindowProcedureと表示用Class情報を登録し、失敗した場合はCOMだけを終了する。
	WNDCLASSEXW window_class{};
	window_class.cbSize = sizeof(window_class);
	window_class.hInstance = instance;
	window_class.lpfnWndProc = WindowProcedure;
	window_class.lpszClassName = Config::WINDOW_CLASS_NAME;
	window_class.hCursor = LoadCursor(nullptr,IDC_ARROW);
	if(RegisterClassExW(&window_class) == 0){
		CoUninitialize();
		return -1;
	}

	// 指定したクライアント領域を得るため、Window枠を含めた外寸へ変換する。
	RECT window_rect{0,0,static_cast<LONG>(Config::WINDOW_WIDTH),static_cast<LONG>(Config::WINDOW_HEIGHT)};
	AdjustWindowRect(&window_rect,WS_OVERLAPPEDWINDOW,FALSE);

	// Windowを生成する。この時点ではGWLP_USERDATAへSystem参照を登録していない。
	HWND window_handle = CreateWindowExW(0,Config::WINDOW_CLASS_NAME,Config::WINDOW_TITLE,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
																			window_rect.right - window_rect.left,window_rect.bottom - window_rect.top,nullptr,nullptr,instance,nullptr);
	if(window_handle == nullptr){
		// Window生成に失敗したため、登録したClassとCOMを終了する。
		UnregisterClassW(Config::WINDOW_CLASS_NAME,instance);
		CoUninitialize();
		return -2;
	}

	// 描画基盤を初期化し、false返却時は既存どおりWindow・Class・COMの終了へ進む。
	GraphicsSystem graphics_system;
	if(!graphics_system.Init(window_handle,Config::WINDOW_WIDTH,Config::WINDOW_HEIGHT)){
		DestroyWindow(window_handle);
		UnregisterClassW(Config::WINDOW_CLASS_NAME,instance);
		CoUninitialize();
		return -3;
	}

	// Windowを入力先として登録する。以後の失敗経路にも、新しい終了呼出は追加しない。
	InputSystem input_system;
	input_system.Initialize(window_handle);

	// 音声基盤を初期化し、失敗した場合は既存の描画基盤とWindow関連の終了順を維持する。
	AudioSystem audio_system;
	if(!audio_system.Initialize()){
		graphics_system.Uninit();
		DestroyWindow(window_handle);
		UnregisterClassW(Config::WINDOW_CLASS_NAME,instance);
		CoUninitialize();
		return -4;
	}

	// ローカルの参照束をWindowへ登録する。WindowはSystemも参照束も所有しない。
	WINDOW_CONTEXT window_context{};
	window_context.inputSystem = &input_system;
	window_context.graphicsSystem = &graphics_system;
	SetWindowLongPtrW(window_handle,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(&window_context));

	// フレームごとの経過時間取得を開始する。初期化位置はImGui初期化より前のままにする。
	GameTimer game_timer;
	game_timer.Initialize();

	// 描画基盤を利用してImGuiを初期化し、失敗した場合は既存の終了経路へ進む。
	ImGuiManager imgui_manager;
	if(!imgui_manager.Init(window_handle,graphics_system)){
		// この経路ではInputのUninitやGWLP_USERDATA解除を追加せず、元の処理順を維持する。
		audio_system.Uninit();
		graphics_system.Uninit();
		DestroyWindow(window_handle);
		UnregisterClassW(Config::WINDOW_CLASS_NAME,instance);
		CoUninitialize();
		return -5;
	}

	// 基盤の初期化成功後にWindowを表示し、初回のWindow更新を要求する。
	ShowWindow(window_handle,showCommand);
	UpdateWindow(window_handle);

	// 各Systemが生存している間にSceneを初期化し、TitleSceneの開始を試行する。
	SceneManager scene_manager;
	scene_manager.Initialize(input_system,graphics_system,audio_system);

	// 終了メッセージを受けるまで、メッセージ処理とゲームフレームを繰り返す。
	bool is_running = true;
	while(is_running){
		// 保留中のメッセージを取り出す。WM_QUIT後も既存どおりループ内の転送処理を維持する。
		MSG message{};
		while(PeekMessage(&message,nullptr,0,0,PM_REMOVE)){
			if(message.message == WM_QUIT){
				is_running = false;
			}
			TranslateMessage(&message);
			DispatchMessage(&message);
		}

		// 終了通知を受けたフレームでは、System更新と描画へ進まない。
		if(!is_running)break;

		// 時間・入力・音声をこの順で更新し、Sceneへ渡すフレーム情報を準備する。
		game_timer.Update();
		input_system.Update();
		audio_system.Update();

		// Escapeでは終了通知を登録し、そのフレームのScene更新・描画を省く。
		if(input_system.IsKeyTriggered(VK_ESCAPE)){
			PostQuitMessage(0);
			continue;
		}

		// Sceneと遷移演出を更新してから、更新後の状態を描画する。
		scene_manager.Update(game_timer.GetDeltaTime());

		// 既存の背景色で描画を開始し、ImGuiフレームをSceneの描画前に準備する。
		const float clear_color[4]{0.06f,0.09f,0.14f,1.0f};
		graphics_system.BeginFrame(clear_color);
		imgui_manager.BeginFrame();

		// Sceneとフェードの描画命令を登録する。描画データの確定はこの後に行う。
		scene_manager.Draw();

		// ImGuiの描画データを出力してから、GraphicsSystemでフレームを表示する。
		imgui_manager.EndFrame();
		graphics_system.EndFrame();
	}

	// 下位Resourceを持つSceneとImGuiを、依存する描画・音声基盤より先に終了する。
	scene_manager.Finalize();
	imgui_manager.Uninit();

	// 後続のWindowメッセージからSystemへアクセスしないよう、通常終了時は参照を解除する。
	SetWindowLongPtrW(window_handle,GWLP_USERDATA,0);

	// 入力制限を解除し、音声、描画の順で明示的に終了する。
	input_system.Uninit();
	audio_system.Uninit();
	graphics_system.Uninit();

	// WM_DESTROYで既に破棄済みの場合は再破棄せず、残っているWindowだけを破棄する。
	if(IsWindow(window_handle))DestroyWindow(window_handle);

	// Window関連の登録を解除してからCOMを終了する。ローカル実体の自動破棄時点も既存のまま。
	UnregisterClassW(Config::WINDOW_CLASS_NAME,instance);
	CoUninitialize();
	return 0;
}