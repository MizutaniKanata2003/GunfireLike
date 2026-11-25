#include "../H/GameScene.h"

// GameSceneのライフサイクルを、読込画面とGameSessionへ中継し、読込からプレイへの暗転と、読込の失敗とプレイの結果（タイトルへ戻る・やり直し・クリア）を、遷移の予約へ反映する。

//========= Windows インクルード=========
#include <windows.h> // VK_SPACEの仮想キーコードで、読込の完了後の確認の入力を指定する。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Core/H/SceneManager.h"
#include "Scene/ResultScene/H/ResultScene.h"
#include "Scene/TitleScene/H/TitleScene.h"

// 暗転の時間をまとめる。見た目を確認して調整する仮値。
namespace
{
  //========= 暗転=========
  // 読込からプレイへの切替の、フェードアウトとフェードインそれぞれの時間。単位は秒。SceneManagerのSceneの切替と同じ値。
  constexpr float ns_FADE_SECONDS = 0.35f;
}

//========= 生成関数=========
// 共有SystemやManagerを所有せず、遷移の予約先と、プレイと読込画面が使用する参照だけを登録する。
GameScene::GameScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem)
  : m_sceneManager(sceneManager),m_inputSystem(inputSystem),m_graphicsSystem(graphicsSystem),m_session(inputSystem,graphicsSystem,audioSystem)
{}

//========= Sceneライフサイクル関数=========
// プレイの状態を初期状態へ戻し、読込中の状態にする。
void GameScene::Initialize()
{
  m_session.Initialize();
  m_isPlaying = false;
  m_isFadingOut = false;
  m_hasRequestedExit = false;
}

// 読込画面と暗転の部品を準備し、プレイの初期化の工程を登録する。
bool GameScene::Init()
{
  // 読込画面の描画基盤と画像を用意する。失敗した場合は、この読込を始めずに、falseを返す。
  if(!m_loadingScreen.Initialize(m_graphicsSystem)){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの読込画面の初期化に失敗しました。");
    return false;
  }

  // 暗転の画像を用意する。失敗しても、暗転の進行と完了の判定は動作し、画像が描かれないだけなので、読込は続ける。
  if(!m_fadeOverlay.Initialize(m_graphicsSystem)){
    Logger::Write(e_LogLevel::e_WARNING,e_LogCategory::e_SCENE,L"GameSceneの暗転の画像の初期化に失敗しました。暗転の画像は描画されません。");
  }

  // 工程を登録する。実行は、Updateの中で、1フレームに1工程ずつ行う。
  m_loadingScreen.Begin(BuildLoadingSteps());
  return true;
}

// 読込中は読込を進め、プレイ中はプレイを更新して、結果に応じた遷移を予約する。
void GameScene::Update(float deltaTime)
{
  // 暗転を進める。読込中も、プレイ中も、同じように進める。
  m_fadeOverlay.Update(deltaTime);

  // 読込が終わるまでは、読込の更新だけを行う。
  if(!m_isPlaying){
    UpdateLoading(deltaTime);
    return;
  }

  // フェードインの間は、プレイを進めない。SceneManagerのSceneの切替と同じ扱い。
  if(m_fadeOverlay.IsPlaying())return;

  // プレイの結果ごとに、遷移の予約先を選ぶ。続ける場合は、何も予約しない。
  switch(m_session.Update(deltaTime)){
    // タイトルへ戻る要求は、タイトルへの遷移を予約する。
  case GAME_SESSION_RESULT::e_RETURN_TO_TITLE:
  m_sceneManager.RequestSceneChange<TitleScene>();
  break;

  // 死亡後のやり直しは、新しいGameSceneを予約する。新しいSceneのInitializeとInitで、通貨・武器・部屋・HPが、全て初期状態から始まる。
  case GAME_SESSION_RESULT::e_RETRY:
  m_sceneManager.RequestSceneChange<GameScene>();
  break;

  // クリアは、結果の画面への遷移を予約する。
  case GAME_SESSION_RESULT::e_CLEARED:
  m_sceneManager.RequestSceneChange<ResultScene>();
  break;

  // プレイを続ける場合は、何もしない。
  case GAME_SESSION_RESULT::e_CONTINUE:
  break;
  }
}

// 読込中は読込画面を、プレイ中はプレイを描画し、最後に、暗転の画像を重ねる。
void GameScene::Draw()
{
  if(m_isPlaying){
    m_session.Draw();
  }
  else{
    m_loadingScreen.Draw(m_graphicsSystem);
  }

  // 暗転の画像は、読込画面とプレイの描画の後に重ねる。
  m_fadeOverlay.Draw(m_graphicsSystem);
}

// 読込画面・暗転・プレイのResourceを解放する。
void GameScene::Finalize()
{
  m_loadingScreen.Uninit();
  m_fadeOverlay.Uninit();
  m_session.Finalize();
}

//========= 補助関数=========
// プレイの初期化の工程を、実行する順に作る。
std::vector<LOADING_STEP> GameScene::BuildLoadingSteps()
{
  // 工程の番号ごとに、GameSessionの初期化を呼ぶ関数と、GameSessionが決めた重みを組にする。関数は、このGameSceneを借用する。
  std::vector<LOADING_STEP> steps;
  for(int step_index = 0; step_index < GameSession::GetInitStepCount(); ++step_index){
    steps.push_back(LOADING_STEP{[this,step_index](){return m_session.RunInitStep(step_index); },GameSession::GetInitStepWeight(step_index)});
  }
  return steps;
}

// 読込中の更新。
void GameScene::UpdateLoading(float deltaTime)
{
  // 失敗でタイトルへの遷移を予約した後は、読込を進めない。
  if(m_hasRequestedExit)return;

  // 暗転（フェードアウト）の最中は、画面が黒くなるのを待つ。黒くなったら、FPS操作とGame用BGMを有効にして、プレイを始め、フェードインする。
  if(m_isFadingOut){
    if(!m_fadeOverlay.IsFadeOutFinished())return;

    m_session.StartPlay();
    m_isPlaying = true;
    m_isFadingOut = false;
    m_fadeOverlay.StartFadeIn(ns_FADE_SECONDS);
    return;
  }

  // 工程を進める。確認の入力は、スペースが押された瞬間。
  m_loadingScreen.Update(deltaTime,m_inputSystem.IsKeyTriggered(VK_SPACE));

  // 工程が失敗した場合は、作りかけのResourceを解放し、タイトルへ戻る遷移を予約する。
  if(m_loadingScreen.HasFailed()){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの読込に失敗しました。タイトルへ戻ります。");
    m_session.Finalize();
    m_sceneManager.RequestSceneChange<TitleScene>();
    m_hasRequestedExit = true;
    return;
  }

  // 読込が終わったら、暗転（フェードアウト）を始める。読込画面は、画面が黒くなるまで、最後の見た目を保つ。
  if(!m_loadingScreen.IsFinished())return;

  m_fadeOverlay.StartFadeOut(ns_FADE_SECONDS);
  m_isFadingOut = true;
}