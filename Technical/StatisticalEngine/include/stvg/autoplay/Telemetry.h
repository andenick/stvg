#pragma once

#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace stvg::autoplay {

struct TurnTelemetry {
    int quarter = 0;
    int year = 0;
    double capital = 0;
    double totalAssets = 0;
    double revenue = 0;
    double costs = 0;
    double netIncome = 0;
    double reputation = 0;
    double capitalRatio = 0;
    double visibilityPct = 0;
    double hiddenRisk = 0;
    int decisionsResolved = 0;
    int pendingConsequences = 0;
    int resolvedConsequences = 0;
    int activeCrises = 0;
    double score = 0;
    int xpEarned = 0;
    double turnTimeMs = 0;
    double spxPrice = 0;
    double vix = 0;
    double fedFundsRate = 0;
    bool hadCrisis = false;
    std::string currentEra;
    int competitorCount = 0;
    bool eraTransitioned = false;
};

struct GameSummary {
    std::string strategyName;
    uint64_t seed = 0;
    std::string difficulty;
    int quartersPlayed = 0;
    bool survived = true;
    double finalCapital = 0;
    double finalScore = 0;
    double avgScore = 0;
    int finalLevel = 1;
    std::string finalTitle;
    int totalXP = 0;
    int crisesEncountered = 0;
    double peakCapital = 0;
    double nadirCapital = 1e18;
    double totalRevenue = 0;
    double totalCosts = 0;
    double totalNetIncome = 0;
    int totalDecisionsMade = 0;
    int totalConsequencesResolved = 0;
    double totalGameTimeMs = 0;
    double avgTurnTimeMs = 0;
    double maxTurnTimeMs = 0;
    bool hadNaN = false;
    bool hadNegativeCapital = false;
    bool hadStuckState = false;
    std::vector<TurnTelemetry> turnHistory;

    // Era-aware fields
    int erasReached = 0;
    std::string finalEra;
    std::string gameEndReason;
    bool aiSingularityOccurred = false;
    int climateTippingPoints = 0;

    // Phase 3.3 B4: death telemetry — answers "where did this bot die"
    // -1 = survived to end of run; otherwise the quarter the game ended.
    int    deathQuarter = -1;
    double leverageAtDeath = 0;     // bank.totalAssets / bank.capital at end
    double capitalRatioAtDeath = 0;
};

struct BatchResult {
    std::string strategyName;
    std::string difficulty;
    int gamesPlayed = 0;
    int gamesSurvived = 0;
    double survivalRate = 0;
    double avgScore = 0;
    double medianScore = 0;
    double stdDevScore = 0;
    double avgCapital = 0;
    double avgLevel = 0;
    double avgCrises = 0;
    double avgTurnTimeMs = 0;
    double maxTurnTimeMs = 0;
    double p95TurnTimeMs = 0;
    int nanCount = 0;
    int stuckCount = 0;
    int bankruptcyCount = 0;

    // Phase 3.3 B4: death telemetry aggregates
    int    medianDeathQuarter = -1;       // -1 if all survived
    double avgLeverageAtDeath = 0;
    std::string mostCommonEndReason;      // top gameEndReason among non-survivors
    int    deathCount = 0;                // games that did not survive

    static BatchResult compute(const std::vector<GameSummary>& games) {
        BatchResult r;
        if (games.empty()) return r;
        r.strategyName = games[0].strategyName;
        r.difficulty = games[0].difficulty;
        r.gamesPlayed = (int)games.size();

        std::vector<double> scores, capitals, crises, turnTimes;
        std::vector<int> deathQuarters;
        std::vector<double> deathLeverages;
        std::map<std::string, int> endReasonCounts;
        for (const auto& g : games) {
            if (g.survived) r.gamesSurvived++;
            scores.push_back(g.finalScore);
            capitals.push_back(g.finalCapital);
            crises.push_back(g.crisesEncountered);
            turnTimes.push_back(g.avgTurnTimeMs);
            r.avgLevel += g.finalLevel;
            if (g.maxTurnTimeMs > r.maxTurnTimeMs) r.maxTurnTimeMs = g.maxTurnTimeMs;
            if (g.hadNaN) r.nanCount++;
            if (g.hadStuckState) r.stuckCount++;
            if (g.hadNegativeCapital) r.bankruptcyCount++;
            if (!g.survived) {
                r.deathCount++;
                if (g.deathQuarter >= 0) deathQuarters.push_back(g.deathQuarter);
                if (g.leverageAtDeath > 0) deathLeverages.push_back(g.leverageAtDeath);
                if (!g.gameEndReason.empty()) endReasonCounts[g.gameEndReason]++;
            }
        }

        r.survivalRate = (double)r.gamesSurvived / r.gamesPlayed;
        r.avgScore = std::accumulate(scores.begin(), scores.end(), 0.0) / scores.size();
        r.avgCapital = std::accumulate(capitals.begin(), capitals.end(), 0.0) / capitals.size();
        r.avgCrises = std::accumulate(crises.begin(), crises.end(), 0.0) / crises.size();
        r.avgTurnTimeMs = std::accumulate(turnTimes.begin(), turnTimes.end(), 0.0) / turnTimes.size();
        r.avgLevel /= r.gamesPlayed;

        // Median score
        std::sort(scores.begin(), scores.end());
        r.medianScore = scores[scores.size() / 2];

        // Std dev
        double sumSq = 0;
        for (double s : scores) sumSq += (s - r.avgScore) * (s - r.avgScore);
        r.stdDevScore = std::sqrt(sumSq / scores.size());

        // P95 turn time
        std::sort(turnTimes.begin(), turnTimes.end());
        r.p95TurnTimeMs = turnTimes[(int)(turnTimes.size() * 0.95)];

        // Death aggregates
        if (!deathQuarters.empty()) {
            std::sort(deathQuarters.begin(), deathQuarters.end());
            r.medianDeathQuarter = deathQuarters[deathQuarters.size() / 2];
        }
        if (!deathLeverages.empty()) {
            r.avgLeverageAtDeath =
                std::accumulate(deathLeverages.begin(), deathLeverages.end(), 0.0)
                / deathLeverages.size();
        }
        if (!endReasonCounts.empty()) {
            auto top = std::max_element(endReasonCounts.begin(), endReasonCounts.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            r.mostCommonEndReason = top->first;
        }

        return r;
    }
};

} // namespace stvg::autoplay
