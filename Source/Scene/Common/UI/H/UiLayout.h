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

// 報酬選択の画像の配置をまとめる。画像は、Assets/Textures/UI/へ置く。
namespace REWARD_LAYOUT
{
	//========= カードの数=========
	// 画面に並べる報酬のカードの数。RewardControllerの報酬の表（REWARD_TABLE）の数と同じにする。
	inline constexpr int ns_CARD_COUNT = 3;

	//========= 背景=========
	// 報酬選択の間、画面全体を暗くする半透明の背景。1280×720。ショップも、同じ画像を使う。
	inline constexpr UI_IMAGE_LAYOUT ns_BACKDROP{L"Assets/Textures/UI/RewardBackdrop.png",{0.0f,0.0f},{1280.0f,720.0f}};

	//========= カード=========
	// カードの枠の画像（通常）。360×480。3枚とも同じ画像を使う。
	inline constexpr const wchar_t* ns_CARD_FRAME_PATH = L"Assets/Textures/UI/RewardCard.png";
	// カードの枠の画像（カーソルが乗っている時）。360×480。
	inline constexpr const wchar_t* ns_CARD_FRAME_HOVER_PATH = L"Assets/Textures/UI/RewardCardHover.png";
	// カード1枚の幅と高さ。基準Canvas上の大きさ。
	inline constexpr XMFLOAT2 ns_CARD_SIZE{360.0f,480.0f};
	// カードの左上の位置。左から順に並べ、カードの間は40。左右の余白は60ずつで、3枚が画面の中央に収まる。上下は、見出しと案内の間へ置く。
	inline constexpr XMFLOAT2 ns_CARD_POSITIONS[ns_CARD_COUNT]{{60.0f,150.0f},{460.0f,150.0f},{860.0f,150.0f}};
}

// ショップ（武器と弾薬の購入・武器の売却・武器の強化の画面）の画像の配置をまとめる。画像は、Assets/Textures/UI/へ置く。
namespace SHOP_LAYOUT
{
	//========= 行とボタンの数=========
	// 武器の行の数。武器の種類の数（SHOP_TABLE）と同じにする。
	inline constexpr int ns_ROW_COUNT = 3;
	// 強化の行の数。強化の項目の数（SHOP_TABLE）と同じにする。
	inline constexpr int ns_UPGRADE_COUNT = 5;
	// ボタンの数。武器、弾薬の購入、強化、ページ切替、閉じるボタン。
	inline constexpr int ns_BUTTON_COUNT = ns_ROW_COUNT * 2 + ns_UPGRADE_COUNT + 2;

	//========= パネル=========
	// 画面中央（640,360）に中心が来る、960×540のパネルの枠。行の枠は、描かない（2つのページで、行の並びが違うため）。
	inline constexpr UI_IMAGE_LAYOUT ns_PANEL{L"Assets/Textures/UI/ShopPanel.png",{160.0f,90.0f},{960.0f,540.0f}};

	//========= 行=========
	// 売買のページで、最初の行の上端のY座標。パネルの上端（90）から、見出しの領域の分だけ下げた位置。基準Canvas上の座標。
	inline constexpr float ns_ROW_TOP = 190.0f;
	// 売買のページの、行の高さと、行の間隔。基準Canvas上の大きさ。行は、隙間なく並ぶ。
	inline constexpr float ns_ROW_HEIGHT = 96.0f;
	// 強化のページで、最初の行の上端のY座標。5行を並べるため、売買のページより少し上から始める。基準Canvas上の座標。
	inline constexpr float ns_UPGRADE_ROW_TOP = 186.0f;
	// 強化のページの、行の間隔。基準Canvas上の大きさ。
	inline constexpr float ns_UPGRADE_ROW_STEP = 68.0f;
	// 強化のページの、行の高さ。ボタンの高さと同じ。基準Canvas上の大きさ。
	inline constexpr float ns_UPGRADE_ROW_HEIGHT = 64.0f;

	//========= ボタン=========
	// ボタンの画像（通常）。240×64。
	inline constexpr const wchar_t* ns_BUTTON_PATH = L"Assets/Textures/UI/ShopButton.png";
	// ボタンの画像（カーソルが乗っている時）。240×64。
	inline constexpr const wchar_t* ns_BUTTON_HOVER_PATH = L"Assets/Textures/UI/ShopButtonHover.png";
	// ボタンの画像（押せない時。所持済み・上限・通貨不足など）。240×64。
	inline constexpr const wchar_t* ns_BUTTON_DISABLED_PATH = L"Assets/Textures/UI/ShopButtonDisabled.png";
	// ボタン1つの幅と高さ。基準Canvas上の大きさ。
	inline constexpr XMFLOAT2 ns_BUTTON_SIZE{240.0f,64.0f};
	// ボタンの左上の位置。並びは、武器の購入と売却3つ（売買のページ。行の中ほど）、弾薬の購入3つ（売買のページ。行の右端）、強化5つ（強化のページ。右端）、
	// ページ切替（左下）、閉じるボタン（下側の中央）の順。購入のボタンは、行の中央に揃える（行の上端から16下）。右端のボタンの右端は1080で、パネルの右端（1120）から40の余白を残す。
	inline constexpr XMFLOAT2 ns_BUTTON_POSITIONS[ns_BUTTON_COUNT]{
		{580.0f,206.0f},{580.0f,302.0f},{580.0f,398.0f},
		{840.0f,206.0f},{840.0f,302.0f},{840.0f,398.0f},
		{840.0f,186.0f},{840.0f,254.0f},{840.0f,322.0f},{840.0f,390.0f},{840.0f,458.0f},
		{200.0f,546.0f},
		{520.0f,546.0f}
	};
}