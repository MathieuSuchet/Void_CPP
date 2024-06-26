#pragma once

#include <Utils/DoubleTapUtils.h>
#include "Rewards/DoubleTap/DoubleTapReward.h"
#include "LoggedCombinedReward.h"

START_DT_NS

class GroundDoubleTapReward : public UseDTReward {
public:
	struct BallZoning {
		//Distance from back wall where you are consider in the backboard zone
		float distFromBackboard = 500.0f;

		//Minimum height to be considered in the backboard zone
		float minHeight = RLGSC::CommonValues::GOAL_HEIGHT;
	};

	struct BallHandling {
		//Weight for similarity towards zone, ball vel
		float ballTowardsZoneW = 2.0f;

		//Distance ball - zone reduction
		float distToZoneReduction = 100.0f;

		//Touch weight
		float touchW = 30.0;

		//Ball height weight
		float ballHeightW = 3.0;
	};

	struct GroundDTArgs {
		BallZoning ballZoning;
		BallHandling ballHandling;
	};

	GroundDoubleTapReward(GroundDTArgs config, DoubleTapReward::DoubleTapArgs dtConfig) : config(config), UseDTReward(dtConfig) {};

	virtual float GetReward(const RLGSC::PlayerData& player, const RLGSC::GameState& state, const RLGSC::Action& prevAction);
	virtual void ClearChanges() override;
private:
	GroundDTArgs config;
};
END_DT_NS