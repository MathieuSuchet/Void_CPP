#pragma once
#include <RLGymSim_CPP/Utils/RewardFunctions/RewardFunction.h>
#include "LoggedCombinedReward.h"
#include "../RLGymPPO_CPP/libsrc/json/nlohmann/json.hpp"

using json = nlohmann::json;

class PinchReward : public LoggableReward {
public:


	//All the ball handling stuff
	struct BallHandling {
		//Acceleration weight
		float ballVelW = 1.0f;

		//Touch weight
		float touchW = 1.0f;

		//Minimum similarity to match goal direction
		float goalDirectionSimilarity = 0.8f;

		//Reward for going towards the net
		float goalDirectionW = 3.0f;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(BallHandling, ballVelW, touchW, goalDirectionSimilarity, goalDirectionW)
	};

	struct PinchArgs : public RewardConfig {
		BallHandling ballHandling = {}; 

		// Inherited via RewardConfig
		void to_json(json& j) override;

	};

	PinchReward(
		PinchArgs* args);

	virtual void Reset(const RLGSC::GameState& initialState);
	virtual float GetReward(const RLGSC::PlayerData& player, const RLGSC::GameState& state, const RLGSC::Action& prevAction);
	virtual void ClearChanges() override;

	virtual RewardConfig* GetConfig() { return config; };



private:
	PinchArgs* config;
	float lastBallSpeed = 0;
};


class PinchWallSetupReward : public LoggableReward {
public:
	//All the wall handling
	struct WallHandling {
		//Minimum height where hit is considered as pinch
		float wallMinHeightToPinch = 150;
	};

	//All the similarity stuff
	struct SimilarityWallSetup {
		//Reward for respecting ball agent parallel
		float similarityBallAgentReward = 0.2f;

		//Threshold for ball agent parallel
		float similarityBallAgentThresh = 0.8f;

		//Threshold for ball wall parallel
		float similarityBallWallThresh = 0.8f;
	};

	//All the ball handling stuff
	struct BallWallSetupHandling {
		//Distance to ball reduction, the lower it is, the more distance to ball is punished
		float ballDistReduction = 1000.0f;

		//Ball agent speed match
		float speedMatchW = 1.0f;

		//Distance between agent and ball where the agent receives a reward
		float agentDistToBallThresh = 550.0f;

		//Allowed offset off the ball on the x axis
		float ballOffsetX = 150.0f;

		//Allowed offset off the ball on the y axis
		float ballOffsetY = 150.0f;

		//Reward for being behind the ball
		float behindTheBallReward = 0.01f;
	};

	//All the ground handling stuff
	struct GroundWallSetupHandling {
		//Punishment for being on ground in ground ban zone
		float groundBanPunishment = 0.1f;

		//Reward for not being on ground in ground ban zone
		float groundBanReward = -0.1f;

		//Reward for being in creeping distance
		float creepingDistanceReward = 0.01f;
	};

	//All the distances
	struct DistancesWallSetup {
		//Distance for pinch setup
		float creepingDistance = 2000.0f;

		//Distance where you should be jumping
		float groundBanDistance = 1000.0f;

		//Distance where the reward will trigger
		float maxDistToTrigger = 4000.0f;
	};

	//All the flip handling
	struct FlipHandlingWallSetup {
		//Reward for keeping the flip in creeping distance
		float hasFlipReward = 1.0f;

		//Punishment for using the flip in creeping distance
		float hasFlipPunishment = -1.0f;

		//Distance to ball min to be considered "on the ball" (outer ball)
		float maxDistance = 100;

		//Reward for flipping on the ball
		float hasFlipRewardWhenBall = 20.0f;

		//Punishment for keeping the flip on the ball
		float hasFlipPunishmentWhenBall = -20.0f;
	};

	struct PinchWallSetupArgs : public RewardConfig{
		DistancesWallSetup distancesWallSetup = {};
		FlipHandlingWallSetup flipHandlingWallSetup = {};
		SimilarityWallSetup similarityWallSetup = {};
		GroundWallSetupHandling groundWallSetupHandling = {};
		BallWallSetupHandling ballWallHandling = {};
		WallHandling wallHandling = {};
		
		PinchReward::PinchArgs pinchRewardConfig = {};
	};

	PinchWallSetupReward(
		PinchWallSetupArgs* args
	) : config(args), pinchReward(PinchReward(&args->pinchRewardConfig)) {};

	virtual void Reset(const RLGSC::GameState& initialState);
	virtual float GetReward(const RLGSC::PlayerData& player, const RLGSC::GameState& state, const RLGSC::Action& prevAction);
	virtual void ClearChanges() override;
	virtual void Log(RLGPC::Report& report, std::string name, float weight = 1.0f) override;

	virtual RewardConfig* GetConfig() { return config; };

private:
	PinchWallSetupArgs* config;
	PinchReward pinchReward;

	float Corner(float x, short xOrientation, short yOrientation);
	Vec GetCornerIntersection(short xFwd, short yFwd, float xPos, float yPos);
};


class PinchCeilingSetupReward : public LoggableReward {
public:

	struct BallGroundHandling {
		// Distance from where the bot is considered too far to be behind the ball
		float agentDistToBallThresh = 550.0f;

		//Ball distance punishment reduction
		float ballDistReduction = 1000.0f;
		
		//Allowed offset off the ball on the x axis
		float ballOffsetX = 150.0f;

