#include "../H/GameSession.h"

// 1回のプレイの初期化、入力に応じた状態更新、射撃・近接攻撃・回避・回復・被弾の反映、部屋のクリア後の報酬選択とその効果の反映、ショップへの入力の受渡し、ジャスト回避の効果、部屋ごとの敵と敵の弾と範囲の予告と出口、描画とデバッグUIの呼出し、Resource終了を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_F1・VK_F2・VK_F4・VK_LBUTTON・VK_RETURNの仮想キーコードで、Capture切替・タイトルへ戻る要求・最初の部屋からの再開・カードやボタンのクリック・ショップを閉じるキーを指定する。

//========= 外部ライブラリ インクルード=========
#include <DirectXMath.h>

//========= Framework インクルード=========
#include "Framework/2D/H/HudCanvas.h"
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/InitCheck.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/CombatResolver.h"
#include "Scene/Common/Combat/H/CombatTable.h"

// このクラスの投影面（Clip面）の距離と、報酬選択・ショップ・武器の持ち替えの入力の補助に使用する固定設定をまとめる。戦闘の数値はCombatTableにある。
namespace
{
	//========= Projection設定=========
	// View空間での近距離Clip面の距離。単位はワールド距離単位。これより近い物は描画されない。
	constexpr float ns_NEAR_CLIP = 0.1f;
	// View空間での遠距離Clip面の距離。単位はワールド距離単位。これより遠い物は描画されない。
	constexpr float ns_FAR_CLIP = 1000.0f;

	//========= 報酬選択・ショップ・持ち替えの入力設定=========
	// カードや購入ボタン・武器を、キーで選ぶ時の、最初のものに対応するキー。1から順に、並びと同じ。クリックが使えない場合の代わりの操作。
	constexpr unsigned char ns_UI_FIRST_KEY = '1';
	// ショップのページ切替に使うキー。
	constexpr unsigned char ns_SHOP_SWITCH_KEY = 'U';
	// 描画先の大きさが無効な場合に、カーソルを置く、基準Canvasの外の位置。どのカードやボタンにも乗らない。
	constexpr float ns_CURSOR_OUTSIDE_CANVAS = -1000.0f;

	//========= 入力の補助関数=========
	// カーソルのクライアント座標を、基準Canvas上の座標へ変換する。HUDの描画と同じ倍率と余白（HudCanvas）の逆変換。
	// inputSystem: カーソルの位置の取得元。
	// graphicsSystem: 描画先の大きさの取得元。
	// 戻り値: 基準Canvas上のカーソルの位置。描画先の大きさが無効な場合は、どのカードやボタンにも乗らない位置。
	XMFLOAT2 ConvertCursorToCanvas(const InputSystem& inputSystem,const GraphicsSystem& graphicsSystem)
	{
		// HUDの描画と同じ、描画先の大きさから決まる倍率と余白を求める。倍率が0なら、変換できない。
		const HudCanvas hud_canvas{graphicsSystem.GetRenderWidth(),graphicsSystem.GetRenderHeight()};
		const float canvas_scale = hud_canvas.GetScale();
		if(canvas_scale <= 0.0f)return XMFLOAT2{ns_CURSOR_OUTSIDE_CANVAS,ns_CURSOR_OUTSIDE_CANVAS};

		// 描画先の座標から余白を引き、倍率で割って、基準Canvas上の座標へ戻す。
		const POINT cursor_position = inputSystem.GetCursorClientPosition();
		const XMFLOAT2& canvas_offset = hud_canvas.GetOffset();
		return XMFLOAT2{(static_cast<float>(cursor_position.x) - canvas_offset.x) / canvas_scale,
			(static_cast<float>(cursor_position.y) - canvas_offset.y) / canvas_scale};
	}

	// 数字キー（1から順）のうち、この更新で押されたキーに対応する添字を返す。複数が押された場合は、最後のキーを返す。
	// inputSystem: キー入力の取得元。
	// count: 対応させる項目の数。この数だけ、1から順にキーを調べる。
	// 戻り値: 押されたキーに対応する添字。どれも押されていなければ-1。
	int FindTriggeredNumberKey(const InputSystem& inputSystem,int count)
	{
		int triggered_index = -1;
		for(int item_index = 0; item_index < count; ++item_index){
			if(inputSystem.IsKeyTriggered(static_cast<unsigned char>(ns_UI_FIRST_KEY + item_index))){
				triggered_index = item_index;
			}
		}
		return triggered_index;
	}
}

