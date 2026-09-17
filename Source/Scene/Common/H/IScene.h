#pragma once

// タイトル、ゲーム、リザルトなど全シーンの共通インターフェース。
// SceneManagerがこの型だけを通して、シーンの初期化・更新・描画・終了を管理する。
class IScene
{
public:
	virtual ~IScene() = default;

	// 浅い初期化を行う。初期値設定と依存関係の準備に使用する。
	virtual void Initialize() = 0;

	// 深い初期化を行う。アセット読込など失敗する可能性がある処理に使用する。
	virtual bool Init() = 0;

	// 1フレーム分のゲームロジックを更新する。
	virtual void Update( float deltaTime ) = 0;

	// ImGuiと3Dオブジェクトの描画命令を発行する。
	virtual void Draw() = 0;

	// Init()で作成したリソースを解放する。
	virtual void Uninit() = 0;
};