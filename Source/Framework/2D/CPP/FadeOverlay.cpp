#include "../H/FadeOverlay.h"

// フェード開始時の状態設定、時間に応じたAlpha更新、ImGui背景による描画を実装する。

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

//========= 外部ライブラリ インクルード=========
#include "imgui.h"

// フェードの時間・進行率・色と、ImGui描画用の固定設定をまとめる。
namespace
{
	//========= フェード時間・進行率定数=========
	// フェード時間として許可する最小秒数。
	constexpr float ns_MIN_FADE_DURATION_SECONDS = 0.01f;
	// フェード進行率の範囲。
	constexpr float ns_FADE_PROGRESS_MIN = 0.0f;
	constexpr float ns_FADE_PROGRESS_MAX = 1.0f;

	//========= Alpha定数=========
	// 完全に透明な状態と完全に不透明な状態のAlpha値。
	constexpr float ns_FULLY_TRANSPARENT_ALPHA = 0.0f;
	constexpr float ns_FULLY_OPAQUE_ALPHA = 1.0f;

	//========= フェード色定数=========
	// 黒いフェード画面に使用するRGB値。
	constexpr float ns_FADE_COLOR_RED = 0.0f;
	constexpr float ns_FADE_COLOR_GREEN = 0.0f;
	constexpr float ns_FADE_COLOR_BLUE = 0.0f;

	//========= ImGui設定定数=========
	// フェード描画に使用するImGuiウィンドウ名。
	constexpr const char* ns_FADE_OVERLAY_WINDOW_NAME = "Fade Overlay";
}

//========= フェード制御関数=========
// 透明な状態から黒い画面へフェードを開始する。
void FadeOverlay::StartFadeOut(float duration)
{
	// 時間を最小値以上へ制限し、再生方向に応じた開始Alphaと経過時間を設定する。
	m_duration = std::max(ns_MIN_FADE_DURATION_SECONDS,duration);
	m_elapsedTime = ns_FULLY_TRANSPARENT_ALPHA;
	m_alpha = ns_FULLY_TRANSPARENT_ALPHA;
	m_mode = FADE_MODE::e_FADE_OUT;
}

// 黒い画面から透明な状態へフェードを開始する。
void FadeOverlay::StartFadeIn(float duration)
{
	// 時間を最小値以上へ制限し、再生方向に応じた開始Alphaと経過時間を設定する。
	m_duration = std::max(ns_MIN_FADE_DURATION_SECONDS,duration);
	m_elapsedTime = ns_FULLY_TRANSPARENT_ALPHA;
	m_alpha = ns_FULLY_OPAQUE_ALPHA;
	m_mode = FADE_MODE::e_FADE_IN;
}

// 経過時間に応じてフェードの透明度を更新する。
void FadeOverlay::Update(float deltaTime)
{
	// 完了後のAlphaを保つため、停止中は経過時間も更新しない。
	if(m_mode == FADE_MODE::e_NONE)return;

	// 経過時間から0.0から1.0のフェード進行率を計算する。
	m_elapsedTime += deltaTime;
	const float progress = std::clamp(m_elapsedTime / m_duration,ns_FADE_PROGRESS_MIN,ns_FADE_PROGRESS_MAX);

	// フェードアウトは進行率をそのままAlphaへ使い、完了時に黒を保持して停止する。
	if(m_mode == FADE_MODE::e_FADE_OUT){
		m_alpha = progress;

		// 完了判定を管理側へ渡せるよう、終端Alphaと停止モードを同時に確定する。
		if(progress >= ns_FADE_PROGRESS_MAX){
			m_alpha = ns_FULLY_OPAQUE_ALPHA;
			m_mode = FADE_MODE::e_NONE;
		}
		return;
	}

	// フェードインでは進行率に応じて黒画面を透明にする。
	m_alpha = ns_FULLY_OPAQUE_ALPHA - progress;

	// 完了後は透明を保持し、以後のUpdateで進行させない。
	if(progress >= ns_FADE_PROGRESS_MAX){
		m_alpha = ns_FULLY_TRANSPARENT_ALPHA;
		m_mode = FADE_MODE::e_NONE;
	}
}

// 現在の透明度で全画面フェードを描画する。
void FadeOverlay::Draw() const
{
	// 透明時にはImGuiウィンドウを作成せず、背景描画を省く。
	if(m_alpha <= ns_FULLY_TRANSPARENT_ALPHA)return;

	// メインViewport全体を覆うImGuiウィンドウを設定する。
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos,ImGuiCond_Always);
	ImGui::SetNextWindowSize(viewport->Size,ImGuiCond_Always);

	// フェード専用の操作できない背景ウィンドウを設定する。
	const ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	// 黒色と現在のAlpha値で背景を描画する。
	ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(ns_FADE_COLOR_RED,ns_FADE_COLOR_GREEN,ns_FADE_COLOR_BLUE,m_alpha));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0.0f);

	// 背景だけを描く。Beginの戻り値にかかわらず、対応するEndは必ず呼ぶ既存構成を維持する。
	ImGui::Begin(ns_FADE_OVERLAY_WINDOW_NAME,nullptr,window_flags);
	ImGui::End();

	// 変更したStyleVarを2個、StyleColorを1個戻し、後続UIへフェード用Styleを残さない。
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();
}