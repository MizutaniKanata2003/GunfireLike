#include "../H/GameScene.h"

// GameSceneのライフサイクルを、GameSessionへ中継し、タイトルへ戻る要求を、タイトルへの遷移予約へ反映する。

//========= Scene インクルード=========
#include "Scene/Common/Core/H/SceneManager.h"
#include "Scene/TitleScene/H/TitleScene.h"

//========= 生成関数=========
// 共有SystemやManagerを所有せず、遷移の予約先と、プレイが使用する参照だけを登録する。
GameScene::GameScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem)
	: m_sceneManager(sceneManager),m_session(inputSystem,graphicsSystem,audioSystem)
{}

//========= Sceneライフサイクル関数=========
// プレイの状態を初期状態へ戻す。
void GameScene::Initialize()
{
	m_session.Initialize();
}

// プレイのResourceを初期化する。
bool GameScene::Init()
{
	return m_session.Init();
}

// プレイを更新し、タイトルへ戻る要求があれば、遷移を予約する。
void GameScene::Update(float deltaTime)
{
	// プレイがタイトルへ戻る要求を返した場合だけ、タイトルへの遷移を予約する。それ以外は、何もしない。
	if(m_session.Update(deltaTime) == GAME_SESSION_RESULT::e_RETURN_TO_TITLE){
		m_sceneManager.RequestSceneChange<TitleScene>();
	}
}

// プレイを描画する。
void GameScene::Draw()
{
	m_session.Draw();
}

// プレイのResourceを解放する。
void GameScene::Finalize()
{
	m_session.Finalize();
}