//========= 生成関数=========
// 共有Systemを所有せず、使用する参照だけを登録する。
GameSession::GameSession(InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem)
	: m_inputSystem(inputSystem),m_graphicsSystem(graphicsSystem),m_audioSystem(audioSystem)
{}

//========= ライフサイクル関数=========
// GPU Resource生成の前に、操作と配置の初期状態を準備する。
void GameSession::Initialize()
{
	// プレイヤー側一式を既定へ戻し、カメラをプレイヤーの開始位置へ合わせる。
	m_player.Initialize();

	// 進行状態を第1部屋の第1ウェーブへ戻し、前回の生成失敗と、前回の生成ObjectとColliderを空にする。Rendererの生成、表からのObject生成、出口と敵の配置はInitで行う。
	m_stage.Initialize();

	// 判定・時間・表示の状態を既定へ戻す。
	m_debugController.Reset();
	m_enemyController.ResetAttackState();
	m_timeScale.Reset();

	// 通貨・報酬選択・敵の生死の記録を初期状態へ戻す。前回のプレイの敵を、倒された敵として扱わない。
	m_economy.Initialize();

	// 武器の所持・強化・保存した弾薬を初期状態へ戻し、ショップを閉じる。
	m_shopFlow.Initialize();
}

// 描画Resourceと生成Objectの準備に成功した後にだけ、ゲーム操作とBGMへ切り替える。
bool GameSession::Init()
{
	// 描画Resource・各Controllerの参照・第1部屋の敵を、この順に準備する。順序は、後の対象が前の対象の参照を借用するため変えない。
	// どれかが失敗したら、生成済みのResourceを含めてFinalizeで終了して、呼出側へ失敗を返す。Captureと再生は変更しない。
	if(!InitRenderResources() || !InitEnemyController() || !InitPlayerAndDebug() || !TrySpawnCurrentWave()){
		Finalize();
		return false;
	}

	// Resource生成とObject生成が成功してからCaptureとBGMを変更し、false返却時には変更しない。
	m_inputSystem.SetMouseCaptureEnabled(true);
	m_audioSystem.PlayGameBgm();
	return true;
}

