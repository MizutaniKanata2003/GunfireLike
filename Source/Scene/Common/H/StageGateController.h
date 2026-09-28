#pragma once

//========= DirectX インクルード=========
#include <DirectXMath.h>

//========= Scene インクルード=========
#include "Scene/Common/H/StageGate.h"

// Gate使用判定の結果。
enum class StageGateUseResult
{
	e_NONE,
	e_PREVIOUS_STAGE,
	e_NEXT_STAGE,
	e_SHOP
};

// 現在StageにおけるGateの表示・使用可否。
struct StageGateAvailability
{
	bool isPreviousGateAvailable{};
	bool isNextGateAvailable{};
};

// Stage番号、Stage Clear状態、Player位置からGateの利用可否を判定する。
// Scene遷移、SE、GameProgressの変更は行わない。
class StageGateController final
{
public:
	//========= Gate状態取得=========
	// 現在StageとStage Clear状態から前後Gateの利用可否を返す。
	[[nodiscard]] StageGateAvailability GetAvailability( int currentStage, int firstStage, int maxStage, bool isCurrentStageCleared ) const;

	//========= Gate使用判定=========
	// Player位置とGate状態から、優先順位に従って使用するGateを返す。
	[[nodiscard]] StageGateUseResult GetUseResult(
	const DirectX::XMFLOAT3& playerPosition, const StageGateAvailability& availability, const StageGate& previousStageGate,
	const StageGate& nextStageGate, const StageGate& shopGate, float interactionRadiusSquared ) const;
};