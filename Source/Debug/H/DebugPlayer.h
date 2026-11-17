#pragma once

// WASDによる水平移動とカメラ追従の検証用に、プレイヤーのワールド座標を保持する。
// 描画・視線更新・Scene遷移・形状に基づく衝突判定は担当せず、X・Zを固定範囲へ制限する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class FpsCamera;
class InputSystem;

// カメラの向きを移動方向に使用するデバッグ用プレイヤー。
class DebugPlayer final
{
public:
	//========= 更新関数=========
	// WASD入力をカメラ基準の水平方向へ合成・正規化して移動し、最後にX・Zの範囲を補正する。
	// deltaTime: 移動量へ掛ける経過秒数。0秒でも範囲補正は行う。負値・非有限値の検証は行わない。
	// input: キーの押下状態を読む非所有参照。状態を消費・変更せず、Captureの有効性もここでは判定しない。
	// camera: 前・右方向を読む非所有参照。向きと位置は変更せず、参照自体も保持しない。
	// Y座標は維持する。GameSceneは視線更新後にこの関数を呼び、補正後の位置をカメラへ反映する。
	void Update(float deltaTime,const InputSystem& input,const FpsCamera& camera);

	//========= Getter関数=========
	// 現在位置への読み取り専用参照を返す。Updateで内容が変わり、この実体の破棄で参照が無効になる。
	[[nodiscard]] const XMFLOAT3& GetPosition() const { return m_position; }

private:
	//========= プレイヤー状態=========
	// カメラ追従・デバッグ表示に使う初期ワールド座標。X=0、Y=1.6、Z=-5を既存設定として維持する。
	// Y=1.6はUpdateで変更しない高さ。Z=-5は許可範囲外なので初回UpdateでZ=-1.5へ補正される。
	// GameSceneは初期化中にUpdate(0秒)を呼ぶ。数値の採用理由は未確認で、距離単位をメートルとは定義しない。
	XMFLOAT3 m_position{0.0f,1.6f,-5.0f};
	// 単位化した水平移動方向へ掛ける速度。6.0は毎秒6ワールド距離単位の既存設定。
	// 最後の範囲補正で移動が制限されるため、境界上で実際に進む距離まで常に6/秒になるとは限らない。
	float m_moveSpeed{6.0f};
};