#include "../H/FadeOverlay.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

//========= 外部ライブラリ インクルード=========
#include "imgui.h"

namespace
{
	//========= フェード時間・進行率定数=========
		// フェード時間として許可する最小秒数。
	constexpr float MIN_FADE_DURATION_SECONDS = 0.01f;
	// フェード進行率の範囲。
	constexpr float FADE_PROGRESS_MIN = 0.0f;
	constexpr float FADE_PROGRESS_MAX = 1.0f;

	//========= Alpha定数=========
	// 完全に透明な状態と完全に不透明な状態のAlpha値。
	constexpr float FULLY_TRANSPARENT_ALPHA = 0.0f;
	constexpr float FULLY_OPAQUE_ALPHA = 1.0f;

	//========= フェード色定数=========
	// 黒いフェード画面に使用するRGB値。
	constexpr float FADE_COLOR_RED = 0.0f;
	constexpr float FADE_COLOR_GREEN = 0.0f;
	constexpr float FADE_COLOR_BLUE = 0.0f;

	//========= ImGui設定定数=========
	// フェード描画に使用するImGuiウィンドウ名。
	constexpr const char* FADE_OVERLAY_WINDOW_NAME = "Fade Overlay";
}

// 透明な状態から黒い画面へフェードを開始する。
void FadeOverlay::StartFadeOut( float duration )
{
	m_Duration = std::max( MIN_FADE_DURATION_SECONDS, duration );
	m_ElapsedTime = FULLY_TRANSPARENT_ALPHA;
	m_Alpha = FULLY_TRANSPARENT_ALPHA;
	m_Mode = FadeMode::e_FADE_OUT;
}

// 黒い画面から透明な状態へフェードを開始する。
void FadeOverlay::StartFadeIn( float duration )
{
	m_Duration = std::max( MIN_FADE_DURATION_SECONDS, duration );
	m_ElapsedTime = FULLY_TRANSPARENT_ALPHA;
	m_Alpha = FULLY_OPAQUE_ALPHA;
	m_Mode = FadeMode::e_FADE_IN;
}

// 経過時間に応じてフェードの透明度を更新する。
void FadeOverlay::Update( float deltaTime )
{
	if ( m_Mode == FadeMode::e_NONE ) return;

	// 経過時間から0.0から1.0のフェード進行率を計算する。
	m_ElapsedTime += deltaTime;
	const float progress = std::clamp(
		m_ElapsedTime / m_Duration,
		FADE_PROGRESS_MIN,
		FADE_PROGRESS_MAX );

	if ( m_Mode == FadeMode::e_FADE_OUT )
	{
		m_Alpha = progress;

		if ( progress >= FADE_PROGRESS_MAX )
		{
			m_Alpha = FULLY_OPAQUE_ALPHA;
			m_Mode = FadeMode::e_NONE;
		}

		return;
	}

	// フェードインでは進行率に応じて黒画面を透明にする。
	m_Alpha = FULLY_OPAQUE_ALPHA - progress;

	if ( progress >= FADE_PROGRESS_MAX )
	{
		m_Alpha = FULLY_TRANSPARENT_ALPHA;
		m_Mode = FadeMode::e_NONE;
	}
}

// 現在の透明度で全画面フェードを描画する。
void FadeOverlay::Draw() const
{
	if ( m_Alpha <= FULLY_TRANSPARENT_ALPHA ) return;

	// メインViewport全体を覆うImGuiウィンドウを設定する。
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos( viewport->Pos, ImGuiCond_Always );
	ImGui::SetNextWindowSize( viewport->Size, ImGuiCond_Always );

	// フェード専用の操作できない背景ウィンドウを設定する。
	const ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	// 黒色と現在のAlpha値で背景を描画する。
	ImGui::PushStyleColor(
		ImGuiCol_WindowBg,
		ImVec4( FADE_COLOR_RED, FADE_COLOR_GREEN, FADE_COLOR_BLUE, m_Alpha ) );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
	ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );

	ImGui::Begin( FADE_OVERLAY_WINDOW_NAME, nullptr, windowFlags );
	ImGui::End();

	ImGui::PopStyleVar( 2 );
	ImGui::PopStyleColor();
}