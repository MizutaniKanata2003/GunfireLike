#pragma once

// タイトル、ゲーム、リザルトなど全Sceneの共通インターフェース。
class IScene
{
public:
	//========= 仮想デストラクタ=========
	// 派生Sceneを安全に破棄するための仮想デストラクタ。
	virtual ~IScene() = default;

	//========= 純粋仮想関数=========
	// 浅い初期化を行い、初期値設定と依存関係の準備を行う。
	virtual void Initialize() = 0;
	// 深い初期化を行い、Asset読込など失敗する可能性がある処理を行う。
	virtual bool Init() = 0;
	// 1フレーム分のゲームロジックを更新する。
	virtual void Update( float deltaTime ) = 0;
	// ImGuiと3Dオブジェクトの描画命令を発行する。
	virtual void Draw() = 0;
	// Initで作成したリソースを解放する。
	virtual void Uninit() = 0;
};