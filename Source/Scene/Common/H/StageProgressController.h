#pragma once

// Enemy撃破後のStage進行ルールが返す結果。
struct StageClearResult
{
	bool shouldMarkStageCleared{};
	bool shouldAddStageClearReward{};
	bool shouldEnterStageClear{};
	bool shouldRequestResult{};
};

// Stage ClearとResult移行に関するルールを判定する。
// GameProgress、SceneManager、AudioSystemは所有しない。
class StageProgressController final
{
public:
	// Enemy撃破後のStage進行結果を返す。
	// 現在Stageが未ClearならClear記録とRewardを要求する。
	// 最終StageならResult、非最終StageならStage Clearへ進める。
	[[nodiscard]] StageClearResult EvaluateEnemyDefeat( bool isCurrentStageAlreadyCleared, int currentStage, int maxStageCount ) const;
};