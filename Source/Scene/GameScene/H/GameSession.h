#pragma once

// 1回のプレイの流れを担当する。初期化・更新・描画・終了の中で、ステージ・プレイヤー側一式・敵・時間（ヒットストップとスロー）・通貨と報酬・ショップ・武器の所持と強化と切替・デバッグ・描画の順序と仲介を行う。
// 移動・戦闘・射撃などの個別の規則は、各クラスが持つ。Scene遷移は担当せず、タイトルへ戻る要求を、更新の結果として呼出側へ返す。

//========= Debug インクルード=========
#include "Debug/H/GameDebugController.h"

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Combat/H/TimeScaleController.h"
#include "Scene/Common/Enemy/H/EnemyController.h"
#include "Scene/Common/Player/H/PlayerSuite.h"
#include "Scene/Common/Shop/H/ShopController.h"
#include "Scene/Common/Stage/H/StageController.h"
#include "Scene/Common/Weapon/H/WeaponInventory.h"
#include "Scene/GameScene/H/EconomyController.h"
#include "Scene/GameScene/H/GameRenderer.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;

// 更新の結果。呼出側が、次の動作を選ぶために使う。
enum class GAME_SESSION_RESULT
{
	e_CONTINUE,       // プレイを続ける。
	e_RETURN_TO_TITLE // タイトルへ戻る要求があった。呼出側がScene遷移を予約する。
};

