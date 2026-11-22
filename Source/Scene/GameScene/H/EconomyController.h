#pragma once

// 通貨の所持・部屋のクリア後の報酬選択・敵の撃破報酬を、まとめて担当する。
// 通貨（Wallet）と報酬選択（RewardController）と、敵の枠ごとの生死の記録を所有し、倒された敵の撃破報酬を通貨へ加え、上限に達した報酬を通貨へ置き換える。
// 時間の停止・Captureの切替・ショップを開く・報酬の効果を使う側へ渡す処理は担当せず、呼出側（GameSession）が、更新の結果を見て行う。

//========= C++標準ライブラリ インクルード=========
#include <array> // std::arrayで、敵の枠ごとの、前回の確認での生死を固定長で保持する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Enemy/H/EnemyWaveTable.h" // 敵の枠の数（ENEMY_WAVE_TABLE::ns_SLOT_COUNT）を使用する。
#include "Scene/Common/Player/H/Wallet.h"
#include "Scene/Common/Reward/H/RewardController.h"

//========= 前方宣言=========
class EnemyManager;

// 通貨・報酬選択・撃破報酬を管理するクラス。
class EconomyController final
{
public:
	//========= 初期化関数=========
	// 通貨を開始時の所持額へ戻し、報酬の取得数を0へ戻して報酬選択を解除し、敵の生死の記録を空にする。新しいプレイの開始の時に呼ぶ。
	void Initialize();

	//========= 更新関数=========
	// 部屋の進行状態とカーソルを渡して、報酬選択へ入る・抜ける判定を行う。
	// カードが選ばれた更新では、上限に達した報酬なら、強化の代わりの通貨を加え、選ばれた報酬をログへ記録する。
	// currentPhase: 現在の部屋の進行状態。
	// cursorCanvasPosition: カーソルの位置。基準Canvas上の座標。
	// isClickTriggered: クリックが、この更新で押されたか。
	// keyChoiceIndex: この更新で押された、カードの番号のキーに対応する添字。押されていなければ-1。
	// 戻り値: この更新で起きた出来事。入る・抜けるの後の処理（Captureの切替・ショップを開く）は、呼出側が行う。
	[[nodiscard]] REWARD_UPDATE_RESULT UpdateReward(ROOM_PHASE currentPhase,const XMFLOAT2& cursorCanvasPosition,bool isClickTriggered,int keyChoiceIndex);
	// 前回の確認から、新しく倒された敵（生きていた枠が、死亡になった）を調べ、種類に応じた撃破報酬を、通貨へ加える。
	// 射撃・近接・デバッグのどの手段で倒された場合も、同じ扱いにする。
	// enemyManager: 敵の枠の取得元。参照は保持しない。
	void UpdateKillRewards(EnemyManager& enemyManager);
	// 敵の枠ごとの生死の記録を、現在の状態へ揃える。報酬は与えない。
	// ウェーブの生成や置換の直後に呼び、置き換えられた敵を、倒された敵として扱わないようにする。
	// enemyManager: 敵の枠の取得元。参照は保持しない。
	void SyncEnemyAliveFlags(EnemyManager& enemyManager);

	//========= 状態の操作関数=========
	// 報酬選択を解除し、部屋の進行状態の記録を戻す。報酬の取得数・通貨は変えない。ステージを最初から再開する時に呼ぶ。
	void ResetRewardSelection() { m_reward.Reset(); }

	//========= Getter関数=========
	// 通貨を返す。増減は、購入・売却・強化の反映から行う。戻り値の参照は、このEconomyControllerの寿命を超えて保持しない。
	[[nodiscard]] Wallet& GetWallet() { return m_wallet; }
	// 通貨を、読み取り専用で返す。
	[[nodiscard]] const Wallet& GetWallet() const { return m_wallet; }
	// 報酬選択を返す。入力の保護や、取得数・効果の倍率の取得に使う。戻り値の参照は、このEconomyControllerの寿命を超えて保持しない。
	[[nodiscard]] RewardController& GetReward() { return m_reward; }
	// 報酬選択を、読み取り専用で返す。
	[[nodiscard]] const RewardController& GetReward() const { return m_reward; }

private:
	//========= 所有状態=========
	// 通貨の所持額。敵を倒した時の報酬と、報酬の置換と、武器の売却で増え、購入・強化で減る。
	Wallet m_wallet{};
	// 部屋のクリア後の報酬選択の状態と、特殊強化の取得数。
	RewardController m_reward{};

	//========= 撃破の検出=========
	// 敵の枠ごとの、前回の確認での生死。生きていた枠が、今回は死亡になっていれば、倒されたとみなす。生成や初期化の後は、現在の状態へ揃える。
	std::array<bool,ENEMY_WAVE_TABLE::ns_SLOT_COUNT> m_wasEnemyAlive{};
};