// F2・F1・F4の入力を先に処理し、操作可能な場合だけ視点・移動・戦闘を更新する。
GAME_SESSION_RESULT GameSession::Update(float deltaTime)
{
	// F2の確認・F1のCapture切替・F4の再開を先に処理する。F2が押された場合は、同じフレームの後続の更新を行わず、タイトルへ戻る要求を返す。
	if(UpdateSessionInput())return GAME_SESSION_RESULT::e_RETURN_TO_TITLE;

	// F3・F5〜F9・Insertのデバッグ操作を反映する。F11が押された場合は、現在のウェーブの敵を全て倒して、次のウェーブまたは出口の開放へ進める。
	// Capture解除中も操作できるよう、Captureの判定より前に処理する。
	if(m_debugController.UpdateCommands()){
		SkipCurrentWave();
	}

	// 報酬選択中とショップが開いている間は、時間を進めない。ヒットストップ・スロー・回復アイテムの間隔・表示の時間も、この更新では0を渡して、画面を閉じるまで止める。
	const float frame_delta_time = (m_economy.GetReward().IsSelecting() || m_shopFlow.IsOpen()) ? 0.0f : deltaTime;

	// ヒットストップとスローの残り時間を進め、プレイヤー側の時間と、敵・判定側の時間を求める。ヒットストップ中は、どちらも0になる。
	// スロー中は、敵・判定側の時間だけが遅くなる。プレイヤーと武器は、スローの影響を受けない。
	const TIME_SCALE_DELTAS time_deltas = m_timeScale.Advance(frame_delta_time);

	// 確認用の攻撃・敵の怯みと転倒・部屋の進行・出口の開閉と、各種の残り時間を、Captureに関係なく進める。
	// その後、ここまでに倒された敵（デバッグ操作で倒した敵を含む）の撃破報酬を、通貨へ加える。
	UpdateWorldAndRoom(time_deltas.worldDeltaTime);
	UpdateTimers(frame_delta_time,time_deltas.gameplayDeltaTime);
	m_economy.UpdateKillRewards(m_enemyController.GetEnemyManager());

	// 部屋のクリアの結果を受けて、報酬選択へ入る・抜ける。抜けた更新で、続けてショップを開く。ショップが開いている間は、購入・売却・強化・閉じる操作を処理する。
	UpdateReward();
	UpdateShop();

	// マウスを解放している間と、報酬選択中・ショップが開いている間は、カメラ・移動・戦闘の操作更新をまとめて停止し、ここで戻る。
	if(!m_inputSystem.IsMouseCaptureEnabled() || m_economy.GetReward().IsSelecting() || m_shopFlow.IsOpen())return GAME_SESSION_RESULT::e_CONTINUE;

	// カードやボタンを選んだ直後は、選んだクリックや数字キーが、射撃や武器の持ち替えとして重ねて反応しないよう、ここで戻る。左クリックが離されたら、解除する。
	if(m_economy.GetReward().UpdateInputGuard(m_inputSystem.IsKeyPressed(VK_LBUTTON)))return GAME_SESSION_RESULT::e_CONTINUE;

	// 武器の持ち替えを、射撃の更新より先に処理する。持ち替えたフレームから、新しい武器で射撃する。
	// 視点とプレイヤーを先に更新し、その結果の位置と動作の状態を使って、戦闘・射撃・敵の行動を更新する。
	// 射撃・近接攻撃で倒された敵の撃破報酬は、次の更新を待たず、ここで通貨へ加える。
	m_shopFlow.UpdateWeaponSwitch(FindTriggeredNumberKey(m_inputSystem,WEAPON_TABLE::ns_WEAPON_COUNT));
	UpdatePlayer(deltaTime,time_deltas.gameplayDeltaTime);
	UpdateCombat(time_deltas.gameplayDeltaTime,time_deltas.worldDeltaTime);
	m_economy.UpdateKillRewards(m_enemyController.GetEnemyManager());
	return GAME_SESSION_RESULT::e_CONTINUE;
}

// 現在の描画先サイズと視点から3D描画とHUDを行い、操作状態をデバッグUIへ表示する。
void GameSession::Draw()
{
	// 描画先の現在サイズをpixel単位で取得する。
	const unsigned int render_width = m_graphicsSystem.GetRenderWidth();
	const unsigned int render_height = m_graphicsSystem.GetRenderHeight();
	// 最小化中などで幅または高さが0なら、アスペクト比の除算と描画を行わず戻る。
	if(render_width == 0 || render_height == 0)return;

	// 現在サイズの比率で左手系の透視投影行列を作り、カメラからView行列を取得する。視野角は動作の状態を反映した現在値を使う。
	// 投影行列の生成関数と角度変換は、MathTypes.hのusing宣言により修飾なしで呼ぶ。
	const float aspect_ratio = static_cast<float>(render_width) / static_cast<float>(render_height);
	const XMMATRIX projection_matrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_player.GetFovDegrees()),aspect_ratio,ns_NEAR_CLIP,ns_FAR_CLIP);
	const XMMATRIX view_matrix = m_player.GetCamera().GetViewMatrix();

	// 3D・画像のHUD・デバッグUIの順に描画する。デバッグUIはゲームの状態を変更しない。
	m_renderer.DrawWorld(m_graphicsSystem,view_matrix,projection_matrix);
	DrawHud();
	m_debugController.DrawUi(m_stage.HasWaveSpawnFailed());
}

