#pragma once

// プレイヤーの位置・姿勢・動作の状態（通常・ダッシュ・しゃがみ・スライディング・回避・蹴り・殴打）を保持する。
// カメラの向きを基準にした水平移動、構え、動作の切替、被弾時の怯み、近接攻撃の当たりの通知、壁のColliderからの押し戻しを行う。
// 近接攻撃のダメージ・敵への効果・視線の更新・射撃・描画は担当しない。

//========= Framework インクルード=========
#include "Framework/Collision/H/CollisionShapes.h"
#include "Framework/Etc/H/MathTypes.h"

//========= 前方宣言=========
class CollisionWorld;
class FpsCamera;
class InputSystem;

// プレイヤーの動作の状態。同時に1つだけ取る。
enum class PLAYER_ACTION_STATE
{
	e_NORMAL,   // 通常の移動。構え（右クリック長押し）ができる。
	e_DASH,     // ダッシュ。構えられない。移動入力と左Shiftの長押しの間、続く。
	e_CROUCH,   // しゃがみ。左Ctrlの長押しの間、続く。視点が低く、移動が遅い。
	e_SLIDE,    // スライディング。ダッシュ中に左Ctrlで始まり、向きを固定して滑る。構えと射撃ができる。
	e_DODGE,    // 回避。W・A・S・Dのいずれかを押してSpaceで始まる。向きを固定して短く移動し、途中でやめられない。無敵時間がある。
	e_KICK,     // 蹴り。Fで始まる。その場で行い、途中でやめられない。動作中は無敵。
	e_PUNCH     // 殴打。Eで始まる。少し前へ踏み込み、途中でやめられない。動作の前半は無敵。
};

// 近接攻撃の種類。当たりの通知で、どちらが当たったかを伝える。
enum class PLAYER_MELEE_TYPE
{
	e_KICK, // 蹴りの当たり。
	e_PUNCH // 殴打の当たり。
};

// WASD移動・ダッシュ・しゃがみ・スライディング・回避・蹴り・殴打・構えを扱い、地形のAABBを通り抜けないように位置を補正するクラス。
class PlayerController final
{
public:
	//========= 初期化・更新関数=========
	// 開始位置を設定し、動作の状態を通常へ戻す。
	// startPosition: 開始位置のワールド座標。Yは立っている時の視点の高さに合わせる。壁の内側を指定すること。
	void Initialize(const XMFLOAT3& startPosition);
	// 入力から動作の状態を切り替え、移動・視点の高さ・構えを更新し、地形のAABBから押し戻す。入力がなくても、押し戻しは行う。
	// deltaTime: この更新で進める経過時間。単位は秒。0なら状態の時間も進まない。
	// input: W・A・S・D・Space・E・F・左Shift・左Ctrl・左右クリックの取得先。参照は保持しない。
	// camera: 移動方向の基準にする視線の取得元。上下の向きは移動に使わない。参照は保持しない。
	// collisionWorld: 押し戻しの基準にするCollisionWorld。STATICレイヤーのAABBを読み取る。参照は保持しない。
	// canKick: 蹴れる相手（射程内で、怯んでいて、転倒していない敵）がいるか。falseの間は、Fを押しても蹴りを始めない。
	void Update(float deltaTime,const InputSystem& input,const FpsCamera& camera,const CollisionWorld& collisionWorld,bool canKick);
	// 被弾などで、プレイヤーを短く怯ませる。怯み中は、通常・ダッシュ・しゃがみの移動速度が下がる。
	// seconds: 怯む時間。単位は秒。すでに怯み中の場合は、長い方の残り時間にする。
	void ApplyFlinch(float seconds);
	// 近接攻撃の当たりが発生していれば、その種類を取り出し、通知を消費する。1回の攻撃につき1度だけtrueになる。
	// meleeType: 当たりが発生している場合の出力先。発生していない場合は変更しない。
	// 戻り値: 当たりが発生していて、取り出せたらtrue。
	[[nodiscard]] bool TryConsumeMeleeImpact(PLAYER_MELEE_TYPE& meleeType);

