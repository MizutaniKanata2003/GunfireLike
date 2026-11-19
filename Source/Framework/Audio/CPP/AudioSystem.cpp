#include "../H/AudioSystem.h"

// AudioEngine、BGMとSEの読込・再生・更新・明示終了を実装する。
// 公開再生名と、音量・パス・例外処理・解放順は既存のまま維持する。

//========= Windows インクルード=========
#include <windows.h>

//========= C++標準ライブラリ インクルード=========
#include <exception>
#include <memory>

//========= 外部ライブラリ インクルード=========
#include <Audio.h>

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// 音量とAssetパスの固定設定をまとめる。音声の可変状態はImplで所有する。
namespace
{
	//========= BGM音量定数=========
	// 各SceneのBGM再生に使用する音量。
	constexpr float ns_TITLE_BGM_VOLUME = 0.45f;
	constexpr float ns_SHOP_BGM_VOLUME = 0.40f;
	constexpr float ns_GAME_BGM_VOLUME = 0.40f;
	constexpr float ns_RESULT_BGM_VOLUME = 0.45f;

	//========= SE音量定数=========
	// ゲーム中のSE再生に使用する音量。
	constexpr float ns_GUN_SE_VOLUME = 0.70f;
	constexpr float ns_WARP_SE_VOLUME = 0.65f;
	constexpr float ns_LOW_HP_SE_VOLUME = 0.75f;
	constexpr float ns_PURCHASE_SE_VOLUME = 0.70f;
	constexpr float ns_DAMAGE_SE_VOLUME = 0.70f;
	constexpr float ns_SPECIAL_SE_VOLUME = 0.80f;
	constexpr float ns_ENEMY_DEFEAT_SE_VOLUME = 0.80f;

	//========= BGMファイルパス定数=========
	// 各SceneのBGMファイルパス。
	constexpr const wchar_t* ns_TITLE_BGM_FILE_PATH = L"Assets/Audio/BGM/Bgm_Title.wav";
	constexpr const wchar_t* ns_SHOP_BGM_FILE_PATH = L"Assets/Audio/BGM/Bgm_Shop.wav";
	constexpr const wchar_t* ns_GAME_BGM_FILE_PATH = L"Assets/Audio/BGM/Bgm_Game.wav";
	constexpr const wchar_t* ns_RESULT_BGM_FILE_PATH = L"Assets/Audio/BGM/Bgm_Result.wav";

	//========= SEファイルパス定数=========
	// ゲーム中のSEファイルパス。
	constexpr const wchar_t* ns_GUN_SE_FILE_PATH = L"Assets/Audio/SE/Se_Gun.wav";
	constexpr const wchar_t* ns_WARP_SE_FILE_PATH = L"Assets/Audio/SE/Se_Warp.wav";
	constexpr const wchar_t* ns_LOW_HP_SE_FILE_PATH = L"Assets/Audio/SE/Se_Lowhp.wav";
	constexpr const wchar_t* ns_PURCHASE_SE_FILE_PATH = L"Assets/Audio/SE/Se_Purchase.wav";
	constexpr const wchar_t* ns_DAMAGE_SE_FILE_PATH = L"Assets/Audio/SE/Se_Damage.wav";
	constexpr const wchar_t* ns_SPECIAL_SE_FILE_PATH = L"Assets/Audio/SE/Se_Special.wav";
	constexpr const wchar_t* ns_ENEMY_DEFEAT_SE_FILE_PATH = L"Assets/Audio/SE/Se_EnemyDefeat.wav";
}

// AudioEngine、BGM、SE、BGM再生Instanceをまとめて保持する。
class AudioSystem::Impl final
{
public:
	//========= AudioEngine=========
	// XAudio2を利用する音声Engineを所有する。SoundEffect群はこの実体を使って生成する。
	std::unique_ptr<AudioEngine> m_audioEngine{};

	//========= BGM=========
	// タイトル用BGMの音声データを所有する。
	std::unique_ptr<SoundEffect> m_titleBGM{};
	// Shop用BGMの音声データを所有する。
	std::unique_ptr<SoundEffect> m_shopBGM{};
	// Game用BGMの音声データを所有する。
	std::unique_ptr<SoundEffect> m_gameBGM{};
	// Result用BGMの音声データを所有する。
	std::unique_ptr<SoundEffect> m_resultBGM{};

