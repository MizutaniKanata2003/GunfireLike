#include "../H/TitleScene.h"

// タイトル用Resourceの初期化、GameSceneへの遷移予約、固定文字列の中央揃えの描画と終了を実装する。

//========= Windows インクルード=========
#include <windows.h>

//========= 外部ライブラリ インクルード=========
#include <DirectXColors.h>

//========= Framework インクルード=========
#include "Framework/2D/H/HudCanvas.h"
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Core/H/SceneManager.h"
#include "Scene/GameScene/H/GameScene.h"

// タイトルに描く文字列の表をまとめる。位置は、基準Canvas（幅1280・高さ720）上の値。
namespace
{
	//========= 型=========
	// 文字1行分の内容と配置。横位置は、描画時に文字の幅を測って中央へ揃えるため、持たない。
	struct TITLE_TEXT_LINE
	{
		// 描く文字列。静的な文字列定数を指し、所有しない。
		const wchar_t* text;
		// 文字の上端の位置。基準Canvas上の座標で、下向きY。
		float positionY;
		// 文字のRGBA色。
		DirectX::XMVECTORF32 color;
		// 文字の拡大率。1でFont元の大きさ。
		float scale;
	};

	//========= タイトルの文字の表=========
	// 上から順に、タイトル名・Enterの案内・Escapeの案内。位置・色・倍率は、従来の値を維持する。
	const TITLE_TEXT_LINE ns_TITLE_LINES[]
	{
		{L"GUNFIRE LIKE",230.0f,DirectX::Colors::Gold,1.5f},
		{L"Enter: テスト画面へ",350.0f,DirectX::Colors::White,0.9f},
		{L"Escape: 終了",400.0f,DirectX::Colors::White,0.8f}
	};
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

// 描画先がある場合だけ、固定のタイトルと操作案内を、横方向の中央へ揃えて描画する。
void TitleScene::Draw()
{
	// 最小化中など、描画先の大きさが0の状態ではPass設定と文字描画を行わない。
	if(m_graphicsSystem.GetRenderWidth() == 0 || m_graphicsSystem.GetRenderHeight() == 0)return;

	// 画面UI用のPassを設定し、一つの文字描画Batchとして描く。
	m_graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);
	m_hudTextRenderer.Begin();

	// 表の各行を、文字の幅を測って、基準Canvasの横方向の中央へ置く。左端 = (Canvasの幅 - 文字の幅) / 2。
	// MeasureTextはCanvas倍率を含まない基準Canvas上の寸法を返すため、基準Canvasの幅と同じ単位で計算できる。
	for(const TITLE_TEXT_LINE& line : ns_TITLE_LINES){
		const XMFLOAT2 text_size = m_hudTextRenderer.MeasureText(line.text,line.scale);
		const XMFLOAT2 text_position{(HudCanvas::VIRTUAL_WIDTH - text_size.x) * 0.5f,line.positionY};
		m_hudTextRenderer.DrawText(line.text,text_position,line.color,line.scale);
	}
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