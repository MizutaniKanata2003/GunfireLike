#pragma once

// 読込の画面を担当する汎用の部品。読込の工程（関数）を、順に、1フレームに1工程ずつ実行し、進み具合を、画面の右下のプログレスバーで表示する。
// 全ての工程が終わったら、確認の入力（スペースなど）を待ち、押されたか、一定の時間（既定10秒）が過ぎたら、完了とする。
// 完了した後も、呼出側が画面を切り替えるまで、最後の見た目（満タンのバーと確認待ちの文字）を描き続ける。暗転などで、黒くなるまでの間に、バーが消えないようにするため。
// どのSceneからでも使える。Sceneの種類・入力の基盤・音声には依存せず、確認の入力は、呼出側が、押されたかをUpdateへ渡す。工程の中身（何を読み込むか）も、呼出側が決める。
// 画像（バーの枠・バーの中身・「読込中」の文字・「確認待ち」の文字）は、Initializeで読み込む。バーは、Virtual Canvas（1280×720）の右下に描く。GraphicsSystemは各呼出中だけ借用し、保持しない。
// 工程の実行中は、画面が止まるため、1つの工程を細かく分けるほど、バーが滑らかに進む。Scene遷移・フェード・Captureは担当しない。

//========= C++標準ライブラリ インクルード=========
#include <cstddef> // std::size_tで、次に実行する工程の番号を保持する。
#include <functional> // std::functionで、読込の工程（呼出側の関数）を保持する。
#include <vector> // 読込の工程を、実行する順に保持する。

//========= Framework インクルード=========
#include "Framework/2D/H/HudSpriteRenderer.h"

//========= 前方宣言=========
class GraphicsSystem;

// 読込の工程の関数。成功したらtrue、失敗したらfalseを返す。呼出側の状態を、キャプチャして使ってよい。
using LoadingStepFunction = std::function<bool()>;

// 読込の工程1つ。実行する関数と、バーの進み具合へ与える重みを持つ。
struct LOADING_STEP
{
  // 実行する関数。空の場合は、何もせず成功として扱う。
  LoadingStepFunction run{};
  // この工程が、バー全体に占める割合の目安。全工程の重みの合計に対する割合で進む。時間のかかる工程ほど、大きくする。0以下の値は、0として扱う。
  float weight{1.0f};
};

// 読込の画面の設定。
struct LOADING_SCREEN_SETTINGS
{
  // 全ての工程が終わってから、確認の入力がなくても、完了とするまでの時間。単位は秒。0以下の場合は、自動では完了せず、入力を待ち続ける。
  float autoProceedSeconds{10.0f};
};

// 読込の画面の状態。
enum class LOADING_STATE
{
  e_IDLE,     // 開始前。Beginを呼ぶまで、何もしない。
  e_LOADING,  // 工程を実行中。バーが進む。
  e_READY,    // 全工程が終わり、確認の入力か、自動の時間を待っている。
  e_FINISHED, // 完了。呼出側が、次の画面へ進んでよい。
  e_FAILED    // 工程が1つ失敗した。以後の工程は、実行しない。
};

// 読込の工程を順に実行し、プログレスバーと文字の画像で、状態を表示するクラス。
class LoadingScreen final
{
public:
  //========= 初期化・終了関数=========
  // 旧Resourceを解放した後、スプライトの描画基盤と、画像4枚を読み込む。画像が見つからなくても、進行と完了の判定は動作する（見つからない画像は、描かない）。
  // graphicsSystem: 生成Deviceの取得元。描画に使う基盤と対応するものを渡す。参照は保持しない。
  // settings: 画面の設定。既定は、自動の完了まで10秒。
  // 戻り値: スプライトの描画基盤を生成できたらtrue。失敗した場合は、部分Resourceを解放してfalse。画像の読込の失敗は、ログへ記録するだけで、trueを返す。
  // 画像の読込にはCOMの初期化が必要。呼出スレッドのCOM初期化は、呼出側で済ませる。
  [[nodiscard]] bool Initialize(GraphicsSystem& graphicsSystem,const LOADING_SCREEN_SETTINGS& settings = LOADING_SCREEN_SETTINGS{});
  // 描画基盤と画像を解放し、工程と状態を、開始前へ戻す。未初期化・終了済みでも呼出可能。
  void Uninit();