	//========= SE=========
	// 発射SEの音声データを所有する。
	std::unique_ptr<SoundEffect> m_gunSE{};
	// ワープSEの音声データを所有する。
	std::unique_ptr<SoundEffect> m_warpSE{};
	// 低HP警告SEの音声データを所有する。
	std::unique_ptr<SoundEffect> m_lowHPSE{};
	// 購入SEの音声データを所有する。
	std::unique_ptr<SoundEffect> m_purchaseSE{};
	// ダメージSEの音声データを所有する。
	std::unique_ptr<SoundEffect> m_damageSE{};
	// 特殊攻撃SEの音声データを所有する。
	std::unique_ptr<SoundEffect> m_specialSE{};
	// 敵撃破SEの音声データを所有する。
	std::unique_ptr<SoundEffect> m_enemyDefeatSE{};

	//========= BGM再生Instance=========
	// 現在のBGM再生Instanceを所有する。新しいBGMの開始時に置き換える。
	std::unique_ptr<SoundEffectInstance> m_BGMInstance{};
};

//========= 生成・破棄関数=========
// 実装をまだ生成せず、未初期化状態で構築する。
AudioSystem::AudioSystem() = default;

// AudioSystemの終了処理を行う。
AudioSystem::~AudioSystem()
{
	// 明示終了済みでも呼べるUninitで、保持している音声Resourceを終了する。
	Uninit();
}

//========= 初期化・終了関数=========
// AudioEngineと全BGM・SEを読み込み、再生可能な状態にする。
bool AudioSystem::Initialize()
{
	// 二重初期化時に、既存の音声リソースを安全に終了する。
	Uninit();

	// Engineと音声データの生成を既存のstd::exception捕捉範囲で実行する。
	try{
		// 音声Resourceの所有先を生成する。途中失敗時はUninitで部分解放する。
		m_impl = std::make_unique<Impl>();

		// XAudio2を利用するAudioEngineを生成する。
		OutputDebugStringW(L"[Audio] Creating AudioEngine...\n");
		m_impl->m_audioEngine = std::make_unique<AudioEngine>(AudioEngine_Default,nullptr,nullptr);

		// 各Sceneで使用するBGMを読み込む。
		OutputDebugStringW(L"[Audio] Loading: Bgm_Title.wav\n");
		m_impl->m_titleBGM = std::make_unique<SoundEffect>(m_impl->m_audioEngine.get(),ns_TITLE_BGM_FILE_PATH);
		OutputDebugStringW(L"[Audio] Loading: Bgm_Shop.wav\n");
		m_impl->m_shopBGM = std::make_unique<SoundEffect>(m_impl->m_audioEngine.get(),ns_SHOP_BGM_FILE_PATH);
		OutputDebugStringW(L"[Audio] Loading: Bgm_Game.wav\n");
		m_impl->m_gameBGM = std::make_unique<SoundEffect>(m_impl->m_audioEngine.get(),ns_GAME_BGM_FILE_PATH);
		OutputDebugStringW(L"[Audio] Loading: Bgm_Result.wav\n");
		m_impl->m_resultBGM = std::make_unique<SoundEffect>(m_impl->m_audioEngine.get(),ns_RESULT_BGM_FILE_PATH);

		// ゲーム中に使用するSEを読み込む。
		OutputDebugStringW(L"[Audio] Loading: Se_Gun.wav\n");
		m_impl->m_gunSE = std::make_unique<SoundEffect>(m_impl->m_audioEngine.get(),ns_GUN_SE_FILE_PATH);
		OutputDebugStringW(L"[Audio] Loading: Se_Warp.wav\n");
		m_impl->m_warpSE = std::make_unique<SoundEffect>(m_impl->m_audioEngine.get(),ns_WARP_SE_FILE_PATH);
		OutputDebugStringW(L"[Audio] Loading: Se_Lowhp.wav\n");
		m_impl->m_lowHPSE = std::make_unique<SoundEffect>(m_impl->m_audioEngine.get(),ns_LOW_HP_SE_FILE_PATH);
		OutputDebugStringW(L"[Audio] Loading: Se_Purchase.wav\n");
		m_impl->m_purchaseSE = std::make_unique<SoundEffect>(m_impl->m_audioEngine.get(),ns_PURCHASE_SE_FILE_PATH);
		OutputDebugStringW(L"[Audio] Loading: Se_Damage.wav\n");
		m_impl->m_damageSE = std::make_unique<SoundEffect>(m_impl->m_audioEngine.get(),ns_DAMAGE_SE_FILE_PATH);
		OutputDebugStringW(L"[Audio] Loading: Se_Special.wav\n");
		m_impl->m_specialSE = std::make_unique<SoundEffect>(m_impl->m_audioEngine.get(),ns_SPECIAL_SE_FILE_PATH);
		OutputDebugStringW(L"[Audio] Loading: Se_EnemyDefeat.wav\n");
		m_impl->m_enemyDefeatSE = std::make_unique<SoundEffect>(m_impl->m_audioEngine.get(),ns_ENEMY_DEFEAT_SE_FILE_PATH);
		// 全音声の生成成功を記録し、再生可能な状態を呼出側へ通知する。
		OutputDebugStringW(L"[Audio] All sound files loaded.\n");
		return true;
	} catch(const std::exception& exception){
		// 例外内容をデバッグ出力へ記録する。捕捉する例外の種類は拡張しない。
		OutputDebugStringA("[Audio] Failed to initialize audio.\n");
		OutputDebugStringA(exception.what());
		OutputDebugStringA("\n");

		// 読込済みの音声を部分解放し、初期化失敗として返す。
		Uninit();
		return false;
	}
}

