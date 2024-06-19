#include <RLGymPPO_CPP/Learner.h>

#include <RLGymSim_CPP/Utils/RewardFunctions/CommonRewards.h>
#include <RLGymSim_CPP/Utils/RewardFunctions/CombinedReward.h>
#include <RLGymSim_CPP/Utils/TerminalConditions/NoTouchCondition.h>
#include <RLGymSim_CPP/Utils/TerminalConditions/GoalScoreCondition.h>
#include <RLGymSim_CPP/Utils/OBSBuilders/DefaultOBS.h>
#include <RLGymSim_CPP/Utils/StateSetters/RandomState.h>
#include <RLGymSim_CPP/Utils/ActionParsers/DiscreteAction.h>

#include "RLBotClient.h"
#include "LoggedCombinedReward.h"
#include "LoggerUtils.h"
#include "WandbConfig.h"
#include <Rewards.h>
#include <States.h>
#include <TerminalConditions.h>
#include <Loggers.h>

using namespace RLGPC; // RLGymPPO
using namespace RLGSC; // RLGymSim

std::vector<Logger*> loggers = {
	//Ball Loggers
	new BallLoggers::BallSpeedLogger(),
	new BallLoggers::BallHeightLogger(),

	//Players loggers
	new PlayerLoggers::PlayerBallTouchLogger(),
	new PlayerLoggers::PlayerInAirLogger(),
	new PlayerLoggers::PlayerSpeedLogger(),
	new PlayerLoggers::PlayerHeightLogger()
};

float maxBallVel = 0.;

// This is our step callback, it's called every step from every RocketSim game
// WARNING: This is called from multiple threads, often simultaneously, 
//	so don't access things apart from these arguments unless you know what you're doing.
// gameMetrics: The metrics for this specific game
void OnStep(GameInst* gameInst, const RLGSC::Gym::StepResult& stepResult, Report& gameMetrics) {

	auto& gameState = stepResult.state;
	try
	{
		((LoggedCombinedReward*)(gameInst->match->rewardFn))->LogRewards(gameMetrics);
		//std::cout <<"Rw: " <<  std::get<1>(((LoggedCombinedReward*)(gameInst->match->rewardFn))->lastRewards[0]) << std::endl;
	}
	catch (const std::exception& e)
	{
		std::cout << "Couldn't log rewards: " << e.what() << std::endl;
	}

	float ballVel = gameState.ball.vel.Length();

	if (ballVel > maxBallVel) {
		maxBallVel = ballVel;
	}

	for (Logger* l : loggers) {
		l->Log(gameMetrics, gameState);
	}
}

// This is our iteration callback, it's called every time we complete an iteration, after learning
// Here we can add custom metrics to the metrics report, for example
void OnIteration(Learner* learner, Report& allMetrics) {
	std::map<std::string, AvgTracker> mTrackersAvg = {};
	std::map<std::string, float> mTrackers = {};
	std::map<std::string, AvgTracker> rTrackers = {};
	
	// Get metrics for every gameInst
	auto allGameMetrics = learner->GetAllGameMetrics();


	for (auto metric : allGameMetrics[0].data) {
		if (metric.first.starts_with(REWARD_HEADER)) {
			if (metric.first.ends_with("_avg_total")) {
				rTrackers[metric.first.substr(0, metric.first.size() - 10)] += 0;
			}
			else if (!metric.first.ends_with("_avg_count")) {
				rTrackers[metric.first] += 0;
			}
		}
	}

	for (const Logger* l : loggers) {
		for (const Metric& m : l->metrics) {
			if (m.isAvg) {
				mTrackersAvg[METRICS_HEADER + m.name] += 0;
			}
			else {
				mTrackers[METRICS_HEADER + m.name] += 0;
			}
		}
	}


	for (auto& gameReport : allGameMetrics) {
		for (auto& val : mTrackers) {
			val.second += gameReport[val.first];
		}

		for (auto& tracker : mTrackersAvg) {
			tracker.second += gameReport.GetAvg(tracker.first);
		}

		for (auto& tracker : rTrackers) {
			if(gameReport.Has(tracker.first) or gameReport.Has(tracker.first + "_avg_total"))
			{
				tracker.second += gameReport.GetAvg(tracker.first);
			}
		}
	}

	for (const auto& tracker : rTrackers) {
		allMetrics[tracker.first] = tracker.second.Get();
	}

	for (const auto& tracker : mTrackers) {
		allMetrics[tracker.first] = tracker.second;
	}

	for (const auto& tracker : mTrackersAvg) {
		allMetrics[tracker.first] = tracker.second.Get();
	}
	
	allMetrics[METRICS_HEADER + std::string("max_ball_speed")] = maxBallVel / CommonValues::BALL_MAX_SPEED * 216;
	maxBallVel = 0;
	std::cout << "End of iteration callback" << std::endl;
}

