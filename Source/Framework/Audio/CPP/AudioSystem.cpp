#include "../H/AudioSystem.h"

//========= C++標準ライブラリ インクルード=========
#include <exception>
#include <memory>

//========= Windows インクルード=========
#include <windows.h>

//========= DirectXTK インクルード=========
#include <Audio.h>

namespace
{
	//========= BGM音量定数=========
		// 各SceneのBGM再生に使用する音量。
	constexpr float TITLE_BGM_VOLUME = 0.45f;
	constexpr float SHOP_BGM_VOLUME = 0.40f;
	constexpr float GAME_BGM_VOLUME = 0.40f;
	constexpr float RESULT_BGM_VOLUME = 0.45f;

	//========= SE音量定数=========
	// ゲーム中のSE再生に使用する音量。
	constexpr float GUN_SE_VOLUME = 0.70f;
	constexpr float WARP_SE_VOLUME = 0.65f;
	constexpr float LOW_HP_SE_VOLUME = 0.75f;
	constexpr float PURCHASE_SE_VOLUME = 0.70f;
	constexpr float DAMAGE_SE_VOLUME = 0.70f;
	constexpr float SPECIAL_SE_VOLUME = 0.80f;
	constexpr float ENEMY_DEFEAT_SE_VOLUME = 0.80f;

	//========= BGMファイルパス定数=========
	// 各SceneのBGMファイルパス。
	constexpr const wchar_t* TITLE_BGM_FILE_PATH = L"Assets/Audio/BGM/Bgm_Title.wav";
	constexpr const wchar_t* SHOP_BGM_FILE_PATH = L"Assets/Audio/BGM/Bgm_Shop.wav";
	constexpr const wchar_t* GAME_BGM_FILE_PATH = L"Assets/Audio/BGM/Bgm_Game.wav";
	constexpr const wchar_t* RESULT_BGM_FILE_PATH = L"Assets/Audio/BGM/Bgm_Result.wav";

	//========= SEファイルパス定数=========
	// ゲーム中のSEファイルパス。
	constexpr const wchar_t* GUN_SE_FILE_PATH = L"Assets/Audio/SE/Se_Gun.wav";
	constexpr const wchar_t* WARP_SE_FILE_PATH = L"Assets/Audio/SE/Se_Warp.wav";
	constexpr const wchar_t* LOW_HP_SE_FILE_PATH = L"Assets/Audio/SE/Se_Lowhp.wav";
	constexpr const wchar_t* PURCHASE_SE_FILE_PATH = L"Assets/Audio/SE/Se_Purchase.wav";
	constexpr const wchar_t* DAMAGE_SE_FILE_PATH = L"Assets/Audio/SE/Se_Damage.wav";
	constexpr const wchar_t* SPECIAL_SE_FILE_PATH = L"Assets/Audio/SE/Se_Special.wav";
	constexpr const wchar_t* ENEMY_DEFEAT_SE_FILE_PATH = L"Assets/Audio/SE/Se_EnemyDefeat.wav";
}

// AudioEngine、BGM、SE、BGM再生Instanceをまとめて保持する。
class AudioSystem::Impl final
{
public:
	//========= AudioEngine=========
	// XAudio2を利用した音声エンジン。
	std::unique_ptr<DirectX::AudioEngine> audioEngine{};

	//========= BGM=========
	// 各Sceneで使用するBGM。
	std::unique_ptr<DirectX::SoundEffect> titleBgm{};
	std::unique_ptr<DirectX::SoundEffect> shopBgm{};
	std::unique_ptr<DirectX::SoundEffect> gameBgm{};
	std::unique_ptr<DirectX::SoundEffect> resultBgm{};

	//========= SE=========
	// ゲーム中に再生するSE。
	std::unique_ptr<DirectX::SoundEffect> gunSe{};
	std::unique_ptr<DirectX::SoundEffect> warpSe{};
	std::unique_ptr<DirectX::SoundEffect> lowHpSe{};
	std::unique_ptr<DirectX::SoundEffect> purchaseSe{};
	std::unique_ptr<DirectX::SoundEffect> damageSe{};
	std::unique_ptr<DirectX::SoundEffect> specialSe{};
	std::unique_ptr<DirectX::SoundEffect> enemyDefeatSe{};

	//========= BGM再生Instance=========
	// 現在ループ再生しているBGMのInstance。
	std::unique_ptr<DirectX::SoundEffectInstance> bgmInstance{};
};

// AudioSystemを生成する。
AudioSystem::AudioSystem() = default;

// AudioSystemの終了処理を行う。
AudioSystem::~AudioSystem()
{
	Uninit();
}

