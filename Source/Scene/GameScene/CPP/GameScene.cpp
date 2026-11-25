#include "../H/GameScene.h"

// GameSceneのライフサイクルを、読込画面とGameSessionへ中継し、読込の失敗とプレイの結果（タイトルへ戻る・やり直し・クリア）を、遷移の予約へ反映する。

//========= C++標準ライブラリ インクルード=========
#include <iterator> // std::sizeで、工程の重みの表の長さを求める。

//========= Windows インクルード=========
#include <windows.h> // VK_SPACEの仮想キーコードで、読込の完了後の確認の入力を指定する。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Core/H/SceneManager.h"
#include "Scene/ResultScene/H/ResultScene.h"
#include "Scene/TitleScene/H/TitleScene.h"

// 読込の工程の重みをまとめる。見た目を確認して調整する仮値。
namespace
{
  //========= 工程の重み=========
  // GameSession::RunInitStepの工程の順（描画Resource・敵・プレイヤーとデバッグ・第1ウェーブ）に対応する、バーの進み具合の重み。敵のモデルの読込が、最も重い（Debugで約1.0〜1.4秒）。
  // 工程の数がこの表より多い場合は、超えた工程の重みを1とする。
  constexpr float ns_INIT_STEP_WEIGHTS[]{2.0f,6.0f,1.0f,1.0f};
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
  m_hasRequestedExit = false;
}

// 読込画面を準備し、プレイの初期化の工程を登録する。
bool GameScene::Init()
{
  // 読込画面の描画基盤と画像を用意する。失敗した場合は、この読込を始めずに、falseを返す。
  if(!m_loadingScreen.Initialize(m_graphicsSystem)){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"GameSceneの読込画面の初期化に失敗しました。");
    return false;
  }

  // 工程を登録する。実行は、Updateの中で、1フレームに1工程ずつ行う。
  m_loadingScreen.Begin(BuildLoadingSteps());
  return true;
}

// 読込中は読込を進め、プレイ中はプレイを更新して、結果に応じた遷移を予約する。
void GameScene::Update(float deltaTime)
{
  // 読込が終わるまでは、読込の更新だけを行う。
  if(!m_isPlaying){
    UpdateLoading(deltaTime);
    return;
  }

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

// 読込中は読込画面を、プレイ中はプレイを描画する。
void GameScene::Draw()
{
  if(!m_isPlaying){
    m_loadingScreen.Draw(m_graphicsSystem);
    return;
  }

  m_session.Draw();
}

// 読込画面とプレイのResourceを解放する。
void GameScene::Finalize()
{
  m_loadingScreen.Uninit();
  m_session.Finalize();
}

//========= 補助関数=========
// プレイの初期化の工程を、実行する順に作る。
std::vector<LOADING_STEP> GameScene::BuildLoadingSteps()
{
  // 工程の番号ごとに、GameSessionの初期化を呼ぶ関数と、重みを組にする。関数は、このGameSceneを借用する。
  std::vector<LOADING_STEP> steps;
  for(int step_index = 0; step_index < GameSession::GetInitStepCount(); ++step_index){
    const bool has_weight = static_cast<std::size_t>(step_index) < std::size(ns_INIT_STEP_WEIGHTS);
    const float weight = has_weight ? ns_INIT_STEP_WEIGHTS[step_index] : 1.0f;
    steps.push_back(LOADING_STEP{[this,step_index](){return m_session.RunInitStep(step_index); },weight});
  }
  return steps;
}

// 読込中の更新。
void GameScene::UpdateLoading(float deltaTime)
{
  // 失敗でタイトルへの遷移を予約した後は、読込を進めない。
  if(m_hasRequestedExit)return;

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

  // 読込が終わるまでは、何もしない。終わったら、FPS操作とGame用BGMを有効にして、プレイを始める。
  if(!m_loadingScreen.IsFinished())return;

  m_session.StartPlay();
  m_isPlaying = true;
}