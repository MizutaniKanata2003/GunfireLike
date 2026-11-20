#pragma once

// 通貨の所持と、通貨の仮の数値の表（初期値・上限・敵の撃破報酬）を定義する。
// 通貨の増減だけを担当する。報酬を与える条件・購入の判断・表示は担当しない。数値は企画書で未確定の仮値で、プレイ確認後に調整する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで、所持額を上限内に収める。

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyWaveTable.h" // 敵の種類（ENEMY_KIND）ごとの撃破報酬を表で引くために使用する。

// 通貨の仮の数値をまとめる。
namespace ECONOMY_TABLE
{
	//========= 所持額=========
	// 開始時の所持額。
	inline constexpr int ns_INITIAL_BALANCE = 0;
	// 所持額の上限。HUDに収まる桁数（5桁）にする。超えた分は切り捨てる。
	inline constexpr int ns_MAX_BALANCE = 99999;

	//========= 撃破報酬=========
	// 敵を1体倒した時に得る通貨。ENEMY_KINDの並び（接近型・射撃型・範囲攻撃型）と同じ順。仮値。
	inline constexpr int ns_KILL_REWARDS[]{10,15,20};
	// 撃破報酬の表の要素数。
	inline constexpr int ns_KILL_REWARD_COUNT = static_cast<int>(sizeof(ns_KILL_REWARDS) / sizeof(ns_KILL_REWARDS[0]));

	//========= 撃破報酬の取得=========
	// 敵の種類に対応する撃破報酬を返す。
	// kind: 倒した敵の種類。
	// 戻り値: 表の報酬。表にない種類は0。
	[[nodiscard]] inline constexpr int GetKillReward(ENEMY_KIND kind)
	{
		// 種類の値を表の添字にし、範囲外なら、配列の外を読まないよう0を返す。
		const int reward_index = static_cast<int>(kind);
		if(reward_index < 0 || reward_index >= ns_KILL_REWARD_COUNT)return 0;
		return ns_KILL_REWARDS[reward_index];
	}
}

// 通貨の所持額を保持し、増やす・使う。
class Wallet final
{
public:
	//========= 初期化関数=========
	// 所持額を指定した値へ戻す。上限を超える値は、上限へ収める。
	// initialBalance: 所持額の初期値。0以上であること。負の値は0へ収める。
	void Initialize(int initialBalance)
	{
		m_balance = std::clamp(initialBalance,0,ECONOMY_TABLE::ns_MAX_BALANCE);
	}

	//========= 増減関数=========
	// 所持額を増やす。上限を超える分は切り捨てる。
	// amount: 増やす額。正の値。0以下の場合は何もしない。
	void Add(int amount)
	{
		// 増やす額が正でない場合は、所持額を変えない。
		if(amount <= 0)return;

		// 加算でint型の範囲を超えないよう、上限との差の内に収めてから足す。
		m_balance += std::min(amount,ECONOMY_TABLE::ns_MAX_BALANCE - m_balance);
	}
	// 所持額を使う。足りない場合は、何も変えない。
	// amount: 使う額。0以上。負の値は使えない。
	// 戻り値: 使えたらtrue。額が負の場合や、所持額が足りない場合はfalse。
	[[nodiscard]] bool TrySpend(int amount)
	{
		// 負の額や、所持額を超える額は、使わない。
		if(amount < 0 || amount > m_balance)return false;

		m_balance -= amount;
		return true;
	}

	//========= Getter関数=========
	// 現在の所持額を返す。0以上で上限以下。
	[[nodiscard]] int GetBalance() const { return m_balance; }

private:
	//========= 所持状態=========
	// 現在の所持額。0以上で上限以下。
	int m_balance{};
};