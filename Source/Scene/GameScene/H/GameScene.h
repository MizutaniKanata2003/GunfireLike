#pragma once

// 移動・ダッシュ・しゃがみ・スライディング・回避・蹴り・殴打・視点・構え・射撃・リロード・回復と、設定表から生成した床・壁、部屋ごとの敵と敵の弾と範囲の予告、出口、画像のHUD、Collisionの可視化を行う最小構成のGameSceneを宣言する。
// 各クラスの更新を仲介する。ステージの世界はStageController、プレイヤー側一式はPlayerSuiteが所有する。Scene切替はManagerへ予約する。個別のゲーム規則は、各Controllerが持つ。

//========= Debug インクルード=========
#include "Debug/H/CollisionDebugView.h"
#include "Debug/H/GameDebugController.h"

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/TimeScaleController.h"
#include "Scene/Common/Core/H/IScene.h"
#include "Scene/Common/Enemy/H/EnemyController.h"
#include "Scene/Common/Player/H/PlayerSuite.h"
#include "Scene/Common/Stage/H/StageController.h"
#include "Scene/GameScene/H/GameHud.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// テスト用の操作・3D描画・HUD・デバッグ表示と、タイトルへの遷移予約を担当するScene。
class GameScene final: public IScene
{
public:
	//========= 生成関数=========
	// Sceneで使用する非所有参照を登録する。Resource生成はInitで行う。
	// sceneManager: タイトルへの遷移予約先。Sceneの使用期間中は有効であること。
	// inputSystem: キー入力の取得先とCapture設定先。Sceneより長く生存すること。
	// graphicsSystem: Resource生成・描画先サイズ取得・描画の基盤。Resource終了まで有効であること。
	// audioSystem: Game用BGMの再生先。Sceneの使用期間中は有効であること。
	GameScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

	//========= Sceneライフサイクル関数=========
	// プレイヤー側一式・ステージ・判定の状態・敵の弾と範囲の予告・デバッグ設定を初期状態へ戻し、カメラをプレイヤー位置へ合わせる。
	void Initialize() override;
	// ステージ・HUD・Collision可視化を初期化し、敵の弾、範囲の予告、第1部屋の敵を生成して、成功後にFPS操作とGame用BGMを有効にする。
	// 戻り値: 各初期化・Object生成・敵の配置が成功したらtrue。falseを検出した場合はFinalize後にfalseを返す。
	// 下位処理の例外は捕捉しない。
	bool Init() override;
	// F2の遷移予約、F1のCapture切替、F4の再開、デバッグ操作を処理し、時間・部屋・プレイヤー・戦闘・敵の順に更新する。
	// deltaTime: 実時間の経過時間。単位は秒。各更新へは、ヒットストップ中は0、スロー中はプレイヤー以外へ縮めた値を、それぞれ渡す。
	// Capture解除中は視点・移動・戦闘・敵の行動の更新を行わない。F2で予約した場合は後続処理を行わない。
	void Update(float deltaTime) override;
	// 描画先サイズに合わせて行列を作り、3D・画像のHUD・読み取り専用のデバッグUIを、この順に描画する。
	// 幅または高さが0なら何もしない。ImGuiフレーム開始後、描画データ確定前に呼ぶ。
	void Draw() override;
	// デバッグ・プレイヤー側・敵を先に終了し、その後にステージ・HUD・Collision可視化を解放する。繰り返し呼出可能。CaptureやBGMは変更しない。
	void Finalize() override;

private:
	//========= 初期化の補助関数=========
	// ステージ・Collision可視化・HUDを初期化する。失敗の後始末は、呼出側のInitがFinalizeで行う。
	// 戻り値: 全て成功したらtrue。失敗した場合は、失敗した対象をログへ記録してfalse。
	[[nodiscard]] bool InitRenderResources();
	// 敵の更新へ、使用する参照と通知先を渡す。失敗の後始末は、呼出側のInitがFinalizeで行う。
	// 戻り値: 登録に成功したらtrue。
	[[nodiscard]] bool InitEnemyController();
	// プレイヤー側一式とデバッグ操作へ、使用する参照と通知先を渡す。失敗の後始末は、呼出側のInitがFinalizeで行う。
	// 戻り値: 両方の登録に成功したらtrue。
	[[nodiscard]] bool InitPlayerAndDebug();

