#include "../H/TitleScene.h"

// タイトルのメニュー画面の初期化、開始ボタンのクリックまたはEnterによるGameSceneへの遷移予約、描画の呼出し、終了を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_RETURNで開始の入力を判定する。

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Core/H/SceneManager.h"
#include "Scene/Common/UI/H/UiLayout.h"
#include "Scene/GameScene/H/GameScene.h"

// タイトルのメニュー画面の構成をまとめる。画像の配置は、TITLE_LAYOUTの表から読む。
namespace
{
	//========= メニュー画面の構成=========
	// タイトルの構成。Sceneの名前、背景・ロゴ・開始ボタンの画像、ボタンの下へ描く操作の案内の文字列を指定する。文字列は静的な文字列定数で、所有しない。
	constexpr BUTTON_MENU_CONFIG ns_MENU_CONFIG{L"TitleScene",TITLE_LAYOUT::ns_BACKGROUND,TITLE_LAYOUT::ns_LOGO,TITLE_LAYOUT::ns_START_BUTTON,
		TITLE_LAYOUT::ns_START_BUTTON_HOVER_PATH,L"CLICK OR ENTER: START    ESC: QUIT"};
}

//========= 生成関数=========
// 共有SystemとManagerの所有権を受け取らず、各操作に使う参照だけを登録する。
TitleScene::TitleScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem)
	: m_sceneManager(sceneManager),m_inputSystem(inputSystem),m_graphicsSystem(graphicsSystem),m_audioSystem(audioSystem)
{}

//========= Sceneライフサイクル関数=========
// Resource生成前に初期化すべき状態がないため、現行実装では何もしない。
void TitleScene::Initialize()
{
	// 初期化処理の共通呼出順を維持し、Resource生成はInitで行う。
}

// メニュー画面の準備ができた場合だけ、タイトル用の共有System設定へ切り替える。
bool TitleScene::Init()
{
	// メニュー画面の初期化がfalseを返した場合は、部分Resourceを解放して管理側へ失敗を返す。失敗した対象のメッセージは、メニュー画面が記録する。
	if(!m_menu.Initialize(m_graphicsSystem,ns_MENU_CONFIG)){
		Finalize();
		return false;
	}

	// Resource生成の成功後にだけCaptureとBGMを変更し、false返却時にはこの副作用を発生させない。
	m_inputSystem.SetMouseCaptureEnabled(false);
	m_audioSystem.PlayTitleBgm();
	return true;
}

// 入力による遷移予約だけを行い、その場でSceneを切り替えない。
void TitleScene::Update(float deltaTime)
{
	// タイトルに時間依存の更新はないため、共通インターフェースの引数を意図的に未使用とする。
	static_cast<void>(deltaTime);

	// 開始ボタンのホバーとクリックを判定する。
	const bool is_button_clicked = m_menu.UpdateButton(m_inputSystem,m_graphicsSystem);

	// 開始ボタンのクリック、またはEnterが押されたフレームで遷移を予約し、フェードと実際の切替はSceneManagerへ任せる。
	if(is_button_clicked || m_inputSystem.IsKeyTriggered(VK_RETURN)){
		m_sceneManager.RequestSceneChange<GameScene>();
	}
}

// メニュー画面の描画を呼び出す。
void TitleScene::Draw()
{
	m_menu.Draw(m_graphicsSystem);
}

// Sceneが所有するメニュー画面の描画Resourceだけを終了する。
void TitleScene::Finalize()
{
	// 遷移先が設定したCaptureやBGMを上書きしないよう、共有Systemの設定は変更しない。
	m_menu.Uninit();
}