// 所有する敵・敵の弾・範囲の予告・プレイヤー側・ステージ・描画を終了する。
void GameSession::Finalize()
{
	// 遷移先のInit後に呼ばれても、遷移先が設定したCaptureやBGMを上書きしない。
	// 各Controllerの参照を先に手放し、敵・敵の弾・範囲の予告は、Collisionと共有Rendererを借用しているため、ステージを解放する前に先に終了する。
	m_debugController.Uninit();
	m_shopFlow.Unlink();
	m_player.Uninit();
	m_enemyController.Uninit();
	m_stage.Uninit();

	// HUDと線描画Rendererを解放する。
	m_renderer.Uninit();
}

//========= 初期化の補助関数=========
// ステージ・描画（HUDとCollision可視化）を初期化する。
bool GameSession::InitRenderResources()
{
	// 描画が借用する参照先を用意する。ステージ・敵・カメラは、このクラスが所有するため、Finalizeまで有効。
	GAME_RENDERER_LINKS renderer_links{};
	renderer_links.stage = &m_stage;
	renderer_links.enemyController = &m_enemyController;
	renderer_links.camera = &m_player.GetCamera();

	// 2つの初期化を、この順に実行する。最初に失敗した対象のメッセージだけをログへ記録し、後続は実行せずfalseを返す。後始末は、呼出側のInitがFinalizeで行う。
	// 共有Renderer・床と壁・出口の初期化失敗を検出する。失敗した対象のログは、StageControllerが記録する。
	// 描画（Collision可視化とHUD）の初期化失敗は、GameRendererが記録する。
	return m_stage.Init(m_graphicsSystem) && m_renderer.Initialize(m_graphicsSystem,renderer_links);
}

// 敵の更新へ、使用する参照と通知先を渡す。
bool GameSession::InitEnemyController()
{
	// 敵の更新に使う参照と、攻撃イベントの通知先（被弾・スロー・記録の反映）を用意する。
	ENEMY_CONTROLLER_LINKS enemy_links{};
	enemy_links.modelRegistry = &m_stage.GetModelRegistry();
	enemy_links.collisionWorld = &m_stage.GetCollisionWorld();
	enemy_links.playerController = &m_player.GetPlayerController();
	enemy_links.playerHealth = &m_player.GetPlayerHealth();
	enemy_links.onAttackEvent = [this](const ENEMY_ATTACK_EVENT& attackEvent){
		ApplyEnemyAttackEvent(attackEvent);
	};

	// 敵の弾と範囲の予告のCube Rendererを借用する。借用できなければ、敵の更新の登録は失敗する。
	return InitCheck::Succeeded(m_enemyController.Initialize(enemy_links),e_LogCategory::e_SCENE,L"GameSessionの敵の更新の初期化に失敗しました。");
}

// プレイヤー側一式・ショップ・デバッグ操作へ、使用する参照と通知先を渡す。
bool GameSession::InitPlayerAndDebug()
{
	// プレイヤー側一式に使う参照と、ヒットストップの通知先を用意する。ヒットストップは、すでに残っている場合は長い方を使う。
	CollisionDebugView& collision_debug_view = m_renderer.GetCollisionDebugView();
	PLAYER_SUITE_LINKS player_links{};
	player_links.inputSystem = &m_inputSystem;
	player_links.collisionWorld = &m_stage.GetCollisionWorld();
	player_links.enemyManager = &m_enemyController.GetEnemyManager();
	player_links.collisionDebugView = &collision_debug_view;
	player_links.onHitStopRequested = [this](float hitStopSeconds){
		m_timeScale.RequestHitStop(hitStopSeconds);
	};

	// ショップが、支払いと反映に使う通貨と射撃を用意する。
	SHOP_FLOW_LINKS shop_links{};
	shop_links.wallet = &m_economy.GetWallet();
	shop_links.shotController = &m_player.GetShotController();

	// デバッグ操作とデバッグUIに使う参照を用意する。ゲームの状態は、各クラスの公開関数を通じてだけ変更される。
	GAME_DEBUG_CONTROLLER_LINKS debug_links{};
	debug_links.inputSystem = &m_inputSystem;
	debug_links.collisionDebugView = &collision_debug_view;
	debug_links.playerController = &m_player.GetPlayerController();
	debug_links.playerHealth = &m_player.GetPlayerHealth();
	debug_links.healItems = &m_player.GetHealItems();
	debug_links.playerCombat = &m_player.GetPlayerCombat();
	debug_links.roomController = &m_stage.GetRoomController();
	debug_links.enemyController = &m_enemyController;
	debug_links.shotController = &m_player.GetShotController();
	debug_links.timeScale = &m_timeScale;

	// プレイヤー側一式、ショップ、デバッグ操作の順に登録する。最初に失敗した対象のメッセージだけをログへ記録し、後続は実行せずfalseを返す。
	// プレイヤー側一式の失敗のログは、PlayerSuiteが、ショップの失敗のログは、ShopFlowが記録する。
	return m_player.Init(player_links)
		&& m_shopFlow.Link(shop_links)
		// 参照先の登録に失敗した場合は、デバッグ操作の登録が失敗する。
		&& InitCheck::Succeeded(m_debugController.Initialize(debug_links),e_LogCategory::e_SCENE,L"GameSessionのデバッグ操作の初期化に失敗しました。");
}

