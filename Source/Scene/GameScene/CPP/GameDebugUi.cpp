#include "../H/GameDebugUi.h"

// デバッグ情報の文字列化とImGuiでの表示を実装する。ゲームの状態は変更しない。

//========= 外部ライブラリ インクルード=========
#include "imgui.h"

// 動作の状態と判定結果を、表示用の名前へ変換する補助関数をまとめる。
namespace
{
	//========= 補助関数=========
	// 動作の状態を、デバッグUIに表示する名前へ変換する。
	// actionState: 変換する動作の状態。
	// 戻り値: 状態の名前。静的な文字列定数。未定義の値ではUNKNOWN。
	const char* GetActionStateName(PLAYER_ACTION_STATE actionState)
	{
		// 状態ごとに名前を選ぶ。
		switch(actionState){
			// 通常。
		case PLAYER_ACTION_STATE::e_NORMAL:
		return "NORMAL";

		// ダッシュ。
		case PLAYER_ACTION_STATE::e_DASH:
		return "DASH";

		// しゃがみ。
		case PLAYER_ACTION_STATE::e_CROUCH:
		return "CROUCH";

		// スライディング。
		case PLAYER_ACTION_STATE::e_SLIDE:
		return "SLIDE";

		// 回避。
		case PLAYER_ACTION_STATE::e_DODGE:
		return "DODGE";

		// 蹴り。
		case PLAYER_ACTION_STATE::e_KICK:
		return "KICK";

		// 殴打。
		case PLAYER_ACTION_STATE::e_PUNCH:
		return "PUNCH";
		}

		// 未定義の値は、不明として扱う。
		return "UNKNOWN";
	}

	// 回避の判定結果を、デバッグUIに表示する名前へ変換する。
	// judgeResult: 変換する判定結果。
	// 戻り値: 結果の名前。静的な文字列定数。未定義の値ではUNKNOWN。
	const char* GetJudgeResultName(DODGE_JUDGE_RESULT judgeResult)
	{
		// 結果ごとに名前を選ぶ。
		switch(judgeResult){
			// まだ判定がない。
		case DODGE_JUDGE_RESULT::e_NONE:
		return "-";

		// 命中した。
		case DODGE_JUDGE_RESULT::e_HIT:
		return "HIT (player damaged)";

		// 無敵で避けた。
		case DODGE_JUDGE_RESULT::e_DODGED:
		return "DODGED";

		// ジャスト回避。
		case DODGE_JUDGE_RESULT::e_JUST_DODGED:
		return "JUST DODGE!";
		}

		// 未定義の値は、不明として扱う。
		return "UNKNOWN";
	}
}

//========= 描画関数=========
// 操作案内・プレイヤー・敵・判定結果を、1つのウィンドウに表示する。
void GameDebugUi::Draw(const GAME_DEBUG_INFO& info)
{
	ImGui::Begin("Minimal Game Scene");

	// 操作の案内を表示する。
	ImGui::TextUnformatted("WASD: Move / Mouse: Look / Left click: Shoot / Right click (hold): Aim / R: Reload / Q: Heal");
	ImGui::TextUnformatted("Shift (hold): Dash / C (hold): Crouch / C while dashing: Slide / Space + WASD: Dodge");
	ImGui::TextUnformatted("F: Kick (flinching enemy only) / E: Punch (cancels reload, knocks down)");
	ImGui::TextUnformatted("F1: Capture / F2: Title / F3: Collision / F4: Respawn enemy / F5: Damage / F6: Heal full / F7: Incoming attack / F8: +Heal item");

	// プレイヤーの位置・HP・動作・無敵・怯み・回復アイテム数を表示する。
	ImGui::Separator();
	ImGui::Text("Position: %.2f, %.2f, %.2f",info.playerPosition.x,info.playerPosition.y,info.playerPosition.z);
	ImGui::Text("Player HP: %.0f / %.0f   Heal items: %d",info.playerHP,info.playerMaxHP,info.healItemCount);
	ImGui::Text("Action: %s%s%s",GetActionStateName(info.actionState),info.isInvincible ? " (INVINCIBLE)" : "",info.isPlayerFlinching ? " (FLINCH)" : "");
	ImGui::Text("Capture: %s  Collision debug: %s  Hit stop: %s  Slow motion: %s",info.isCaptureEnabled ? "ON" : "OFF",
							info.isCollisionDebugVisible ? "ON" : "OFF",info.isHitStopActive ? "ON" : "OFF",info.isSlowMotionActive ? "ON" : "OFF");

	// 確認用の攻撃を予告中の場合だけ、当たるまでの残り時間を表示する。判定結果は常に表示する。
	if(info.hasIncomingAttack){
		ImGui::Text("Incoming attack in: %.2f s",info.secondsUntilHit);
	}
	ImGui::Text("Last attack result: %s",GetJudgeResultName(info.lastJudgeResult));

	// 敵の状態を表示する。
	ImGui::Separator();
	// 生きている場合は、HPと怯み・転倒の有無を表示する。
	if(info.isEnemyAlive){
		ImGui::Text("Enemy HP: %.0f / %.0f%s%s",info.enemyHP,info.enemyMaxHP,info.isEnemyFlinching ? " (Flinch)" : "",
								info.isEnemyKnockedDown ? " (Knocked down)" : "");
	}
	// 死亡している場合は、F4で再配置できることを表示する。
	else{
		ImGui::TextUnformatted("Enemy: defeated (F4: respawn)");
	}

	// 直近の命中の表示時間が残っている間だけ、命中した旨を表示する。
	if(info.isHitMarkerActive){
		ImGui::TextUnformatted("HIT!");
	}

	// 可視化の表示中でRayが命中している場合だけ、命中したColliderのレイヤーのbitと距離を表示する。
	if(info.isCollisionDebugVisible && info.hasRayHit){
		ImGui::Text("Ray hit: layer bit=%u distance=%.2f",info.rayHitLayer,info.rayHitDistance);
	}

	ImGui::End();
}