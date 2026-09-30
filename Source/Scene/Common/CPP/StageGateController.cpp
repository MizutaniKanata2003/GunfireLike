#include "../H/StageGateController.h"

//========= C++標準ライブラリ インクルード=========
#include <algorithm>

// 現在StageとStage Clear状態から前後Gateの利用可否を返す。
StageGateAvailability StageGateController::GetAvailability( int currentStage, int firstStage, int maxStage, bool isCurrentStageCleared ) const
{
	StageGateAvailability availability {};

	availability.isPreviousGateAvailable = currentStage > firstStage;

	availability.isNextGateAvailable = currentStage < maxStage && isCurrentStageCleared;

	return availability;
}

// Player位置とGate状態から、優先順位に従って使用するGateを返す。
StageGateUseResult StageGateController::GetUseResult( const DirectX::XMFLOAT3& playerPosition, const StageGateAvailability& availability,
													  const StageGate& previousStageGate, const StageGate& nextStageGate,
													  const StageGate& shopGate, float interactionRadiusSquared ) const
{
	const float safeInteractionRadiusSquared = std::max( 0.0f, interactionRadiusSquared );

	if ( availability.isPreviousGateAvailable && previousStageGate.IsPlayerNear( playerPosition, safeInteractionRadiusSquared ) )
	{
		return StageGateUseResult::e_PREVIOUS_STAGE;
	}

	if ( availability.isNextGateAvailable && nextStageGate.IsPlayerNear( playerPosition, safeInteractionRadiusSquared ) )
	{
		return StageGateUseResult::e_NEXT_STAGE;
	}

	if ( shopGate.IsPlayerNear( playerPosition, safeInteractionRadiusSquared ) )
	{
		return StageGateUseResult::e_SHOP;
	}

	return StageGateUseResult::e_NONE;
}