	//========= Getter関数=========
	// 現在のワールド座標への読み取り専用参照を返す。Yは現在の視点の高さ。参照先はこのクラスの寿命まで有効。
	[[nodiscard]] const XMFLOAT3& GetPosition() const { return m_position; }
	// 体の当たりを表す球を返す。中心は現在の位置、半径は壁との押し戻しに使う値と同じ。
	[[nodiscard]] COLLISION_SPHERE GetBodySphere() const;
	// 構えているかを返す。右クリックを押している間で、ダッシュ・回避・蹴り・殴打の間はfalse。
	[[nodiscard]] bool IsAiming() const { return m_isAiming; }
	// 現在の動作の状態を返す。
	[[nodiscard]] PLAYER_ACTION_STATE GetActionState() const { return m_actionState; }
	// 無敵かを返す。回避・蹴り・殴打の、無敵の時間の内だけtrue。
	[[nodiscard]] bool IsInvincible() const;
	// 射撃してよい状態かを返す。回避・蹴り・殴打の間はfalse、それ以外はtrue。
	[[nodiscard]] bool IsShootingAllowed() const;
	// 怯み中かを返す。
	[[nodiscard]] bool IsFlinching() const { return m_flinchRemainingSeconds > 0.0f; }
	// 最後に回避を始めてからの経過時間を返す。単位は秒。回避をしていない間も増え続け、一度も回避していない場合は十分に大きい値。
	[[nodiscard]] float GetSecondsSinceDodgeStart() const { return m_secondsSinceDodgeStart; }

private:
	//========= 補助関数=========
	// 動作の状態を切り替え、その状態の経過時間を0へ戻す。蹴り・殴打に入る場合は、当たりの通知を待つ状態にする。
	// newState: 新しい動作の状態。
	void EnterState(PLAYER_ACTION_STATE newState);
	// 入力から、動作の状態の切替を判断する。
	// input: 入力の取得先。
	// hasMoveInput: W・A・S・Dの合成方向があるか。
	// moveX: 合成した移動方向のX成分。単位ベクトル。入力なしなら0。
	// moveZ: 合成した移動方向のZ成分。単位ベクトル。入力なしなら0。
	// forwardX: 視線を水平面へ投影した前方向のX成分。殴打の踏み込みの向きに使う。
	// forwardZ: 視線を水平面へ投影した前方向のZ成分。
	// canKick: 蹴れる相手がいるか。
	void UpdateActionState(const InputSystem& input,bool hasMoveInput,float moveX,float moveZ,float forwardX,float forwardZ,bool canKick);

	//========= 位置・状態=========
	// プレイヤー（カメラ）のワールド座標。Yは現在の視点の高さで、状態に応じて滑らかに変わる。
	XMFLOAT3 m_position{0.0f,1.6f,0.0f};
	// 現在の動作の状態。
	PLAYER_ACTION_STATE m_actionState{PLAYER_ACTION_STATE::e_NORMAL};
	// 現在の動作の状態になってからの経過時間。単位は秒。
	float m_stateElapsedSeconds{};
	// 構えているか。
	bool m_isAiming{};

	//========= 動作の補助状態=========
	// スライディング・回避・殴打の踏み込みで固定する移動方向のX成分。単位ベクトル。
	float m_fixedDirectionX{};
	// スライディング・回避・殴打の踏み込みで固定する移動方向のZ成分。単位ベクトル。
	float m_fixedDirectionZ{};
	// 直近に移動入力があった時の方向のX成分。入力が途切れた瞬間に、スライディングへ入る場合の向きに使う。
	float m_lastMoveDirectionX{};
	// 直近に移動入力があった時の方向のZ成分。
	float m_lastMoveDirectionZ{1.0f};
	// ダッシュが構えや射撃で終わった後、左Shiftを一度離すまで、ダッシュへ戻らないようにする。
	bool m_isDashLocked{};
	// 最後に回避を始めてからの経過時間。単位は秒。
	float m_secondsSinceDodgeStart{};

	//========= 近接攻撃・怯み=========
	// 近接攻撃の当たる時刻をまだ迎えていないか。蹴り・殴打に入った時にtrue、当たる時刻に達したらfalseになる。
	bool m_isMeleeImpactPending{};
	// 近接攻撃の当たりが発生していて、まだ取り出されていないか。
	bool m_isMeleeImpactReady{};
	// 怯みの残り時間。単位は秒。0より大きい間は怯み中。
	float m_flinchRemainingSeconds{};
};