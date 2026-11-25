#pragma once

// 1回のプレイを行うGameSceneを宣言する。担当は、Sceneのライフサイクルと、読込画面（工程ごとの初期化と、スペースか10秒での開始）と、読込からプレイへの暗転（フェードアウト・フェードイン）と、プレイの結果に応じた遷移予約（タイトルへ戻る・やり直し・クリア後の遷移）だけ。
// 初期化・更新・描画・終了の中身は、GameSessionが担当する。読込画面の部品は、LoadingScreenが、暗転の部品は、FadeOverlayが担当する。将来、次のSceneへ渡す値が必要になった場合の取得関数も、このクラスへ追加する。

//========= C++標準ライブラリ インクルード=========
#include <vector> // 読込の工程を、実行する順に作って、読込画面へ渡す。

//========= Framework インクルード=========
#include "Framework/SceneParts/H/FadeOverlay.h"
#include "Framework/SceneParts/H/LoadingScreen.h"

//========= Scene インクルード=========
#include "Scene/Common/Core/H/IScene.h"
#include "Scene/GameScene/H/GameSession.h"

//========= 前方宣言=========
class AudioSystem;
class GraphicsSystem;
class InputSystem;
class SceneManager;

// IScene実装の骨組み（4つの前方宣言・同じ4つの参照を受ける生成関数・5つのoverride宣言）は、TitleScene・ResultScene・GameSceneで同じ形になり、重複として検出されるが、例外とする（規約02の13.8）。
// 理由は、overrideは各クラスが宣言する必要があること（C++の規則）と、生成関数が受ける4つの参照は、各Sceneの依存の契約であること。
// 共通の基底や部品へまとめる案は、所有を優先する13.4に反し、Sceneの生成・遷移の予約を行う呼出し側の変更も要るため、採用しない。
// 1回のプレイのSceneとして、読込画面の進行と、GameSessionの呼出しと、結果に応じた遷移の予約を行う。
class GameScene final: public IScene
{
public:
  //========= 生成関数=========
  // Sceneで使用する非所有参照を登録する。Resource生成はInitで行う。
  // sceneManager: 遷移の予約先。Sceneの使用期間中は有効であること。
  // inputSystem: キー入力の取得先とCapture設定先。Sceneより長く生存すること。
  // graphicsSystem: Resource生成・描画先サイズ取得・描画の基盤。Resource終了まで有効であること。
  // audioSystem: Game用BGMの再生先。Sceneの使用期間中は有効であること。
  GameScene(SceneManager& sceneManager,InputSystem& inputSystem,GraphicsSystem& graphicsSystem,AudioSystem& audioSystem);

  //========= Sceneライフサイクル関数=========
  // プレイの状態を初期状態へ戻し、読込中の状態にする。
  void Initialize() override;
  // 読込画面と暗転の部品を準備し、プレイの初期化を、工程に分けて登録する。工程は、Updateの中で、1フレームに1つずつ実行する。
  // 戻り値: 読込画面を準備できたらtrue。失敗した場合は、falseを返し、SceneManagerが、現在のSceneを維持する。暗転の部品の準備に失敗した場合は、暗転の画像が描かれないだけで、trueを返す。
  bool Init() override;
  // 読込中は、工程を進め、全工程が終わった後に、スペースか10秒で、暗転（フェードアウト）を始める。画面が黒くなったら、プレイを開始し、フェードインする。工程が失敗したら、タイトルへの遷移を予約する。
  // プレイ中は、フェードインが終わるまで進めず、その後、プレイを1回分更新し、結果に応じて遷移を予約する。タイトルへ戻る要求はタイトルへ、死亡後のやり直しは、新しいGameSceneへ、クリアは、クリア後の遷移先へ予約する。
  // deltaTime: 実時間の経過時間。単位は秒。
  void Update(float deltaTime) override;
  // 読込中は、読込画面（右下のバー）を描き、プレイ中は、プレイの3D・HUD・デバッグUIを描画する。最後に、暗転の画像を重ねる。ImGuiフレーム開始後、描画データ確定前に呼ぶ。
  void Draw() override;
  // 読込画面・暗転・プレイのResourceを解放する。繰り返し呼出可能。CaptureやBGMは変更しない。
  void Finalize() override;

private:
  //========= 補助関数=========
  // プレイの初期化の工程を、実行する順に作る。重みは、時間のかかる工程ほど大きい。
  // 戻り値: 読込画面へ渡す工程の列。工程は、このGameSceneの寿命の間だけ有効。
  [[nodiscard]] std::vector<LOADING_STEP> BuildLoadingSteps();
  // 読込中の更新。工程を進め、失敗したらタイトルへ戻る遷移を予約し、完了したら、暗転して、プレイを開始する。
  // deltaTime: 実時間の経過時間。単位は秒。
  void UpdateLoading(float deltaTime);

  //========= 参照・所有状態=========
  // Scene遷移の予約先となる非所有参照。
  SceneManager& m_sceneManager;
  // 読込画面の確認の入力（スペース）の取得先となる非所有参照。
  InputSystem& m_inputSystem;
  // 読込画面と暗転の描画先となる非所有参照。
  GraphicsSystem& m_graphicsSystem;
  // 1回のプレイの流れ。
  GameSession m_session;
  // 読込の進行と、右下のバーの表示。
  LoadingScreen m_loadingScreen{};
  // 読込からプレイへの切替の暗転。SceneManagerの暗転とは別に、このSceneが所有する。
  FadeOverlay m_fadeOverlay{};
  // 読込が終わり、プレイを開始したか。falseの間は、読込画面を更新・描画する。
  bool m_isPlaying{};
  // 読込が終わり、プレイへ切り替えるための暗転（フェードアウト）の最中か。画面が黒くなったら、プレイを開始する。
  bool m_isFadingOut{};
  // 読込の失敗で、タイトルへの遷移を、予約済みか。予約後は、読込を進めない。
  bool m_hasRequestedExit{};
};