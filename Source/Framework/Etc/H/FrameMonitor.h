#pragma once

// 1フレームの処理時間を、段階（メッセージ処理・システム更新・Scene更新・描画・画面表示）ごとに測り、FPSをWindowのタイトルへ表示する。
// 1フレームが基準より長くかかった場合は、どの段階に時間がかかったかを、ログへ出す。
// フレームレートの制限や待機は行わない。ゲーム時間（GameTimer）とは独立に、実時間で測る。

//========= Windows インクルード=========
#include <windows.h> // HWNDと、SetWindowTextWでWindowのタイトルを書き換える。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで、集計区間の最悪の処理時間と、最も長い段階を求める。
#include <array> // 段階ごとの処理時間と名前を、固定長で保持する。
#include <chrono> // std::chrono::steady_clockで、実時間の差分を測る。
#include <cstdio> // swprintf_sで、タイトルとログの文字列を組み立てる。
#include <string> // 基準のタイトルを、所有して保持する。

//========= Framework インクルード=========
#include "Framework/Etc/H/Logger.h"

// 1フレームの処理の段階。値は、段階ごとの配列の添字と同じ。
enum class FRAME_PHASE
{
  e_MESSAGE,      // Windowメッセージの処理。Windowの移動やサイズ変更の操作中は、ここに時間がかかる。
  e_SYSTEM,       // 時間・入力・音声の更新。
  e_SCENE_UPDATE, // Sceneの更新と、Sceneの切替（Resourceの読込を含む）。
  e_DRAW,         // 描画命令の登録とImGuiの描画データの出力。
  e_PRESENT       // 画面への表示。垂直同期の待ちを含む。
};

// フレームの処理時間の測定と、FPSの表示、低FPSのログ出力に使う数値をまとめる。
namespace FRAME_MONITOR_TABLE
{
  //========= 段階=========
  // 段階の数。FRAME_PHASEの要素数と同じ。
  inline constexpr int ns_PHASE_COUNT = 5;
  // 段階の名前。添字は、FRAME_PHASEの値と同じ。ログに出す。静的な文字列定数を指し、所有しない。
  inline constexpr const wchar_t* ns_PHASE_NAMES[ns_PHASE_COUNT]{L"メッセージ処理",L"システム更新",L"Scene更新",L"描画",L"画面表示"};

  //========= 低FPSの基準=========
  // この値より低いFPSに相当する長いフレーム（1000 / この値 ミリ秒より長いフレーム）を、低FPSとして、ログに出す。単位はFPS。仮値。
  inline constexpr float ns_LOW_FPS = 30.0f;
  // 低FPSのログを出す最短の間隔。この間に起きた低FPSは、回数だけを数えて、次のログへ添える。単位は秒。連続した低下で、ログがあふれないようにする。
  inline constexpr float ns_LOG_INTERVAL_SECONDS = 1.0f;

  //========= 表示=========
  // タイトルのFPS表示を、更新する間隔。この間の平均を表示する。単位は秒。
  inline constexpr float ns_TITLE_UPDATE_SECONDS = 0.5f;
}

// フレームの処理時間を測り、FPSをWindowのタイトルへ表示して、低FPSをログへ出す。
class FrameMonitor final
{
public:
  //========= 初期化関数=========
  // 表示先のWindowと基準のタイトルを登録し、集計を始める。
  // windowHandle: FPSをタイトルへ表示するWindow。このFrameMonitorより長く生存すること。
  // baseTitle: FPSの前に付ける、基準のタイトル。終端付きの文字列。コピーして保持する。
  void Initialize(HWND windowHandle,const wchar_t* baseTitle)
  {
    m_windowHandle = windowHandle;
    m_baseTitle = baseTitle;
    m_windowSeconds = 0.0f;
    m_windowFrames = 0;
    m_windowWorstMilliseconds = 0.0f;
    m_secondsSinceLastLog = FRAME_MONITOR_TABLE::ns_LOG_INTERVAL_SECONDS;
    m_suppressedCount = 0;
  }

