#pragma once

// 一人称視点の位置・Yaw・Pitchと、射撃の反動による一時的な視線のずれを保持し、方向ベクトルと左手系View行列を生成する。
// 反動の追従・復帰・上限の数値は、RECOIL_PROFILEとして外から受け取る。1発ごとの反動量は、ApplyRecoilの引数で受け取る。
// 移動位置の決定や描画は担当しない。位置の追従は呼出側がSetPositionで反映する。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class InputSystem;

// 反動による視線のずれの、追従・復帰・上限の設定。武器ごとの表（WeaponTable）から、カメラへ渡す。
// 既定値は、D1より前の固定値（企画書で未確定の仮値）と同じ。
struct RECOIL_PROFILE
{
	//========= 追従=========
	// 実際のずれが目標へ近づく速さ。1秒あたりに差の何倍を縮めるかを表し、大きいほど速く、小さいほど滑らかに見える。
	float followRate{16.0f};

	//========= 復帰=========
	// 最後の反動から、目標が0へ戻り始めるまでの時間。単位は秒。連射の間は戻り始めないよう、連射の間隔より短くする。
	float recoveryDelaySeconds{0.15f};
	// 目標が0へ戻る速さ。1秒あたりに残りの何倍を縮めるかを表す。
	float recoveryRate{6.0f};

	//========= 上限=========
	// 反動のずれの目標のPitchの上限。単位は度。連射で視線が上へ上がり続けないための値。
	float maxPitchDegrees{12.0f};
	// 反動のずれの目標のYawの上限。単位は度。左右へ振れ続けないための値。
	float maxYawDegrees{6.0f};
};

// マウス入力で視線方向を更新するカメラ。公開クラス名は既存参照との互換性のため維持する。
class FpsCamera final
{
public:
	//========= 更新関数=========
	// FPS操作中のマウス差分を消費し、YawとPitchを更新する。位置は変更しない。反動によるずれは変更しない。
	// input: FPS操作状態と累積差分の取得先。ConsumeMouseDeltaで取得先の差分をクリアする。
	// 解除中は何もしない。回転量にdeltaTimeは掛けず、Pitchだけを±89度へ制限する。
	void Update(InputSystem& input);
	// 反動によるずれを、目標へ滑らかに近づけ、射撃をやめてしばらく経つと、元の視線へ戻す。毎フレーム呼ぶ。追従と復帰の速さは、反動の設定による。
	// deltaTime: この更新で進める経過時間。単位は秒。0なら何も進めない。
	void UpdateRecoil(float deltaTime);
	// 射撃の反動として、視線を動かす目標を加える。実際の視線は、UpdateRecoilで目標へ滑らかに近づく。位置とマウスで決めた向きは変更しない。
	// yawRadians: 目標へ加えるYawの角度。単位はラジアン。正は右向き。累積は、反動の設定の上限を超えない。
	// pitchRadians: 目標へ加えるPitchの角度。単位はラジアン。正は上向き。累積は、反動の設定の上限を超えない。
	void ApplyRecoil(float yawRadians,float pitchRadians);

	//========= Getter関数=========
	// 保持するワールド座標への読み取り専用参照を返す。参照先の寿命はこのCameraの寿命まで。
	[[nodiscard]] const XMFLOAT3& GetPosition() const { return m_position; }
	// マウスで決めたY軸周りの水平方向の回転角をラジアンで返す。反動によるずれを含まない。周期内へ折り返す処理は行わない。
	[[nodiscard]] float GetYaw() const { return m_yaw; }
	// マウスで決めた垂直方向の回転角をラジアンで返す。反動によるずれを含まない。正値は上向きの視線に対応する。
	[[nodiscard]] float GetPitch() const { return m_pitch; }
	// YawとPitchに反動によるずれを加えた、単位前方向ベクトルを返す。全てが0なら+Z方向。
	[[nodiscard]] XMFLOAT3 GetForward() const;
	// ワールド上方向と前方向の外積を正規化した右方向ベクトルを返す。
	[[nodiscard]] XMFLOAT3 GetRight() const;
	// 現在位置・前方向・ワールド上方向から左手系View行列を返す。
	[[nodiscard]] XMMATRIX GetViewMatrix() const;

	//========= Setter関数=========
	// 位置をコピーする。向きの変更や、移動範囲の制限は行わない。
	// position: 設定するワールド座標。呼出後に引数への参照は保持しない。
	void SetPosition(const XMFLOAT3& position) { m_position = position; }
	// 反動の設定をコピーする。現在の反動のずれと目標は変更せず、次の更新から新しい設定を使う。
	// recoilProfile: 設定する反動の追従・復帰・上限。呼出後に引数への参照は保持しない。
	void SetRecoilProfile(const RECOIL_PROFILE& recoilProfile) { m_recoilProfile = recoilProfile; }

private:
	//========= カメラ操作設定=========
	// マウス差分1pixelに対する回転倍率。単位はラジアン/pixel。
	float m_mouseSensitivity{0.0025f};

	//========= カメラTransform情報=========
	// カメラのワールド座標。既定値はプレイヤー追従反映前の初期位置。
	XMFLOAT3 m_position{0.0f,1.6f,-5.0f};
	// マウスで決めた水平方向の回転角。単位はラジアン。
	float m_yaw{};
	// マウスで決めた垂直方向の回転角。単位はラジアン。Updateで制限する。
	float m_pitch{};

	//========= 反動の状態=========
	// 反動の追従・復帰・上限の設定。既定は、RECOIL_PROFILEの既定値。
	RECOIL_PROFILE m_recoilProfile{};
	// 反動によるYawのずれの目標。単位はラジアン。射撃で増え、射撃をやめて少し経つと0へ戻る。
	float m_recoilTargetYaw{};
	// 反動によるPitchのずれの目標。単位はラジアン。射撃で増え、射撃をやめて少し経つと0へ戻る。
	float m_recoilTargetPitch{};
	// 実際に視線へ加えているYawのずれ。単位はラジアン。目標へ滑らかに近づく。
	float m_recoilYaw{};
	// 実際に視線へ加えているPitchのずれ。単位はラジアン。目標へ滑らかに近づく。
	float m_recoilPitch{};
	// 最後に反動を加えてからの経過時間。単位は秒。一定時間を過ぎると、目標が0へ戻り始める。
	float m_secondsSinceRecoil{1000.0f};
};