#include "Rewards.h"

PinchReward::PinchReward(
	PinchArgs* args
) :
	config(args)
{
	this->lastBallSpeed = 0;
}

void PinchReward::Reset(const RLGSC::GameState& initialState)
{
	this->lastBallSpeed = 0;
}

float PinchReward::GetReward(const RLGSC::PlayerData& player, const RLGSC::GameState& state, const RLGSC::Action& prevAction)
{
	//REWARD
	float reward = 0.f;	
	float ballAccel = (state.ball.vel.Length2D() - lastBallSpeed) / RLConst::BALL_MAX_SPEED;
	AddLog(reward, "Ball touch", config->ballHandling.touchW);
	AddLog(reward, "Ball accel", ballAccel * config->ballHandling.ballVelW);

	float ballGoalSimilarity = ((player.team == Team::ORANGE ? RLGSC::CommonValues::BLUE_GOAL_CENTER : RLGSC::CommonValues::ORANGE_GOAL_CENTER) - state.ball.pos).Normalized().Dot(state.ball.vel.Normalized());
	if (ballGoalSimilarity < config->ballHandling.goalDirectionSimilarity) {
		AddLog(reward, "Towards net", config->ballHandling.goalDirectionW);
	}

	lastBallSpeed = state.ball.vel.Length2D();
    return reward;
}

void PinchReward::ClearChanges()
{
	float temp = 0;
	LoggableReward::ClearChanges();
	AddLog(temp, "Ball touch", 0, true);
	AddLog(temp, "Ball accel", 0, true);
	AddLog(temp, "Towards net", 0, true);


}


void PinchWallSetupReward::Reset(const RLGSC::GameState& initialState)
{
	this->pinchReward.Reset(initialState);
}

float PinchWallSetupReward::GetReward(const RLGSC::PlayerData& player, const RLGSC::GameState& state, const RLGSC::Action& prevAction)
{
	float reward = 0.f;
	auto ballDir = state.ball.pos.Normalized();
	auto agentDir = player.carState.vel.Normalized();
	short targetDir = ballDir.x < 0 ? -1 : 1;

	float wallInterceptionY = (ballDir.y / ballDir.x) * (RLGSC::CommonValues::SIDE_WALL_X * targetDir - state.ball.pos.x) + state.ball.pos.y;
	Vec interceptPoint = Vec(RLGSC::CommonValues::SIDE_WALL_X * targetDir, wallInterceptionY, 0);
	Vec intersectionPoint = GetCornerIntersection(ballDir.x, ballDir.y, state.ball.pos.x, state.ball.pos.y);

	//First things first, is the intersect point valid
	if (std::abs(intersectionPoint.x) <= RLGSC::CommonValues::SIDE_WALL_X or std::abs(intersectionPoint.x) >= RLGSC::CommonValues::SIDE_WALL_X - 1152) {
		interceptPoint = intersectionPoint;
	};


	Vec agentToBall = state.ball.pos - player.carState.pos;
	float agentToBallDist = agentToBall.Length();


	//First thing, detect if we trigger based on ball dist to wall intercept
	if (state.ball.pos.Dist2D(interceptPoint) < config->distancesWallSetup.maxDistToTrigger) {
		//Testing if before creeping
		if (state.ball.pos.Dist2D(interceptPoint) > config->distancesWallSetup.creepingDistance) {
			//Match direction and speed
			float speedDiff = std::max(0.01f, std::abs(state.ball.vel.Dist(player.carState.vel)) / RLGSC::CommonValues::SUPERSONIC_THRESHOLD);
			float directionSimilarity = agentDir.Dot(ballDir);

			if (agentToBallDist < config->ballWallHandling.agentDistToBallThresh) {
				if (abs(agentToBall.x) < config->ballWallHandling.ballOffsetX and abs(agentToBall.y) < config->ballWallHandling.ballOffsetY) {
					AddLog(reward, "Behind the ball", config->ballWallHandling.behindTheBallReward);
				}
			}

			float speedMatchingBonus = 1 / pow(speedDiff, 0.1f) * config->ballWallHandling.speedMatchW;
			float ballDistPunish = state.ball.pos.Dist(player.carState.pos) / config->ballWallHandling.ballDistReduction;
			float directionMatchBonus = directionSimilarity >= config->similarityWallSetup.similarityBallAgentThresh ? config->similarityWallSetup.similarityBallAgentReward : 0;

			AddLog(reward, "Speed matching", speedMatchingBonus);
			AddLog(reward, "Player distance to ball", -ballDistPunish);
			AddLog(reward, "Direction matching", directionMatchBonus);
		}

		//Creeping distance
		else {
			//Being in this zone is good already, get some snacks
			AddLog(reward, "Creeping distance reward", config->groundWallSetupHandling.creepingDistanceReward);

			float agentDistToIntercept = Vec(RLGSC::CommonValues::SIDE_WALL_X * targetDir, player.carState.pos.y, 0).Dist2D(player.carState.pos);

			//Being in the ground ban zone
			if (agentDistToIntercept <= config->distancesWallSetup.groundBanDistance) {
				//If grounded, get spanked, else, you're doing great
				float groundChange = player.carState.isOnGround ? config->groundWallSetupHandling.groundBanPunishment : config->groundWallSetupHandling.groundBanReward;
				AddLog(reward, "Creeping grounded", groundChange);

				//If still relatively far
				if (state.ball.pos.Dist(player.carState.pos) > config->flipHandlingWallSetup.maxDistance + RLGSC::CommonValues::BALL_RADIUS) {
					//Don't use your flip or punished
					float flipChange = player.hasFlip ? config->flipHandlingWallSetup.hasFlipReward : config->flipHandlingWallSetup.hasFlipPunishment;
					AddLog(reward, "Creeping flip", flipChange);
				}

				//Accel reward on ball hit
				else if (player.ballTouchedStep and state.ball.pos.z >= config->wallHandling.wallMinHeightToPinch) {
					//You still got your flip when hitting the ball ? punished, else good
					float flipBallChange = !player.hasFlip ? config->flipHandlingWallSetup.hasFlipPunishmentWhenBall : config->flipHandlingWallSetup.hasFlipRewardWhenBall;
					AddLog(reward, "Ball flip", flipBallChange);
					AddLog(reward, "Pinch reward", this->pinchReward.GetReward(player, state, prevAction));
				}
			}
		}
	}

	return reward;
}

