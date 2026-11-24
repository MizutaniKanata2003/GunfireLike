#pragma once

// 背景・見出し・ボタン1つの画像と、操作の案内の文字で構成する、ボタン1つのメニュー画面の、描画Resource・ボタンの判定・描画を担当する。
// ButtonMenuFlowが、メンバーとして所有して使う。Sceneの遷移は、使う側のSceneが担当する。マウスキャプチャ・BGMの設定とEnterの判定は、ButtonMenuFlowが担当する。

//========= Framework インクルード=========
#include "Framework/2D/H/HudSpriteRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

//========= Scene インクルード=========
#include "Scene/Common/UI/H/UiButton.h"
#include "Scene/Common/UI/H/UiLayout.h"

//========= 前方宣言=========
class GraphicsSystem;
class InputSystem;

// ボタン1つのメニュー画面の構成。画像の配置と文字列は、使う側のSceneが、表として定義する。
struct BUTTON_MENU_CONFIG
{
	//========= 名前=========
	// 初期化の失敗をログへ記録する時に、メッセージの先頭へ付ける、Sceneの名前。静的な文字列定数を指し、所有しない。
	const wchar_t* sceneName;

	//========= 画像=========
	// 背景の画像の配置。
	UI_IMAGE_LAYOUT background;
	// 見出し（ロゴ）の画像の配置。
	UI_IMAGE_LAYOUT logo;
	// ボタンの画像（通常）の配置。ボタンの当たりも、この位置と大きさにする。
	UI_IMAGE_LAYOUT button;
	// ボタンの画像（カーソルが乗っている時）のパス。位置と大きさは、通常と同じ。静的な文字列定数を指し、所有しない。
	const wchar_t* buttonHoverPath;

	//========= 文字=========
	// ボタンの下へ描く、操作の案内の文字列。静的な文字列定数を指し、所有しない。
	const wchar_t* guideText;
};

// ボタン1つのメニュー画面の、画像・文字の描画Resourceと、ボタンの当たりを所有するクラス。
class ButtonMenuView final
{
public:
	//========= 初期化・終了関数=========
	// 画像・文字の描画を初期化し、配置表の画像を読み込み、ボタンの矩形を設定する。
	// graphicsSystem: Resource生成に使う描画基盤。Uninitまで有効であること。
	// config: 画面の構成。値をコピーして保持する。文字列は、静的な文字列定数であること。
	// 戻り値: 画像・文字の描画の初期化に成功すればtrue。失敗した場合は、失敗した対象をログへ記録し、部分Resourceを解放してfalse。
	// 画像の読込失敗は、その画像だけを描かず、全体の失敗にはしない。
	[[nodiscard]] bool Initialize(GraphicsSystem& graphicsSystem,const BUTTON_MENU_CONFIG& config);
	// 画像・文字の描画Resourceを解放し、ボタンのホバーを解除する。未初期化・終了済みでも呼出可能。
	void Uninit();

	//========= 更新・描画関数=========
	// カーソルの位置から、ボタンのホバーを更新し、クリックされたかを返す。
	// inputSystem: カーソルの位置とクリックの取得元。
	// graphicsSystem: カーソルの座標変換に使う、描画先の大きさの取得元。
	// 戻り値: カーソルがボタンの上にあり、かつ、この更新でクリックされたらtrue。
	[[nodiscard]] bool UpdateButton(const InputSystem& inputSystem,const GraphicsSystem& graphicsSystem);
	// UI用Render Passで、背景・見出し・ボタンの画像と、操作の案内の文字を描く。描画先の幅または高さが0なら何もしない。
	// 描画後はOPAQUE Passへ設定する。呼出前のPassを保存・復元する処理ではない。
	// graphicsSystem: 描画基盤。
	void Draw(GraphicsSystem& graphicsSystem);

private:
	//========= 構成=========
	// Initializeで受け取った画面の構成。
	BUTTON_MENU_CONFIG m_config{};

	//========= 描画Resource=========
	// 画像の描画Resourceを所有する。Uninitで明示的に解放する。
	HudSpriteRenderer m_spriteRenderer{};
	// 操作の案内の文字の描画Resourceを所有する。Uninitで明示的に解放する。
	HudTextRenderer m_textRenderer{};
	// 背景の画像の識別値。読込失敗や未初期化は0で、0の画像は描かない。
	HudTextureHandle m_backgroundTexture{};
	// 見出しの画像の識別値。0なら描かない。
	HudTextureHandle m_logoTexture{};
	// ボタンの画像（通常）の識別値。0なら描かない。
	HudTextureHandle m_buttonTexture{};
	// ボタンの画像（カーソルが乗っている時）の識別値。0なら描かない。
	HudTextureHandle m_buttonHoverTexture{};

	//========= ボタン=========
	// ボタンの当たりとホバーの判定。
	UiButton m_button{};
};