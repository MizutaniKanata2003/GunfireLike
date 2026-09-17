#pragma once

#include <memory>

// BGMとSEのロード、再生、更新、終了処理を管理する。
class AudioSystem final
{
public:
	// AudioSystemを生成する。
	AudioSystem();

	// AudioSystemの終了処理を行う。
	~AudioSystem();

	AudioSystem( const AudioSystem& ) = delete;
	AudioSystem& operator=( const AudioSystem& ) = delete;

	AudioSystem( AudioSystem&& ) = delete;
	AudioSystem& operator=( AudioSystem&& ) = delete;

	// AudioEngineと全BGM・SEを読み込み、再生可能な状態にする。
	bool Initialize();

	// AudioEngineの状態を更新し、音声デバイスの状態変化へ対応する。
	void Update();

	// BGM、SE、AudioEngineを安全な順序で終了する。
	void Uninit();

	// Title用BGMをループ再生する。
	void PlayTitleBgm();

	// Shop用BGMをループ再生する。
	void PlayShopBgm();

	// Game用BGMをループ再生する。
	void PlayGameBgm();

	// Result用BGMをループ再生する。
	void PlayResultBgm();

	// 現在再生しているBGMを即時停止する。
	void StopBgm();

	// 銃の発射SEを再生する。
	void PlayGunSe();

	// ワープSEを再生する。
	void PlayWarpSe();

	// 低HP警告SEを再生する。
	void PlayLowHpSe();

	// 購入SEを再生する。
	void PlayPurchaseSe();

	// ダメージSEを再生する。
	void PlayDamageSe();

	// 特殊攻撃SEを再生する。
	void PlaySpecialSe();

	// 敵撃破SEを再生する。
	void PlayEnemyDefeatSe();

private:
	class Impl;

	// AudioEngineとSoundEffect群を隠蔽する実装クラス。
	std::unique_ptr<Impl> m_Impl{};
};