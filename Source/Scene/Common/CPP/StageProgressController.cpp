#include "../H/StageProgressController.h"

// Enemy撃破後のStage進行結果を返す。
StageClearResult StageProgressController::EvaluateEnemyDefeat( bool isCurrentStageAlreadyCleared, int currentStage, int maxStageCount ) const
{
	StageClearResult result {};

	if ( isCurrentStageAlreadyCleared ) { return result; }

	result.shouldMarkStageCleared = true;
	result.shouldAddStageClearReward = true;

	if ( currentStage >= maxStageCount )
	{
		result.shouldRequestResult = true;

		return result;
	}

	result.shouldEnterStageClear = true;

	return result;
}