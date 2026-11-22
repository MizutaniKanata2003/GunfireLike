#pragma once

// 部屋とウェーブの進行を管理する。部屋の数とウェーブの数は、敵の編成表（ENEMY_WAVE_TABLE）に従う。敵の生成と出口の描画・衝突は呼出側が行う。

// 部屋の進行状態。
enum class ROOM_PHASE
{
	e_NEEDS_SPAWN, // 現在のウェーブの生成待ち。
	e_FIGHTING,    // 生成済みの敵と戦闘中。
	e_EXIT_OPEN,   // 部屋を全滅させ、出口の通過待ち。
	e_COMPLETE     // 最終部屋の出口を通過済み。
};

// 敵の生成完了、全滅、出口通過を別々に受け取る進行管理。
class RoomController final
{
public:
	//========= 進行関数=========
	// 初期部屋の第1ウェーブを生成待ちに戻す。
	void Initialize();
	// 生成待ちの場合だけ戦闘中に移る。全敵の登録成功後に呼ぶ。
	void NotifyWaveSpawned();
	// 戦闘中に敵が全滅した場合、次のウェーブを要求するか出口を開く。
	// hasLivingEnemies: 現在のウェーブに生存中の敵がいるか。
	void Update(bool hasLivingEnemies);
	// 出口開放中の場合だけ、次の部屋へ進む。最終部屋では攻略完了にする。
	// 戻り値: 出口通過を受理したらtrue。
	[[nodiscard]] bool TryAdvanceRoom();

	//========= Getter関数=========
	// 現在の部屋番号を1始まりで返す。攻略完了後は最終部屋番号を保持する。
	[[nodiscard]] int GetRoomNumber() const;
	// 現在のウェーブ番号を1始まりで返す。出口開放後は最終ウェーブ番号を保持する。
	[[nodiscard]] int GetWaveNumber() const;
	// 現在の進行状態を返す。
	[[nodiscard]] ROOM_PHASE GetPhase() const;

private:
	//========= 進行状態=========
	// 1始まりの部屋番号。初期状態は第1部屋。
	int m_roomNumber{1};
	// 1始まりのウェーブ番号。部屋に入ると第1ウェーブに戻す。
	int m_waveNumber{1};
	// 初期状態では、敵を登録するまで全滅判定を行わない。
	ROOM_PHASE m_phase{ROOM_PHASE::e_NEEDS_SPAWN};
};