void PinchWallSetupReward::ClearChanges()
{
	float temp = 0;
	LoggableReward::ClearChanges();
	AddLog(temp, "Speed matching", 0, true);
	AddLog(temp, "Player distance to ball", 0, true);
	AddLog(temp, "Direction matching", 0, true);
	AddLog(temp, "Creeping distance reward", 0, true);
	AddLog(temp, "Creeping grounded", 0, true);
	AddLog(temp, "Creeping flip", 0, true);
	AddLog(temp, "Ball flip", 0, true);
	AddLog(temp, "Pinch reward", 0, true);
	this->pinchReward.ClearChanges();
}

float PinchWallSetupReward::Corner(float x, short xOrientation, short yOrientation)
{
	return xOrientation * x + 8064 * yOrientation;
}

Vec PinchWallSetupReward::GetCornerIntersection(short xFwd, short yFwd, float xPos, float yPos)
{
	float xResult = 0;
	float yResult = 0;

	short xOrientation = 0;
	short yOrientation = 0;

	//Right
	if (xFwd > 0) {

		//Top
		if (yFwd > 0) {
			xOrientation = -1;
			yOrientation = 1;
		}
		else {
			xOrientation = 1;
			yOrientation = -1;
		}
	}
	else {
		//Bottom
		if (yFwd < 0) {
			xOrientation = -1;
			yOrientation = -1;
		}
		else {
			xOrientation = 1;
			yOrientation = 1;
		}
	}

	xResult = (xFwd * (8064 * yOrientation - yPos) + yFwd * xPos) / (yFwd - xFwd * xOrientation);

	yResult = Corner(xResult, xOrientation, yOrientation);
	return Vec(xResult, yResult, 0.0f);
}

void PinchWallSetupReward::Log(RLGPC::Report& report, std::string name, float weight)
{
	LoggableReward::Log(report, name, weight);
	this->pinchReward.Log(report, name, weight);
}

void PinchCeilingSetupReward::Reset(const RLGSC::GameState& initialState)
{
	this->pinchReward.Reset(initialState);
}