// ステージ・プレイヤー側一式・敵・時間・通貨と報酬・ショップ・武器の所持と強化と切替・デバッグ・描画を仲介して、1回のプレイを進めるクラス。
class GameSession final
{
public:
	//========= 生成関数=========
	// 共有Systemを所有せず、使用する参照だけを登録する。Resource生成はInitで行う。
	// inputSystem: キー入力の取得先とCapture設定先。このGameSessionより長く生存すること。
	// graphicsSystem: Resource生成・描画先サイズ取得・描画の基盤。Finalizeまで有効であること。
	// audioSystem: Game用BGMの再生先。このGameSessionの使用期間中は有効であること。
	GameSession(InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

	//========= ライフサイクル関数=========
	// プレイヤー側一式・ステージ・判定の状態・通貨と報酬・ショップ・武器の所持と強化・敵の弾と範囲の予告・デバッグ設定を初期状態へ戻し、カメラをプレイヤー位置へ合わせる。
	void Initialize();
	// ステージ・描画（HUDとCollision可視化）を初期化し、敵の弾、範囲の予告、第1部屋の敵を生成して、成功後にFPS操作とGame用BGMを有効にする。
	// 戻り値: 各初期化・Object生成・敵の配置が成功したらtrue。falseを検出した場合はFinalize後にfalseを返し、CaptureとBGMは変更しない。
	[[nodiscard]] bool Init();
	// F2・F1・F4の入力、デバッグ操作を処理し、時間・部屋・報酬選択・ショップ・武器の切替・プレイヤー・戦闘・敵・撃破の報酬の順に更新する。
	// deltaTime: 実時間の経過時間。単位は秒。各更新へは、ヒットストップ中は0、スロー中はプレイヤー以外へ縮めた値を、それぞれ渡す。報酬選択中とショップが開いている間は、時間を進めない。
	// Capture解除中と、報酬選択中・ショップが開いている間は、視点・移動・戦闘・敵の行動の更新を行わない。F2が押された場合は後続の更新を行わない。
	// 戻り値: F2が押されたらe_RETURN_TO_TITLE。それ以外はe_CONTINUE。
	[[nodiscard]] GAME_SESSION_RESULT Update(float deltaTime);
	// 描画先サイズに合わせて行列を作り、3D・画像のHUD・読み取り専用のデバッグUIを、この順に描画する。
	// 幅または高さが0なら何もしない。ImGuiフレーム開始後、描画データ確定前に呼ぶ。
	void Draw();
	// デバッグ・プレイヤー側・敵を先に終了し、その後にステージ・描画を解放する。繰り返し呼出可能。CaptureやBGMは変更しない。
	void Finalize();

private:
	//========= 初期化の補助関数=========
	// ステージ・描画（HUDとCollision可視化）を初期化する。失敗の後始末は、呼出側のInitがFinalizeで行う。
	// 戻り値: 全て成功したらtrue。失敗した場合は、失敗した対象をログへ記録してfalse。
	[[nodiscard]] bool InitRenderResources();
	// 敵の更新へ、使用する参照と通知先を渡す。失敗の後始末は、呼出側のInitがFinalizeで行う。
	// 戻り値: 登録に成功したらtrue。
	[[nodiscard]] bool InitEnemyController();
	// プレイヤー側一式とデバッグ操作へ、使用する参照と通知先を渡す。失敗の後始末は、呼出側のInitがFinalizeで行う。
	// 戻り値: 両方の登録に成功したらtrue。
	[[nodiscard]] bool InitPlayerAndDebug();

	//========= 更新の補助関数=========
	// F2の確認、F1のCapture切替、F4の最初の部屋からの再開を処理する。再開では、報酬選択とショップも解除する。
	// 戻り値: F2が押された場合はtrue。呼出側は、後続の更新を行わず戻る。
	[[nodiscard]] bool UpdateSessionInput();
	// 確認用の攻撃の判定と、敵の怯み・転倒・部屋の進行・出口の開閉を、Captureに関係なく更新する。
	// worldDeltaTime: スローを反映した経過時間。単位は秒。ヒットストップ中と、報酬選択中・ショップが開いている間は0。
	void UpdateWorldAndRoom(float worldDeltaTime);
	// 回復アイテムの間隔・Collision可視化の表示時間・命中表示・反動のずれを、Captureに関係なく更新する。
	// deltaTime: 実時間の経過時間。単位は秒。表示用と反動の時間に使う。報酬選択中とショップが開いている間は0。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。回復アイテムの間隔に使う。
	void UpdateTimers(float deltaTime,float gameplayDeltaTime);
	// 部屋のクリアで報酬選択へ入る判定と、カードが選ばれた時に抜ける判定を行い、入る時はCaptureを解除する。カードが選ばれたら、続けてショップを開く。取得した報酬の効果のうち、常に効くもの（制圧射撃・殴打追撃）を、使う側へ渡す。
	void UpdateReward();
	// ショップが開いている間、カーソルとキーから購入・売却・強化・閉じる操作を判定する。購入・売却・強化が求められたら、反映する。閉じる時は、Captureを再開する。
	void UpdateShop();
	// 数字キー（1から順に、表の武器の並び）が押された瞬間に、所持している武器へ持ち替える。未所持の武器や、装備中の武器のキーは、何もしない。Capture中で、報酬選択・ショップが閉じている間だけ呼ぶ。
	void UpdateWeaponSwitch();
	// ショップでの武器の購入を反映する。通貨を支払い、武器を所持に加え、その武器へ持ち替える。どちらかに失敗した場合は、何も変えない。
	void ApplyShopPurchase();
	// ショップでの武器の売却を反映する。武器を所持から外し、売却で得る通貨（強化に使った費用の回収を含む）を通貨へ加える。手放した武器を装備していた場合は、所持している別の武器を装備する。外せなかった場合は、何も変えない。
	void ApplyShopSale();
	// ショップでの弾薬の購入を反映する。装備中の武器なら予備弾薬を、装備していない武器なら保存した予備弾薬を、上限内で増やし、通貨を支払う。1発も増やせなかった場合は、何も変えない。
	void ApplyShopAmmoPurchase();
	// ショップでの強化の購入を反映する。装備中の武器の、選ばれた項目の段階を1つ上げ、通貨を支払い、使用中の武器の性能値を強化後の値へ差し替える。装備中の武器が判別できない場合や、上げられなかった場合は、何も変えない。
	void ApplyShopUpgrade();
	// 装備中の武器の弾薬を保存してから、所持している別の武器へ持ち替える。装備中の武器と同じ種類が指定された場合は、何もしない。
	// weaponIndex: 持ち替える先の武器の種類の値（表の添字）。表の範囲内で、所持していること。
	void SwitchToWeapon(int weaponIndex);
	// 所持している武器を、強化を反映した性能値で装備する。保存した弾薬があれば戻し、なければ、強化後の装弾数で満タンになる。
	// weaponIndex: 装備する武器の種類の値（表の添字）。表の範囲内であること。
	void EquipOwnedWeapon(int weaponIndex);
	// ジャスト回避が成立した時の効果を反映する。スローを開始し、回避反撃を取得していれば、一定時間、射撃のダメージを増やす。
	void StartJustDodgeEffects();
	// 視点・プレイヤーの動作・出口の通過・殴打でのリロードのキャンセル・視野角を更新する。Capture中だけ呼ぶ。
	// deltaTime: 実時間の経過時間。単位は秒。視野角の補間に使う。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。プレイヤーの動作に使う。
	void UpdatePlayer(float deltaTime,float gameplayDeltaTime);
	// プレイヤーの戦闘・射撃・敵の行動と弾を更新し、Collision可視化の視線のRayを更新する。Capture中だけ呼ぶ。
	// gameplayDeltaTime: ヒットストップを反映した経過時間。単位は秒。プレイヤーの戦闘と射撃に使う。
	// worldDeltaTime: スローを反映した経過時間。単位は秒。敵の行動と弾に使う。
	void UpdateCombat(float gameplayDeltaTime,float worldDeltaTime);
	// 確認用の攻撃が当たる瞬間を判定し、結果に応じてプレイヤーへのダメージとジャスト回避の効果を反映する。
	// worldDeltaTime: スローを反映した経過時間。単位は秒。ヒットストップ中は0。
	void UpdateDodgeJudge(float worldDeltaTime);
	// 敵の攻撃イベントを受け取り、命中ならプレイヤーへの被弾の反応を、ジャスト回避ならその効果を反映し、求められた場合は直近の判定結果を記録する。
	// attackEvent: EnemyControllerが通知した攻撃イベント。敵の更新の途中で呼ばれる。
	void ApplyEnemyAttackEvent(const ENEMY_ATTACK_EVENT& attackEvent);
	// 現在の部屋とウェーブの敵をEnemyControllerへ生成させ、成功した場合は、スライディングの転倒記録を初期化して、進行管理へ生成完了を通知する。
	// 生成の成否に関係なく、敵の枠ごとの生死の記録を、生成後の状態へ揃える。置き換えられた敵を、倒された敵として扱わないため。
	// 戻り値: 登録に成功したらtrue。失敗した場合はEnemyManagerが空にし、進行管理には通知しない。
	[[nodiscard]] bool TrySpawnCurrentWave();
	// デバッグ用に、現在のウェーブの生きている敵を全て倒し、敵の予告・弾・範囲の予告を消す。次の更新の全滅判定で、次のウェーブまたは出口の開放へ進む。
	void SkipCurrentWave();
	// 出口の通過を判定し、次の部屋へ進んだ場合だけ、プレイヤーを開始位置へ戻す。
	void UpdateExitPass();

	//========= 描画の補助関数=========
	// 現在の値（HP・弾薬・通貨・報酬選択・ショップの状態など）を集めて、画像のHUDの描画をGameRendererへ求める。
	void DrawHud();

	//========= System参照=========
	// 操作入力とCapture設定に使用する非所有参照。
	InputSystem& m_inputSystem;
	// Resource初期化と描画に使用する非所有参照。
	GraphicsSystem& m_graphicsSystem;
	// Init成功後のGame用BGM再生に使用する非所有参照。
	AudioSystem& m_audioSystem;

	//========= 所有状態=========
	// プレイヤー・HP・回復アイテム・カメラ・近接攻撃と接触と回復・射撃・視野角を所有する。ヒットストップは、TimeScaleControllerへ求める。
	PlayerSuite m_player{};
	// 現在のウェーブの敵・敵の弾・範囲の予告・敵ごとの攻撃予告を所有し、更新と描画を行う。Finalizeで、登録先のCollisionWorldと共有Rendererより先に解除する。
	EnemyController m_enemyController{};
	// ヒットストップとスローの残り時間を持ち、更新ごとに、プレイヤー側と、敵・判定側の経過時間を求める。
	TimeScaleController m_timeScale{};
	// デバッグ操作・確認用の攻撃の判定・デバッグUIの情報の組立を担当する。ゲームの状態は、参照を通じてだけ変更する。
	GameDebugController m_debugController{};
	// 共有Renderer・床と壁・出口・CollisionWorld・部屋の進行を所有する。敵の生成と全滅判定は、GameSessionが仲介する。
	StageController m_stage{};
	// 通貨の所持額・部屋のクリア後の報酬選択と特殊強化の取得数・敵の撃破報酬の検出を担当する。選択中は、時間とプレイヤーの操作を止める。
	EconomyController m_economy{};
	// 所持している武器と、武器ごとの強化・切替で保存した弾薬の記録。ショップの購入・売却・強化と、持ち替えで変わる。
	WeaponInventory m_inventory{};
	// 武器と弾薬の購入、武器の売却、武器の強化の画面の状態。報酬選択の後に開き、開いている間は、時間とプレイヤーの操作を止める。
	ShopController m_shop{};
	// 3Dのワールド・画像のHUD・Collision可視化の描画を担当する。Collision可視化は、プレイヤー・デバッグが借用する。
	GameRenderer m_renderer{};
};