	//========= 更新の補助関数=========
	// F2の遷移予約、F1のCapture切替、F4の最初の部屋からの再開を処理する。
	// 戻り値: F2でタイトルへの遷移を予約した場合はtrue。呼出側は、後続の更新を行わず戻る。
	[[nodiscard]] bool UpdateSceneInput();
	// 確認用の攻撃の判定と、敵の怯み・転倒・部屋の進行・出口の開閉を、Captureに関係なく更新する。
	// worldDeltaTime: スローを反映した経過時間。単位は秒。ヒットストップ中は0。
	void UpdateWorldAndRoom(float worldDeltaTime);
	// 回復アイテムの間隔・Collision可視化の表示時間・命中表示・反動のずれを、Captureに関係なく更新する。
	// deltaTime: 実時間の経過時間。単位は秒。表示用と反動の時間に使う。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。回復アイテムの間隔に使う。
	void UpdateTimers(float deltaTime,float gameplayDeltaTime);
	// 視点・プレイヤーの動作・出口の通過・殴打でのリロードのキャンセル・視野角を更新する。Capture中だけ呼ぶ。
	// deltaTime: 実時間の経過時間。単位は秒。視野角の補間に使う。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。プレイヤーの動作に使う。
	void UpdatePlayer(float deltaTime,float gameplayDeltaTime);
	// プレイヤーの戦闘・射撃・敵の行動と弾を更新し、Collision可視化の視線のRayを更新する。Capture中だけ呼ぶ。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。プレイヤーの戦闘と射撃に使う。
	// worldDeltaTime: スローを反映した経過時間。単位は秒。敵の行動と弾に使う。
	void UpdateCombat(float gameplayDeltaTime,float worldDeltaTime);
	// 確認用の攻撃が当たる瞬間を判定し、結果に応じてプレイヤーへのダメージとスローを反映する。
	// worldDeltaTime: スローを反映した経過時間。単位は秒。ヒットストップ中は0。
	void UpdateDodgeJudge(float worldDeltaTime);
	// 敵の攻撃イベントを受け取り、命中ならプレイヤーへの被弾の反応を、ジャスト回避ならスローを反映し、求められた場合は直近の判定結果を記録する。
	// attackEvent: EnemyControllerが通知した攻撃イベント。敵の更新の途中で呼ばれる。
	void ApplyEnemyAttackEvent(const ENEMY_ATTACK_EVENT& attackEvent);
	// 現在の部屋とウェーブの敵をEnemyControllerへ生成させ、成功した場合は、スライディングの転倒記録を初期化して、進行管理へ生成完了を通知する。
	// 戻り値: 登録に成功したらtrue。失敗した場合はEnemyManagerが空にし、進行管理には通知しない。
	[[nodiscard]] bool TrySpawnCurrentWave();
	// デバッグ用に、現在のウェーブの生きている敵を全て倒し、敵の予告・弾・範囲の予告を消す。次の更新の全滅判定で、次のウェーブまたは出口の開放へ進む。
	void SkipCurrentWave();
	// 出口の通過を判定し、次の部屋へ進んだ場合だけ、プレイヤーを開始位置へ戻す。
	void UpdateExitPass();

	//========= 描画の補助関数=========
	// 床・壁・範囲の予告・出口・敵・敵の弾・Collisionの線を、この順に3Dで描画する。
	// viewMatrix: World空間からView空間への変換行列。
	// projectionMatrix: View空間からClip空間への変換行列。
	void DrawWorld(const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix);
	// 3Dの上に、画像のHUD（照準・HPバー・弾薬・リロード表示・回復アイテム数・低HP警告）を描画する。
	void DrawHud();

	//========= System・Scene参照=========
	// Scene遷移の予約先となる非所有参照。
	SceneManager& m_sceneManager;
	// 操作入力とCapture設定に使用する非所有参照。
	InputSystem& m_inputSystem;
	// Resource初期化と描画に使用する非所有参照。
	GraphicsSystem& m_graphicsSystem;
	// Init成功後のGame用BGM再生に使用する非所有参照。
	AudioSystem& m_audioSystem;

	//========= Scene所有状態=========
	// プレイヤー・HP・回復アイテム・カメラ・近接攻撃と接触と回復・射撃・視野角を所有する。ヒットストップは、TimeScaleControllerへ求める。
	PlayerSuite m_player{};
	// 現在のウェーブの敵・敵の弾・範囲の予告・敵ごとの攻撃予告を所有し、更新と描画を行う。Finalizeで、登録先のCollisionWorldと共有Rendererより先に解除する。
	EnemyController m_enemyController{};
	// ヒットストップとスローの残り時間を持ち、更新ごとに、プレイヤー側と、敵・判定側の経過時間を求める。
	TimeScaleController m_timeScale{};
	// デバッグ操作・確認用の攻撃の判定・デバッグUIの情報の組立を担当する。ゲームの状態は、参照を通じてだけ変更する。
	GameDebugController m_debugController{};
	// 共有Renderer・床と壁・出口・CollisionWorld・部屋の進行を所有する。敵の生成と全滅判定は、GameSceneが仲介する。
	StageController m_stage{};

	//========= HUDとデバッグ=========
	// 画像で描くHUD。
	GameHud m_gameHud{};
	// Collisionの可視化（Colliderの線・視線のRay・弾道）。F3で表示を切り替える。
	CollisionDebugView m_collisionDebugView{};
};