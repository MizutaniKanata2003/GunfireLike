#include "../H/GameScene.h"

// GameSceneのライフサイクルを、GameSessionへ中継し、プレイの結果（タイトルへ戻る・やり直し・クリア）を、遷移の予約へ反映する。

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

// プレイを更新し、結果に応じた遷移を予約する。
void GameScene::Update(float deltaTime)
{
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

	// クリアは、結果の画面（F8で追加）がまだないため、タイトルへの遷移を予約する。
	case GAME_SESSION_RESULT::e_CLEARED:
	m_sceneManager.RequestSceneChange<TitleScene>();
	break;

	// プレイを続ける場合は、何もしない。
	case GAME_SESSION_RESULT::e_CONTINUE:
	break;
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