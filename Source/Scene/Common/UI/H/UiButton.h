#pragma once

// 画面上の矩形のボタン1つの、当たり・ホバー・クリックの判定を担当する。
// 画像の描画・クリックの結果の処理は担当しない。位置と大きさ・カーソルの位置は、基準Canvas（幅1280・高さ720）の座標で扱う。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// 矩形のボタン。カーソルの位置から、ホバーとクリックを判定する。
class UiButton final
{
public:
	//========= 初期化関数=========
	// ボタンの矩形を設定し、ホバーを解除する。
	// position: 矩形の左上の位置。基準Canvas上の座標で、右向きX・下向きY。
	// size: 矩形の幅と高さ。基準Canvas上の大きさ。どちらかが0以下なら、どこにもホバーしない。
	void Initialize(const XMFLOAT2& position,const XMFLOAT2& size)
	{
		m_position = position;
		m_size = size;
		m_isHovered = false;
	}

	//========= 更新関数=========
	// カーソルが矩形の中にあるかを調べて、ホバーの状態を更新し、クリックされたかを返す。
	// cursorPosition: カーソルの位置。基準Canvas上の座標。矩形の上端と左端は内側、下端と右端は外側として扱う。
	// isClickTriggered: クリックが、この更新で押されたか。
	// 戻り値: カーソルが矩形の中にあり、かつクリックされたらtrue。
	[[nodiscard]] bool Update(const XMFLOAT2& cursorPosition,bool isClickTriggered)
	{
		// 矩形の内側の条件を、左・上・右・下の順に調べる。大きさが0以下の場合は、条件が成り立たない。
		m_isHovered = cursorPosition.x >= m_position.x && cursorPosition.x < m_position.x + m_size.x &&
			cursorPosition.y >= m_position.y && cursorPosition.y < m_position.y + m_size.y;
		return m_isHovered && isClickTriggered;
	}
	// ホバーを解除する。ボタンを使わなくなった時に呼ぶ。
	void ClearHover() { m_isHovered = false; }

	//========= Getter関数=========
	// 直近の更新で、カーソルが矩形の中にあったかを返す。
	[[nodiscard]] bool IsHovered() const { return m_isHovered; }

private:
	//========= 矩形=========
	// 矩形の左上の位置。基準Canvas上の座標。
	XMFLOAT2 m_position{};
	// 矩形の幅と高さ。基準Canvas上の大きさ。
	XMFLOAT2 m_size{};

	//========= 状態=========
	// 直近の更新で、カーソルが矩形の中にあったか。
	bool m_isHovered{};
};