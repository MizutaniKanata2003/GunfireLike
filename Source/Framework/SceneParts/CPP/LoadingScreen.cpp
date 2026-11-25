#include "../H/LoadingScreen.h"

// 読込の工程の順次の実行、表示の進み具合の更新、確認待ちの完了判定、右下のバーと文字の描画を実装する。数値と画像の配置は、このファイルの定数にまとめた仮値。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::minで、表示の進み具合を実際の進み具合より先へ進めず、std::maxで重みを0以上にし、std::clampで、進み具合を0〜1へ制限する。
#include <cmath> // std::sinで、確認待ちの文字の点滅の濃さを求める。
#include <string> // std::to_wstringで、失敗した工程の番号をログへ出す。
#include <utility> // std::moveで、工程の列を受け取って保持する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/Logger.h"

// 読込の画面の画像・配置・動きの数値をまとめる。見た目を確認して調整する仮値。
namespace
{
  //========= 画像=========
  // バーの枠の画像。作業ディレクトリ基準。840×56。
  constexpr wchar_t ns_FRAME_TEXTURE_PATH[] = L"Assets/Textures/UI/LoadingBarFrame.png";
  // バーの中身の画像。枠と同じ大きさで、左右に、枠の内側までの余白を持つ。
  constexpr wchar_t ns_FILL_TEXTURE_PATH[] = L"Assets/Textures/UI/LoadingBarFill.png";
  // 「読込中」の文字の画像。800×80。右寄せ。
  constexpr wchar_t ns_LOADING_LABEL_TEXTURE_PATH[] = L"Assets/Textures/UI/LoadingLabelNow.png";
  // 「確認待ち」の文字の画像。800×80。右寄せ。
  constexpr wchar_t ns_READY_LABEL_TEXTURE_PATH[] = L"Assets/Textures/UI/LoadingLabelReady.png";

  //========= 配置（Virtual Canvas 1280×720）=========
  // バーの枠の位置と大きさ。右下。右の余白40、下の余白44。
  constexpr HUD_RECT ns_BAR_RECT{820.0f,648.0f,420.0f,28.0f};
  // 文字の位置と大きさ。バーの真上。画像が右寄せのため、文字は、バーの右端にそろう。
  constexpr HUD_RECT ns_LABEL_RECT{840.0f,602.0f,400.0f,40.0f};
  // バーの中身の画像で、左右の余白が、画像の幅に占める割合。進み具合を、枠の内側の幅へ合わせるために使う。8÷840。
  constexpr float ns_FILL_INSET_RATIO = 0.0095238f;

  //========= 動き=========
  // 表示のバーが進む速さの上限。単位は、満タンを1とした割合/秒。2.0なら、0から満タンまで、最短0.5秒。
  constexpr float ns_DISPLAY_SPEED = 2.0f;
  // 確認待ちの文字の点滅の速さ。単位は、1秒あたりの周期の数。
  constexpr float ns_BLINK_CYCLES_PER_SECOND = 1.2f;
  // 確認待ちの文字の濃さの、最小と、振れ幅。濃さは、最小〜（最小＋振れ幅）の間で変わる。
  constexpr float ns_BLINK_MIN_ALPHA = 0.5f;
  constexpr float ns_BLINK_AMPLITUDE = 0.5f;
  // 円周率の2倍。点滅の位相を、正弦波の角度へ直すために使う。
  constexpr float ns_TWO_PI = 6.28318530718f;
  // 表示のバーが、この値以上になったら、満タンとみなす。浮動小数点の誤差で、確認待ちへ進めなくなるのを防ぐ。
  constexpr float ns_FULL_PROGRESS_THRESHOLD = 0.999f;
}

