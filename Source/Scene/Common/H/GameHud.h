#pragma once

//========= Framework インクルード=========
#include "Framework/2D/H/HudRenderer.h"
#include "Framework/2D/H/HudTextRenderer.h"

//========= 前方宣言=========
class GraphicsSystem;

// GameSceneのHUD表示に必要な読み取り専用状態。
struct GameHudState
{
	int currentStage{};
	int maxStageCount{};
	int currency{};

	float playerCurrentHp{};
	float playerMaxHp{};

	bool isSpecialAttackUnlocked{};
	bool isSpecialAttackReady{};
	float specialAttackCooldownTimer{};

	bool isNearPreviousGate{};
	bool isNearNextGate{};
	bool isNearShopGate{};

	bool showTutorial{};
	bool showSpecialAttackUnlockHint{};
	bool showStageClearMessage{};
};

// Crosshair、Player HP、Stage、通貨、操作案内、Tutorialを描画する。
class GameHud final
{
public:
	//========= 描画関数=========
	// GameSceneの画面固定HUDをScreen UI Passで描画する。
	void Draw( HudRenderer& hudRenderer, HudTextRenderer& hudTextRenderer, GraphicsSystem& graphicsSystem, const GameHudState& hudState ) const;
};