//========= 更新の補助関数=========
// F2の確認、F1のCapture切替、F4の最初の部屋からの再開を処理する。
bool GameSession::UpdateSessionInput()
{
	// F2が押された瞬間は、同じフレームのCapture切替や移動を行わず、呼出側へ戻るよう伝える。Scene遷移の予約は、呼出側が行う。
	if(m_inputSystem.IsKeyTriggered(VK_F2)){
		return true;
	}

	// F1が押された瞬間は、ゲーム操作とImGui操作に使うCapture状態を反転する。
	if(m_inputSystem.IsKeyTriggered(VK_F1)){
		m_inputSystem.SetMouseCaptureEnabled(!m_inputSystem.IsMouseCaptureEnabled());
	}

	// F4では最初の部屋から作り直す。旧ウェーブと、旧ウェーブの弾・範囲の予告は、同じ更新の生成処理で置き換わる。通貨・報酬の取得数・武器の所持と強化は、そのまま引き継ぐ。
	if(m_inputSystem.IsKeyTriggered(VK_F4)){
		m_stage.Restart();
		m_enemyController.ResetAttackState();
		m_player.GetPlayerCombat().ClearSlideKnockdownRecord();

		// 報酬選択かショップの最中に再開した場合は、解除したCaptureを戻して、選択と購入画面を終える。
		if(m_economy.GetReward().IsSelecting() || m_shopFlow.IsOpen()){
			m_inputSystem.SetMouseCaptureEnabled(true);
		}
		m_economy.ResetRewardSelection();
		m_shopFlow.Close();
	}
	return false;
}

// 確認用の攻撃の判定と、敵の怯み・転倒・部屋の進行・出口の開閉を、Captureに関係なく更新する。
void GameSession::UpdateWorldAndRoom(float worldDeltaTime)
{
	// 確認用の攻撃の判定、敵の怯み・転倒を、Captureに関係なく進める。
	UpdateDodgeJudge(worldDeltaTime);
	m_enemyController.GetEnemyManager().Update(worldDeltaTime);

	// 生成待ちなら現在のウェーブを生成し、結果を記録する。失敗した場合は再試行せず、F4で最初からやり直すまで敵なしで待つ。
	if(m_stage.ShouldSpawnWave()){
		m_stage.RecordSpawnResult(TrySpawnCurrentWave());
	}

	// 敵の全滅の結果から、ウェーブまたは部屋を進め、出口を開閉する。
	m_stage.UpdateProgress(m_enemyController.GetEnemyManager().HasLivingEnemies());
}

// 回復アイテムの間隔・Collision可視化の表示時間・命中表示・反動のずれを、Captureに関係なく更新する。
void GameSession::UpdateTimers(float deltaTime,float gameplayDeltaTime)
{
	// プレイヤー側の回復アイテムの間隔・命中表示・反動のずれを進める。Collision可視化の表示用の残り時間は、実時間で進め、0未満にしない。
	m_player.UpdateTimers(deltaTime,gameplayDeltaTime);
	m_renderer.GetCollisionDebugView().Update(deltaTime);
}

