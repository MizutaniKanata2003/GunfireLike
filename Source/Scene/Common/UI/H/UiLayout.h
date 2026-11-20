#pragma once

// 画像で描くUIの配置を、画像のパスと、基準Canvas上の位置・大きさの表として定義する。
// 配置の座標は、基準Canvas（幅1280・高さ720）上の値。描画・画像の読込・状態の判断は担当しない。

//========= Framework インクルード=========
#include "Framework/Etc/H/MathTypes.h"

// 画像1枚分のUIの配置。
struct UI_IMAGE_LAYOUT
{
	// 画像ファイルのパス。作業ディレクトリ基準。静的な文字列定数を指し、所有しない。
	const wchar_t* texturePath;
	// 画像の左上の位置。基準Canvas上の座標で、右向きX・下向きY。
	XMFLOAT2 position;
	// 画像を描く幅と高さ。基準Canvas上の大きさ。
	XMFLOAT2 size;
};

// ゲーム中のHUDの画像の配置をまとめる。画像は、Assets/Textures/UI/へ置く。
namespace HUD_LAYOUT
{
	//========= 照準=========
	// 画面中央（640,360）に中心が来る64×64の照準。構えている間は、中心を保ったまま小さく描く。
	inline constexpr UI_IMAGE_LAYOUT ns_CROSSHAIR{L"Assets/Textures/UI/Crosshair.png",{608.0f,328.0f},{64.0f,64.0f}};

	//========= HPバー=========
	// 左下のHPバーの枠。320×32。
	inline constexpr UI_IMAGE_LAYOUT ns_HP_FRAME{L"Assets/Textures/UI/HpFrame.png",{36.0f,654.0f},{320.0f,32.0f}};
	// HPバーの中身。枠の内側（上下左右に4の余白）へ収まる312×24。HPの割合に応じて左から表示する。色は描画時に決める。
	inline constexpr UI_IMAGE_LAYOUT ns_HP_FILL{L"Assets/Textures/UI/HpFill.png",{40.0f,658.0f},{312.0f,24.0f}};

	//========= 弾薬=========
	// 右下の弾薬表示の枠。200×64。左側に弾のアイコンがあり、数字は右側に表示する。
	inline constexpr UI_IMAGE_LAYOUT ns_AMMO_FRAME{L"Assets/Textures/UI/AmmoFrame.png",{1044.0f,626.0f},{200.0f,64.0f}};

	//========= 通貨=========
	// 右上の通貨のアイコン。48×48。右端の余白は、HPバーの左端の余白（36）と同じ。所持額の数字は、アイコンの左隣に描く。
	inline constexpr UI_IMAGE_LAYOUT ns_CURRENCY_ICON{L"Assets/Textures/UI/CoinIcon.png",{1196.0f,24.0f},{48.0f,48.0f}};

	//========= 低HP警告=========
	// 画面全体を覆う、周辺が赤くなる警告。1280×720。
	inline constexpr UI_IMAGE_LAYOUT ns_LOW_HP_WARNING{L"Assets/Textures/UI/LowHpWarning.png",{0.0f,0.0f},{1280.0f,720.0f}};
}