		//Allowed offset off the ball on the y axis
		float ballOffsetY = 150.0f;

		//Reward for being behind the ball
		float behindTheBallReward = 0.01f;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(BallGroundHandling, agentDistToBallThresh, ballDistReduction, ballOffsetX, ballOffsetY, behindTheBallReward)
	};

	struct AgentSimilarity {
		
		//Reward for being above similarityBallAgentThresh (matching ball direction)
		float similarityBallAgentReward = 0.1f;

		//Min similarity between agent and ball's normalized velocities
		float similarityBallAgentThresh = 0.8f;

		//Weight of the reward for the agent matching ball speed
		float speedMatchW = 1.0f;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(AgentSimilarity, similarityBallAgentReward, similarityBallAgentThresh, speedMatchW)
	};

	struct CeilingHandling {
		//Distance from ceiling where considered to correct height (generally one ball away from ceiling)
		float distToCeilThresh = RLGSC::CommonValues::BALL_RADIUS + 20;

		//Distance for being on the "ceiling zone"
		float onCeilingReward = 0.01f;

		//Height from which being grounded is banned
		float banZoneHeight = 1500.0f;

		//Punishment for being grounded in the ground ban zone
		float groundedBan = -1.0f;

		//Punishment for being grounded in the ground ban zone
		float ungroundedReward = 1.0f;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(CeilingHandling, distToCeilThresh, onCeilingReward, banZoneHeight, groundedBan, ungroundedReward)

	};

	struct GroundHandling {
		//Agent similarity options
		AgentSimilarity agentSimilarity = {};

		//Ball ground handling options
		BallGroundHandling ballGroundHandling = {};

		//Distance from the wall to be away from it
		float distWallThresh = 50.0f + RLGSC::CommonValues::BALL_RADIUS;

		//Height from where you are considered off ground
		float groundThresh = 200.0f;

		//Touch reward
		float touchReward = 1.0f;

		//Ball and agent limit similarity to wall
		float wallAgentAndBallThreshold = 0.5f;

		//Ball and agent punishment for not similarity to wall
		float wallAgentAndBallPunishment = -2.0f;

		NLOHMANN_DEFINE_TYPE_INTRUSIVE(GroundHandling, agentSimilarity, ballGroundHandling, distWallThresh, groundThresh, touchReward, wallAgentAndBallThreshold, wallAgentAndBallPunishment)

	};

	struct WallHandling {
		//Ball distance punishment reduction (on wall)
		float ballDistReduction = 500.0f;

		//Ball height weight
		float ballHeightW = 15.0f;

		//Reward for being under the ball
		float underTheBallReward = 10.0f;

		//Offset of Y under the ball
		float underBallOffsetY = 100.0f;
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(WallHandling, ballDistReduction, ballHeightW, underTheBallReward)

	};

	struct PinchCeilingSetupArgs: public RewardConfig {
		GroundHandling groundHandling = {};
		WallHandling wallHandling = {};
		CeilingHandling ceilingHandling = {};
		PinchReward::PinchArgs pinchRewardConfig = {};

		// Inherited via RewardConfig
		void to_json(json& j) override;

	};

	PinchCeilingSetupReward(PinchCeilingSetupArgs* args): config(args), pinchReward(PinchReward(&args->pinchRewardConfig)) {};
	virtual void Reset(const RLGSC::GameState& initialState);
	virtual float GetReward(const RLGSC::PlayerData& player, const RLGSC::GameState& state, const RLGSC::Action& prevAction);
	virtual void ClearChanges() override;
	virtual void Log(RLGPC::Report& report, std::string name, float weight = 1.0f) override;

	virtual RewardConfig* GetConfig() { return config; };
private:
	PinchCeilingSetupArgs* config;
	PinchReward pinchReward;
};

class PinchGroundSetupReward : public LoggableReward {
public:
	struct PinchGroundSetupArgs: public RewardConfig {
		PinchReward::PinchArgs pinchRewardConfig = {};

		// Inherited via RewardConfig
		void to_json(json& j) override;

	};

	PinchGroundSetupReward(PinchGroundSetupArgs args) : config(args), pinchReward(PinchReward(&args.pinchRewardConfig)) {};
	virtual void Reset(const RLGSC::GameState& initialState);
	virtual float GetReward(const RLGSC::PlayerData& player, const RLGSC::GameState& state, const RLGSC::Action& prevAction);
	virtual void ClearChanges() override;
	virtual void Log(RLGPC::Report& report, std::string name, float weight = 1.0f) override;

private:
	PinchGroundSetupArgs config;
	PinchReward pinchReward;
};

class PinchTeamSetupReward : public LoggableReward {
public:
	struct PinchTeamSetupArgs {
		PinchReward::PinchArgs pinchRewardConfig = {};
	};

	PinchTeamSetupReward(PinchTeamSetupArgs args): config(args), pinchReward(PinchReward(&args.pinchRewardConfig)) {};
	virtual void Reset(const RLGSC::GameState& initialState);
	virtual float GetReward(const RLGSC::PlayerData& player, const RLGSC::GameState& state, const RLGSC::Action& prevAction);
	virtual void ClearChanges() override;
	virtual void Log(RLGPC::Report& report, std::string name, float weight = 1.0f) override;

private:
	PinchTeamSetupArgs config;
	PinchReward pinchReward;
};