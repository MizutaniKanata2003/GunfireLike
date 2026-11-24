#pragma once

// ゲーム中の報酬選択・ショップ・武器の持ち替えが共有する、入力の補助関数（カーソルの座標変換と、数字キーの判定）をまとめる。
// 状態は持たず、入力の取得元と描画先の大きさを引数で受け取る。入力の意味づけ（どのキーが何に対応するか）は、呼出側が行う。

//========= Framework インクルード=========
#include "Framework/2D/H/HudCanvas.h"
#include "Framework/DirectX/H/GraphicsSystem.h"
#include "Framework/Etc/H/MathTypes.h"
#include "Framework/Input/H/InputSystem.h"

// ゲーム中の画面操作（カード・ボタン・武器の選択）に使う入力の補助関数と、固定設定をまとめる。
namespace GAME_INPUT_HELPER
{
  //========= キー設定=========
  // カードや購入ボタン・武器を、キーで選ぶ時の、最初のものに対応するキー。1から順に、並びと同じ。クリックが使えない場合の代わりの操作。
  inline constexpr unsigned char ns_UI_FIRST_KEY = '1';
  // ショップのページ切替に使うキー。
  inline constexpr unsigned char ns_SHOP_SWITCH_KEY = 'U';

  //========= 座標設定=========
  // 描画先の大きさが無効な場合に、カーソルを置く、基準Canvasの外の位置。どのカードやボタンにも乗らない。
  inline constexpr float ns_CURSOR_OUTSIDE_CANVAS = -1000.0f;

  //========= 補助関数=========
  // カーソルのクライアント座標を、基準Canvas上の座標へ変換する。HUDの描画と同じ倍率と余白（HudCanvas）の逆変換。
  // inputSystem: カーソルの位置の取得元。
  // graphicsSystem: 描画先の大きさの取得元。
  // 戻り値: 基準Canvas上のカーソルの位置。描画先の大きさが無効な場合は、どのカードやボタンにも乗らない位置。
  [[nodiscard]] inline XMFLOAT2 ConvertCursorToCanvas(const InputSystem& inputSystem,const GraphicsSystem& graphicsSystem)
  {
    // HUDの描画と同じ、描画先の大きさから決まる倍率と余白を求める。倍率が0以下なら、変換できない。
    const HudCanvas hud_canvas{graphicsSystem.GetRenderWidth(),graphicsSystem.GetRenderHeight()};
    const float canvas_scale = hud_canvas.GetScale();
    if(canvas_scale <= 0.0f)return XMFLOAT2{ns_CURSOR_OUTSIDE_CANVAS,ns_CURSOR_OUTSIDE_CANVAS};

    // 描画先の座標から余白を引き、倍率で割って、基準Canvas上の座標へ戻す。
    const POINT cursor_position = inputSystem.GetCursorClientPosition();
    const XMFLOAT2& canvas_offset = hud_canvas.GetOffset();
    return XMFLOAT2{(static_cast<float>(cursor_position.x) - canvas_offset.x) / canvas_scale,
      (static_cast<float>(cursor_position.y) - canvas_offset.y) / canvas_scale};
  }

  // 数字キー（1から順）のうち、この更新で押されたキーに対応する添字を返す。複数が押された場合は、最後のキーを返す。
  // inputSystem: キー入力の取得元。
  // count: 対応させる項目の数。この数だけ、1から順にキーを調べる。
  // 戻り値: 押されたキーに対応する添字。どれも押されていなければ-1。
  [[nodiscard]] inline int FindTriggeredNumberKey(const InputSystem& inputSystem,int count)
  {
    // 先頭のキーから順に調べ、押されたキーの添字を記録する。複数あれば、後のキーが残る。
    int triggered_index = -1;
    for(int item_index = 0; item_index < count; ++item_index){
      if(inputSystem.IsKeyTriggered(static_cast<unsigned char>(ns_UI_FIRST_KEY + item_index))){
        triggered_index = item_index;
      }
    }
    return triggered_index;
  }
}
