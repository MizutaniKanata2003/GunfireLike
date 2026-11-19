#include "../H/SceneManager.h"

// Sceneの所有権移動、初期化成否に応じた切替、フェード進行と終了処理を実装する。

//========= C++標準ライブラリ インクルード=========
#include <memory>
#include <utility>

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/TitleScene/H/TitleScene.h"

// Scene切替の前後に適用するフェード時間をまとめる。
namespace
{
	//========= Scene遷移定数=========
	// フェードイン・アウトそれぞれにかける時間。単位は秒。
	constexpr float ns_SCENE_FADE_DURATION_SECONDS = 0.35f;
}

//========= 生成・破棄関数=========
// 明示的な終了を忘れた場合も、保持するSceneの終了処理を実行する。
SceneManager::~SceneManager()
{
	// SceneのResourceを所有ポインターの破棄より先に終了する。
	Finalize();
}

//========= ライフサイクル関数=========
// 既存の管理状態を終了し、登録したSystemを使ってTitleSceneの開始を試行する。
void SceneManager::Initialize(InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem)
{
	// 再初期化でも以前のSceneを残さず、以前のSystem参照を使って終了を完了させる。
	Finalize();

	// Systemの所有権は受け取らず、以後のScene構築に使う参照先だけを登録する。
	m_inputSystem = &inputSystem;
	m_graphicsSystem = &graphicsSystem;
	m_audioSystem = &audioSystem;

	// 起動時は黒への待機を挟まずTitleSceneの初期化を試行し、フェードインへ進む。
	RequestSceneChange<TitleScene>();
	ApplySceneChange();
}

// フェード状態に応じて現在Sceneの更新と予約Sceneの切替を制御する。
void SceneManager::Update(float deltaTime)
{
	// フェードイン・アウトの再生中はScene更新を止め、遷移演出中のゲーム進行を抑止する。
	if(!m_fadeOverlay.IsPlaying() && m_currentScene)m_currentScene->Update(deltaTime);

	// Scene更新内で作られた予約も、このフレームでフェードアウト開始の対象にする。
	if(m_nextScene && !m_fadeOverlay.IsPlaying())m_fadeOverlay.StartFadeOut(ns_SCENE_FADE_DURATION_SECONDS);

	// 開始したフレームも含めてフェードを進め、切替判定には更新後のAlphaを使う。
	m_fadeOverlay.Update(deltaTime);

	// 予約があり、黒い終端へ達した場合だけSceneの初期化と所有権の切替を試行する。
	if(m_nextScene && m_fadeOverlay.IsFadeOutFinished())ApplySceneChange();
}

// 現在Sceneとフェードを既存順序で描画する。
void SceneManager::Draw()
{
	// Sceneがない場合もフェードは描けるよう、Sceneの有無はこの描画だけで判定する。
	if(m_currentScene)m_currentScene->Draw();

	// Sceneの描画命令の後にフェードを登録し、遷移演出を重ねる。
	m_fadeOverlay.Draw();
}

// 所有Sceneを終了してから、所有していないSystem参照を解除する。
void SceneManager::Finalize()
{
	// 通常の予約SceneにはInitialize・Initを行っていないため、Finalizeせず破棄する。
	m_nextScene.reset();

	// 現在SceneはSystem参照が有効な間にResourceを終了し、その後に実体を破棄する。
	if(m_currentScene){
		m_currentScene->Finalize();
		m_currentScene.reset();
	}

	// System自体は終了せず、以後のScene生成に使わないよう非所有ポインターを解除する。
	m_inputSystem = nullptr;
	m_graphicsSystem = nullptr;
	m_audioSystem = nullptr;
}

//========= 補助関数=========
// 遷移先を初期化できた場合だけ現在Sceneを終了し、所有権を切り替える。
void SceneManager::ApplySceneChange()
{
	// 切替候補がない場合は、Sceneやフェード状態を変更しない。
	if(!m_nextScene)return;

	// 予約を一時所有へ移し、遷移先の初期化が成功するまで現在Sceneを保持する。
	std::unique_ptr<IScene> next_scene = std::move(m_nextScene);

	// 初期状態の準備後に失敗し得るResource初期化を行い、falseなら候補だけを終了する。
	next_scene->Initialize();
	const bool is_next_scene_initialized = next_scene->Init();
	if(!is_next_scene_initialized){
		// 候補の部分Resourceを終了し、保持した現在Sceneへフェードインで戻す。
		Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"次Scene初期化失敗。現在Sceneを維持します。");
		next_scene->Finalize();
		m_fadeOverlay.StartFadeIn(ns_SCENE_FADE_DURATION_SECONDS);
		return;
	}

	// 候補の成功後にだけ旧Sceneを終了する。この時点では候補が変更した共有Systemの状態も存在し得る。
	if(m_currentScene){
		m_currentScene->Finalize();
		m_currentScene.reset();
	}

	// 初期化済み候補の所有権を移し、切替後のSceneを黒から表示する。
	m_currentScene = std::move(next_scene);
	m_fadeOverlay.StartFadeIn(ns_SCENE_FADE_DURATION_SECONDS);
}