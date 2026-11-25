#pragma once

// HUDの、画面全体に重ねる画像（報酬選択・ショップの暗幕、低HP警告、スライディングの速度線）を、読み込んで描画する。GameHudから、画面全体の演出を切り出したもの。
// 画像の読込と描画は、GameHudが所有する画像用Rendererを借用して行い、所有しない。表示するかどうかの判断（報酬選択中か、低HPか、スライディング中か）は、呼出側が行い、値として受け取る。
// 速度線は、スライディングの開始・終了に合わせて、濃さを滑らかに変える。濃さの時間は、描画のたびの実時間の差で進める（速度線は、見た目だけの状態で、ゲームの進行に関わらない）。
// 画像の配置は、UiLayoutの表から読む。文字・照準・HPバーなど、画面の一部に置くHUDは担当しない。

//========= C++標準ライブラリ インクルード=========
#include <chrono> // std::chronoで、前回の描画からの実時間を測り、速度線の濃さを時間で変える。

//========= Framework インクルード=========
#include "Framework/2D/H/HudSpriteRenderer.h" // 画像用Rendererと、画像の識別値（HudTextureHandle）を使用する。

//========= 前方宣言=========
class GraphicsSystem;

// 画面全体に重ねる画像を担当するクラス。
class GameHudScreenEffect final
{
public:
  //========= 初期化・終了関数=========
  // 画像用Rendererを借用し、画面全体の画像を全て読み込む。画像の読込に失敗したもの（識別値が0）は、描かない。
  // graphicsSystem: 画像の生成に使う描画基盤。
  // spriteRenderer: 借用する画像用Renderer。Initialize済みであること。このGameHudScreenEffectより長く生存させる。COMの初期化は、呼出側が済ませておくこと。
  void Initialize(GraphicsSystem& graphicsSystem,HudSpriteRenderer& spriteRenderer);
  // 借用した参照と、画像の識別値と、速度線の濃さを手放す。画像そのものは、画像用Rendererの解放で無効になる。未初期化・終了済みでも呼出可能。
  void Uninit();

  //========= 描画関数=========
  // 画面全体を暗くする半透明の背景を描く。報酬選択中とショップが開いている間に、他のHUDより先に呼ぶ。画像用Rendererを借用していない場合は、何もしない。
  // graphicsSystem: 描画基盤。呼出前に、画面UI用のRender Pass（e_SCREEN_UI）を設定しておくこと。
  void DrawBackdrop(GraphicsSystem& graphicsSystem);
  // 画面全体へ重ねる演出を、スライディングの速度線、低HP警告の順に描く。他のHUDより先に呼び、HUDがその上に重なるようにする。画像用Rendererを借用していない場合は、何もしない。
  // 速度線は、isSlidingがtrueの間は濃くなり、falseの間は薄くなる。濃さを時間で進めるため、毎フレーム呼ぶこと。
  // graphicsSystem: 描画基盤。呼出前に、画面UI用のRender Pass（e_SCREEN_UI）を設定しておくこと。
  // isSliding: スライディング中か。trueなら、速度線を濃くしていく。falseなら、薄くしていき、見えなくなったら描かない。
  // isLowHealth: 低HPか。trueなら、低HP警告を描く。
  void DrawOverlays(GraphicsSystem& graphicsSystem,bool isSliding,bool isLowHealth);

private:
  //========= 速度線の補助関数=========
  // 前回の呼出しからの実時間で、速度線の濃さの割合を、スライディング中は上げ、そうでなければ下げる。割合は、0〜1へ制限する。初回の呼出しは、時間を進めない。
  // isSliding: スライディング中か。
  // 戻り値: 進めた後の、速度線の濃さの割合。0〜1。曲線で滑らかにする前の、直線の値。
  [[nodiscard]] float AdvanceSlideLinesRatio(bool isSliding);

  //========= 参照先=========
  // 画像の読込と描画に使う画像用Renderer。非所有で、GameHudが所有する。未登録はnullptr。
  HudSpriteRenderer* m_spriteRenderer{};

  //========= 読み込んだ画像=========
  // 報酬選択とショップの背景（暗幕）の画像。
  HudTextureHandle m_backdropTexture{};
  // 低HP警告の画像。
  HudTextureHandle m_lowHpWarningTexture{};
  // スライディングの速度線の画像。
  HudTextureHandle m_slideLinesTexture{};

  //========= 速度線の濃さ=========
  // 速度線の濃さの割合。0〜1。0で見えず、1で最も濃い。スライディング中に上がり、そうでなければ下がる。
  float m_slideLinesRatio{};
  // 前回、濃さを進めた時刻。前回からの実時間を求めるために使う。
  std::chrono::steady_clock::time_point m_lastAdvanceTime{};
  // 前回の時刻を持っているか。falseの間は、時間を進めない。
  bool m_hasLastAdvanceTime{};
};