#pragma once

// ステージの世界（共有Renderer・設定表から生成した床と壁・出口・CollisionWorld・部屋とウェーブの進行）を所有し、初期化・解放・進行の更新・描画・出口の通過判定を担当する。
// 敵の生成・敵の全滅の判定・プレイヤーの位置の戻し・描画Passの設定は担当しない。敵の生成結果と全滅の結果は、呼出側が受け取って、このクラスへ通知する。

//========= C++標準ライブラリ インクルード=========
#include <vector> // 設定表から生成したSceneObjectを、表の順に保持する可変長配列として使用する。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionWorld.h"
#include "Framework/Etc/H/MathTypes.h"

//========= Scene インクルード=========
#include "Scene/Common/Object/H/ModelRegistry.h"
#include "Scene/Common/Object/H/SceneObject.h"
#include "Scene/Common/Stage/H/ExitDoor.h"
#include "Scene/Common/Stage/H/RoomController.h"

//========= 前方宣言=========
class GraphicsSystem;

// 床・壁・出口・Collision・部屋の進行を、1つの世界としてまとめて扱う。
class StageController final
{
public:
  //========= 初期化・終了関数=========
  // 進行状態を第1部屋の第1ウェーブへ戻し、前回の生成失敗を解除し、生成済みObjectとColliderを空にする。Rendererの生成・Object生成・出口の配置は、Initで行う。
  void Initialize();
  // 共有Renderer・床と壁・出口を、この順に初期化する。最初に失敗した対象をログへ記録し、後続は実行せずfalseを返す。後始末は、呼出側がUninitで行う。
  // graphicsSystem: Resource生成に使う描画基盤。Uninitまで有効であること。
  // 戻り値: 全て成功したらtrue。
  [[nodiscard]] bool Init(GraphicsSystem& graphicsSystem);
  // 出口・Object・共有Renderer・Colliderを、この順に解放する。出口とObjectは共有RendererとCollisionWorldを借用しているため、それらより先に解放する。未初期化でも、繰り返しても呼出可能。
  void Uninit();

  //========= 進行関数=========
  // 最初の部屋の第1ウェーブから、進行をやり直す。前回の生成失敗も解除する。敵と弾の消去は、呼出側が行う。
  void Restart();
  // 現在のウェーブを生成する必要があるかを返す。生成待ちで、かつ前回の生成に失敗していない場合だけtrue。失敗した場合は、Restartまで再試行しない。
  [[nodiscard]] bool ShouldSpawnWave() const;
  // 現在のウェーブの生成結果を記録する。失敗した場合は、Restartまで再試行を止める。
  // isSuccess: 敵の生成に成功したか。
  void RecordSpawnResult(bool isSuccess);
  // 敵の生成が成功したことを、進行管理へ通知する。戦闘中へ移り、全滅の判定が始まる。
  void NotifyWaveSpawned();
  // 敵の全滅の結果から、ウェーブまたは部屋を進め、進行状態に合わせて出口を開閉する。
  // hasLivingEnemies: 現在のウェーブに生存中の敵がいるか。
  void UpdateProgress(bool hasLivingEnemies);
  // 出口が開いていて、位置が通過範囲に入っていて、進行管理が通過を受理した場合だけ、次の部屋へ進める。
  // position: 判定する位置のワールド座標。プレイヤーの中心を渡す。
  // 戻り値: 次の部屋へ進んだらtrue。プレイヤーの位置を戻す処理は、呼出側が行う。
  [[nodiscard]] bool TryPassExit(const XMFLOAT3& position);

  //========= 描画関数=========
  // 設定表から生成した床と壁を、表の順に描画する。描画Passは、呼出側が設定しておくこと。
  // graphicsSystem: 描画基盤。
  // viewMatrix: World空間からView空間への変換行列。
  // projectionMatrix: View空間からClip空間への変換行列。
  void DrawObjects(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;
  // 出口を描画する。閉じている間は扉、開いている間は床の目印。床と壁の描画の後に呼ぶ。
  // graphicsSystem: 描画基盤。
  // viewMatrix: World空間からView空間への変換行列。
  // projectionMatrix: View空間からClip空間への変換行列。
  void DrawExit(GraphicsSystem& graphicsSystem,const XMMATRIX& viewMatrix,const XMMATRIX& projectionMatrix) const;

  //========= Getter関数=========
  // 現在の部屋番号を1始まりで返す。
  [[nodiscard]] int GetRoomNumber() const { return m_roomController.GetRoomNumber(); }
  // 現在のウェーブ番号を1始まりで返す。
  [[nodiscard]] int GetWaveNumber() const { return m_roomController.GetWaveNumber(); }
  // 現在のウェーブの生成に失敗したかを返す。
  [[nodiscard]] bool HasWaveSpawnFailed() const { return m_hasWaveSpawnFailed; }
  // 進行管理の参照を返す。デバッグUIの表示用に、他のクラスへ借用させる。戻り値の参照は、このStageControllerの寿命を超えて保持しない。
  [[nodiscard]] RoomController& GetRoomController() { return m_roomController; }
  // 共有Rendererの参照を返す。敵の弾・範囲の予告が、Rendererを借用するために使う。戻り値の参照は、このStageControllerの寿命を超えて保持しない。
  [[nodiscard]] ModelRegistry& GetModelRegistry() { return m_modelRegistry; }
  // CollisionWorldの参照を返す。衝突・命中判定とCollider登録のために、他のクラスへ借用させる。戻り値の参照は、このStageControllerの寿命を超えて保持しない。
  [[nodiscard]] CollisionWorld& GetCollisionWorld() { return m_collisionWorld; }

private:
  //========= 描画ResourceとCollision=========
  // Cube・OBJの共有Rendererを所有する。m_stageObjects・m_exitDoorと、このStageControllerの外の敵が借用するため、先に宣言し、後に破棄する。
  ModelRegistry m_modelRegistry{};
  // 床・壁・出口のAABBと、敵の球を登録するCollision。
  CollisionWorld m_collisionWorld{};

  //========= ステージObject=========
  // STAGE_LAYOUTの表から生成した床と壁。Rendererを借用するだけで所有しない。表の順が描画順になる。
  std::vector<SceneObject> m_stageObjects{};
  // 部屋の出口。進行状態に合わせて開閉し、閉じている間は通行を止め、開いている間は通過を判定する。
  ExitDoor m_exitDoor{};

  //========= 進行状態=========
  // 3部屋×2ウェーブの進行状態。
  RoomController m_roomController{};
  // 現在のウェーブの生成に失敗したか。trueの間は生成を再試行しない。Restartで解除する。
  bool m_hasWaveSpawnFailed{};
};