float PinchCeilingSetupReward::GetReward(const RLGSC::PlayerData& player, const RLGSC::GameState& state, const RLGSC::Action& prevAction)
{
	float reward = 0.0f;
	auto ballDir = state.ball.vel.Normalized();
	auto agentDir = player.carState.vel.Normalized();
	short targetDir = ballDir.x < 0 ? -1 : 1;

	RLGSC::PhysObj ball = state.ball;
	RocketSim::CarState agentState = player.carState;

	//Ground to wall
	if (std::abs(ball.pos.x) < RLGSC::CommonValues::SIDE_WALL_X - config->groundHandling.distWallThresh 
		and agentState.pos.z < (config->groundHandling.groundThresh + 17)
		and ball.pos.z < (RLGSC::CommonValues::BALL_RADIUS + config->groundHandling.groundThresh)
		) {

		Vec agentToBall = ball.pos - agentState.pos;
		float agentToBallDist = agentToBall.Length();

		if (agentToBallDist < config->groundHandling.ballGroundHandling.agentDistToBallThresh) {
			if (abs(agentToBall.x) < config->groundHandling.ballGroundHandling.ballOffsetX and abs(agentToBall.y) < config->groundHandling.ballGroundHandling.ballOffsetY) {
				AddLog(reward, "Ground/Behind the ball", config->groundHandling.ballGroundHandling.behindTheBallReward);
			}
		}

		float speedDiff = std::max(0.01f, std::abs(state.ball.vel.Dist(player.carState.vel)) / RLGSC::CommonValues::SUPERSONIC_THRESHOLD);
		float directionSimilarity = agentDir.Dot(ballDir);

		//Zooming like the ball is zooming, i like it
		float speedMatchingBonus = 1 / pow(speedDiff, 0.1f) * config->groundHandling.agentSimilarity.speedMatchW;

		//Going too far away from the ball ? I don't like it
		float ballDistPunish = state.ball.pos.Dist(player.carState.pos) / config->groundHandling.ballGroundHandling.ballDistReduction;

		//Going parallel to the wall, i will send you to Jesus
		float wallComparisonPunish = Vec(targetDir, 0, 0).Dot(agentState.vel.Normalized()) > config->groundHandling.wallAgentAndBallThreshold and Vec(targetDir, 0, 0).Dot(ball.vel.Normalized()) > config->groundHandling.wallAgentAndBallThreshold ? 0 : config->groundHandling.wallAgentAndBallPunishment;

		//You're matching the ball's direction ? Here, a snack
		float directionMatchBonus = directionSimilarity >= config->groundHandling.agentSimilarity.similarityBallAgentThresh ? config->groundHandling.agentSimilarity.similarityBallAgentReward : 0;

		AddLog(reward, "Ground/Speed matching", speedMatchingBonus);
		AddLog(reward, "Ground/Wall alignment punishment", wallComparisonPunish);
		AddLog(reward, "Ground/Player distance to ball", -ballDistPunish);
		AddLog(reward, "Ground/Direction matching", directionMatchBonus);

		if (player.ballTouchedStep) {
			AddLog(reward, "Ground/Ball touch", config->groundHandling.touchReward);
		}
	}

	//Wall stuff
	if (std::abs(ball.pos.x) >= RLGSC::CommonValues::SIDE_WALL_X - config->groundHandling.distWallThresh 
		and std::abs(agentState.pos.x) >= RLGSC::CommonValues::SIDE_WALL_X - config->groundHandling.distWallThresh
	) {

		if (agentState.pos.z < ball.pos.z and std::abs(agentState.pos.y) > std::abs(ball.pos.y - config->wallHandling.underBallOffsetY) and std::abs(agentState.pos.y) < std::abs(ball.pos.y + config->wallHandling.underBallOffsetY)) {
			AddLog(reward, "Wall/Under the ball", config->wallHandling.underTheBallReward);
		}

		float ballDistPunish = state.ball.pos.Dist(player.carState.pos) / config->wallHandling.ballDistReduction;
		AddLog(reward, "Wall/Agent dist to ball", -ballDistPunish);
		
		//Reward for ball height ?
		AddLog(reward, "Wall/Ball height", ball.pos.z / RLGSC::CommonValues::CEILING_Z * config->wallHandling.ballHeightW);
	}

	if (player.carState.pos.z > config->ceilingHandling.banZoneHeight and ball.pos.z > config->ceilingHandling.banZoneHeight) {
		if (player.carState.isOnGround) AddLog(reward, "Ceiling/Ground ban", config->ceilingHandling.groundedBan);
		else AddLog(reward, "Ceiling/Off Ground Reward", config->ceilingHandling.ungroundedReward);
	}

	//If within ceiling range
	if (ball.pos.z > RLGSC::CommonValues::CEILING_Z - config->ceilingHandling.distToCeilThresh) {
		//Already good being there, get some snack
		AddLog(reward, "Ceiling/Ceiling zone", config->ceilingHandling.onCeilingReward);
		
		//Touching the ball and ceiling range ? Sure thing buddy
		if (player.ballTouchedStep) {
			AddLog(reward, "Ceiling/Ceiling pinch", this->pinchReward.GetReward(player, state, prevAction));
		}
	}

	return reward;
}

