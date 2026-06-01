#pragma once

#include "BotStrategy.h"
#include "EraAwareBots.h"  // for PersonalityDrivenBot
#include "Telemetry.h"
#include "../core/QuarterlyTurnManager.h"
#include "../core/GameConfig.h"
#include <chrono>
#include <cmath>
#include <spdlog/spdlog.h>

namespace stvg::autoplay {

class GameRunner {
public:
    // v3: optional custom-economy hook that gets attached immediately after
    // the internal QuarterlyTurnManager is constructed. Null = baseline.
    CustomEconomyHook* customEcon = nullptr;

    // Run a single complete game
    GameSummary runGame(BotStrategy& bot, const GameConfig& config,
                         uint64_t seed, int maxQuarters = 380) {
        GameSummary summary;
        summary.strategyName = bot.name();
        summary.seed = seed;
        summary.difficulty = "normal";

        // Create game with specific seed
        SimulationConfig simCfg;
        simCfg.seed = seed;
        simCfg.startYear = config.timing.startYear;
        simCfg.startQuarter = config.timing.startQuarter;
        simCfg.quarterDurationDays = config.timing.quarterDurationDays;

        BankConfig bankCfg;
        bankCfg.startingCapital = config.bank.startingCapital;
        bankCfg.startingEmployees = config.bank.startingEmployees;

        auto gameStart = std::chrono::high_resolution_clock::now();

        QuarterlyTurnManager mgr(simCfg, bankCfg, config.restrictions.forceCeoId);
        mgr.setCustomEconomy(customEcon);  // v3: inject before any ticks
        mgr.setSkipDeleveraging(config.restrictions.noDeleveraging);
        mgr.setSkipCrises(config.restrictions.noCrises);
        summary.peakCapital = mgr.bank().capital;
        summary.nadirCapital = mgr.bank().capital;

        double prevScore = -1;
        int stuckCount = 0;

        for (int q = 0; q < maxQuarters; ++q) {
            auto turnStart = std::chrono::high_resolution_clock::now();

            // 1. QuarterStart → DecisionPhase
            mgr.advancePhase();

            // 2. Bot makes decisions
            for (const auto& dec : mgr.pendingDecisions()) {
                if (dec.resolved) continue;
                std::string optionId = bot.chooseOption(mgr, dec);
                if (!optionId.empty()) {
                    mgr.submitDecision(dec.id, optionId);
                    summary.totalDecisionsMade++;
                }
            }

            // 3. Simulation
            mgr.advancePhase(); // → Simulation
            mgr.advancePhase(); // → QuarterEnd (runs 63 days internally)

            // 4. Handle crisis phases
            if (mgr.currentPhase() == TurnPhase::CrisisResponse) {
                summary.crisesEncountered++;
                // Every bot is now PersonalityDrivenBot — pull stressResponse
                // directly from the personality vector. Legacy name branches
                // are gone; behavior is data-driven.
                std::string botName = bot.name();
                bool isPersonalityBot = (botName.rfind("Personality_", 0) == 0);
                double stressResponse = 0.5;
                if (isPersonalityBot) {
                    stressResponse = static_cast<PersonalityDrivenBot&>(bot).personality().stressResponse;
                }
                for (const auto& crisis : mgr.crisisEngine().activeCrises()) {
                    if (!crisis.resolved) {
                        std::string response = (stressResponse > 0.6) ? "aggressive" : "measured";
                        // Escalating crises always need decisive action
                        if (crisis.severity > 7) response = "aggressive";
                        mgr.respondToCrisis(crisis.id, response);
                    }
                }
                // If still in crisis_response (unresolved crises), force advance
                if (mgr.currentPhase() == TurnPhase::CrisisResponse) {
                    mgr.advancePhase();
                }
            }
            while (mgr.currentPhase() != TurnPhase::QuarterComplete) {
                mgr.advancePhase();
            }

            auto turnEnd = std::chrono::high_resolution_clock::now();
            double turnMs = std::chrono::duration<double, std::milli>(turnEnd - turnStart).count();

            // Collect telemetry
            TurnTelemetry tt;
            tt.quarter = mgr.state().currentQuarter;
            tt.year = mgr.state().currentYear;
            tt.capital = mgr.bank().capital;
            tt.totalAssets = mgr.bank().totalAssets;
            tt.revenue = mgr.bank().totalRevenue;
            tt.costs = mgr.bank().totalCosts;
            tt.netIncome = mgr.bank().netIncome;
            tt.reputation = mgr.bank().reputation;
            tt.capitalRatio = mgr.bank().capitalRatio();
            tt.visibilityPct = mgr.bank().visibilityPct();
            tt.hiddenRisk = mgr.bank().totalHiddenRisk();
            tt.pendingConsequences = mgr.pendingConsequenceCount();
            tt.resolvedConsequences = (int)mgr.resolvedConsequences().size();
            tt.score = mgr.lastReport().score.total;
            tt.xpEarned = mgr.lastReport().score.xpEarned;
            tt.turnTimeMs = turnMs;
            tt.hadCrisis = (summary.crisesEncountered > 0);
            tt.activeCrises = (int)mgr.crisisEngine().activeCrises().size();

            if (!mgr.state().markets.empty()) {
                tt.spxPrice = mgr.state().markets[0].currentPrice;
            }
            tt.vix = mgr.state().economics.vix;
            tt.fedFundsRate = mgr.state().economics.fedFundsRate;
            tt.currentEra = mgr.currentEra().name;
            tt.competitorCount = mgr.competitorEngine().activeCount(mgr.state().currentYear);

            summary.turnHistory.push_back(tt);

            // Update summary
            summary.totalRevenue += tt.revenue;
            summary.totalCosts += tt.costs;
            summary.totalNetIncome += tt.netIncome;
            summary.totalConsequencesResolved += tt.resolvedConsequences;
            if (tt.capital > summary.peakCapital) summary.peakCapital = tt.capital;
            if (tt.capital < summary.nadirCapital) summary.nadirCapital = tt.capital;
            if (turnMs > summary.maxTurnTimeMs) summary.maxTurnTimeMs = turnMs;

            // NaN check
            if (std::isnan(tt.capital) || std::isinf(tt.capital) ||
                std::isnan(tt.score) || std::isinf(tt.score)) {
                summary.hadNaN = true;
            }

            // Negative capital check
            if (tt.capital < 0) {
                summary.hadNegativeCapital = true;
                summary.survived = false;
            }

            // Stuck state detection: score plateau lasting kBalance.stuckMinQuarters
            // quarters within kBalance.stuckScoreThreshold delta. Phase 3 retuned
            // from (0.01, 3) → (0.05, 6) — old threshold flagged natural plateaus.
            if (std::abs(tt.score - prevScore) < kBalance.stuckScoreThreshold) stuckCount++;
            else stuckCount = 0;
            if (stuckCount >= kBalance.stuckMinQuarters) summary.hadStuckState = true;
            prevScore = tt.score;

            summary.quartersPlayed = q + 1;

            // Early exit if game over
            if (mgr.isGameOver()) {
                auto endState = mgr.checkGameEnd();
                summary.survived = endState.isVictory;
                summary.deathQuarter = q + 1;
                summary.leverageAtDeath = mgr.bank().leverageRatio();
                summary.capitalRatioAtDeath = mgr.bank().capitalRatio();
                break;
            }

            // Advance to next quarter
            mgr.advancePhase(); // QuarterComplete → QuarterStart
        }

        auto gameEnd = std::chrono::high_resolution_clock::now();
        summary.totalGameTimeMs = std::chrono::duration<double, std::milli>(gameEnd - gameStart).count();
        summary.avgTurnTimeMs = summary.totalGameTimeMs / std::max(summary.quartersPlayed, 1);

        // Final state
        summary.finalCapital = mgr.bank().capital;
        summary.finalScore = mgr.lastReport().score.total;
        summary.finalLevel = mgr.progression().level;
        summary.finalTitle = mgr.progression().title;
        summary.totalXP = mgr.progression().totalExperience;

        // Avg score
        double scoreSum = 0;
        for (const auto& t : summary.turnHistory) scoreSum += t.score;
        summary.avgScore = scoreSum / std::max((int)summary.turnHistory.size(), 1);

        // Era-aware final state
        summary.finalEra = mgr.currentEra().name;
        summary.erasReached = mgr.eraEngine().currentEraIndex() + 1;
        auto endState = mgr.checkGameEnd();
        if (endState.reason != GameEndReason::None) {
            nlohmann::json reasonJson = endState.reason;
            summary.gameEndReason = reasonJson.get<std::string>();
        }

        return summary;
    }

    // Run N games with a strategy
    std::vector<GameSummary> runBatch(BotStrategy& bot, const GameConfig& config,
                                       int numGames, uint64_t baseSeed = 1,
                                       int maxQuarters = 380) {
        std::vector<GameSummary> results;
        results.reserve(numGames);
        for (int i = 0; i < numGames; ++i) {
            results.push_back(runGame(bot, config, baseSeed + i, maxQuarters));
        }
        return results;
    }
};

} // namespace stvg::autoplay