// 部屋のクリアで報酬選択へ入る判定と、カードが選ばれた時に抜ける判定を行い、取得した報酬の効果を各クラスへ渡す。
void GameSession::UpdateReward()
{
	// 部屋の進行状態と、カーソルの位置（基準Canvas上）、左クリック、カードの番号のキーを渡して、状態の変化を受け取る。
	// 上限に達した報酬の置換の通貨と、選ばれた報酬のログは、EconomyControllerが処理する。
	const REWARD_UPDATE_RESULT reward_result = m_economy.UpdateReward(m_stage.GetRoomController().GetPhase(),
																																		ConvertCursorToCanvas(m_inputSystem,m_graphicsSystem),
																																		m_inputSystem.IsKeyTriggered(VK_LBUTTON),
																																		FindTriggeredNumberKey(m_inputSystem,REWARD_TABLE::ns_CARD_COUNT));

	// 入った更新では、Captureを解除して、マウスをカードの操作へ渡す。ゲームの操作と時間は、Updateが止める。
	if(reward_result == REWARD_UPDATE_RESULT::e_STARTED){
		m_inputSystem.SetMouseCaptureEnabled(false);
		Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,L"部屋のクリアで、報酬選択へ入りました。");
	}
	// 抜けた更新では、続けてショップを開く。Captureは、ショップを閉じるまで解除したままにする。出口は、すでに開いている。
	else if(reward_result == REWARD_UPDATE_RESULT::e_FINISHED){
		m_shopFlow.Open();
	}

	// 常に効く特殊強化の効果を、使う側へ渡す。毎回渡して、再開や初期化の後も、取得数と一致させる。
	// 制圧射撃は、射撃で敵へ与える怯み時間の倍率として、射撃の担当へ渡す。殴打追撃は、殴打が当たった敵へ付ける印の倍率と時間として、戦闘の担当へ渡す。
	// 回避反撃は、ジャスト回避の時だけ効くため、ここでは渡さない（StartJustDodgeEffectsが渡す）。
	const RewardController& reward = m_economy.GetReward();
	m_player.GetShotController().SetFlinchScale(reward.GetSuppressiveFlinchScale());
	m_player.GetPlayerCombat().SetFollowUpStrike(reward.GetFollowUpDamageScale(),REWARD_TABLE::ns_FOLLOW_UP_SECONDS);
}

// ショップが開いている間、入力をShopFlowへ渡し、閉じる操作があればCaptureを再開する。
void GameSession::UpdateShop()
{
	// 閉じている間は、何もしない。
	if(!m_shopFlow.IsOpen())return;

	// カーソルの位置（基準Canvas上）、左クリック、数字キー、Enter、Uを渡す。数字キーは、売買のページの武器と弾薬のボタンの数だけ調べる（強化のページは、先頭の5個を使う）。
	SHOP_FLOW_INPUT shop_input{};
	shop_input.cursorCanvasPosition = ConvertCursorToCanvas(m_inputSystem,m_graphicsSystem);
	shop_input.isClickTriggered = m_inputSystem.IsKeyTriggered(VK_LBUTTON);
	shop_input.numberKeyIndex = FindTriggeredNumberKey(m_inputSystem,SHOP_TABLE::ns_UPGRADE_BUTTON_OFFSET);
	shop_input.isCloseKeyTriggered = m_inputSystem.IsKeyTriggered(VK_RETURN);
	shop_input.isSwitchKeyTriggered = m_inputSystem.IsKeyTriggered(ns_SHOP_SWITCH_KEY);

	// 閉じた場合は、Captureを戻して、ゲームの操作へ戻る。選んだクリックが射撃にならないよう、入力の保護が、続けて働く。
	if(m_shopFlow.Update(shop_input)){
		m_inputSystem.SetMouseCaptureEnabled(true);
		Logger::Write(e_LogLevel::e_INFO,e_LogCategory::e_SCENE,L"ショップを閉じました。");
	}
}

