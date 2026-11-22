#pragma once

// 1回のプレイを行うGameSceneを宣言する。担当は、Sceneのライフサイクルと、プレイの結果に応じた遷移予約（タイトルへ戻る・やり直し・クリア後の遷移）だけ。
// 初期化・更新・描画・終了の中身は、GameSessionが担当する。将来、次のSceneへ渡す値が必要になった場合の取得関数も、このクラスへ追加する。

//========= Scene インクルード=========
#include "Scene/Common/Core/H/IScene.h"
#include "Scene/GameScene/H/GameSession.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// 1回のプレイのSceneとして、GameSessionの呼出しと、結果に応じた遷移の予約を行う。
class GameScene final: public IScene
{
public:
	//========= 生成関数=========
	// Sceneで使用する非所有参照を登録する。Resource生成はInitで行う。
	// sceneManager: 遷移の予約先。Sceneの使用期間中は有効であること。
	// inputSystem: キー入力の取得先とCapture設定先。Sceneより長く生存すること。
	// graphicsSystem: Resource生成・描画先サイズ取得・描画の基盤。Resource終了まで有効であること。
	// audioSystem: Game用BGMの再生先。Sceneの使用期間中は有効であること。
	GameScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

	//========= Sceneライフサイクル関数=========
	// プレイの状態を初期状態へ戻す。
	void Initialize() override;
	// プレイのResourceを初期化し、成功後にFPS操作とGame用BGMを有効にする。
	// 戻り値: 成功したらtrue。失敗した場合は後始末済みで、CaptureとBGMは変更しない。
	bool Init() override;
	// プレイを1回分更新し、結果に応じて遷移を予約する。タイトルへ戻る要求はタイトルへ、死亡後のやり直しは、新しいGameSceneへ、クリアは、クリア後の遷移先へ予約する。
	// deltaTime: 実時間の経過時間。単位は秒。
	void Update(float deltaTime) override;
	// プレイの3D・HUD・デバッグUIを描画する。ImGuiフレーム開始後、描画データ確定前に呼ぶ。
	void Draw() override;
	// プレイのResourceを解放する。繰り返し呼出可能。CaptureやBGMは変更しない。
	void Finalize() override;

private:
	//========= 参照・所有状態=========
	// Scene遷移の予約先となる非所有参照。
	SceneManager& m_sceneManager;
	// 1回のプレイの流れ。
	GameSession m_session;
};