  //========= 測定関数=========
  // 1フレームの測定を始める。メインループの先頭で呼ぶ。
  void BeginFrame()
  {
    m_frameStart = std::chrono::steady_clock::now();
    m_phaseStart = m_frameStart;
    m_phaseMilliseconds.fill(0.0f);
  }
  // 現在の段階の終わりを記録する。前の段階の終わりから、この呼出までの時間が、この段階の処理時間になる。
  // phase: 終わった段階。
  void EndPhase(FRAME_PHASE phase)
  {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    m_phaseMilliseconds[static_cast<std::size_t>(phase)] = ToMilliseconds(now - m_phaseStart);
    m_phaseStart = now;
  }
  // 1フレームの測定を終える。画面表示の段階を記録し、集計とタイトルの更新、低FPSの判定を行う。メインループの末尾で、画面表示の直後に呼ぶ。
  void EndFrame()
  {
    EndPhase(FRAME_PHASE::e_PRESENT);
    const float frame_milliseconds = ToMilliseconds(m_phaseStart - m_frameStart);
    const float frame_seconds = frame_milliseconds * 0.001f;

    // 集計区間へ、このフレームの時間を加える。区間が一定の長さに達したら、平均のFPSをタイトルへ表示する。
    m_windowSeconds += frame_seconds;
    ++m_windowFrames;
    m_windowWorstMilliseconds = std::max(m_windowWorstMilliseconds,frame_milliseconds);
    if(m_windowSeconds >= FRAME_MONITOR_TABLE::ns_TITLE_UPDATE_SECONDS){
      UpdateTitle();
    }

    // 前回のログからの時間を進め、低FPSに相当する長いフレームなら、内訳をログへ出す。
    m_secondsSinceLastLog += frame_seconds;
    if(frame_milliseconds > 1000.0f / FRAME_MONITOR_TABLE::ns_LOW_FPS){
      WriteSlowFrameLog(frame_milliseconds);
    }
  }

private:
  //========= 補助関数=========
  // 時間の差分を、ミリ秒（小数）へ換算する。
  // duration: 換算する時間の差分。
  // 戻り値: ミリ秒。
  [[nodiscard]] static float ToMilliseconds(std::chrono::steady_clock::duration duration)
  {
    return std::chrono::duration<float,std::milli>(duration).count();
  }
  // 集計区間の平均FPSと、1フレームの平均・最悪の時間を、Windowのタイトルへ表示し、集計区間を新しく始める。
  void UpdateTitle()
  {
    const float average_milliseconds = m_windowSeconds * 1000.0f / static_cast<float>(m_windowFrames);
    const float frames_per_second = static_cast<float>(m_windowFrames) / m_windowSeconds;
    wchar_t buffer[160]{};
    swprintf_s(buffer,L"  |  FPS %.1f  (平均 %.1f ms / 最悪 %.1f ms)",frames_per_second,average_milliseconds,m_windowWorstMilliseconds);
    SetWindowTextW(m_windowHandle,(m_baseTitle + buffer).c_str());

    m_windowSeconds = 0.0f;
    m_windowFrames = 0;
    m_windowWorstMilliseconds = 0.0f;
  }
  // 低FPSに相当する長いフレームの、段階ごとの内訳と、最も長かった段階を、ログへ出す。前回のログから間隔が短い場合は、回数だけを数えて出さない。
  // frameMilliseconds: このフレーム全体の時間。単位はミリ秒。
  void WriteSlowFrameLog(float frameMilliseconds)
  {
    // 前回のログから一定の時間が経っていない場合は、ログを出さず、抑制した回数へ加える。
    if(m_secondsSinceLastLog < FRAME_MONITOR_TABLE::ns_LOG_INTERVAL_SECONDS){
      ++m_suppressedCount;
      return;
    }

    // 最も長かった段階を求める。
    std::size_t longest_index = 0;
    for(std::size_t phase_index = 1; phase_index < m_phaseMilliseconds.size(); ++phase_index){
      if(m_phaseMilliseconds[phase_index] > m_phaseMilliseconds[longest_index]){
        longest_index = phase_index;
      }
    }

    wchar_t buffer[512]{};
    swprintf_s(buffer,L"低FPSを検出: 1フレーム %.1f ms (%.1f FPS相当) | %ls %.1f / %ls %.1f / %ls %.1f / %ls %.1f / %ls %.1f ms | 最も長い段階: %ls | 直前のログ以降に抑制した回数: %u",
               frameMilliseconds,1000.0f / frameMilliseconds,
               FRAME_MONITOR_TABLE::ns_PHASE_NAMES[0],m_phaseMilliseconds[0],FRAME_MONITOR_TABLE::ns_PHASE_NAMES[1],m_phaseMilliseconds[1],
               FRAME_MONITOR_TABLE::ns_PHASE_NAMES[2],m_phaseMilliseconds[2],FRAME_MONITOR_TABLE::ns_PHASE_NAMES[3],m_phaseMilliseconds[3],
               FRAME_MONITOR_TABLE::ns_PHASE_NAMES[4],m_phaseMilliseconds[4],FRAME_MONITOR_TABLE::ns_PHASE_NAMES[longest_index],m_suppressedCount);
    Logger::Write(e_LogLevel::e_WARNING,e_LogCategory::e_GAME,buffer);
    m_secondsSinceLastLog = 0.0f;
    m_suppressedCount = 0;
  }

  //========= 表示先=========
  // FPSを表示するWindow。非所有。
  HWND m_windowHandle{};
  // FPSの前に付ける、基準のタイトル。
  std::wstring m_baseTitle{};

  //========= 1フレームの測定=========
  // このフレームの開始時刻。
  std::chrono::steady_clock::time_point m_frameStart{};
  // 現在の段階の開始時刻。段階の終わりごとに、その時刻へ進める。
  std::chrono::steady_clock::time_point m_phaseStart{};
  // このフレームの、段階ごとの処理時間。単位はミリ秒。添字は、FRAME_PHASEの値と同じ。
  std::array<float,FRAME_MONITOR_TABLE::ns_PHASE_COUNT> m_phaseMilliseconds{};

  //========= 集計=========
  // 現在の集計区間の経過時間。単位は秒。
  float m_windowSeconds{};
  // 現在の集計区間のフレーム数。
  unsigned int m_windowFrames{};
  // 現在の集計区間で最も長かったフレームの時間。単位はミリ秒。
  float m_windowWorstMilliseconds{};
  // 前回の低FPSのログからの経過時間。単位は秒。
  float m_secondsSinceLastLog{};
  // 前回の低FPSのログ以降に、間隔が短いため、ログを出さなかった回数。
  unsigned int m_suppressedCount{};
};