// ジャスト回避が成立した時の効果を反映する。
void GameSession::StartJustDodgeEffects()
{
	// ジャスト回避のスローを開始する。連続した場合は、時間を数え直す。
	m_timeScale.StartSlowMotion();

	// 回避反撃を取得している（倍率が1.0より大きい）場合だけ、一定時間、射撃のダメージを増やす。未取得なら、何も増やさない。
	const float damage_scale = m_economy.GetReward().GetCounterDodgeDamageScale();
	if(damage_scale > 1.0f){
		m_player.GetShotController().ActivateDamageBoost(damage_scale,REWARD_TABLE::ns_COUNTER_DODGE_SECONDS);
	}
}

// 視点・プレイヤーの動作・出口の通過・殴打でのリロードのキャンセル・視野角を更新する。
void GameSession::UpdatePlayer(float deltaTime,float gameplayDeltaTime)
{
	// 視線とプレイヤーの動作と位置を更新する。ヒットストップ中も視線は動かせる。
	m_player.UpdateMovement(gameplayDeltaTime);

	// 移動後の位置で、出口の通過を判定する。通過した場合は、プレイヤーが開始位置へ戻るため、以降の更新は新しい位置から行う。
	UpdateExitPass();

	// 通過後の動作の状態を使って、殴打でのリロードのキャンセルと、視野角の補間を行う。
	m_player.UpdateView(deltaTime);
}

// プレイヤーの戦闘・射撃・敵の行動と弾を更新し、Collision可視化の視線のRayを更新する。
void GameSession::UpdateCombat(float gameplayDeltaTime,float worldDeltaTime)
{
	// 近接攻撃・接触・回復・射撃を、この順に更新する。
	m_player.UpdateCombat(gameplayDeltaTime);
	// 当フレームの回避・移動・敵への怯みを反映してから、敵の行動と、敵の弾の命中を判定する。
	m_enemyController.Update(worldDeltaTime);

	// 更新後のカメラから、Collision可視化の視線のRayを更新する。非表示の間は判定を行わない。
	m_renderer.GetCollisionDebugView().UpdateRay(m_player.GetCamera(),m_stage.GetCollisionWorld());
}

// 確認用の攻撃が当たる瞬間の判定結果を、プレイヤーへのダメージとジャスト回避の効果へ反映する。
void GameSession::UpdateDodgeJudge(float worldDeltaTime)
{
	// 予告した攻撃が当たる瞬間だけ、結果が返る。結果の記録は、GameDebugControllerが行う。
	const DODGE_JUDGE_RESULT judge_result = m_debugController.UpdateDodgeJudge(worldDeltaTime);

	// 命中した場合は、ダメージ・怯み・ヒットストップを与える。
	if(judge_result == DODGE_JUDGE_RESULT::e_HIT){
		m_player.GetPlayerCombat().ApplyHitReaction(COMBAT_TABLE::ns_SIMULATED_ATTACK_DAMAGE,COMBAT_TABLE::ns_DAMAGE_HIT_STOP_SECONDS);
	}
	// ジャスト回避の場合は、スローと回避反撃を開始する。
	else if(judge_result == DODGE_JUDGE_RESULT::e_JUST_DODGED){
		StartJustDodgeEffects();
	}
}

// 敵の攻撃イベントを、プレイヤーへの被弾・ジャスト回避の効果・表示用の記録へ反映する。
void GameSession::ApplyEnemyAttackEvent(const ENEMY_ATTACK_EVENT& attackEvent)
{
	// 表示用の記録が求められた場合だけ、直近の判定結果を更新する。接近型の結果は求めない。
	if(attackEvent.shouldRecordResult){
		m_debugController.RecordJudgeResult(attackEvent.result);
	}

	// 無敵でなく命中した場合は、ダメージ・怯み・ヒットストップを与える。
	if(attackEvent.result == DODGE_JUDGE_RESULT::e_HIT){
		m_player.GetPlayerCombat().ApplyHitReaction(attackEvent.damage,COMBAT_TABLE::ns_DAMAGE_HIT_STOP_SECONDS);
	}
	// 実際に回避中のジャスト回避の場合は、スローと回避反撃を開始する。
	else if(attackEvent.shouldStartSlowMotion){
		StartJustDodgeEffects();
	}
}

