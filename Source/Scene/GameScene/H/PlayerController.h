#pragma once

// プレイヤーの位置と構え状態を保持し、カメラの向きを基準にした水平移動と、壁のColliderからの押し戻しを行う。
// 視線の更新・射撃・描画は担当しない。位置の高さ（Y）は開始位置のまま変更しない。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class CollisionWorld;
class FpsCamera;
class InputSystem;

// WASD移動と構え（右クリック長押し）を扱い、地形のAABBを通り抜けないように位置を補正するクラス。
class PlayerController final
{
public:
  //========= 初期化・更新関数=========
  // 開始位置を設定し、構え状態を解除する。
  // startPosition: 開始位置のワールド座標。Yはカメラの高さとして以後変更しない。壁の内側を指定すること。
  void Initialize(const XMFLOAT3& startPosition);
  // 構え入力と移動入力を反映し、地形のAABBから押し戻して位置を更新する。入力がなくても、押し戻しは行う。
  // deltaTime: この更新で進める経過時間。単位は秒。
  // input: W・A・S・Dと右クリックの取得先。参照は保持しない。
  // camera: 移動方向の基準にする視線の取得元。上下の向きは移動に使わない。参照は保持しない。
  // collisionWorld: 押し戻しの基準にするCollisionWorld。STATICレイヤーのAABBを読み取る。参照は保持しない。
  void Update(float deltaTime,const InputSystem& input,const FpsCamera& camera,const CollisionWorld& collisionWorld);

  //========= Getter関数=========
  // 現在のワールド座標への読み取り専用参照を返す。参照先はこのクラスの寿命まで有効。
  [[nodiscard]] const XMFLOAT3& GetPosition() const { return m_position; }
  // 構えているかを返す。右クリックを押している間はtrue。
  [[nodiscard]] bool IsAiming() const { return m_isAiming; }

private:
  //========= 位置・状態=========
  // プレイヤー（カメラ）のワールド座標。Yは開始位置の高さを維持する。
  XMFLOAT3 m_position{0.0f,1.6f,0.0f};
  // 構えているか。右クリックを押している間だけtrue。
  bool m_isAiming{};
};
