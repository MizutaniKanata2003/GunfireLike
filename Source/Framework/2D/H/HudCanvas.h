#pragma once

// 1280×720のVirtual CanvasをRender Target内へ収める座標・サイズ・文字倍率の変換を提供する。
// 縦横比を保った均一倍率と中央寄せOffsetを保持する。描画やGPU Resourceの所有は担当しない。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// HUDの基準座標と現在の描画サイズの間を変換する。
// Render Targetのサイズ変更後は、新しいサイズを渡して作り直す。
class HudCanvas final
{
public:
	//========= Canvas定数=========
	// HUDとScene UIが使用する基準幅。既存の公開定数名との互換性のため名称を維持する。
	static constexpr float VIRTUAL_WIDTH = 1280.0f;
	// HUDとScene UIが使用する基準高さ。既存の公開定数名との互換性のため名称を維持する。
	static constexpr float VIRTUAL_HEIGHT = 720.0f;

	//========= 生成関数=========
	// Render Target内に基準領域全体が収まる均一倍率と、中央寄せOffsetを計算する。
	// renderWidth: Render Targetの幅。単位はpixel。
	// renderHeight: Render Targetの高さ。単位はpixel。
	// どちらかが0の場合は倍率とOffsetを0のままにし、各変換結果も0になる。
	HudCanvas(unsigned int renderWidth,unsigned int renderHeight);

	//========= 座標変換関数=========
	// 基準座標へ倍率と中央寄せOffsetを反映し、Render Target座標を返す。
	// virtualPosition: 左上を原点とし、右向きX・下向きYで表すVirtual Canvas座標。範囲外も制限しない。
	[[nodiscard]] XMFLOAT2 ToScreenPosition(const XMFLOAT2& virtualPosition) const;
	// 基準サイズへ倍率だけを反映し、Render Target上のサイズを返す。
	// virtualSize: Virtual Canvas基準の幅と高さ。負値や範囲外も制限しない。
	[[nodiscard]] XMFLOAT2 ToScreenSize(const XMFLOAT2& virtualSize) const;
	// 基準倍率にCanvasの倍率を掛け、Render Target用の文字倍率を返す。
	// virtualScale: Virtual Canvas基準の文字倍率。0以下も制限せず計算する。
	[[nodiscard]] float ToScreenScale(float virtualScale) const;

	//========= Getter関数=========
	// 基準領域全体をRender Target内へ収める均一倍率を返す。サイズ無効時は0。
	[[nodiscard]] float GetScale() const { return m_scale; }
	// 中央寄せOffsetをpixel単位で返す。参照の有効期間はこのCanvasの寿命まで。
	[[nodiscard]] const XMFLOAT2& GetOffset() const { return m_offset; }

private:
	//========= Canvas変換状態=========
	// 座標・サイズ・文字倍率の変換へ適用する、縦横共通の倍率。単位なし。
	float m_scale{};
	// 描画先の余白を左右・上下へ均等に分けるための移動量。単位はpixel。
	XMFLOAT2 m_offset{};
};