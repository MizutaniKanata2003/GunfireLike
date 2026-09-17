#pragma once

// ImGuiを使った全画面フェードの状態と進行を管理する。
class FadeOverlay final
{
public:
	//========= フェード制御関数=========
	// 透明な状態から黒い画面へフェードを開始する。
	void StartFadeOut( float duration );
	// 黒い画面から透明な状態へフェードを開始する。
	void StartFadeIn( float duration );
	// 経過時間に応じてフェードの透明度を更新する。
	void Update( float deltaTime );
	// 現在の透明度で全画面フェードを描画する。
	void Draw() const;

	//========= Getter関数=========
	// フェード処理が再生中かを返す。
	[[nodiscard]] bool IsPlaying() const
	{
		return m_Mode != FadeMode::e_NONE;
	}

	// フェードアウトが完了し、画面が完全に黒い状態かを返す。
	[[nodiscard]] bool IsFadeOutFinished() const
	{
		return m_Mode == FadeMode::e_NONE && m_Alpha >= 1.0f;
	}

private:
	//========= 列挙型=========
	// 現在実行しているフェードの種類。
	enum class FadeMode
	{
		e_NONE,
		e_FADE_IN,
		e_FADE_OUT
	};

	//========= メンバー変数=========
	// 現在の黒画面の不透明度とフェード開始からの経過時間。
	float m_Alpha{};
	float m_ElapsedTime{};

	// フェードにかける時間を秒で保持する。
	float m_Duration{ 0.35f };

	// 現在実行中のフェード状態。
	FadeMode m_Mode{ FadeMode::e_NONE };
};