// 現在の部屋とウェーブをEnemyControllerへ生成させ、成功した場合だけ転倒の記録と進行状態を新しい編成に合わせる。
bool GameSession::TrySpawnCurrentWave()
{
	// 生成を試みる。成功しても失敗しても、旧ウェーブの敵は、この中で解除される。
	const bool is_spawned = m_enemyController.TrySpawnWave(m_stage.GetRoomNumber(),m_stage.GetWaveNumber());

	// 敵の枠ごとの生死の記録を、生成後の状態へ揃える。解除された旧ウェーブの敵を、倒された敵として扱わず、撃破報酬を与えない。
	m_economy.SyncEnemyAliveFlags(m_enemyController.GetEnemyManager());

	// 登録に失敗した場合は、EnemyControllerが失敗を記録し、EnemyManagerが生成済みの枠を解除して空にする。進行管理へ生成完了を通知せず、失敗を返す。
	if(!is_spawned)return false;

	// 新しい敵がスライディングの転倒記録を引き継がないよう初期化し、全滅判定を開始する。
	m_player.GetPlayerCombat().ClearSlideKnockdownRecord();
	m_stage.NotifyWaveSpawned();
	return true;
}

// 生きている全ての敵を、残りHPと同じダメージで倒し、敵の予告・弾・範囲の予告を消す。
void GameSession::SkipCurrentWave()
{
	// 全枠を先頭から調べ、生きている敵だけを倒す。倒された敵のColliderは、ApplyDamageの中で解除される。
	for(Enemy& enemy : m_enemyController.GetEnemyManager().GetEnemies()){
		// 死亡中の敵は、倒す対象がないため次の枠へ進む。
		if(!enemy.IsAlive())continue;

		enemy.ApplyDamage(enemy.GetCurrentHP(),m_stage.GetCollisionWorld());
	}

	// 倒された敵の予告・飛行中の弾・範囲の予告を消す。全滅の判定は、次の更新で進行管理が行う。
	m_enemyController.ResetAttackState();
}

// 出口を通過したプレイヤーを、次の部屋の開始位置へ移す。
void GameSession::UpdateExitPass()
{
	// 出口が閉じている、位置が通過範囲の外にある、または進行管理が通過を受理しなかった場合は、何もしない。最終部屋の通過後や、受理済みの重複した通知では受理されない。
	if(!m_stage.TryPassExit(m_player.GetPlayerController().GetPosition()))return;

	// 次の部屋へ進んだので、プレイヤーを開始位置へ戻し、カメラ位置を合わせる。出口は、次の更新の進行状態の同期で閉じる。
	m_player.ReturnToStart();
}

//========= 描画の補助関数=========
// 現在の値を集めて、画像のHUDの描画をGameRendererへ求める。
void GameSession::DrawHud()
{
	// 現在の値をHUDへ渡して描画する。弾薬とリロードの値は、射撃を担当するShotControllerの武器から読み取る。ショップと武器の所持に関する項目は、ShopFlowが書き込む。
	const WeaponSystem& weapon = m_player.GetShotController().GetWeaponSystem();
	const RewardController& reward = m_economy.GetReward();
	GAME_HUD_STATE hud_state{};
	hud_state.currentHP = m_player.GetPlayerHealth().GetCurrentHP();
	hud_state.maxHP = m_player.GetPlayerHealth().GetMaxHP();
	hud_state.healItemCount = m_player.GetHealItems().GetCount();
	hud_state.magazineAmmo = weapon.GetMagazineAmmo();
	hud_state.magazineSize = weapon.GetMagazineSize();
	hud_state.reserveAmmo = weapon.GetReserveAmmo();
	hud_state.isReloading = weapon.IsReloading();
	hud_state.reloadProgress = weapon.GetReloadProgress();
	hud_state.isAiming = m_player.GetPlayerController().IsAiming();
	hud_state.currency = m_economy.GetWallet().GetBalance();
	hud_state.isRewardSelecting = reward.IsSelecting();
	hud_state.hoveredRewardIndex = reward.GetHoveredIndex();
	hud_state.rewardOwnedCounts = reward.GetAcquiredCounts();
	m_shopFlow.FillHudState(hud_state);
	m_renderer.DrawHud(m_graphicsSystem,hud_state);
}