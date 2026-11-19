#include "../H/GameSession.h"

// 1回のプレイの初期化、入力に応じた状態更新、射撃・近接攻撃・回避・回復・被弾の反映、部屋ごとの敵と敵の弾と範囲の予告と出口、画像のHUD・デバッグUIの描画、Resource終了を実装する。

//========= Windows インクルード=========
#include <windows.h> // VK_F1・VK_F2・VK_F4の仮想キーコードで、Capture切替・タイトルへ戻る要求・最初の部屋からの再開のキーを指定する。

//========= 外部ライブラリ インクルード=========
#include <DirectXMath.h>

//========= Framework インクルード=========
#include "Framework/Audio/H/AudioSystem.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/InitCheck.h"
#include "Framework/Etc/H/Logger.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/CombatResolver.h"
#include "Scene/Common/Combat/H/CombatTable.h"

// このクラスの投影面（Clip面）の距離に使用する固定設定をまとめる。戦闘の数値はCombatTableにある。
namespace
{
  //========= Projection設定=========
  // View空間での近距離Clip面の距離。単位はワールド距離単位。これより近い物は描画されない。
  constexpr float ns_NEAR_CLIP = 0.1f;
  // View空間での遠距離Clip面の距離。単位はワールド距離単位。これより遠い物は描画されない。
  constexpr float ns_FAR_CLIP = 1000.0f;
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

  // F3・F5〜F9のデバッグ操作を反映する。F11が押された場合は、現在のウェーブの敵を全て倒して、次のウェーブまたは出口の開放へ進める。
  // Capture解除中も操作できるよう、Captureの判定より前に処理する。
  if(m_debugController.UpdateCommands()){
    SkipCurrentWave();
  }

  // ヒットストップとスローの残り時間を実時間で進め、プレイヤー側の時間と、敵・判定側の時間を求める。ヒットストップ中は、どちらも0になる。
  // スロー中は、敵・判定側の時間だけが遅くなる。プレイヤーと武器は、スローの影響を受けない。
  const TIME_SCALE_DELTAS time_deltas = m_timeScale.Advance(deltaTime);

  // 確認用の攻撃・敵の怯みと転倒・部屋の進行・出口の開閉と、各種の残り時間を、Captureに関係なく進める。
  UpdateWorldAndRoom(time_deltas.worldDeltaTime);
  UpdateTimers(deltaTime,time_deltas.gameplayDeltaTime);

  // マウスを解放している間は、カメラ・移動・戦闘の操作更新をまとめて停止し、ここで戻る。
  if(!m_inputSystem.IsMouseCaptureEnabled())return GAME_SESSION_RESULT::e_CONTINUE;

  // 視点とプレイヤーを先に更新し、その結果の位置と動作の状態を使って、戦闘・射撃・敵の行動を更新する。
  UpdatePlayer(deltaTime,time_deltas.gameplayDeltaTime);
  UpdateCombat(time_deltas.gameplayDeltaTime,time_deltas.worldDeltaTime);
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
  DrawWorld(view_matrix,projection_matrix);
  DrawHud();
  m_debugController.DrawUi(m_stage.HasWaveSpawnFailed());
}

// 所有する敵・敵の弾・範囲の予告・プレイヤー側・ステージ・HUD・Collision可視化を終了する。
void GameSession::Finalize()
{
  // 遷移先のInit後に呼ばれても、遷移先が設定したCaptureやBGMを上書きしない。
  // 各Controllerの参照を先に手放し、敵・敵の弾・範囲の予告は、Collisionと共有Rendererを借用しているため、ステージを解放する前に先に終了する。
  m_debugController.Uninit();
  m_player.Uninit();
  m_enemyController.Uninit();
  m_stage.Uninit();

  // HUDと線描画Rendererを解放する。
  m_gameHud.Uninit();
  m_collisionDebugView.Uninit();
}