// AudioEngineの状態を更新し、音声デバイスの状態変化へ対応する。
void AudioSystem::Update()
{
	// 未初期化や終了後は、Engineの更新・Resetを行わない。
	if(!m_impl || !m_impl->m_audioEngine)return;

	// Engine更新がfalseなら既存どおりResetを試行し、戻り値による追加分岐は行わない。
	try{
		if(!m_impl->m_audioEngine->Update())m_impl->m_audioEngine->Reset();
	} catch(const std::exception& exception){
		// 例外内容をデバッグ出力へ記録する。捕捉する例外の種類は拡張しない。
		OutputDebugStringA("[Audio] AudioEngine update/reset failed.\n");
		OutputDebugStringA(exception.what());
		OutputDebugStringA("\n");
	}
}

// BGM、SE、AudioEngineを安全な順序で終了する。
void AudioSystem::Uninit()
{
	// 未初期化・終了済みなら、再生停止とResource解放を行わない。
	if(!m_impl)return;

	// BGMを停止し、AudioEngineより先に再生Instanceを解放する。
	if(m_impl->m_BGMInstance){
		m_impl->m_BGMInstance->Stop(true);
		m_impl->m_BGMInstance.reset();
	}

	// AudioEngineを終了し、XAudio2の再生VoiceとCallbackを停止する。
	m_impl->m_audioEngine.reset();

	// AudioEngine停止後にSEを解放する。
	m_impl->m_enemyDefeatSE.reset();
	m_impl->m_specialSE.reset();
	m_impl->m_damageSE.reset();
	m_impl->m_purchaseSE.reset();
	m_impl->m_lowHPSE.reset();
	m_impl->m_warpSE.reset();
	m_impl->m_gunSE.reset();

	// AudioEngine停止後にBGMを解放する。
	m_impl->m_resultBGM.reset();
	m_impl->m_gameBGM.reset();
	m_impl->m_shopBGM.reset();
	m_impl->m_titleBGM.reset();

	// 最後に実装本体を解放する。
	m_impl.reset();
}

//========= BGM再生関数=========
// Title用BGMをループ再生する。
void AudioSystem::PlayTitleBgm()
{
	// 対象音声が未読込の場合は、現在のBGMへ影響を与えず戻る。
	if(!m_impl || !m_impl->m_titleBGM)return;

	// 現在のBGMを即時停止・解放してから、新しい再生Instanceを生成する。
	StopBgm();
	m_impl->m_BGMInstance = m_impl->m_titleBGM->CreateInstance();
	if(!m_impl->m_BGMInstance)return;

	// 固定音量を設定し、ループ再生を開始する。
	m_impl->m_BGMInstance->SetVolume(ns_TITLE_BGM_VOLUME);
	m_impl->m_BGMInstance->Play(true);
}

// Shop用BGMをループ再生する。
void AudioSystem::PlayShopBgm()
{
	// 対象音声が未読込の場合は、現在のBGMへ影響を与えず戻る。
	if(!m_impl || !m_impl->m_shopBGM)return;

	// 現在のBGMを即時停止・解放してから、新しい再生Instanceを生成する。
	StopBgm();
	m_impl->m_BGMInstance = m_impl->m_shopBGM->CreateInstance();
	if(!m_impl->m_BGMInstance)return;

	// 固定音量を設定し、ループ再生を開始する。
	m_impl->m_BGMInstance->SetVolume(ns_SHOP_BGM_VOLUME);
	m_impl->m_BGMInstance->Play(true);
}