  //========= 制御関数=========
  // 読込の工程を登録し、先頭から実行する状態にする。実行中でも、新しい工程で最初からやり直す。
  // steps: 実行する順の工程。空の場合は、工程なしで、すぐに確認待ちへ進む。
  void Begin(std::vector<LOADING_STEP> steps);
  // 時間を進める。読込中は、前回の更新の後に、1回以上描画されていれば、次の工程を1つだけ実行する（バーの進みを、工程の間に、必ず、画面へ出すため）。確認待ちでは、入力か時間で、完了へ進む。
  // 工程が失敗した場合は、状態を失敗にし、以後の工程は実行しない。ログへ、失敗を記録する。
  // deltaTime: この更新で進める経過時間。単位は秒。表示の滑らかな動きと、自動の完了の時間に使う。
  // isConfirmPressed: 確認の入力が、この更新で押されたか（押された瞬間）。確認待ちの状態の間だけ使う。
  void Update(float deltaTime,bool isConfirmPressed);
  // 読込中・確認待ち・完了の間だけ、右下のバーと文字を描く。開始前と失敗の状態では、何も描かない。Render PassをScreen UIへ設定する。描いたことを記録し、次の工程の実行を許可する。
  // graphicsSystem: ContextとRender Passの設定先。
  void Draw(GraphicsSystem& graphicsSystem);

  //========= Getter関数=========
  // 現在の状態を返す。
  [[nodiscard]] LOADING_STATE GetState() const { return m_state; }
  // 読込が完了し、呼出側が、次の画面へ進んでよいかを返す。
  [[nodiscard]] bool IsFinished() const { return m_state == LOADING_STATE::e_FINISHED; }
  // 工程が1つ失敗したかを返す。
  [[nodiscard]] bool HasFailed() const { return m_state == LOADING_STATE::e_FAILED; }
  // 工程の進み具合を返す。完了した工程の重みの合計を、全工程の重みの合計で割った値。0〜1。表示は、これへ、滑らかに近づける。
  [[nodiscard]] float GetProgress() const;

private:
  //========= 更新の補助関数=========
  // 読込中の更新。表示の進み具合を、実際の進み具合へ近づけ、描画済みなら、次の工程を1つ実行する。全工程が終わり、表示が満タンになったら、確認待ちへ進む。
  // deltaTime: この更新で進める経過時間。単位は秒。
  void UpdateLoading(float deltaTime);
  // 確認待ちの更新。時間を数え、確認の入力があるか、自動の時間が過ぎたら、完了にする。
  // deltaTime: この更新で進める経過時間。単位は秒。
  // isConfirmPressed: 確認の入力が、この更新で押されたか。
  void UpdateReady(float deltaTime,bool isConfirmPressed);
  // 次の工程を1つ実行し、成功したら、重みを加えて、次へ進む。失敗したら、状態を失敗にする。
  void RunNextStep();

  //========= 描画Resource=========
  // バーと文字を貼る矩形を描画するRenderer。Initializeで生成し、Uninitで解放する。
  HudSpriteRenderer m_spriteRenderer{};
  // バーの枠の画像。0は未読込で、描かない。
  HudTextureHandle m_frameTexture{};
  // バーの中身の画像。枠と同じ大きさで、左から進み具合の割合だけ描く。0は未読込で、描かない。
  HudTextureHandle m_fillTexture{};
  // 「読込中」の文字の画像。読込中に描く。0は未読込で、描かない。
  HudTextureHandle m_loadingLabelTexture{};
  // 「確認待ち」の文字の画像。確認待ちの間、点滅して描く。0は未読込で、描かない。
  HudTextureHandle m_readyLabelTexture{};

  //========= 工程の状態=========
  // 実行する工程。Beginで登録する。
  std::vector<LOADING_STEP> m_steps{};
  // 次に実行する工程の番号。全工程が終わると、工程の数と同じ。
  std::size_t m_nextStepIndex{};
  // 完了した工程の重みの合計。
  float m_completedWeight{};
  // 全工程の重みの合計。0以下の重みは、0として数える。
  float m_totalWeight{};
  // 表示しているバーの進み具合。0〜1。実際の進み具合へ、一定の速さで近づける。
  float m_displayedProgress{};

  //========= 画面の状態=========
  // 現在の状態。
  LOADING_STATE m_state{LOADING_STATE::e_IDLE};
  // 確認待ちになってからの経過時間。単位は秒。自動の完了に使う。
  float m_readyElapsedSeconds{};
  // 文字の点滅に使う経過時間。単位は秒。
  float m_blinkSeconds{};
  // 前回の工程の実行の後に、描画したか。falseの間は、次の工程を実行しない。
  bool m_hasDrawnSinceStep{};
  // 確認の入力がなくても、完了とするまでの時間。単位は秒。0以下なら、自動では完了しない。
  float m_autoProceedSeconds{};
};
