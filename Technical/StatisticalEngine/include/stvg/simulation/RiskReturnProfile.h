#pragma once

#include "../math/RandomEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <cmath>
#include <algorithm>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Standardized Risk/Return Profile
//
// Reusable across: events, deals, investment decisions, business
// line expansion. Every opportunity in the game uses this struct
// to define its risk/reward characteristics. All values are
// modifiable via JSON configuration.
// ════════════════════════════════════════════════════════════════════

struct RiskReturnProfile {
    int riskLevel = 5;              // 1-10 (1=safe, 10=extremely risky)
    double returnMin = -0.10;       // Worst-case return (e.g., -10%)
    double returnMax = 0.20;        // Best-case return (e.g., +20%)
    double successProbability = 0.6; // Chance of positive outcome (0-1)
    int durationQuarters = 2;       // Quarters until resolution
    double failureLoss = 0.15;      // % of capital lost on failure
    double windfallProbability = 0.05; // Chance of outsized return (0-0.2)
    double windfallMultiplier = 3.0;   // Multiplier on returnMax for windfall

    // Generate a random outcome from this profile
    double generateOutcome(math::RandomEngine& rng) const {
        // Check for windfall first (rare but spectacular)
        if (rng.bernoulli(windfallProbability)) {
            // Windfall: return returnMax * multiplier with some noise
            double windfallReturn = returnMax * windfallMultiplier;
            windfallReturn *= (0.8 + rng.uniform() * 0.4); // ±20% noise
            return windfallReturn;
        }

        // Check for success
        if (rng.bernoulli(successProbability)) {
            // Success: return within [returnMin, returnMax] range
            // Skewed toward the middle (not uniform)
            double t = (rng.uniform() + rng.uniform()) / 2.0; // Triangular-ish
            return returnMin + t * (returnMax - returnMin);
        }

        // Failure: lose failureLoss with some variance
        double loss = failureLoss * (0.5 + rng.uniform()); // 50-150% of stated loss
        return -loss;
    }

    // Expected value (for AI decision-making)
    double expectedValue() const {
        double windfallEV = windfallProbability * returnMax * windfallMultiplier;
        double successEV = successProbability * (1 - windfallProbability) * (returnMin + returnMax) / 2.0;
        double failureEV = (1 - successProbability) * (-failureLoss);
        return windfallEV + successEV + failureEV;
    }

    // Variance proxy (for risk assessment)
    double riskScore() const {
        double range = returnMax - returnMin + failureLoss;
        return range * (1.0 - successProbability) * riskLevel / 10.0;
    }
};

// JSON serialization
inline void to_json(nlohmann::json& j, const RiskReturnProfile& r) {
    j = nlohmann::json{
        {"riskLevel", r.riskLevel},
        {"returnRange", {r.returnMin * 100, r.returnMax * 100}}, // Store as percentages
        {"successProbability", r.successProbability},
        {"duration", r.durationQuarters},
        {"failureLoss", r.failureLoss},
        {"windfallProbability", r.windfallProbability},
        {"windfallMultiplier", r.windfallMultiplier}
    };
}

inline void from_json(const nlohmann::json& j, RiskReturnProfile& r) {
    r.riskLevel = j.value("riskLevel", 5);
    if (j.contains("returnRange") && j["returnRange"].is_array() && j["returnRange"].size() >= 2) {
        r.returnMin = j["returnRange"][0].get<double>() / 100.0; // Convert from %
        r.returnMax = j["returnRange"][1].get<double>() / 100.0;
    }
    r.successProbability = j.value("successProbability", 0.6);
    r.durationQuarters = j.value("duration", 2);
    r.failureLoss = j.value("failureLoss", 0.15);
    r.windfallProbability = j.value("windfallProbability", 0.05);
    r.windfallMultiplier = j.value("windfallMultiplier", 3.0);
}

} // namespace stvg::simulation