// Game用BGMをループ再生する。
void AudioSystem::PlayGameBgm()
{
	// 対象音声が未読込の場合は、現在のBGMへ影響を与えず戻る。
	if(!m_impl || !m_impl->m_gameBGM)return;

	// 現在のBGMを即時停止・解放してから、新しい再生Instanceを生成する。
	StopBgm();
	m_impl->m_BGMInstance = m_impl->m_gameBGM->CreateInstance();
	if(!m_impl->m_BGMInstance)return;

	// 固定音量を設定し、ループ再生を開始する。
	m_impl->m_BGMInstance->SetVolume(ns_GAME_BGM_VOLUME);
	m_impl->m_BGMInstance->Play(true);
}

// Result用BGMをループ再生する。
void AudioSystem::PlayResultBgm()
{
	// 対象音声が未読込の場合は、現在のBGMへ影響を与えず戻る。
	if(!m_impl || !m_impl->m_resultBGM)return;

	// 現在のBGMを即時停止・解放してから、新しい再生Instanceを生成する。
	StopBgm();
	m_impl->m_BGMInstance = m_impl->m_resultBGM->CreateInstance();
	if(!m_impl->m_BGMInstance)return;

	// 固定音量を設定し、ループ再生を開始する。
	m_impl->m_BGMInstance->SetVolume(ns_RESULT_BGM_VOLUME);
	m_impl->m_BGMInstance->Play(true);
}

// 現在再生しているBGMを即時停止する。
void AudioSystem::StopBgm()
{
	// 再生Instanceがない場合は停止処理を行わない。
	if(!m_impl || !m_impl->m_BGMInstance)return;

	// 即時停止後にInstanceを解放し、次のBGM開始に以前の再生状態を残さない。
	m_impl->m_BGMInstance->Stop(true);
	m_impl->m_BGMInstance.reset();
}

//========= SE再生関数=========
// 銃の発射SEを再生する。
void AudioSystem::PlayGunSe()
{
	// 対象音声が未読込の場合は、再生を行わない。
	if(!m_impl || !m_impl->m_gunSE)return;

	// 既存の音量・Pitch 0・Pan 0で単発再生を要求する。
	m_impl->m_gunSE->Play(ns_GUN_SE_VOLUME,0.0f,0.0f);
}

// ワープSEを再生する。
void AudioSystem::PlayWarpSe()
{
	// 対象音声が未読込の場合は、再生を行わない。
	if(!m_impl || !m_impl->m_warpSE)return;

	// 既存の音量・Pitch 0・Pan 0で単発再生を要求する。
	m_impl->m_warpSE->Play(ns_WARP_SE_VOLUME,0.0f,0.0f);
}

// 低HP警告SEを再生する。
void AudioSystem::PlayLowHpSe()
{
	// 対象音声が未読込の場合は、再生を行わない。
	if(!m_impl || !m_impl->m_lowHPSE)return;

	// 既存の音量・Pitch 0・Pan 0で単発再生を要求する。
	m_impl->m_lowHPSE->Play(ns_LOW_HP_SE_VOLUME,0.0f,0.0f);
}

// 購入SEを再生する。
void AudioSystem::PlayPurchaseSe()
{
	// 対象音声が未読込の場合は、再生を行わない。
	if(!m_impl || !m_impl->m_purchaseSE)return;

	// 既存の音量・Pitch 0・Pan 0で単発再生を要求する。
	m_impl->m_purchaseSE->Play(ns_PURCHASE_SE_VOLUME,0.0f,0.0f);
}

// ダメージSEを再生する。
void AudioSystem::PlayDamageSe()
{
	// 対象音声が未読込の場合は、再生を行わない。
	if(!m_impl || !m_impl->m_damageSE)return;

	// 既存の音量・Pitch 0・Pan 0で単発再生を要求する。
	m_impl->m_damageSE->Play(ns_DAMAGE_SE_VOLUME,0.0f,0.0f);
}

// 特殊攻撃SEを再生する。
void AudioSystem::PlaySpecialSe()
{
	// 対象音声が未読込の場合は、再生を行わない。
	if(!m_impl || !m_impl->m_specialSE)return;

	// 既存の音量・Pitch 0・Pan 0で単発再生を要求する。
	m_impl->m_specialSE->Play(ns_SPECIAL_SE_VOLUME,0.0f,0.0f);
}

// 敵撃破SEを再生する。
void AudioSystem::PlayEnemyDefeatSe()
{
	// 対象音声が未読込の場合は、再生を行わない。
	if(!m_impl || !m_impl->m_enemyDefeatSE)return;

	// 既存の音量・Pitch 0・Pan 0で単発再生を要求する。
	m_impl->m_enemyDefeatSE->Play(ns_ENEMY_DEFEAT_SE_VOLUME,0.0f,0.0f);
}