//========= 初期化の補助関数=========
// ステージ・Collision可視化・HUDを初期化する。
bool GameSession::InitRenderResources()
{
  // 3つの初期化を、この順に実行する。最初に失敗した対象のメッセージだけをログへ記録し、後続は実行せずfalseを返す。後始末は、呼出側のInitがFinalizeで行う。
  // 共有Renderer・床と壁・出口の初期化失敗を検出する。失敗した対象のログは、StageControllerが記録する。
  return m_stage.Init(m_graphicsSystem)
         // Collision可視化の初期化失敗を検出する。
         && InitCheck::Succeeded(m_collisionDebugView.Initialize(m_graphicsSystem),e_LogCategory::e_SCENE,L"GameSessionの線描画Renderer初期化に失敗しました。")
         // 画像のHUDの初期化失敗を検出する。個々の画像の読込失敗はここでは失敗にならない。
         && InitCheck::Succeeded(m_gameHud.Initialize(m_graphicsSystem),e_LogCategory::e_SCENE,L"GameSessionのHUD初期化に失敗しました。");
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

// プレイヤー側一式とデバッグ操作へ、使用する参照と通知先を渡す。
bool GameSession::InitPlayerAndDebug()
{
  // プレイヤー側一式に使う参照と、ヒットストップの通知先を用意する。ヒットストップは、すでに残っている場合は長い方を使う。
  PLAYER_SUITE_LINKS player_links{};
  player_links.inputSystem = &m_inputSystem;
  player_links.collisionWorld = &m_stage.GetCollisionWorld();
  player_links.enemyManager = &m_enemyController.GetEnemyManager();
  player_links.collisionDebugView = &m_collisionDebugView;
  player_links.onHitStopRequested = [this](float hitStopSeconds){
    m_timeScale.RequestHitStop(hitStopSeconds);
  };

  // デバッグ操作とデバッグUIに使う参照を用意する。ゲームの状態は、各クラスの公開関数を通じてだけ変更される。
  GAME_DEBUG_CONTROLLER_LINKS debug_links{};
  debug_links.inputSystem = &m_inputSystem;
  debug_links.collisionDebugView = &m_collisionDebugView;
  debug_links.playerController = &m_player.GetPlayerController();
  debug_links.playerHealth = &m_player.GetPlayerHealth();
  debug_links.healItems = &m_player.GetHealItems();
  debug_links.playerCombat = &m_player.GetPlayerCombat();
  debug_links.roomController = &m_stage.GetRoomController();
  debug_links.enemyController = &m_enemyController;
  debug_links.shotController = &m_player.GetShotController();
  debug_links.timeScale = &m_timeScale;

  // プレイヤー側一式、デバッグ操作の順に登録する。最初に失敗した対象のメッセージだけをログへ記録し、後続は実行せずfalseを返す。
  // プレイヤー側一式の失敗のログは、PlayerSuiteが記録する。
  return m_player.Init(player_links)
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

  // F4では最初の部屋から作り直す。旧ウェーブと、旧ウェーブの弾・範囲の予告は、同じ更新の生成処理で置き換わる。
  if(m_inputSystem.IsKeyTriggered(VK_F4)){
    m_stage.Restart();
    m_enemyController.ResetAttackState();
    m_player.GetPlayerCombat().ClearSlideKnockdownRecord();
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
  m_collisionDebugView.Update(deltaTime);
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
  m_collisionDebugView.UpdateRay(m_player.GetCamera(),m_stage.GetCollisionWorld());
}

// 確認用の攻撃が当たる瞬間の判定結果を、プレイヤーへのダメージとスローへ反映する。
void GameSession::UpdateDodgeJudge(float worldDeltaTime)
{
  // 予告した攻撃が当たる瞬間だけ、結果が返る。結果の記録は、GameDebugControllerが行う。
  const DODGE_JUDGE_RESULT judge_result = m_debugController.UpdateDodgeJudge(worldDeltaTime);

  // 命中した場合は、ダメージ・怯み・ヒットストップを与える。
  if(judge_result == DODGE_JUDGE_RESULT::e_HIT){
    m_player.GetPlayerCombat().ApplyHitReaction(COMBAT_TABLE::ns_SIMULATED_ATTACK_DAMAGE,COMBAT_TABLE::ns_DAMAGE_HIT_STOP_SECONDS);
  }
  // ジャスト回避の場合は、スローを開始する。連続した場合は、時間を数え直す。
  else if(judge_result == DODGE_JUDGE_RESULT::e_JUST_DODGED){
    m_timeScale.StartSlowMotion();
  }
}

// 敵の攻撃イベントを、プレイヤーへの被弾・スロー・表示用の記録へ反映する。
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
  // 実際に回避中のジャスト回避の場合は、スローを開始する。連続した場合は、時間を数え直す。
  else if(attackEvent.shouldStartSlowMotion){
    m_timeScale.StartSlowMotion();
  }
}

// 現在の部屋とウェーブをEnemyControllerへ生成させ、成功した場合だけ転倒の記録と進行状態を新しい編成に合わせる。
bool GameSession::TrySpawnCurrentWave()
{
  // 登録に失敗した場合は、EnemyControllerが失敗を記録し、EnemyManagerが生成済みの枠を解除して空にする。進行管理へ生成完了を通知せず、失敗を返す。
  if(!m_enemyController.TrySpawnWave(m_stage.GetRoomNumber(),m_stage.GetWaveNumber()))return false;

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
// 床・壁・範囲の予告・出口・敵・敵の弾・Collisionの線を、この順に3Dで描画する。
void GameSession::DrawWorld(const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix)
{
  // 床・壁・範囲の予告・出口・敵・敵の弾はOPAQUE Passで描画するため、描画前にPassを設定する。
  m_graphicsSystem.SetRenderPass(e_RenderPass::e_OPAQUE);

  // 生成済みの全Objectを、表の順（床→左壁→右壁→手前壁→奥壁）に描画する。
  m_stage.DrawObjects(m_graphicsSystem,viewMatrix,projectionMatrix);

  // 床の上に、範囲の予告を描画する。床の後に描き、敵や出口より先に描くことで、敵の体が予告の上に重なる。続けて出口を描画する。
  m_enemyController.DrawAreaTelegraph(m_graphicsSystem,viewMatrix,projectionMatrix);
  m_stage.DrawExit(m_graphicsSystem,viewMatrix,projectionMatrix);
  // 続けて全枠の敵を出現順に描画し、その後に飛行中の敵の弾を描画する。死亡枠はEnemy::Drawが描画を省略する。
  m_enemyController.DrawEnemies(m_graphicsSystem,viewMatrix,projectionMatrix);
  // 敵と弾の後に、表示中ならCollisionの線を重ねる。
  m_collisionDebugView.Draw(m_graphicsSystem,m_stage.GetCollisionWorld(),m_player.GetCamera(),viewMatrix,projectionMatrix);
}

// 3Dの上に、画像のHUDを描画する。
void GameSession::DrawHud()
{
  // 3Dの上に画像のHUDを重ねるため、深度比較なし・アルファ合成の画面UI用Passへ切り替える。
  m_graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);

  // 現在の値をHUDへ渡して描画する。弾薬とリロードの値は、射撃を担当するShotControllerの武器から読み取る。
  const WeaponSystem& weapon = m_player.GetShotController().GetWeaponSystem();
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
  m_gameHud.Draw(m_graphicsSystem,hud_state);
}