//========= 初期化・終了関数=========
// 描画基盤と画像を生成する。
bool LoadingScreen::Initialize(GraphicsSystem& graphicsSystem,const LOADING_SCREEN_SETTINGS& settings)
{
  // 旧Resourceを先に解放する。以後の失敗でも、旧状態へは復帰しない。
  Uninit();

  // スプライトの描画基盤を生成する。失敗のログは、HudSpriteRendererの下位処理が記録する。
  if(!m_spriteRenderer.Initialize(graphicsSystem))return false;

  // 画像を読み込む。読み込めなかった画像は、識別値が0のままで、描かない。失敗のログは、TextureLoaderが記録する。
  m_frameTexture = m_spriteRenderer.LoadTexture(graphicsSystem,ns_FRAME_TEXTURE_PATH);
  m_fillTexture = m_spriteRenderer.LoadTexture(graphicsSystem,ns_FILL_TEXTURE_PATH);
  m_loadingLabelTexture = m_spriteRenderer.LoadTexture(graphicsSystem,ns_LOADING_LABEL_TEXTURE_PATH);
  m_readyLabelTexture = m_spriteRenderer.LoadTexture(graphicsSystem,ns_READY_LABEL_TEXTURE_PATH);

  m_autoProceedSeconds = settings.autoProceedSeconds;
  return true;
}

// 描画基盤と画像を解放し、状態を開始前へ戻す。
void LoadingScreen::Uninit()
{
  // 画像の識別値は、Rendererの解放で全て無効になる。
  m_spriteRenderer.Uninit();
  m_frameTexture = {};
  m_fillTexture = {};
  m_loadingLabelTexture = {};
  m_readyLabelTexture = {};

  // 工程と状態を、開始前へ戻す。
  m_steps.clear();
  m_nextStepIndex = 0;
  m_completedWeight = 0.0f;
  m_totalWeight = 0.0f;
  m_displayedProgress = 0.0f;
  m_state = LOADING_STATE::e_IDLE;
  m_readyElapsedSeconds = 0.0f;
  m_blinkSeconds = 0.0f;
  m_hasDrawnSinceStep = false;
  m_autoProceedSeconds = 0.0f;
}

//========= 制御関数=========
// 工程を登録し、先頭から実行する状態にする。
void LoadingScreen::Begin(std::vector<LOADING_STEP> steps)
{
  m_steps = std::move(steps);

  // 全工程の重みの合計を数える。0以下の重みは、0として数える。
  m_totalWeight = 0.0f;
  for(const LOADING_STEP& step : m_steps){
    m_totalWeight += std::max(0.0f,step.weight);
  }

  // 進み具合と、確認待ちの時間を、最初へ戻す。最初の工程の前に、0%のバーを、1回描く。
  m_nextStepIndex = 0;
  m_completedWeight = 0.0f;
  m_displayedProgress = 0.0f;
  m_readyElapsedSeconds = 0.0f;
  m_hasDrawnSinceStep = false;
  m_state = LOADING_STATE::e_LOADING;
}

// 状態に応じて、読込中か、確認待ちの更新を行う。
void LoadingScreen::Update(float deltaTime,bool isConfirmPressed)
{
  m_blinkSeconds += deltaTime;

  if(m_state == LOADING_STATE::e_LOADING){
    UpdateLoading(deltaTime);
  }
  else if(m_state == LOADING_STATE::e_READY){
    UpdateReady(deltaTime,isConfirmPressed);
  }
}

