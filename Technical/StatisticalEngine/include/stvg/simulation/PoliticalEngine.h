#pragma once

#include "../core/Types.h"
#include "../math/RandomEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Political Engine — lobbying, political capital, regulatory influence
//
// Political capital is a resource that can be earned (meetings, PAC
// contributions, testimony) and spent (lobbying, regulatory evasion).
// Effectiveness is heavily era-dependent: deregulation eras are
// golden for lobbying; post-crisis eras are hostile.
// ════════════════════════════════════════════════════════════════════

struct PoliticalState {
    double politicalCapital = 0;          // 0-100 (earned/spent resource)
    double publicOpinion = 0;             // -1.0 (hostile) to 1.0 (supportive of banks)
    double lobbyingEffectiveness = 1.0;   // Era modifier (0.5 = post-crisis, 1.5 = deregulation)
    int successfulLobbies = 0;
    int failedLobbies = 0;
    int totalSpent = 0;
    std::string currentEra = "post_war";
};

inline void to_json(nlohmann::json& j, const PoliticalState& s) {
    j = nlohmann::json{
        {"politicalCapital", s.politicalCapital},
        {"publicOpinion", s.publicOpinion},
        {"lobbyingEffectiveness", s.lobbyingEffectiveness},
        {"successfulLobbies", s.successfulLobbies},
        {"failedLobbies", s.failedLobbies},
        {"totalSpent", s.totalSpent},
        {"currentEra", s.currentEra}
    };
}

class PoliticalEngine {
public:
    explicit PoliticalEngine(math::RandomEngine& rng) : rng_(rng) {}

    // ── Core Actions ────────────────────────────────────────────

    // Attempt to lobby against a regulation. Returns true if successful.
    // Cost: political capital spent regardless of outcome.
    // Success probability: base * effectiveness * opinion mod * capital mod * personality mod
    // ceoPoliticalSavvy (0..1) scales success by 0.5x to 1.4x — Webb-style insiders
    // dramatically outperform compliance-focused CEOs at the lobbying game.
    bool lobbyRegulation(const std::string& regulationId, double investment,
                         double ceoPoliticalSavvy = 0.5) {
        double capitalCost = 5.0 + investment / 1e6; // $1M = 1 point
        state_.politicalCapital = std::max(0.0, state_.politicalCapital - capitalCost);
        state_.totalSpent++;

        double baseProb = 0.45;
        double capitalMod = std::min(state_.politicalCapital / 50.0, 1.5); // More capital = better odds
        double opinionMod = 0.8 + state_.publicOpinion * 0.4; // -1 opinion = 0.4x, +1 = 1.2x
        double personalityMod = 0.5 + ceoPoliticalSavvy * 0.9; // 0.5x to 1.4x
        double prob = std::clamp(baseProb * state_.lobbyingEffectiveness * capitalMod * opinionMod * personalityMod,
                                  0.05, 0.95);

        bool success = rng_.bernoulli(prob);
        if (success) {
            state_.successfulLobbies++;
            spdlog::debug("Lobby SUCCESS against {} (prob {:.1f}%)", regulationId, prob * 100);
        } else {
            state_.failedLobbies++;
            // Failed lobbying attracts negative attention
            state_.publicOpinion = std::max(-1.0, state_.publicOpinion - 0.05);
            spdlog::debug("Lobby FAILED against {} (prob {:.1f}%)", regulationId, prob * 100);
        }
        return success;
    }

    // Earn political capital through various activities
    void earnCapital(double amount, const std::string& reason = "") {
        state_.politicalCapital = std::min(100.0, state_.politicalCapital + amount);
        if (!reason.empty()) {
            spdlog::debug("Political capital +{:.1f}: {}", amount, reason);
        }
    }

    // Spend political capital (for specific actions like CEO abilities)
    void spendCapital(double amount) {
        state_.politicalCapital = std::max(0.0, state_.politicalCapital - amount);
    }

    // Check if we have enough capital for an action
    bool canAfford(double cost) const {
        return state_.politicalCapital >= cost;
    }

    // Can we attempt to evade a specific regulation?
    bool canEvadeRegulation(const std::string& regulationId, double requiredCapital = 15.0) const {
        return state_.politicalCapital >= requiredCapital
            && state_.lobbyingEffectiveness >= 0.5;
    }

    // Cost to regulate AI (scales with AI advancement)
    double costToRegulateAI(double aiEfficiency) const {
        return 5.0 + aiEfficiency * 2.0;
    }

    // ── Era Configuration ───────────────────────────────────────

    void setEraModifier(const std::string& eraId) {
        state_.currentEra = eraId;
        if      (eraId == "post_war")   state_.lobbyingEffectiveness = 0.8;
        else if (eraId == "expansion")  state_.lobbyingEffectiveness = 1.0;
        else if (eraId == "volatility") state_.lobbyingEffectiveness = 1.2;
        else if (eraId == "big_bang")   state_.lobbyingEffectiveness = 1.5; // Peak deregulation
        else if (eraId == "shadow")     state_.lobbyingEffectiveness = 1.3;
        else if (eraId == "reform")     state_.lobbyingEffectiveness = 0.5; // Post-crisis crackdown
        else if (eraId == "modern")     state_.lobbyingEffectiveness = 0.7;
        else                            state_.lobbyingEffectiveness = 1.0;

        spdlog::info("Political era set to {}: lobbying effectiveness = {:.1f}",
            eraId, state_.lobbyingEffectiveness);
    }

    // ── Annual Simulation ───────────────────────────────────────

    void simulateYear(int year, double bankReputation, bool crisisActive) {
        // Political capital naturally decays (relationships fade)
        state_.politicalCapital *= 0.90; // 10% annual decay

        // Reputation feeds into political influence
        if (bankReputation > 60.0) {
            earnCapital(2.0, "high bank reputation");
        }

        // Public opinion drifts based on crises and era
        if (crisisActive) {
            // Banking crises reduce public support
            state_.publicOpinion = std::max(-1.0, state_.publicOpinion - 0.15);
        } else {
            // Slow recovery toward neutral
            state_.publicOpinion += (0.0 - state_.publicOpinion) * 0.1;
        }

        // Clamp
        state_.publicOpinion = std::clamp(state_.publicOpinion, -1.0, 1.0);
    }

    // ── Accessors ───────────────────────────────────────────────

    double currentCapital() const { return state_.politicalCapital; }
    double publicOpinion() const { return state_.publicOpinion; }
    double effectiveness() const { return state_.lobbyingEffectiveness; }
    const PoliticalState& state() const { return state_; }

private:
    math::RandomEngine& rng_;
    PoliticalState state_;
};

} // namespace stvg::simulation
