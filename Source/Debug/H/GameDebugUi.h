#pragma once

// ゲーム画面のデバッグ情報を、ImGuiで読み取り専用の一覧として表示する。操作案内・プレイヤーと敵の状態・確認用の判定結果を表示する。
// ゲームの状態は変更しない。表示する値はGAME_DEBUG_INFOとして受け取り、このクラスは状態を持たない。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/H/PlayerController.h"
#include "Scene/Common/H/RoomController.h"
#include "Scene/Common/H/JustDodgeJudge.h"

// デバッグ表示に必要な、ゲームの状態の値。
struct GAME_DEBUG_INFO
{
	//========= プレイヤー=========
	// プレイヤーの位置。
	XMFLOAT3 playerPosition{};
	// プレイヤーの現在のHP。
	float playerHP{};
	// プレイヤーの最大HP。
	float playerMaxHP{};
	// プレイヤーの動作の状態。
	PLAYER_ACTION_STATE actionState{PLAYER_ACTION_STATE::e_NORMAL};
	// プレイヤーが無敵か。
	bool isInvincible{};
	// プレイヤーが怯み中か。
	bool isPlayerFlinching{};
	// 所持している回復アイテムの数。
	int healItemCount{};

	//========= 敵=========
	// 敵が生きているか。
	bool isEnemyAlive{};
	// 敵の現在のHP。
	float enemyHP{};
	// 敵の最大HP。
	float enemyMaxHP{};
	// 敵が怯み中か。
	bool isEnemyFlinching{};
	// 敵が転倒中か。
	bool isEnemyKnockedDown{};
	// 直近に弾が敵へ命中して、表示時間が残っているか。
	bool isHitMarkerActive{};

	//========= 状態=========
	// Captureが有効か。
	bool isCaptureEnabled{};
	// ヒットストップ中か。
	bool isHitStopActive{};
	// スロー中か。
	bool isSlowMotionActive{};

	//========= 部屋の進行=========
	// 現在の部屋番号。1始まり。
	int roomNumber{};
	// 現在のウェーブ番号。1始まり。
	int waveNumber{};
	// 現在の進行状態。
	ROOM_PHASE roomPhase{ROOM_PHASE::e_NEEDS_SPAWN};
	// 現在のウェーブの生成に失敗して、再試行を止めているか。
	bool hasWaveSpawnFailed{};

	//========= デバッグ操作=========
	// 一撃で敵を倒す設定が有効か。有効の間、プレイヤーの射撃・蹴り・殴打が、敵の残りHPと同じダメージを与える。
	bool isOneHitKillEnabled{};

	//========= 確認用の攻撃の判定=========
	// 確認用の攻撃を予告中か。
	bool hasIncomingAttack{};
	// 確認用の攻撃が当たるまでの残り時間。単位は秒。予告中だけ有効。
	float secondsUntilHit{};
	// 直近の判定結果。
	DODGE_JUDGE_RESULT lastJudgeResult{DODGE_JUDGE_RESULT::e_NONE};

	//========= Collision可視化=========
	// Collision可視化を表示中か。
	bool isCollisionDebugVisible{};
	// 視線のRayが命中しているか。
	bool hasRayHit{};
	// Rayが命中したColliderのレイヤーのbit。hasRayHitがtrueの間だけ有効。
	unsigned int rayHitLayer{};
	// Rayの命中までの距離。hasRayHitがtrueの間だけ有効。
	float rayHitDistance{};
};

// デバッグ情報をImGuiで表示する静的関数。
class GameDebugUi final
{
public:
	//========= 描画関数=========
	// デバッグ用のウィンドウを開き、情報を一覧で表示する。ImGuiのフレーム開始後、描画データ確定前に呼ぶ。
	// info: 表示する値。参照は保持しない。
	static void Draw(const GAME_DEBUG_INFO& info);

private:
	//========= 生成禁止関数=========
	// 状態を持たず、静的関数だけを使用するため構築を禁止する。
	GameDebugUi() = delete;
};