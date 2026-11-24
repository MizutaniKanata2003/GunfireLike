#pragma once

// 全画面を覆う黒い画像のAlphaと、フェードの進行・描画を管理する。
// Scene生成・切替や入力停止は担当しない。SceneManager等の利用側が状態を読み、Update・Drawを呼ぶ。
// 黒い画像（Assets/Textures/FadeBlack.png）は、HudSpriteRendererで、Canvas全体へ貼って描画する。ImGuiは使わない。
// 描画には、Initializeによる画像の読込が必要。GraphicsSystemは各呼出中だけ借用し、保持しない。

//========= Framework インクルード=========
#include "Framework/2D/H/HudSpriteRenderer.h"

//========= 前方宣言=========
class GraphicsSystem;

// 開始時に現在のAlphaを引き継がず端点へ戻し、指定時間で透明と黒の間を変化させる。
class FadeOverlay final
{
public:
  //========= ライフサイクル関数=========
  // 旧ResourceをUninitした後、スプライト描画の基盤と、フェード用の黒い画像を生成する。
  // graphicsSystem: 生成Deviceの取得元。描画に使う基盤と対応するものを渡す。参照は保持しない。
  // 戻り値: 全生成成功ならtrue。検出した生成・読込の失敗では、部分Resourceを解放してfalse。画像の読込の失敗のログは、TextureLoaderが記録する。
  // falseの場合も、フェードの進行と完了の判定は動作する。Drawは何も描かない。
  // 画像の読込にはCOMの初期化が必要。呼出スレッドのCOM初期化は、利用側で済ませる。
  [[nodiscard]] bool Initialize(GraphicsSystem& graphicsSystem);
  // スプライト描画の基盤と画像を解放する。フェードの進行状態は変更しない。未初期化・終了済みでも呼出可能。
  void Uninit();

  //========= フェード制御関数=========
  // 透明(Alpha=0)から黒(Alpha=1)へのフェードを開始する。再生中でも経過時間とAlphaを端点へ戻す。
  // duration: 指定する秒数。std::maxで最小0.01秒を適用する。値の追加の有限性検証は行わない。
  void StartFadeOut(float duration);
  // 黒(Alpha=1)から透明(Alpha=0)へのフェードを開始する。再生中でも黒から再開始する。
  // duration: 指定する秒数。0.01秒未満は0.01秒へ制限する。現在Alphaからの残り時間ではない。
  void StartFadeIn(float duration);
  // 再生中だけ経過秒数を加算し、0〜1へ制限した進行率からAlphaを更新する。完了後は端点を保持する。
  // deltaTime: 加算する経過秒数。負値を除外せず、非有限値の独自検証も行わない。停止中は加算しない。
  void Update(float deltaTime);
  // 現在Alphaの黒い画像を、Canvas全体へ描画する。透明時、または画像がない場合は何もしない。Sceneや時間・再生状態は変更しない。
  // graphicsSystem: ContextとRender Passの設定先。描画の前に、Screen UI Passへ設定する。描画後も元へ戻さない。
  // Scene・HUDなどの描画の後に呼ぶ。画像は、ImGuiのウィンドウより下に描かれる。
  void Draw(GraphicsSystem& graphicsSystem);

  //========= Getter関数=========
  // 再生モードが停止以外ならtrue。呼出側がScene更新の抑止等を判断するための値で、この取得自体は停止しない。
  [[nodiscard]] bool IsPlaying() const { return m_mode != FADE_MODE::e_NONE; }
  // 停止中かつAlpha>=1ならtrue。完了通知を消費せず、次の開始まで黒い終端ではtrueを維持する。
  [[nodiscard]] bool IsFadeOutFinished() const { return m_mode == FADE_MODE::e_NONE && m_alpha >= 1.0f; }

private:
  //========= 列挙型=========
  // Alphaの更新方向と停止状態。列挙順と暗黙値は変更しない。
  enum class FADE_MODE
  {
    e_NONE,     // 停止中。Updateで経過時間とAlphaを変更しない。
    e_FADE_IN,  // 黒から透明へ進め、完了後は停止中へ移す。
    e_FADE_OUT  // 透明から黒へ進め、完了後は停止中へ移す。
  };

  //========= 描画Resource=========
  // 黒い画像を貼る矩形を描画するRendererを所有する。Initializeで生成し、Uninitで解放する。
  HudSpriteRenderer m_spriteRenderer{};
  // 黒い画像の識別値。0は未読込または読込失敗で、Drawは何も描かない。
  HudTextureHandle m_fadeTexture{};

  //========= フェード進行状態=========
  // 黒い画像の不透明度。構築時0は透明、1は黒い終端。描画順による表示条件は別に存在する。
  float m_alpha{};
  // 開始からの累積秒数。構築時0で、開始メソッドは必ず0へ戻す。進行率と異なり、この値自体はclampしない。
  float m_elapsedTime{};
  // 所要秒数の初期値0.35を維持する。実際には開始メソッドの引数で上書きされ、最小0.01秒を適用する。
  // 0.35という具体値の採用経緯は未確認。初期停止状態なので、値があるだけで自動再生は始まらない。
  float m_duration{0.35f};
  // 初期停止。開始で方向を設定し、Updateの完了判定で停止へ戻す。終端Alphaはそのまま残す。
  FADE_MODE m_mode{FADE_MODE::e_NONE};
};