#include "../H/TitleScene.h"

// タイトル用Resourceの初期化、GameSceneへの遷移予約、固定文字列の描画と終了を実装する。

//========= Windows インクルード=========
#include <windows.h>

//========= 外部ライブラリ インクルード=========
#include <DirectXColors.h>

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/H/SceneManager.h"
#include "Scene/GameScene/H/GameScene.h"

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

// 文字描画の準備ができた場合だけ、タイトル用の共有System設定へ切り替える。
bool TitleScene::Init()
{
	// 初期化がfalseを返した場合は、部分Resourceを解放して管理側へ失敗を返す。
	if(!m_hudTextRenderer.Initialize(m_graphicsSystem)){
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"TitleSceneの文字描画初期化に失敗しました。");
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

	// Enterが押されたフレームで遷移を予約し、フェードと実際の切替はSceneManagerへ任せる。
	if(m_inputSystem.IsKeyTriggered(VK_RETURN)){
		m_sceneManager.RequestSceneChange<GameScene>();
	}
}

// 描画先がある場合だけ、固定のタイトルと操作案内を描画する。
void TitleScene::Draw()
{
	// 最小化中など、描画先の大きさが0の状態ではPass設定と文字描画を行わない。
	if(m_graphicsSystem.GetRenderWidth() == 0 || m_graphicsSystem.GetRenderHeight() == 0)return;

	// 画面UI用のPassを設定し、一つの文字描画Batchとして既存の位置・色・倍率で描く。
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);
	m_hudTextRenderer.Begin();
	m_hudTextRenderer.DrawText(L"GUNFIRE LIKE",XMFLOAT2{400.0f,230.0f},DirectX::Colors::Gold,1.5f);
	m_hudTextRenderer.DrawText(L"Enter: テスト画面へ",XMFLOAT2{400.0f,350.0f},DirectX::Colors::White,0.9f);
	m_hudTextRenderer.DrawText(L"Escape: 終了",XMFLOAT2{400.0f,400.0f},DirectX::Colors::White,0.8f);
	m_hudTextRenderer.End();

	// 呼出前のPassへ戻す方式には変えず、既存どおりOPAQUEを明示的に設定する。
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);
}

// Sceneが所有する文字描画Resourceだけを終了する。
void TitleScene::Finalize()
{
	// 遷移先が設定したCaptureやBGMを上書きしないよう、共有Systemの設定は変更しない。
	m_hudTextRenderer.Uninit();
}