#pragma once

// ImGuiによる黒い全画面フェードの状態・進行・描画を提供する。
// Sceneの生成・切替や入力制御は担当しない。UpdateとDrawは管理側から呼び出す。

// フェード開始時に端点のAlphaへ戻し、指定時間で透明と黒の間を変化させる。
class FadeOverlay final
{
public:
	//========= フェード制御関数=========
	// 透明から黒へフェードアウトを開始する。再生中でも現在のAlphaを引き継がず再開始する。
	// duration: フェード時間。単位は秒。0.01秒未満は0.01秒へ制限する。
	void StartFadeOut(float duration);
	// 黒から透明へフェードインを開始する。再生中でも黒の状態から再開始する。
	// duration: フェード時間。単位は秒。0.01秒未満は0.01秒へ制限する。
	void StartFadeIn(float duration);
	// 再生中の経過時間とAlphaを更新し、進行率が1へ達したら停止状態へ移す。
	// deltaTime: 加算する経過時間。単位は秒。負値を除外する処理は行わない。
	void Update(float deltaTime);
	// 現在のAlphaでメインViewportを覆う黒い背景を描画する。透明なら何もしない。
	// ImGuiのフレーム開始後、描画データ確定前に呼ぶ。進行状態やSceneは変更しない。
	void Draw() const;

	//========= Getter関数=========
	// フェードインまたはフェードアウトの再生中ならtrueを返す。
	[[nodiscard]] bool IsPlaying() const { return m_mode != FADE_MODE::e_NONE; }
	// 再生が停止し、Alphaが1以上の黒い状態ならtrueを返す。
	[[nodiscard]] bool IsFadeOutFinished() const { return m_mode == FADE_MODE::e_NONE && m_alpha >= 1.0f; }

private:
	//========= 列挙型=========
	// Alphaの更新方向と停止状態を表す、内部用の再生モード。
	enum class FADE_MODE
	{
		e_NONE,     // 停止中。現在のAlphaは維持する。
		e_FADE_IN,  // 黒から透明へ変化させる。
		e_FADE_OUT  // 透明から黒へ変化させる。
	};

	//========= フェード進行状態=========
	// 黒い背景の不透明度。0で透明、1で完全な黒。
	float m_alpha{};
	// 今回のフェード開始からの経過時間。単位は秒。
	float m_elapsedTime{};
	// 今回のフェードにかける時間。単位は秒。開始時に最小時間を適用して更新する。
	float m_duration{0.35f};
	// 現在の再生モード。完了後は停止中となり、終端Alphaを保持する。
	FADE_MODE m_mode{FADE_MODE::e_NONE};
};