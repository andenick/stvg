#pragma once

#include "../core/BankState.h"
#include "../core/GameConfig.h"
#include "../core/ConsequenceTracker.h"
#include "../simulation/TraderAI.h"
#include "../simulation/DecisionEngine.h"
#include <string>
#include <vector>

namespace stvg::game {

namespace DecisionHandler {

    bool submit(
        const std::string& decisionId,
        const std::string& optionId,
        std::vector<simulation::Decision>& pendingDecisions,
        ActionPointBudget& actionPoints,
        Bank& bank,
        ConsequenceTracker& consequenceTracker,
        simulation::TraderAIEngine& traderAI,
        std::vector<simulation::Trader>& traders,
        int& recentDividendQuarters,
        const SimulationConfig& config,
        const SimulationState& state);

} // namespace DecisionHandler

} // namespace stvg::game
