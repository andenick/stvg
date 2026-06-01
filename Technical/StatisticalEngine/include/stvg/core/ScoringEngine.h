#pragma once

#include "Types.h"
#include "BankState.h"
#include <nlohmann/json.hpp>
#include <string>
#include <cmath>
#include <algorithm>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// Scoring + Player Progression
// ════════════════════════════════════════════════════════════════════

struct QuarterScore {
    double financialScore = 0;    // 50% weight
    double riskScore = 0;         // 25% weight
    double stakeholderScore = 0;  // 25% weight
    double total = 0;
    int xpEarned = 0;

    void calculate(const Bank& bank, bool hadCrisis, int decisionsCorrect, int decisionsTotal,
                   double startingCapital = 10e9, int currentEraIndex = 0) {
        // Financial: log growth scaling (prevents ceiling at moderate growth) + ROE
        double capitalGrowthPct = (bank.capital - startingCapital) / startingCapital * 100.0;
        double roe = (bank.capital > 0) ? bank.netIncome / bank.capital * 100.0 : 0.0;
        double growthComponent = (capitalGrowthPct > 0)
            ? std::log1p(capitalGrowthPct) * 15.0  // ln(1+50%)*15=6.1, ln(1+200%)*15=16.5
            : capitalGrowthPct * 2.0;               // Negative growth penalized linearly
        financialScore = std::clamp(10.0 + growthComponent + roe * 5.0, 0.0, 100.0);

        // Risk: per-division average hidden risk (0-1 range, not divided by capital)
        int divCount = std::max((int)bank.divisions.size(), 1);
        double avgHiddenRisk = bank.totalHiddenRisk() / divCount;
        riskScore = std::clamp(100.0 - avgHiddenRisk * 120.0, 0.0, 100.0);

        // Stakeholder: reputation-dominant (70%), morale secondary (30%)
        double avgMorale = bank.averageMorale();
        stakeholderScore = std::clamp(bank.reputation * 0.7 + avgMorale * 0.3, 0.0, 100.0);

        // Decision quality feeds into score (not just XP)
        double decisionBonus = 0.0;
        if (decisionsTotal > 0) {
            decisionBonus = ((double)decisionsCorrect / decisionsTotal) * 15.0;
        }

        // Era progression bonus: reward reaching later eras (0-15 points)
        double eraBonus = currentEraIndex * 2.5;

        // Weighted total: 35% financial + 25% risk + 20% stakeholder + 15% decisions + era bonus
        total = financialScore * 0.35 + riskScore * 0.25 + stakeholderScore * 0.20
              + decisionBonus + eraBonus;

        // Crisis quarter penalty
        if (hadCrisis) total *= 0.85;
        total = std::clamp(total, 0.0, 100.0);

        // XP calculation
        xpEarned = (int)(total * 0.5);
        if (!hadCrisis) xpEarned += 20;
        if (decisionsTotal > 0) {
            double accuracy = (double)decisionsCorrect / decisionsTotal;
            xpEarned += (int)(accuracy * 30);
        }
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(QuarterScore,
    financialScore, riskScore, stakeholderScore, total, xpEarned)

struct PlayerProgression {
    int level = 1;
    int experience = 0;
    int totalExperience = 0;
    int skillPoints = 0;
    std::string title = "Teller";

    int xpToNextLevel() const {
        return (int)(100.0 * std::pow(1.5, level - 1));
    }

    void addExperience(int xp) {
        experience += xp;
        totalExperience += xp;
        while (experience >= xpToNextLevel()) {
            experience -= xpToNextLevel();
            level++;
            skillPoints += 2;
            updateTitle();
        }
    }

    void updateTitle() {
        if (level >= 50) title = "Financial Mastermind";
        else if (level >= 40) title = "Wall Street Legend";
        else if (level >= 30) title = "Banking Mogul";
        else if (level >= 25) title = "CEO";
        else if (level >= 20) title = "Bank President";
        else if (level >= 15) title = "Chief Risk Officer";
        else if (level >= 10) title = "Executive VP";
        else if (level >= 7) title = "Senior VP";
        else if (level >= 5) title = "Vice President";
        else if (level >= 3) title = "Bank Manager";
        else title = "Junior Banker";
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerProgression,
    level, experience, totalExperience, skillPoints, title)

} // namespace stvg
