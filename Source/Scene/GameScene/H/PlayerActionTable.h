#pragma once

// プレイヤーの移動速度・動作の長さ・無敵の時間・視点の高さ・体の大きさの数値を1か所へまとめる。企画書で未確定の数値は、プレイ確認後に調整する仮値。
// 数値の定義だけを持ち、動作の判断や状態は担当しない。単位は、特に記載がない限り、ワールド距離単位と秒。

// プレイヤーの動作に使う数値をまとめる。
namespace PLAYER_ACTION_TABLE
{
  //========= 体の大きさ=========
  // 壁との押し戻しに使う球の半径。単位はワールド距離単位。
  // 内側の面がX=±9.9・Z=-1.9・Z=17.9の壁に対し、中心の移動範囲がX=±9.5・Z=-1.5〜17.5（旧DebugPlayerの範囲）と一致する値。
  inline constexpr float ns_BODY_RADIUS = 0.4f;
  // 1回の押し戻しの前に進める最大距離。単位はワールド距離単位。壁の厚み（0.2）以下にして、中心が壁の反対側へ抜けないようにする。
  inline constexpr float ns_MAX_STEP_DISTANCE = 0.2f;

  //========= 移動速度=========
  // 通常時の水平移動速度。単位はワールド距離単位/秒。旧DebugPlayerの設定と同じ。
  inline constexpr float ns_WALK_SPEED = 6.0f;
  // 構え中の移動速度の倍率。企画書では「構え射撃は移動速度が低下する」とだけ定められており、具体値は未確定の仮値。
  inline constexpr float ns_AIM_SPEED_MULTIPLIER = 0.5f;
  // ダッシュの移動速度。単位はワールド距離単位/秒。通常の1.5倍の仮値。
  inline constexpr float ns_DASH_SPEED = 9.0f;
  // しゃがみ中の移動速度の倍率。仮値。
  inline constexpr float ns_CROUCH_SPEED_MULTIPLIER = 0.5f;
  // 怯み中の移動速度の倍率。通常・ダッシュ・しゃがみの速度へ掛ける。企画書で未確定の仮値。
  inline constexpr float ns_FLINCH_SPEED_MULTIPLIER = 0.4f;

  //========= スライディング=========
  // スライディングの長さ。単位は秒。仮値。
  inline constexpr float ns_SLIDE_SECONDS = 0.8f;
  // スライディング開始時の速度。単位はワールド距離単位/秒。ダッシュより速い仮値。
  inline constexpr float ns_SLIDE_START_SPEED = 10.0f;
  // スライディング終了時の速度。単位はワールド距離単位/秒。開始から終了へ、時間に比例して減速する。
  inline constexpr float ns_SLIDE_END_SPEED = 4.0f;

  //========= 回避=========
  // 回避の長さ。単位は秒。途中でやめられない。仮値。
  inline constexpr float ns_DODGE_SECONDS = 0.3f;
  // 回避の移動速度。単位はワールド距離単位/秒。0.3秒で約3.6進む仮値。
  inline constexpr float ns_DODGE_SPEED = 12.0f;
  // 回避の開始から無敵が続く時間。単位は秒。回避の長さ以下の仮値。
  inline constexpr float ns_DODGE_INVINCIBLE_SECONDS = 0.25f;
  // 一度も回避していない状態を表す、十分に大きい経過時間。単位は秒。
  inline constexpr float ns_SECONDS_WITHOUT_DODGE = 1000.0f;

  //========= 蹴り=========
  // 蹴りの長さ。単位は秒。途中でやめられない。仮値。
  inline constexpr float ns_KICK_SECONDS = 0.45f;
  // 蹴りの開始から、相手へ当たるまでの時間。単位は秒。蹴りの長さ以下の仮値。
  inline constexpr float ns_KICK_IMPACT_SECONDS = 0.15f;
  // 蹴りの開始から無敵が続く時間。単位は秒。企画書では「動作中無敵」のため、蹴りの長さと同じ。
  inline constexpr float ns_KICK_INVINCIBLE_SECONDS = ns_KICK_SECONDS;

  //========= 殴打=========
  // 殴打の長さ。単位は秒。途中でやめられない。仮値。
  inline constexpr float ns_PUNCH_SECONDS = 0.5f;
  // 殴打の開始から、相手へ当たるまでの時間。単位は秒。殴打の長さ以下の仮値。
  inline constexpr float ns_PUNCH_IMPACT_SECONDS = 0.15f;
  // 殴打の開始から無敵が続く時間。単位は秒。企画書で未確定の仮値。
  inline constexpr float ns_PUNCH_INVINCIBLE_SECONDS = 0.35f;
  // 殴打で前へ踏み込む時間。単位は秒。開始からこの時間だけ前へ進み、その後は止まる。仮値。
  inline constexpr float ns_PUNCH_LUNGE_SECONDS = 0.25f;
  // 殴打の踏み込みの速度。単位はワールド距離単位/秒。仮値。
  inline constexpr float ns_PUNCH_LUNGE_SPEED = 3.0f;

  //========= 視点の高さ=========
  // 立っている時の視点の高さ。単位はワールド距離単位。旧DebugPlayerの初期の高さ。
  inline constexpr float ns_STAND_EYE_HEIGHT = 1.6f;
  // しゃがみ中の視点の高さ。仮値。
  inline constexpr float ns_CROUCH_EYE_HEIGHT = 1.0f;
  // スライディング中の視点の高さ。しゃがみより低い仮値。
  inline constexpr float ns_SLIDE_EYE_HEIGHT = 0.8f;
  // 回避中の視点の高さ。少し低くして、身をかわす動きを出す仮値。
  inline constexpr float ns_DODGE_EYE_HEIGHT = 1.3f;
  // 蹴り中の視点の高さ。仮値。
  inline constexpr float ns_KICK_EYE_HEIGHT = 1.4f;
  // 殴打中の視点の高さ。仮値。
  inline constexpr float ns_PUNCH_EYE_HEIGHT = 1.5f;
  // 視点の高さが目標へ近づく速さ。1秒あたりに差の何倍を縮めるかを表し、大きいほど速い。仮値。
  inline constexpr float ns_EYE_HEIGHT_SMOOTHING_RATE = 14.0f;
}