// AudioEngineと全BGM・SEを読み込み、再生可能な状態にする。
bool AudioSystem::Initialize()
{
	// 二重初期化時に、既存の音声リソースを安全に終了する。
	Uninit();

	try
	{
		m_Impl = std::make_unique<Impl>();

		// XAudio2を利用するAudioEngineを生成する。
		OutputDebugStringW( L"[Audio] Creating AudioEngine...\n" );
		m_Impl->audioEngine = std::make_unique<DirectX::AudioEngine>(
			DirectX::AudioEngine_Default,
			nullptr,
			nullptr );

		// 各Sceneで使用するBGMを読み込む。
		OutputDebugStringW( L"[Audio] Loading: Bgm_Title.wav\n" );
		m_Impl->titleBgm = std::make_unique<DirectX::SoundEffect>( m_Impl->audioEngine.get(), TITLE_BGM_FILE_PATH );

		OutputDebugStringW( L"[Audio] Loading: Bgm_Shop.wav\n" );
		m_Impl->shopBgm = std::make_unique<DirectX::SoundEffect>( m_Impl->audioEngine.get(), SHOP_BGM_FILE_PATH );

		OutputDebugStringW( L"[Audio] Loading: Bgm_Game.wav\n" );
		m_Impl->gameBgm = std::make_unique<DirectX::SoundEffect>( m_Impl->audioEngine.get(), GAME_BGM_FILE_PATH );

		OutputDebugStringW( L"[Audio] Loading: Bgm_Result.wav\n" );
		m_Impl->resultBgm = std::make_unique<DirectX::SoundEffect>( m_Impl->audioEngine.get(), RESULT_BGM_FILE_PATH );

		// ゲーム中に使用するSEを読み込む。
		OutputDebugStringW( L"[Audio] Loading: Se_Gun.wav\n" );
		m_Impl->gunSe = std::make_unique<DirectX::SoundEffect>( m_Impl->audioEngine.get(), GUN_SE_FILE_PATH );

		OutputDebugStringW( L"[Audio] Loading: Se_Warp.wav\n" );
		m_Impl->warpSe = std::make_unique<DirectX::SoundEffect>( m_Impl->audioEngine.get(), WARP_SE_FILE_PATH );

		OutputDebugStringW( L"[Audio] Loading: Se_Lowhp.wav\n" );
		m_Impl->lowHpSe = std::make_unique<DirectX::SoundEffect>( m_Impl->audioEngine.get(), LOW_HP_SE_FILE_PATH );

		OutputDebugStringW( L"[Audio] Loading: Se_Purchase.wav\n" );
		m_Impl->purchaseSe = std::make_unique<DirectX::SoundEffect>( m_Impl->audioEngine.get(), PURCHASE_SE_FILE_PATH );

		OutputDebugStringW( L"[Audio] Loading: Se_Damage.wav\n" );
		m_Impl->damageSe = std::make_unique<DirectX::SoundEffect>( m_Impl->audioEngine.get(), DAMAGE_SE_FILE_PATH );

		OutputDebugStringW( L"[Audio] Loading: Se_Special.wav\n" );
		m_Impl->specialSe = std::make_unique<DirectX::SoundEffect>( m_Impl->audioEngine.get(), SPECIAL_SE_FILE_PATH );

		OutputDebugStringW( L"[Audio] Loading: Se_EnemyDefeat.wav\n" );
		m_Impl->enemyDefeatSe = std::make_unique<DirectX::SoundEffect>( m_Impl->audioEngine.get(), ENEMY_DEFEAT_SE_FILE_PATH );

		OutputDebugStringW( L"[Audio] All sound files loaded.\n" );

		return true;
	}
	catch ( const std::exception& exception )
	{
		OutputDebugStringA( "[Audio] Failed to initialize audio.\n" );
		OutputDebugStringA( exception.what() );
		OutputDebugStringA( "\n" );

		Uninit();

		return false;
	}
}

// AudioEngineの状態を更新し、音声デバイスの状態変化へ対応する。
void AudioSystem::Update()
{
	if ( !m_Impl || !m_Impl->audioEngine ) return;

	try
	{
		if ( !m_Impl->audioEngine->Update() ) m_Impl->audioEngine->Reset();
	}
	catch ( const std::exception& exception )
	{
		OutputDebugStringA( "[Audio] AudioEngine update/reset failed.\n" );
		OutputDebugStringA( exception.what() );
		OutputDebugStringA( "\n" );
	}
}

