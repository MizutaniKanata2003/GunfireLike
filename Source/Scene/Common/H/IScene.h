#pragma once

// SceneManagerがSceneの初期化・更新・描画・終了を呼び出すための共通インターフェース。
// 具体的なResourceやSystem参照の所有は派生Sceneが定義し、この型自体は状態を保持しない。

// タイトルやゲームなどのSceneに、共通のライフサイクル操作を要求する。
class IScene
{
public:
	//========= 仮想デストラクター=========
	// 基底ポインター経由でも派生Sceneのデストラクターを呼べるようにする。
	// Finalizeを自動では呼ばない。必要な終了処理は破棄前に管理側から呼び出す。
	virtual ~IScene() = default;

	//========= ライフサイクル関数=========
	// 初期値設定などの初期状態を準備する。失敗成否を返すResource初期化はInitへ分離する。
	virtual void Initialize() = 0;
	// Asset読込など、失敗する可能性のある初期化を行う。
	// 戻り値: 初期化成功ならtrue、呼出側で扱う初期化失敗ならfalse。
	// SceneManagerはfalse返却後にもFinalizeを呼ぶため、部分初期化状態からの終了を考慮する。
	virtual bool Init() = 0;
	// 1フレーム分の入力やゲーム状態を更新する。
	// deltaTime: この更新で進める経過時間。単位は秒。具体的な値の扱いは派生実装が定義する。
	virtual void Update(float deltaTime) = 0;
	// Sceneが担当する描画命令を発行する。具体的な描画方式は派生実装が定義する。
	virtual void Draw() = 0;
	// 所有Resourceを必要な順序で終了する。共有Systemの寿命は管理側が維持する。
	// 派生実装内で失敗時に終了済みの場合も含め、管理側からの終了呼出を受けられるようにする。
	virtual void Finalize() = 0;
};