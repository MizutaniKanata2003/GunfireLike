#include "../H/ResultScene.h"

// リザルトのメニュー画面の初期化、ボタンのクリックまたはEnterによるTitleSceneへの遷移予約、描画の呼出し、終了を実装する。

//========= Scene インクルード=========
#include "Scene/Common/Core/H/SceneManager.h"
#include "Scene/Common/UI/H/UiLayout.h"
#include "Scene/TitleScene/H/TitleScene.h"

// 重複の例外（規約02の13.8）: このファイルとTitleScene・ResultSceneのもう一方の.cppは、構成表の内容と遷移先を除き、同じ構造（構成表・生成関数・Initialize・Init・Update・Draw・Finalize）になり、重複として検出されるが、例外とする。
// 理由は、IScene実装の各関数を、各クラスが定義する必要があること（C++の規則）。処理の本体はButtonMenuFlowへ移し済みで、各関数は、ButtonMenuFlowを呼ぶだけになっている。
// 1つのクラスへまとめる案は、所有を優先する13.4に反し、Sceneの生成・遷移の予約を行う呼出し側の変更も要るため、採用しない。

// リザルトのメニュー画面の構成をまとめる。画像の配置は、TITLE_LAYOUTとRESULT_LAYOUTの表から読む。
namespace
{
  //========= メニュー画面の構成=========
  // リザルトの構成。Sceneの名前、背景（タイトルと同じ画像）・見出し・タイトルへ戻るボタンの画像、ボタンの下へ描く操作の案内の文字列を指定する。文字列は静的な文字列定数で、所有しない。
  constexpr BUTTON_MENU_CONFIG ns_MENU_CONFIG{L"ResultScene",TITLE_LAYOUT::ns_BACKGROUND,RESULT_LAYOUT::ns_LOGO,RESULT_LAYOUT::ns_TITLE_BUTTON,
    RESULT_LAYOUT::ns_TITLE_BUTTON_HOVER_PATH,L"CLICK OR ENTER: BACK TO TITLE"};
}

//========= 生成関数=========
// 共有SystemとManagerの所有権を受け取らず、各操作に使う参照だけを登録する。
ResultScene::ResultScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem)
  : m_sceneManager(sceneManager),m_inputSystem(inputSystem),m_graphicsSystem(graphicsSystem),m_audioSystem(audioSystem)
{}

//========= Sceneライフサイクル関数=========
// Resource生成前に初期化すべき状態がないため、現行実装では何もしない。
void ResultScene::Initialize()
{
  // 初期化処理の共通呼出順を維持し、Resource生成はInitで行う。
}

// メニュー画面の準備ができた場合だけ、リザルト用の共有System設定へ切り替える。
bool ResultScene::Init()
{
  // メニュー画面の初期化がfalseを返した場合は、部分Resourceを解放して管理側へ失敗を返す。失敗した対象のメッセージは、メニュー画面が記録する。
  // 成功した場合だけ、ButtonMenuFlowが、マウスキャプチャの解除とBGMの再生を行う。リザルト専用のBGMはないため、タイトルのBGMを使う。
  if(!m_menuFlow.Init(m_graphicsSystem,m_inputSystem,m_audioSystem,ns_MENU_CONFIG)){
    Finalize();
    return false;
  }
  return true;
}

// 入力による遷移予約だけを行い、その場でSceneを切り替えない。
void ResultScene::Update(float deltaTime)
{
  // リザルトに時間依存の更新はないため、共通インターフェースの引数を意図的に未使用とする。
  static_cast<void>(deltaTime);

  // ボタンのクリック、またはEnterが押されたフレームで遷移を予約し、フェードと実際の切替はSceneManagerへ任せる。
  if(m_menuFlow.Update(m_inputSystem,m_graphicsSystem)){
    m_sceneManager.RequestSceneChange<TitleScene>();
  }
}

// メニュー画面の描画を呼び出す。
void ResultScene::Draw()
{
  m_menuFlow.Draw(m_graphicsSystem);
}

// Sceneが所有するメニュー画面の描画Resourceだけを終了する。
void ResultScene::Finalize()
{
  // 遷移先が設定したCaptureやBGMを上書きしないよう、共有Systemの設定は変更しない。
  m_menuFlow.Uninit();
}