// 読込中と確認待ちの間だけ、バーと文字を描く。
void LoadingScreen::Draw(GraphicsSystem& graphicsSystem)
{
  // 読込中・確認待ち・完了の間だけ描く。完了した後も、最後の見た目を保つ。それ以外は、何も描かず、次の工程の実行も許可しない。
  const bool is_visible = m_state == LOADING_STATE::e_LOADING || m_state == LOADING_STATE::e_READY || m_state == LOADING_STATE::e_FINISHED;
  if(!is_visible)return;

  // 描いたことを記録する。次の更新で、次の工程を実行してよい。
  m_hasDrawnSinceStep = true;

  // 直前のSceneのStateを引き継がず、Alpha Blend有効・Depth無効のScreen UI Passへ設定する。
  graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);

  // バーの枠を描き、その上に、中身を、進み具合の割合だけ描く。中身の画像は、左右に余白を持つため、割合を、余白を除いた幅へ直す。
  const HUD_SPRITE_APPEARANCE opaque_appearance{};
  m_spriteRenderer.DrawSprite(graphicsSystem,m_frameTexture,ns_BAR_RECT,opaque_appearance);
  const float fill_ratio = ns_FILL_INSET_RATIO + m_displayedProgress * (1.0f - 2.0f * ns_FILL_INSET_RATIO);
  m_spriteRenderer.DrawSprite(graphicsSystem,m_fillTexture,ns_BAR_RECT,HUD_SPRITE_APPEARANCE{XMFLOAT4{1.0f,1.0f,1.0f,1.0f},fill_ratio});

  // 読込中は、「読込中」の文字を、そのまま描く。確認待ちでは、「確認待ち」の文字を、点滅させて描く。
  if(m_state == LOADING_STATE::e_LOADING){
    m_spriteRenderer.DrawSprite(graphicsSystem,m_loadingLabelTexture,ns_LABEL_RECT,opaque_appearance);
    return;
  }
  const float blink_alpha = ns_BLINK_MIN_ALPHA + ns_BLINK_AMPLITUDE * 0.5f * (1.0f + std::sin(m_blinkSeconds * ns_BLINK_CYCLES_PER_SECOND * ns_TWO_PI));
  m_spriteRenderer.DrawSprite(graphicsSystem,m_readyLabelTexture,ns_LABEL_RECT,HUD_SPRITE_APPEARANCE{XMFLOAT4{1.0f,1.0f,1.0f,blink_alpha},1.0f});
}

//========= Getter関数=========
// 完了した重みの割合を返す。
float LoadingScreen::GetProgress() const
{
  // 工程がない、または重みの合計が0の場合は、すでに満タンとして扱う。
  if(m_totalWeight <= 0.0f)return 1.0f;

  return std::clamp(m_completedWeight / m_totalWeight,0.0f,1.0f);
}

//========= 更新の補助関数=========
// 読込中の更新。
void LoadingScreen::UpdateLoading(float deltaTime)
{
  // 表示のバーを、実際の進み具合へ、一定の速さで近づける。実際の進み具合を、追い越さない。
  m_displayedProgress = std::min(GetProgress(),m_displayedProgress + ns_DISPLAY_SPEED * deltaTime);

  // 工程が残っている間は、前回の工程の後に描画していれば、次の工程を1つ実行する。描画していなければ、このフレームは、何もしない。
  if(m_nextStepIndex < m_steps.size()){
    if(m_hasDrawnSinceStep)RunNextStep();
    return;
  }

  // 全工程が終わり、表示のバーが満タンになったら、確認待ちへ進む。
  if(m_displayedProgress >= ns_FULL_PROGRESS_THRESHOLD){
    m_displayedProgress = 1.0f;
    m_readyElapsedSeconds = 0.0f;
    m_state = LOADING_STATE::e_READY;
  }
}

// 確認待ちの更新。
void LoadingScreen::UpdateReady(float deltaTime,bool isConfirmPressed)
{
  m_readyElapsedSeconds += deltaTime;

  // 確認の入力があるか、自動の時間（0以下なら、自動では完了しない）が過ぎたら、完了にする。
  const bool is_auto_proceed = m_autoProceedSeconds > 0.0f && m_readyElapsedSeconds >= m_autoProceedSeconds;
  if(isConfirmPressed || is_auto_proceed)m_state = LOADING_STATE::e_FINISHED;
}

// 次の工程を1つ実行する。
void LoadingScreen::RunNextStep()
{
  // 次の工程の前に、描画を、必ず挟む。実行中は、画面が止まるため、先に、記録を戻す。
  m_hasDrawnSinceStep = false;

  // 関数が空の工程は、何もせず成功として扱う。失敗したら、以後の工程を実行せず、状態を失敗にする。
  const LOADING_STEP& step = m_steps[m_nextStepIndex];
  const bool is_succeeded = !step.run || step.run();
  if(!is_succeeded){
    Logger::Write(e_LogLevel::e_ERROR,e_LogCategory::e_SCENE,L"読込の工程が失敗しました。工程の番号: " + std::to_wstring(m_nextStepIndex));
    m_state = LOADING_STATE::e_FAILED;
    return;
  }

  // 成功した工程の重みを加え、次の工程へ進む。
  m_completedWeight += std::max(0.0f,step.weight);
  ++m_nextStepIndex;
}