void PinchCeilingSetupReward::ClearChanges()
{
	float temp;
	AddLog(temp, "Ground/Behind the ball", 0, true);
	AddLog(temp, "Ground/Speed matching" , 0, true);
	AddLog(temp, "Ground/Wall alignment punishment", 0, true);
	AddLog(temp, "Ground/Player distance to ball", 0, true);
	AddLog(temp, "Ground/Direction matching", 0, true);
	AddLog(temp, "Ground/Ball touch", 0, true);
	AddLog(temp, "Wall/Under the ball", 0, true);
	AddLog(temp, "Wall/Agent dist to ball", 0, true);
	AddLog(temp, "Wall/Ball height", 0, true);
	AddLog(temp, "Ceiling/Ground ban", 0, true);
	AddLog(temp, "Ceiling/Off Ground Reward", 0, true);
	AddLog(temp, "Ceiling/Off Ground Reward", 0, true);
	AddLog(temp, "Ceiling/Ceiling zone", 0, true);
	AddLog(temp, "Ceiling/Ceiling pinch", 0, true);
	this->pinchReward.ClearChanges();
}

void PinchCeilingSetupReward::Log(RLGPC::Report& report, std::string name, float weight)
{
	LoggableReward::Log(report, name, weight);
	this->pinchReward.Log(report, name, weight);
}

void PinchTeamSetupReward::Reset(const RLGSC::GameState& initialState)
{
	this->pinchReward.Reset(initialState);
}

float PinchTeamSetupReward::GetReward(const RLGSC::PlayerData& player, const RLGSC::GameState& state, const RLGSC::Action& prevAction)
{
	return 0.0f;
}

void PinchTeamSetupReward::ClearChanges()
{
	this->pinchReward.ClearChanges();
}

void PinchTeamSetupReward::Log(RLGPC::Report& report, std::string name, float weight)
{
	LoggableReward::Log(report, name, weight);
	this->pinchReward.Log(report, name, weight);
}

void PinchGroundSetupReward::Reset(const RLGSC::GameState& initialState)
{
}

float PinchGroundSetupReward::GetReward(const RLGSC::PlayerData& player, const RLGSC::GameState& state, const RLGSC::Action& prevAction)
{
	float reward = 0.0f;


	/* Pseudo algorithm idea */

	return reward;
}

void PinchGroundSetupReward::Log(RLGPC::Report& report, std::string name, float weight)
{
	LoggableReward::Log(report, name, weight);
	this->pinchReward.Log(report, name, weight);
}

void PinchGroundSetupReward::ClearChanges()
{
	this->pinchReward.ClearChanges();
}

void PinchReward::PinchArgs::to_json(json& j)
{
	j.update({ "ballHandling", this->ballHandling });
}

void PinchCeilingSetupReward::PinchCeilingSetupArgs::to_json(json& j)
{
	nlohmann::to_json(j, this->groundHandling);
	nlohmann::to_json(j, this->ceilingHandling);
	this->pinchRewardConfig.to_json(j);
	nlohmann::to_json(j, this->wallHandling);
}

void PinchGroundSetupReward::PinchGroundSetupArgs::to_json(json& j)
{
}