// Create the RLGymSim environment for each of our games
EnvCreateResult EnvCreateFunc() {
	constexpr int TICK_SKIP = 8;
	constexpr float NO_TOUCH_TIMEOUT_SECS = 7.f;

	PinchWallSetupReward::PinchWallSetupArgs args =
	{
			{
				.name = "PinchWallConfig",
				.weight = 1
			},
			{
				.creepingDistance = 2000.0f,
				.groundBanDistance = 1000.0f,
				.maxDistToTrigger = 4000.0f,
			},
			{
				.hasFlipReward = 1.0f,
				.hasFlipPunishment = 1.0f,
				.maxDistance = 50.0f,
				.hasFlipRewardWhenBall = 20.0f,
				.hasFlipPunishmentWhenBall = -20.0f
			},
			{
				.similarityBallAgentReward = 1.0f,
				.similarityBallAgentThresh = 0.9f,
				.similarityBallWallThresh = 0.9f,
			},
			{
				.groundBanPunishment = -0.1f,
				.groundBanReward = 1.0f,
				.creepingDistanceReward = 0.001f
			},
			{
				.ballDistReduction = 500.0f,
				.speedMatchW = 1.0f,
				.agentDistToBallThresh = 500.0f,
				.ballOffsetX = 200.0f,
				.ballOffsetY = 200.0f,
				.behindTheBallReward = 0.01f
			},
			{
				.wallMinHeightToPinch = 150.0f
			},
			{
				.ballHandling = {
					.ballVelW = 1000.0f,
					.touchW = 20.0f
				}
			}
	};

	PinchCeilingSetupReward::PinchCeilingSetupArgs pinchCeilingArgs(
		{
			.agentSimilarity =
			{
				.similarityBallAgentReward = 1.0f,
				.similarityBallAgentThresh = 0.9f,
				.speedMatchW = 0.1f
			},
			.ballGroundHandling =
			{
				.agentDistToBallThresh = 550.0f,
				.ballDistReduction = 2000.0f,
				.ballOffsetX = 250.0f,
				.ballOffsetY = 250.0f,
				.behindTheBallReward = 3.5f,
			},
			.distWallThresh = 50.0f + RLGSC::CommonValues::BALL_RADIUS,
			.groundThresh = 250.0f,
			.touchReward = 40.0f,
			.wallAgentAndBallThreshold = 0.8f,
			.wallAgentAndBallPunishment = -2.0f
		},
		{
			.ballDistReduction = 50.0f,
			.ballHeightW = 20.0f,
			.underTheBallReward = 50.0f,
			.underBallOffsetY = 350.0f
		},
		{
			.distToCeilThresh = RLGSC::CommonValues::BALL_RADIUS + 50.0f,
			.onCeilingReward = 10.0f,
			.banZoneHeight = 1500.0f,
			.groundedBan = -350.0f,
			.ungroundedReward = 60.0f
		},
		{
			.ballHandling =
			{
				.ballVelW = 20000.0f,
				.touchW = 200.0f
			}
		}
	});

	auto rewards = new LoggedCombinedReward( // Format is { RewardFunc(), weight, name }
		{
			//{new PinchWallSetupReward(args), 1.0f, "WallPinchReward"},
			{new PinchCeilingSetupReward(new PinchCeilingSetupReward::PinchCeilingSetupArgs(pinchCeilingArgs)), 1.0f, "CeilingPinchReward"},
			//{new PinchCornerSetupReward({}), 1.0f, "CornerPinchReward"},
			//{new PinchTeamSetupReward({}), 1.0f, "TeamPinchReward"},
		},
		false
	);

	for (int i = 0; i < rewards->rewardFuncs.size(); i++) {
		LoggableReward* lr = dynamic_cast<LoggableReward*>(rewards->rewardFuncs[i]);
		if (lr == nullptr) continue;
		json j;
		lr->GetConfig()->to_json(j);
		VOID_LOG(j);
	}

	std::vector<TerminalCondition*> terminalConditions = {
		new TimeoutCondition(NO_TOUCH_TIMEOUT_SECS * 120 / TICK_SKIP),
		new GoalScoreCondition()
	};

	auto obs = new DefaultOBS();
	auto actionParser = new DiscreteAction();
	auto stateSetter = new CeilingPinchSetter();

	Match* match = new Match(
		rewards,
		terminalConditions,
		obs,
		actionParser,
		stateSetter,

		1, // Team size
		false // Spawn opponents
	);

	Gym* gym = new Gym(match, TICK_SKIP);
	return { match, gym };
}

