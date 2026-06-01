#ifdef STVG_WASM_BUILD

#include <emscripten/bind.h>
#include <string>
#include <memory>

#include "stvg/core/QuarterlyTurnManager.h"
#include "stvg/core/BankState.h"
#include "stvg/core/GameConfig.h"
#include "stvg/core/CEOProfile.h"
#include "stvg/core/RiskWeightedAssets.h"

using namespace emscripten;

class WasmGame {
public:
    WasmGame() {
        SimulationConfig cfg;
        cfg.seed = 42;
        mgr_ = std::make_unique<stvg::QuarterlyTurnManager>(cfg);
    }

    void createGame(uint32_t seed, const std::string& ceoId, const std::string& startPos) {
        SimulationConfig cfg;
        cfg.seed = seed;

        stvg::BankConfig bc;
        stvg::StartingPosition pos = stvg::StartingPosition::CommercialBank;
        if (startPos == "trading_firm") pos = stvg::StartingPosition::TradingFirm;
        else if (startPos == "investment_bank") pos = stvg::StartingPosition::InvestmentBank;
        else if (startPos == "community_bank") pos = stvg::StartingPosition::CommunityBank;
        else if (startPos == "universal_bank") pos = stvg::StartingPosition::UniversalBank;
        bc = stvg::bankConfigForPosition(pos);

        mgr_ = std::make_unique<stvg::QuarterlyTurnManager>(cfg, bc, ceoId, pos);
    }

    std::string getState() {
        if (!mgr_) return "{}";
        return mgr_->toPlayerJson().dump();
    }

    std::string getGodState() {
        if (!mgr_) return "{}";
        return mgr_->toGodJson().dump();
    }

    std::string advancePhase() {
        if (!mgr_) return "{}";
        mgr_->advancePhase();
        return mgr_->toPlayerJson().dump();
    }

    bool submitDecision(const std::string& decisionId, const std::string& optionId) {
        if (!mgr_) return false;
        return mgr_->submitDecision(decisionId, optionId);
    }

    std::string getDecisions() {
        if (!mgr_) return "[]";
        return nlohmann::json(mgr_->pendingDecisions()).dump();
    }

    std::string getMessages() {
        if (!mgr_) return "[]";
        return nlohmann::json(mgr_->messages()).dump();
    }

    std::string getReport() {
        if (!mgr_) return "{}";
        return nlohmann::json(mgr_->lastReport()).dump();
    }

    std::string getCeos() {
        auto profiles = stvg::CEOProfile::allProfiles();
        return nlohmann::json(profiles).dump();
    }

    std::string getScenarios() {
        // Load scenario JSON files from virtual filesystem
        nlohmann::json scenarios = nlohmann::json::array();
        for (const auto& path : {"data/scenarios/tutorial.json",
                                  "data/scenarios/crisis_mode.json",
                                  "data/scenarios/full_game.json",
                                  "data/scenarios/speedrun.json",
                                  "data/scenarios/ai_endgame.json"}) {
            std::ifstream f(path);
            if (f.is_open()) {
                try { scenarios.push_back(nlohmann::json::parse(f)); } catch (...) {}
            }
        }
        return scenarios.dump();
    }

    std::string getStartingPositions() {
        return stvg::allStartingPositionMetadata().dump();
    }

    std::string saveGame() {
        if (!mgr_) return "{}";
        return mgr_->saveGame().dump();
    }

    bool loadGame(const std::string& jsonStr) {
        if (!mgr_) return false;
        try {
            auto j = nlohmann::json::parse(jsonStr);
            return mgr_->loadGame(j);
        } catch (...) {
            return false;
        }
    }

    bool respondToCrisis(const std::string& crisisId, const std::string& responseType) {
        if (!mgr_) return false;
        return mgr_->respondToCrisis(crisisId, responseType);
    }

    std::string currentPhase() {
        if (!mgr_) return "\"none\"";
        return "\"" + std::string(mgr_->currentPhaseName()) + "\"";
    }

    int currentYear() { return mgr_ ? mgr_->state().currentYear : 0; }
    int currentQuarter() { return mgr_ ? mgr_->state().currentQuarter : 0; }
    bool isGameOver() { return mgr_ ? mgr_->isGameOver() : false; }

    // Simulation tick (for real-time mode)
    std::string tickSimulationDay() {
        if (!mgr_) return "{}";
        bool done = mgr_->tickSimulationDay();
        nlohmann::json result = {
            {"done", done},
            {"marketTick", mgr_->marketTickPayload()}
        };
        return result.dump();
    }

    void prepareSimulation() {
        if (mgr_) mgr_->prepareSimulation();
    }

    void completeSimulationPhase() {
        if (mgr_) mgr_->completeSimulationPhase();
    }

private:
    std::unique_ptr<stvg::QuarterlyTurnManager> mgr_;
};

EMSCRIPTEN_BINDINGS(stvg_module) {
    class_<WasmGame>("WasmGame")
        .constructor<>()
        .function("createGame", &WasmGame::createGame)
        .function("getState", &WasmGame::getState)
        .function("getGodState", &WasmGame::getGodState)
        .function("advancePhase", &WasmGame::advancePhase)
        .function("submitDecision", &WasmGame::submitDecision)
        .function("getDecisions", &WasmGame::getDecisions)
        .function("getMessages", &WasmGame::getMessages)
        .function("getReport", &WasmGame::getReport)
        .function("getCeos", &WasmGame::getCeos)
        .function("getScenarios", &WasmGame::getScenarios)
        .function("getStartingPositions", &WasmGame::getStartingPositions)
        .function("saveGame", &WasmGame::saveGame)
        .function("loadGame", &WasmGame::loadGame)
        .function("respondToCrisis", &WasmGame::respondToCrisis)
        .function("currentPhase", &WasmGame::currentPhase)
        .function("currentYear", &WasmGame::currentYear)
        .function("currentQuarter", &WasmGame::currentQuarter)
        .function("isGameOver", &WasmGame::isGameOver)
        .function("tickSimulationDay", &WasmGame::tickSimulationDay)
        .function("prepareSimulation", &WasmGame::prepareSimulation)
        .function("completeSimulationPhase", &WasmGame::completeSimulationPhase);
}

#endif // STVG_WASM_BUILD
