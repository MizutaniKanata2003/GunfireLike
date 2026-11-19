#pragma once

// 現在Sceneと遷移予約Sceneを所有し、更新・描画・フェードを伴う切替・終了を管理する。
// Input・Graphics・Audioの各Systemは所有せず、Sceneへ渡す参照先として保持する。

//========= C++標準ライブラリ インクルード=========
#include <memory>

//========= Framework インクルード=========
#include "Framework/2D/H/FadeOverlay.h"

//========= Scene インクルード=========
#include "IScene.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;

// Sceneの予約と実際の切替を分離し、遷移先の初期化成功後にだけ所有する現在Sceneを置き換える。
class SceneManager final
{
public:
	//========= 生成・破棄関数=========
	// Finalizeを呼び、保持するSceneを終了する。各Systemの破棄前にSceneを終了しておくこと。
	~SceneManager();

	//========= ライフサイクル関数=========
	// 既存Sceneを終了し、System参照を登録してTitleSceneの初期化を試行する。
	// inputSystem: Sceneへ渡す入力基盤。非所有で、Sceneの終了まで有効であること。
	// graphicsSystem: Sceneへ渡す描画基盤。非所有で、SceneのResource終了まで有効であること。
	// audioSystem: Sceneへ渡す音声基盤。非所有で、Sceneの終了まで有効であること。
	// 戻り値による成功通知は行わない。TitleSceneのInit失敗はログへ記録する。
	void Initialize(InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);
	// Scene更新、遷移開始、フェード更新、黒画面での切替を既存順序で行う。
	// deltaTime: この更新で進める経過時間。単位は秒。Sceneとフェードへそのまま渡す。
	// フェードイン・アウトのどちらの再生中も、現在SceneのUpdateは呼ばない。
	void Update(float deltaTime);
	// 現在Sceneを描画してからフェードを重ねる。Scene切替は行わない。
	// ImGuiのフレーム開始後、描画データ確定前に呼ぶ。
	void Draw();
	// 予約Sceneを破棄し、現在SceneをFinalize後に破棄して、System参照を解除する。
	// System自体の終了処理とフェード状態のリセットは行わない。終了済みでも呼出可能。
	void Finalize();

	//========= Scene遷移関数=========
	// Sceneを生成して次の遷移先として予約する。この呼出ではInitialize・Initは実行しない。
	// TScene: ISceneへ変換でき、Manager・Input・Graphics・Audio参照で構築できるScene型。
	// 事前条件: InitializeでSystem参照が登録済みであること。Finalize後には呼ばない。
	// 既存予約がある場合は置き換える。生成・メモリ確保などの例外は捕捉しない。
	template<class TScene>
	void RequestSceneChange() { m_nextScene = std::make_unique<TScene>(*this,*m_inputSystem,*m_graphicsSystem,*m_audioSystem); }

private:
	//========= 補助関数=========
	// 予約Sceneを一時所有してInitialize・Initを実行し、成功後に現在Sceneと置き換える。
	// Initがfalseなら一時SceneをFinalizeし、現在Sceneは保持してフェードインを開始する。
	// 予約なしなら何もしない。例外捕捉や共有Systemの副作用の巻き戻しは行わない。
	void ApplySceneChange();

	//========= Framework参照=========
	// Sceneの構築時に渡す入力基盤への非所有ポインター。Finalize時にnullptrへ戻す。
	InputSystem* m_inputSystem{};
	// Sceneの構築時に渡す描画基盤への非所有ポインター。Finalize時にnullptrへ戻す。
	GraphicsSystem* m_graphicsSystem{};
	// Sceneの構築時に渡す音声基盤への非所有ポインター。Finalize時にnullptrへ戻す。
	AudioSystem* m_audioSystem{};

	//========= Scene管理=========
	// 現在更新・描画するSceneを所有する。置換・終了時にはFinalizeしてから破棄する。
	std::unique_ptr<IScene> m_currentScene{};
	// 次の切替候補を所有する。通常の予約状態ではInitialize・Initは未実行。
	std::unique_ptr<IScene> m_nextScene{};

	//========= Scene遷移演出=========
	// Scene切替を黒画面で行うためのフェード状態と描画を保持する。
	FadeOverlay m_fadeOverlay{};
};