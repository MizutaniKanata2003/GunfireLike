#pragma once

// 一人称視点の位置・Yaw・Pitchを保持し、前・右方向と左手系View行列を生成する。
// プレイヤーの移動、衝突判定、描画は担当しない。位置追従は呼出側がSetPositionで反映する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class InputSystem;

// マウス入力で視線を更新するカメラ。公開名FpsCameraは既存参照との互換性のため維持する。
class FpsCamera final
{
public:
	//========= 更新関数=========
	// FPS操作中だけマウス差分を消費してYaw・Pitchを更新する。位置は変更しない。
	// input: Capture設定と累積差分を取得する非所有参照。ConsumeMouseDeltaにより入力側の差分をクリアする。
	// Capture解除中は差分を消費しない。回転へdeltaTimeは掛けず、Pitchだけを±89度へ制限する。
	void Update(InputSystem& input);

	//========= Getter関数=========
	// 現在のワールド座標を読み取り専用参照で返す。SetPositionで内容が変わり、この実体の破棄で無効になる。
	[[nodiscard]] const XMFLOAT3& GetPosition() const { return m_position; }
	// 水平方向の回転角をラジアンで返す。0を+Z方向の基準とし、周期内への折り返しは行わない。
	[[nodiscard]] float GetYaw() const { return m_yaw; }
	// 垂直方向の回転角をラジアンで返す。正値で前方向のY成分が正となり、上を向く。
	[[nodiscard]] float GetPitch() const { return m_pitch; }
	// Yaw・Pitchから単位前方向を値で返す。両角度が0なら(0,0,1)。位置には依存しない。
	[[nodiscard]] XMFLOAT3 GetForward() const;
	// ワールド上方向×前方向を単位化した右方向を値で返す。角度0なら(1,0,0)。
	[[nodiscard]] XMFLOAT3 GetRight() const;
	// 現在位置・前方向・ワールド上方向からWorld→Viewの左手系行列を返す。Projection合成と転置は行わない。
	[[nodiscard]] XMMATRIX GetViewMatrix() const;

	//========= Setter関数=========
	// ワールド座標をコピーする。Yaw・Pitchの変更、プレイヤー位置の変更、移動範囲の補正は行わない。
	// position: 設定するワールド座標。引数への参照は保持しない。値の範囲・有限性の検証は行わない。
	void SetPosition(const XMFLOAT3& position) { m_position = position; }

private:
	//========= カメラ操作設定=========
	// マウス差分1pixelあたり0.0025ラジアン回転する既存倍率。XはYawへ加算、YはPitchから減算する。
	// 時間倍率ではない。数値の採用理由は未確認のため、感度の意味と既存値だけを示す。
	float m_mouseSensitivity{0.0025f};

	//========= カメラTransform情報=========
	// プレイヤー追従前の初期ワールド座標。X=0、Y=1.6、Z=-5という既存設定を維持する。
	// GameSceneは初期化時に補正済みプレイヤー位置で上書きする。このCamera自体は座標をclampしない。
	XMFLOAT3 m_position{0.0f,1.6f,-5.0f};
	// 水平回転角。初期値0ラジアンは+Z方向を基準とする。Updateで累積し、周期内には折り返さない。
	float m_yaw{};
	// 垂直回転角。初期値0ラジアンは水平。Updateで±89度相当のラジアンへ制限する。
	float m_pitch{};
};