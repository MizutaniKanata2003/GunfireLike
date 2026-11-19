#pragma once

// DirectXTKのAudioEngineと音声データを所有し、BGM切替・SE再生・Engine更新・終了を提供する。
// 実装はImplに隠蔽する。COM初期化と、どの場面で再生するかの判断は呼出側が担当する。

//========= C++標準ライブラリ インクルード=========
#include <memory>

// 4種類のBGMと7種類のSEを読み込み、現在のBGM再生Instanceを一つ保持する。
class AudioSystem final
{
public:
	//========= 生成・破棄関数=========
	// 未初期化状態で構築する。Engineと音声データはInitializeで生成する。
	AudioSystem();
	// Uninitを呼び、保持している音声Resourceを既存順序で終了する。
	~AudioSystem();

	//========= コピー・ムーブ禁止=========
	// Resource所有と再生状態の複製を避けるため、別実体からのコピー構築を禁止する。
	AudioSystem(const AudioSystem&) = delete;
	// Resource所有と再生状態の複製を避けるため、別実体からのコピー代入を禁止する。
	AudioSystem& operator=(const AudioSystem&) = delete;
	// 音声実装の所有者を別実体へ移さないため、ムーブ構築を禁止する。
	AudioSystem(AudioSystem&&) = delete;
	// 音声実装の所有者を別実体へ移さないため、ムーブ代入を禁止する。
	AudioSystem& operator=(AudioSystem&&) = delete;

	//========= 初期化・終了関数=========
	// 既存Resourceを終了し、Engine、全BGM、全SEを順に生成する。呼出スレッドのCOMは初期化済みとする。
	// 戻り値: 全読込成功ならtrue。std::exception捕捉時は部分解放してfalse。その他の例外は捕捉しない。
	// 読込進捗と失敗内容は既存のデバッグ出力へ記録する。
	bool Initialize();
	// Engineを更新し、falseが返った場合はResetを試行する。未初期化なら何もしない。
	// std::exceptionはデバッグ出力へ記録する。Reset結果の追加判定やBGM再開処理は行わない。
	void Update();
	// BGM Instanceを停止・解放し、Engine、SE、BGMデータ、Implの順に解放する。
	// 未初期化・部分初期化・終了済みでも呼出可能。終了順は既存実装を維持する。
	void Uninit();

	//========= BGM再生関数=========
	// Title用BGMを固定音量でループ再生する。対象未読込なら何もしない。
	// 以下のBGM開始関数は現在BGMを止めてInstanceを作り直す。同一曲でも再開始し、例外は捕捉しない。
	void PlayTitleBgm();
	// Shop用BGMへ切り替えてループ再生する。対象未読込なら何もしない。
	void PlayShopBgm();
	// Game用BGMへ切り替えてループ再生する。対象未読込なら何もしない。
	void PlayGameBgm();
	// Result用BGMへ切り替えてループ再生する。対象未読込なら何もしない。
	void PlayResultBgm();
	// 現在BGMを即時停止し、再生Instanceを解放する。Instanceがなければ何もしない。
	void StopBgm();

	//========= SE再生関数=========
	// 発射SEを単発再生する。以下のSE関数も、対象未読込なら何もせず、例外は捕捉しない。
	void PlayGunSe();
	// ワープSEを単発再生する。
	void PlayWarpSe();
	// 低HP警告SEを単発再生する。HPの判定や繰返し制御は呼出側が担当する。
	void PlayLowHpSe();
	// 購入SEを単発再生する。
	void PlayPurchaseSe();
	// ダメージSEを単発再生する。
	void PlayDamageSe();
	// 特殊攻撃SEを単発再生する。
	void PlaySpecialSe();
	// 敵撃破SEを単発再生する。
	void PlayEnemyDefeatSe();

private:
	//========= 前方宣言=========
	// Engine、音声データ、BGM再生Instanceの所有を隠蔽する実装クラス。
	class Impl;

	//========= 実装所有=========
	// 音声Resourceをまとめた実装を所有する。Initializeで生成し、Uninitで解放する。
	std::unique_ptr<Impl> m_impl{};
};