// BGM、SE、AudioEngineを安全な順序で終了する。
void AudioSystem::Uninit()
{
	if ( !m_Impl ) return;

	// BGMを停止し、AudioEngineより先に再生Instanceを解放する。
	if ( m_Impl->bgmInstance )
	{
		m_Impl->bgmInstance->Stop( true );
		m_Impl->bgmInstance.reset();
	}

	// AudioEngineを終了し、XAudio2の再生VoiceとCallbackを停止する。
	m_Impl->audioEngine.reset();

	// AudioEngine停止後にSEを解放する。
	m_Impl->enemyDefeatSe.reset();
	m_Impl->specialSe.reset();
	m_Impl->damageSe.reset();
	m_Impl->purchaseSe.reset();
	m_Impl->lowHpSe.reset();
	m_Impl->warpSe.reset();
	m_Impl->gunSe.reset();

	// AudioEngine停止後にBGMを解放する。
	m_Impl->resultBgm.reset();
	m_Impl->gameBgm.reset();
	m_Impl->shopBgm.reset();
	m_Impl->titleBgm.reset();

	// 最後に実装本体を解放する。
	m_Impl.reset();
}

// Title用BGMをループ再生する。
void AudioSystem::PlayTitleBgm()
{
	if ( !m_Impl || !m_Impl->titleBgm ) return;

	StopBgm();

	m_Impl->bgmInstance = m_Impl->titleBgm->CreateInstance();
	if ( !m_Impl->bgmInstance ) return;

	m_Impl->bgmInstance->SetVolume( TITLE_BGM_VOLUME );
	m_Impl->bgmInstance->Play( true );
}

// Shop用BGMをループ再生する。
void AudioSystem::PlayShopBgm()
{
	if ( !m_Impl || !m_Impl->shopBgm ) return;

	StopBgm();

	m_Impl->bgmInstance = m_Impl->shopBgm->CreateInstance();
	if ( !m_Impl->bgmInstance ) return;

	m_Impl->bgmInstance->SetVolume( SHOP_BGM_VOLUME );
	m_Impl->bgmInstance->Play( true );
}

// Game用BGMをループ再生する。
void AudioSystem::PlayGameBgm()
{
	if ( !m_Impl || !m_Impl->gameBgm ) return;

	StopBgm();

	m_Impl->bgmInstance = m_Impl->gameBgm->CreateInstance();
	if ( !m_Impl->bgmInstance ) return;

	m_Impl->bgmInstance->SetVolume( GAME_BGM_VOLUME );
	m_Impl->bgmInstance->Play( true );
}

// Result用BGMをループ再生する。
void AudioSystem::PlayResultBgm()
{
	if ( !m_Impl || !m_Impl->resultBgm ) return;

	StopBgm();

	m_Impl->bgmInstance = m_Impl->resultBgm->CreateInstance();
	if ( !m_Impl->bgmInstance ) return;

	m_Impl->bgmInstance->SetVolume( RESULT_BGM_VOLUME );
	m_Impl->bgmInstance->Play( true );
}

// 現在再生しているBGMを即時停止する。
void AudioSystem::StopBgm()
{
	if ( !m_Impl || !m_Impl->bgmInstance ) return;

	m_Impl->bgmInstance->Stop( true );
	m_Impl->bgmInstance.reset();
}

// 銃の発射SEを再生する。
void AudioSystem::PlayGunSe()
{
	if ( !m_Impl || !m_Impl->gunSe ) return;

	m_Impl->gunSe->Play( GUN_SE_VOLUME, 0.0f, 0.0f );
}

// ワープSEを再生する。
void AudioSystem::PlayWarpSe()
{
	if ( !m_Impl || !m_Impl->warpSe ) return;

	m_Impl->warpSe->Play( WARP_SE_VOLUME, 0.0f, 0.0f );
}

// 低HP警告SEを再生する。
void AudioSystem::PlayLowHpSe()
{
	if ( !m_Impl || !m_Impl->lowHpSe ) return;

	m_Impl->lowHpSe->Play( LOW_HP_SE_VOLUME, 0.0f, 0.0f );
}

// 購入SEを再生する。
void AudioSystem::PlayPurchaseSe()
{
	if ( !m_Impl || !m_Impl->purchaseSe ) return;

	m_Impl->purchaseSe->Play( PURCHASE_SE_VOLUME, 0.0f, 0.0f );
}

// ダメージSEを再生する。
void AudioSystem::PlayDamageSe()
{
	if ( !m_Impl || !m_Impl->damageSe ) return;

	m_Impl->damageSe->Play( DAMAGE_SE_VOLUME, 0.0f, 0.0f );
}

// 特殊攻撃SEを再生する。
void AudioSystem::PlaySpecialSe()
{
	if ( !m_Impl || !m_Impl->specialSe ) return;

	m_Impl->specialSe->Play( SPECIAL_SE_VOLUME, 0.0f, 0.0f );
}

// 敵撃破SEを再生する。
void AudioSystem::PlayEnemyDefeatSe()
{
	if ( !m_Impl || !m_Impl->enemyDefeatSe ) return;

	m_Impl->enemyDefeatSe->Play( ENEMY_DEFEAT_SE_VOLUME, 0.0f, 0.0f );
}