int main() {
	// Initialize RocketSim with collision meshes
	RocketSim::Init("./collision_meshes");

	// Make configuration for the learner
	LearnerConfig cfg = {};

	// Play around with these to see what the optimal is for your machine, more isn't always better
	cfg.numThreads = 1;
	cfg.numGamesPerThread = 1;

	// We want a large itr/batch size
	// You'll want to increase this as your bot improves, up to an extent
	int tsPerItr = 200 * 1000;
	cfg.timestepsPerIteration = tsPerItr;
	cfg.ppo.batchSize = tsPerItr;
	cfg.ppo.miniBatchSize = 250 * 100; // Lower this if too much VRAM is being allocated
	cfg.expBufferSize = tsPerItr * 3;
	
	// This is just set to 1 to match rlgym-ppo example
	// I've found the best value is somewhere between 2 and 4
	// Increasing this will lower SPS, but increase step efficiency
	cfg.ppo.epochs = 3; 

	// Reasonable starting entropy
	cfg.ppo.entCoef = 0.01f;

	// Decently-strong learning rate to start, may start to be too high around 100m steps
	cfg.ppo.policyLR = 8e-4;
	cfg.ppo.criticLR = 8e-4;

	// Default model size
	cfg.ppo.policyLayerSizes = { 256, 256, 256 };
	cfg.ppo.criticLayerSizes = { 256, 256, 256 };
	
	cfg.sendMetrics = false; // Send metrics
	cfg.renderMode = not cfg.sendMetrics; // render
	cfg.renderTimeScale = 1.5f;
	cfg.renderDuringTraining = false; //Activate that so it doesn't override

	cfg.checkpointsToKeep = 30;

	cfg.metricsGroupName = WANDB_ENTITY;
	cfg.metricsProjectName = WANDB_PROJECT;
	cfg.metricsRunName = WANDB_RUN_NAME;

	// Make the learner with the environment creation function and the config we just made
	Learner learner = Learner(EnvCreateFunc, cfg);

	// Set up our callbacks
	learner.stepCallback = OnStep;
	learner.iterationCallback = OnIteration;


	// Start learning!
	learner.Learn();

	//RLBotClient::Run({ .port = 23233, .obsBuilder = new DefaultOBS(), .actionParser = new DiscreteAction(), .policyPath = "checkpoints/42011264", .obsSize = 70,  .policyLayerSizes = {256, 256, 256}, .tickSkip = 8 });
	std::cout << "Learning done" << std::endl;

	return 0;
}