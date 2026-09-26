#include "../H/GameHud.h"

//========= C++標準ライブラリ インクルード=========
#include <cwchar>

//========= DirectX インクルード=========
#include <DirectXColors.h>

//========= Framework インクルード=========
#include "Framework/DirectX/H/GraphicsSystem.h"

namespace
{
	//========= HUD定数=========
	// Gate操作案内の表示位置と文字Scale。
	constexpr float HUD_GATE_HINT_X = 440.0f;
	constexpr float HUD_GATE_HINT_Y = 590.0f;
	constexpr float HUD_MESSAGE_SCALE = 0.85f;

	// Tutorial操作説明の表示位置と文字Scale。
	constexpr float HUD_TUTORIAL_X = 40.0f;
	constexpr float HUD_TUTORIAL_Y = 80.0f;
	constexpr float HUD_TUTORIAL_LINE_HEIGHT = 28.0f;
	constexpr float HUD_TUTORIAL_SCALE = 0.65f;

	// HUD文字の各表示位置とScale。
	constexpr float HUD_STAGE_TEXT_X = 520.0f;
	constexpr float HUD_STAGE_TEXT_Y = 24.0f;
	constexpr float HUD_CURRENCY_TEXT_X = 1040.0f;
	constexpr float HUD_CURRENCY_TEXT_Y = 24.0f;
	constexpr float HUD_HP_TEXT_X = 40.0f;
	constexpr float HUD_HP_TEXT_Y = 612.0f;
	constexpr float HUD_SPECIAL_ATTACK_TEXT_X = 1040.0f;
	constexpr float HUD_SPECIAL_ATTACK_TEXT_Y = 640.0f;
	constexpr float HUD_COOLDOWN_TEXT_X = 1040.0f;
	constexpr float HUD_COOLDOWN_TEXT_Y = 670.0f;
	constexpr float HUD_TEXT_SCALE = 0.75f;
	constexpr float HUD_COOLDOWN_TEXT_SCALE = 0.65f;
}

