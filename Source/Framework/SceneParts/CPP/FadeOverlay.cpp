#include "../H/FadeOverlay.h"

// 画像の読込、開始時の端点設定、秒数からのAlpha計算、黒い画像のCanvas全体への描画を実装する。
// 既存の数値・計算式を維持し、Scene切替や入力制御は追加しない。

//========= C++標準ライブラリ インクルード=========
#include <algorithm> // std::maxで所要時間の最小値を適用し、std::clampでフェード進行率を0〜1へ制限する。

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

// フェードの時間・進行率・色と、黒い画像の設定をまとめる。
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

  //========= 黒い画像の定数=========
  // 黒い画像のパス。作業ディレクトリ基準。画像を差し替えると、フェードの見た目を変えられる。画像のAlphaは不透明（1）であること。
  constexpr wchar_t ns_FADE_TEXTURE_PATH[] = L"Assets/Textures/FadeBlack.png";
  // 黒い画像を貼る矩形。Canvas全体（左上(0,0)・幅1280・高さ720）。HudCanvasの基準領域1280×720と対応する既存値で、実Render Targetのpixelではない。
  constexpr HUD_RECT ns_FADE_RECT{0.0f,0.0f,1280.0f,720.0f};
  // 画像の全体を描く表示割合。1で左端から右端まで。
  constexpr float ns_FULL_VISIBLE_WIDTH_RATIO = 1.0f;
}

//========= ライフサイクル関数=========
// スプライト描画の基盤と、黒い画像を生成する。
bool FadeOverlay::Initialize(GraphicsSystem& graphicsSystem)
{
  // 旧所有参照を先に解放する。以後の生成失敗でも旧描画状態へは復帰しない。
  Uninit();

  // スプライト描画の基盤を生成する。失敗のログは、HudSpriteRendererの下位処理が記録する。
  if(!m_spriteRenderer.Initialize(graphicsSystem))return false;

  // 黒い画像を読み込む。失敗した場合は、TextureLoaderがログを記録し、識別値は0のままになる。
  m_fadeTexture = m_spriteRenderer.LoadTexture(graphicsSystem,ns_FADE_TEXTURE_PATH);
  // 読込失敗なら、先に生成した基盤も解放してfalseを返す。
  if(m_fadeTexture == 0){
    m_spriteRenderer.Uninit();
    return false;
  }
  return true;
}

// 基盤と画像を解放する。
void FadeOverlay::Uninit()
{
  // 画像の識別値は、Rendererの解放で全て無効になる。フェードの進行状態は変更しない。
  m_spriteRenderer.Uninit();
  m_fadeTexture = {};
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

// 時間やSceneを変更せず、現在Alphaの黒い画像をCanvas全体へ描画する。
void FadeOverlay::Draw(GraphicsSystem& graphicsSystem)
{
  // Alphaが0以下、または画像がなければ何も描かない。停止中でもAlphaが正なら描画を続ける。
  if(m_alpha <= ns_FULLY_TRANSPARENT_ALPHA || m_fadeTexture == 0)return;

  // 直前のSceneのStateを引き継がず、Alpha Blend有効・Depth無効のScreen UI Passへ設定する。
  graphicsSystem.SetRenderPass(e_RenderPass::e_SCREEN_UI);

  // 画像の色には白(1,1,1)を掛けて、画像本来の黒を保つ。Alphaだけを、現在のフェードのAlphaにする。画像の全体を描く。
  const HUD_SPRITE_APPEARANCE appearance{XMFLOAT4{1.0f,1.0f,1.0f,m_alpha},ns_FULL_VISIBLE_WIDTH_RATIO};
  m_spriteRenderer.DrawSprite(graphicsSystem,m_fadeTexture,ns_FADE_RECT,appearance);
}