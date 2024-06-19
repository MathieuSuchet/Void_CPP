#pragma once
#include <RLGymSim_CPP/Utils/RewardFunctions/RewardFunction.h>
#include <numeric>
#include <RLGymPPO_CPP/Util/Report.h>
#include "../RLGymPPO_CPP/libsrc/json/nlohmann/json.hpp"

using json = nlohmann::json;

struct RewardConfig {
	virtual void to_json(json& j) = 0;
};

class LoggableReward : public RLGSC::RewardFunction {
public:
	virtual void Log(RLGPC::Report& report, std::string name, float weight = 1.0f);

	virtual void AddLog(float& value, std::string name, float val, bool empty = false);
	virtual void ClearChanges();
	virtual RLGSC::FList GetAllRewards(const RLGSC::GameState& state, const RLGSC::ActionSet& prevActions, bool final) override;
	std::vector<std::pair<std::string, RLGSC::FList>> changes = {};

	virtual RewardConfig* GetConfig() = 0;

protected:
};


class LoggedCombinedReward : public RLGSC::RewardFunction {
public:
	std::vector<RewardFunction*> rewardFuncs;
	std::vector<float> rewardWeights;
	std::vector<std::tuple<std::string, float>> lastRewards;
	std::vector<std::string> names;
	bool ownsFuncs;

	LoggedCombinedReward(std::vector<RewardFunction*> rewardFuncs, std::vector<float> rewardWeights, std::vector<std::string> names, bool ownsFuncs = false);

	LoggedCombinedReward(std::vector<std::tuple<RewardFunction*, float, std::string>> funcsWithWeights, bool ownsFuncs = false);

	virtual void LogRewards(RLGPC::Report& report);

protected:
	virtual void Reset(const RLGSC::GameState& initialState) {
		for (auto func : rewardFuncs)
			func->Reset(initialState);
		
	}

	virtual void PreStep(const RLGSC::GameState& state) {
		for (auto func : rewardFuncs)
			func->PreStep(state);
	}

	virtual std::vector<float> GetAllRewards(const RLGSC::GameState& state, const RLGSC::ActionSet& prevAction, bool final) {
		std::vector<float> allRewards(state.players.size());
		lastRewards.clear();

		for (int i = 0; i < rewardFuncs.size(); i++) {
			auto rewards = rewardFuncs[i]->GetAllRewards(state, prevAction, final);
			float mean = std::accumulate(rewards.begin(), rewards.end(), 0.0) / rewards.size() * rewardWeights[i];
			this->lastRewards.push_back({this->names[i], mean});
			for (int j = 0; j < rewards.size(); j++) {
				allRewards[j] += rewards[j] * rewardWeights[i];
			}
		}

		return allRewards;
	}

	virtual ~LoggedCombinedReward() {
		if (ownsFuncs)
			for (auto func : rewardFuncs)
				delete func;
	}
};