// GameSceneの画面固定HUDをScreen UI Passで描画する。
void GameHud::Draw(
HudRenderer& hudRenderer,
HudTextRenderer& hudTextRenderer,
GraphicsSystem& graphicsSystem,
const GameHudState& hudState ) const
{
	graphicsSystem.SetRenderPass( e_RenderPass::e_SCREEN_UI );

	hudRenderer.DrawCrosshair( graphicsSystem );
	hudRenderer.DrawPlayerHealthBar(
	graphicsSystem,
	hudState.playerCurrentHp,
	hudState.playerMaxHp );
	hudRenderer.DrawLowHealthWarning(
	graphicsSystem,
	hudState.playerCurrentHp,
	hudState.playerMaxHp );

	hudTextRenderer.Begin();

	wchar_t stageText[ 64 ]{};
	wchar_t currencyText[ 64 ]{};
	wchar_t healthText[ 64 ]{};

	swprintf_s(
	stageText,
	L"ステージ %d / %d",
	hudState.currentStage,
	hudState.maxStageCount );

	swprintf_s(
	currencyText,
	L"所持金: %d G",
	hudState.currency );

	swprintf_s(
	healthText,
	L"HP: %.0f / %.0f",
	hudState.playerCurrentHp,
	hudState.playerMaxHp );

	hudTextRenderer.DrawText(
	stageText,
	DirectX::XMFLOAT2{
	HUD_STAGE_TEXT_X,
	HUD_STAGE_TEXT_Y },
	DirectX::Colors::White,
	0.90f );

	hudTextRenderer.DrawText(
	currencyText,
	DirectX::XMFLOAT2{
	HUD_CURRENCY_TEXT_X,
	HUD_CURRENCY_TEXT_Y },
	DirectX::Colors::Gold,
	HUD_TEXT_SCALE );

	hudTextRenderer.DrawText(
	healthText,
	DirectX::XMFLOAT2{
	HUD_HP_TEXT_X,
	HUD_HP_TEXT_Y },
	DirectX::Colors::White,
	HUD_TEXT_SCALE );

	const wchar_t* specialAttackText = L"";
	DirectX::XMVECTORF32 specialAttackColor =
		DirectX::Colors::Yellow;

	if ( !hudState.isSpecialAttackUnlocked )
	{
		specialAttackText = L"Q: 未解放";
		specialAttackColor = DirectX::Colors::Yellow;
	}
	else if ( hudState.isSpecialAttackReady )
	{
		specialAttackText = L"Q: 使用可能";
		specialAttackColor = DirectX::Colors::Lime;
	}
	else
	{
		specialAttackText = L"Q: 待機中";
		specialAttackColor = DirectX::Colors::Orange;
	}

	hudTextRenderer.DrawText(
	specialAttackText,
	DirectX::XMFLOAT2{
	HUD_SPECIAL_ATTACK_TEXT_X,
	HUD_SPECIAL_ATTACK_TEXT_Y },
	specialAttackColor,
	HUD_TEXT_SCALE );

	if ( hudState.isSpecialAttackUnlocked &&
	!hudState.isSpecialAttackReady )
	{
		wchar_t cooldownText[ 64 ]{};

		swprintf_s(
		cooldownText,
		L"残り %.1f 秒",
		hudState.specialAttackCooldownTimer );

		hudTextRenderer.DrawText(
		cooldownText,
		DirectX::XMFLOAT2{
		HUD_COOLDOWN_TEXT_X,
		HUD_COOLDOWN_TEXT_Y },
		DirectX::Colors::White,
		HUD_COOLDOWN_TEXT_SCALE );
	}

	if ( hudState.isNearNextGate )
	{
		hudTextRenderer.DrawText(
		L"E: 次のステージへ",
		DirectX::XMFLOAT2{
		HUD_GATE_HINT_X,
		HUD_GATE_HINT_Y },
		DirectX::Colors::Cyan,
		HUD_MESSAGE_SCALE );
	}
	else if ( hudState.isNearPreviousGate )
	{
		hudTextRenderer.DrawText(
		L"E: 前のステージへ",
		DirectX::XMFLOAT2{
		HUD_GATE_HINT_X,
		HUD_GATE_HINT_Y },
		DirectX::Colors::Violet,
		HUD_MESSAGE_SCALE );
	}
	else if ( hudState.isNearShopGate )
	{
		hudTextRenderer.DrawText(
		L"E: ショップへ",
		DirectX::XMFLOAT2{
		HUD_GATE_HINT_X,
		HUD_GATE_HINT_Y },
		DirectX::Colors::Gold,
		HUD_MESSAGE_SCALE );
	}

	if ( hudState.showTutorial )
	{
		hudTextRenderer.DrawText(
		L"WASD: 移動",
		DirectX::XMFLOAT2{
		HUD_TUTORIAL_X,
		HUD_TUTORIAL_Y },
		DirectX::Colors::White,
		HUD_TUTORIAL_SCALE );

		hudTextRenderer.DrawText(
		L"マウス: 視点移動",
		DirectX::XMFLOAT2{
		HUD_TUTORIAL_X,
		HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT },
		DirectX::Colors::White,
		HUD_TUTORIAL_SCALE );

		hudTextRenderer.DrawText(
		L"左クリック: 射撃",
		DirectX::XMFLOAT2{
		HUD_TUTORIAL_X,
		HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT * 2.0f },
		DirectX::Colors::White,
		HUD_TUTORIAL_SCALE );

		hudTextRenderer.DrawText(
		L"Q: 範囲攻撃",
		DirectX::XMFLOAT2{
		HUD_TUTORIAL_X,
		HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT * 3.0f },
		DirectX::Colors::White,
		HUD_TUTORIAL_SCALE );

		hudTextRenderer.DrawText(
		L"ゲートに近づいてEキー",
		DirectX::XMFLOAT2{
		HUD_TUTORIAL_X,
		HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT * 4.0f },
		DirectX::Colors::White,
		HUD_TUTORIAL_SCALE );

		hudTextRenderer.DrawText(
		L"死亡時: 所持金の25%を失う",
		DirectX::XMFLOAT2{
		HUD_TUTORIAL_X,
		HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT * 5.0f },
		DirectX::Colors::Orange,
		HUD_TUTORIAL_SCALE );

		if ( hudState.showSpecialAttackUnlockHint )
		{
			hudTextRenderer.DrawText(
			L"Qは未解放です - ショップで解放できます",
			DirectX::XMFLOAT2{
			HUD_TUTORIAL_X,
			HUD_TUTORIAL_Y + HUD_TUTORIAL_LINE_HEIGHT * 6.0f },
			DirectX::Colors::Yellow,
			HUD_TUTORIAL_SCALE );
		}
	}

	if ( hudState.showStageClearMessage )
	{
		hudTextRenderer.DrawText(
		L"ステージクリア！ 青いゲートへ",
		DirectX::XMFLOAT2{ 420.0f, 90.0f },
		DirectX::Colors::Lime,
		0.90f );
	}

	hudTextRenderer.End();

	graphicsSystem.SetRenderPass( e_RenderPass::e_OPAQUE );
}