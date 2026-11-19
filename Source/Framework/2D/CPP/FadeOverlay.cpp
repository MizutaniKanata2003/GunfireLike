#include "../H/FadeOverlay.h"

// 開始時の端点設定、秒数からのAlpha計算、メインViewportを覆うImGui背景の登録を実装する。
// 既存の数値・計算式・描画Flag・Style操作の順序を維持し、Scene切替や入力制御は追加しない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで所要時間の最小値を適用し、std::clampでフェード進行率を0〜1へ制限する。

//========= 外部ライブラリ インクルード=========
#include "imgui.h" // メインViewportの取得、背景Windowの登録、StyleのPush/Popを使用する。

// フェードの時間・進行率・色と、ImGui背景Windowの固定設定をまとめる。
namespace
{
	//========= フェード時間・進行率定数=========
	// 最小所要時間0.01秒=10ミリ秒。正の分母を準備するための既存制限で、入力全般の妥当性検証ではない。
	constexpr float ns_MIN_FADE_DURATION_SECONDS = 0.01f;
	// 進行率の下限0は開始点。負の経過時間で比率が下回ってもAlpha計算に使う進行率はここへ収める。
	constexpr float ns_FADE_PROGRESS_MIN = 0.0f;
	// 進行率の上限1は所要時間到達。完了判定にも同じ値を使用する。
	constexpr float ns_FADE_PROGRESS_MAX = 1.0f;

	//========= Alpha定数=========
	// Alpha=0は透明。経過時間を0へ戻す代入にも既存コードで使うが、その場合の単位は秒。
	constexpr float ns_FULLY_TRANSPARENT_ALPHA = 0.0f;
	// Alpha=1は不透明側の端点。フェードインでは1-進行率を計算する基準にも使う。
	constexpr float ns_FULLY_OPAQUE_ALPHA = 1.0f;

	//========= フェード色定数=========
	// RGB各成分を0とする黒の既存設定。変化させるのは別に保持するAlphaで、RGBは固定する。
	constexpr float ns_FADE_COLOR_RED = 0.0f;
	constexpr float ns_FADE_COLOR_GREEN = 0.0f;
	constexpr float ns_FADE_COLOR_BLUE = 0.0f;

	//========= ImGui設定定数=========
	// ImGui Windowの識別名。同名Windowとの対応に影響するため、既存文字列を維持する。
	constexpr const char* ns_FADE_OVERLAY_WINDOW_NAME = "Fade Overlay";
}

//========= フェード制御関数=========
// 透明な端点から黒へ進む状態を準備する。
void FadeOverlay::StartFadeOut(float duration)
{
	// 所要時間を最小値以上にして、経過秒数・Alpha・方向をまとめて再設定する。現在Alphaの連続性は保持しない。
	m_duration = std::max(ns_MIN_FADE_DURATION_SECONDS,duration);
	m_elapsedTime = ns_FULLY_TRANSPARENT_ALPHA;
	m_alpha = ns_FULLY_TRANSPARENT_ALPHA;
	m_mode = FADE_MODE::e_FADE_OUT;
}

// 黒い端点から透明へ進む状態を準備する。
void FadeOverlay::StartFadeIn(float duration)
{
	// 再生途中でも経過時間0・Alpha1から再開する。指定時間は残り距離から逆算せず、引数に最小制限だけを掛ける。
	m_duration = std::max(ns_MIN_FADE_DURATION_SECONDS,duration);
	m_elapsedTime = ns_FULLY_TRANSPARENT_ALPHA;
	m_alpha = ns_FULLY_OPAQUE_ALPHA;
	m_mode = FADE_MODE::e_FADE_IN;
}

// 再生中の時間とAlphaを更新し、完了した方向の端点を確定する。
void FadeOverlay::Update(float deltaTime)
{
	// 停止中は経過秒数もAlphaも変更しない。黒い終端で停止した後も、次の開始までは黒を保持する。
	if(m_mode == FADE_MODE::e_NONE)return;

	// 経過秒/所要秒で単位なしの進行率を求める。制限するのは比率だけで、累積秒数自体はそのまま残す。
	m_elapsedTime += deltaTime;
	const float progress = std::clamp(m_elapsedTime / m_duration,ns_FADE_PROGRESS_MIN,ns_FADE_PROGRESS_MAX);

	// フェードアウト指定なら進行率をそのままAlphaにし、完了後も黒い端点を残す。
	if(m_mode == FADE_MODE::e_FADE_OUT){
		// 比率0で透明、1で黒となる線形の変化。イージング等の新しい式は追加しない。
		m_alpha = progress;

		// 進行率が終端へ達したらAlpha1と停止を同時に確定し、利用側の切替判定が成立する状態にする。
		if(progress >= ns_FADE_PROGRESS_MAX){
			m_alpha = ns_FULLY_OPAQUE_ALPHA;
			m_mode = FADE_MODE::e_NONE;
		}
		return;
	}

	// 残るフェードイン経路では1-進行率で黒を減らす。方向に応じた条件の追加や列挙値検証は行わない。
	m_alpha = ns_FULLY_OPAQUE_ALPHA - progress;

	// 完了したらAlpha0と停止を確定する。以後のDrawは透明判定で省略される。
	if(progress >= ns_FADE_PROGRESS_MAX){
		m_alpha = ns_FULLY_TRANSPARENT_ALPHA;
		m_mode = FADE_MODE::e_NONE;
	}
}

// 時間やSceneを変更せず、現在Alphaの黒い背景をImGuiの描画命令として登録する。
void FadeOverlay::Draw() const
{
	// Alphaが0以下ならWindowもStyle操作も登録せず戻る。停止中でもAlphaが正なら描画を続ける。
	if(m_alpha <= ns_FULLY_TRANSPARENT_ALPHA)return;

	// メインViewportの位置・全体サイズをそのまま使う。Virtual CanvasではなくImGuiの座標系へ合わせる。
	// Alwaysにより毎回この位置と寸法を適用する。Viewportは借用し、このクラスでは保持しない。
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos,ImGuiCond_Always);
	ImGui::SetNextWindowSize(viewport->Size,ImGuiCond_Always);

	// 装飾なし・移動なし・設定保存なし・入力なし・Navigationなし・Focus時の前面化なしをORで組み合わせる。
	// 自動の最前面保証やアプリ全体の入力停止を追加する指定ではない。既存のWindow Flag構成を維持する。
	const ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	// 背景色に固定RGBと現在Alphaを渡し、丸みと枠幅を0にして矩形の背景にする。
	// StyleVarは2回、StyleColorは1回Pushするため、後段のPop数と対応を保つ。
	ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(ns_FADE_COLOR_RED,ns_FADE_COLOR_GREEN,ns_FADE_COLOR_BLUE,m_alpha));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.0f);

	// 本文なしのWindow背景を登録する。p_open=nullptrで閉じるためのboolは用意しない。
	// Beginの戻り値にかかわらず対応Endを呼ぶ。GPU描画はここでは行わず、ImGuiの描画バックエンドへ任せる。
	ImGui::Begin(ns_FADE_OVERLAY_WINDOW_NAME,nullptr,window_flags);
	ImGui::End();

	// 上でPushしたStyleだけを戻し、後続UIへ丸み・枠幅・背景色を残さない。例外時の自動復元機構は追加しない。
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}