#pragma once

#include "Types.h"
#include "../math/RandomEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <algorithm>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// Consequence Tracking System
//
// Every decision generates 1-3 "consequence cards" that resolve in
// future quarters (1-3 quarters later). This creates the "staggered
// timer" mechanic that research shows is the #1 driver of
// "one more turn" syndrome in management sims.
//
// Players always have 2-5 pending consequences creating anticipation.
// ════════════════════════════════════════════════════════════════════

enum class ConsequenceType {
    Financial, Reputational, Operational, Personnel, Regulatory
};
NLOHMANN_JSON_SERIALIZE_ENUM(ConsequenceType, {
    {ConsequenceType::Financial, "financial"},
    {ConsequenceType::Reputational, "reputational"},
    {ConsequenceType::Operational, "operational"},
    {ConsequenceType::Personnel, "personnel"},
    {ConsequenceType::Regulatory, "regulatory"},
})

struct Consequence {
    std::string id;
    std::string sourceDecisionId;     // Which decision caused this
    std::string sourceDecisionTitle;   // For display: "Approved Aggressive Lending"
    std::string title;                 // "Mortgage Defaults Begin"
    std::string description;           // Full narrative text
    ConsequenceType type;
    int createdQuarter;                // When the decision was made
    int resolveQuarter;                // When this consequence fires
    double financialImpact = 0;        // +/- dollars
    double reputationImpact = 0;       // +/- points
    double riskImpact = 0;             // +/- risk level
    double moraleImpact = 0;           // +/- morale points
    bool resolved = false;
    bool positive = false;             // Good news or bad news?
};

inline void to_json(nlohmann::json& j, const Consequence& c) {
    j = nlohmann::json{
        {"id", c.id}, {"sourceDecisionId", c.sourceDecisionId},
        {"sourceDecisionTitle", c.sourceDecisionTitle},
        {"title", c.title}, {"description", c.description},
        {"type", c.type}, {"createdQuarter", c.createdQuarter},
        {"resolveQuarter", c.resolveQuarter},
        {"financialImpact", c.financialImpact},
        {"reputationImpact", c.reputationImpact},
        {"riskImpact", c.riskImpact}, {"moraleImpact", c.moraleImpact},
        {"resolved", c.resolved}, {"positive", c.positive}
    };
}

class ConsequenceTracker {
public:
    explicit ConsequenceTracker(math::RandomEngine& rng) : rng_(rng) {}

    // Generate consequences from a player's decision
    void addFromDecision(const std::string& decisionId,
                          const std::string& decisionTitle,
                          const std::string& optionId,
                          const std::string& optionTitle,
                          double financialImpact,
                          double riskChange,
                          double successProb,
                          int currentQuarter) {

        bool succeeded = rng_.bernoulli(successProb);
        // Delay: 1-4 quarters (weighted toward 2-3 for dramatic pacing)
        int delay = 1 + (int)(rng_.uniform() * rng_.uniform() * 4); // Skewed: more 1-2, fewer 4
        delay = std::clamp(delay, 1, 4);

        // Random magnitude variation (±30% from base) creates variety
        double magnitudeVariance = 0.7 + rng_.uniform() * 0.6; // 0.7 to 1.3

        // Pick from themed consequence pools for variety
        static const std::vector<std::string> positiveFinTitles = {
            "Investment Pays Off", "Returns Exceed Expectations", "Market Validates Your Call",
            "Profitable Quarter Ahead", "Revenue Stream Established", "Growth Thesis Confirmed",
            "Strategic Bet Pays Dividends", "Timing Proves Excellent"};
        static const std::vector<std::string> positiveRiskTitles = {
            "Risk Reduction Working", "Portfolio Strengthened", "Hedges Prove Their Worth",
            "Safety Margins Restored", "Compliance Investment Pays Off", "Risk Culture Improving"};
        static const std::vector<std::string> positiveMoraleTitles = {
            "Decision Bearing Fruit", "Team Rallies Behind Strategy", "Culture Strengthening",
            "Morale Boost From Success", "Confidence Growing Across Divisions"};
        static const std::vector<std::string> negativeFinTitles = {
            "Expected Returns Fall Short", "Market Turns Against Position",
            "Investment Thesis Collapses", "Revenue Shortfall Emerging",
            "Write-Down Required", "Losses Mount Unexpectedly"};
        static const std::vector<std::string> negativeRiskTitles = {
            "Risk Materializes", "Hidden Exposure Surfaces", "Worst-Case Scenario Unfolds",
            "Black Swan Event Hits Portfolio", "Concentration Risk Proves Fatal"};
        static const std::vector<std::string> negativeMoraleTitles = {
            "Unintended Side Effects", "Staff Backlash Intensifies", "Key Departures Follow Decision",
            "Culture Damage Spreading", "Reputational Fallout Continues"};

        auto pickRandom = [&](const std::vector<std::string>& pool) -> std::string {
            return pool[(int)(rng_.uniform() * pool.size()) % pool.size()];
        };

        Consequence c;
        c.id = "csq_" + std::to_string(++counter_) + "_q" + std::to_string(currentQuarter);
        c.sourceDecisionId = decisionId;
        c.sourceDecisionTitle = decisionTitle + " → " + optionTitle;
        c.createdQuarter = currentQuarter;
        c.resolveQuarter = currentQuarter + delay;
        c.positive = succeeded;

        if (succeeded) {
            if (financialImpact > 0) {
                c.title = pickRandom(positiveFinTitles);
                c.description = "Your decision on '" + optionTitle + "' is generating returns.";
                c.type = ConsequenceType::Financial;
                c.financialImpact = financialImpact * 0.8 * magnitudeVariance;
                c.reputationImpact = 2.0 + rng_.uniform() * 4.0;
            } else if (riskChange < 0) {
                c.title = pickRandom(positiveRiskTitles);
                c.description = "Risk mitigation from '" + optionTitle + "' is strengthening your position.";
                c.type = ConsequenceType::Operational;
                c.riskImpact = riskChange * magnitudeVariance;
                c.reputationImpact = 1.5 + rng_.uniform() * 3.0;
            } else {
                c.title = pickRandom(positiveMoraleTitles);
                c.description = "'" + optionTitle + "' is building positive momentum across the bank.";
                c.type = ConsequenceType::Personnel;
                c.moraleImpact = 2.0 + rng_.uniform() * 6.0;
            }
        } else {
            if (financialImpact > 0) {
                c.title = pickRandom(negativeFinTitles);
                c.description = "'" + optionTitle + "' has not delivered. Conditions proved hostile.";
                c.type = ConsequenceType::Financial;
                c.financialImpact = -financialImpact * 0.3 * magnitudeVariance;
                c.reputationImpact = -(1.0 + rng_.uniform() * 3.0);
            } else if (riskChange > 0) {
                c.title = pickRandom(negativeRiskTitles);
                c.description = "The risk from '" + optionTitle + "' has come home to roost.";
                c.type = ConsequenceType::Financial;
                c.financialImpact = riskChange * -5e3 * magnitudeVariance;  // magnitude rescaled /10,000 ($1M-scale)
                c.reputationImpact = -(2.0 + rng_.uniform() * 3.0);
                c.moraleImpact = -(2.0 + rng_.uniform() * 4.0);
            } else {
                c.title = pickRandom(negativeMoraleTitles);
                c.description = "'" + optionTitle + "' had consequences you didn't anticipate.";
                c.type = ConsequenceType::Personnel;
                c.moraleImpact = -(4.0 + rng_.uniform() * 8.0);
                c.reputationImpact = -(1.0 + rng_.uniform() * 3.0);
            }
        }
        pending_.push_back(c);

        // Sometimes add a secondary consequence (30% chance)
        if (rng_.bernoulli(0.3) && std::abs(riskChange) > 0.05) {
            Consequence secondary;
            secondary.id = "csq_" + std::to_string(++counter_) + "_q" + std::to_string(currentQuarter);
            secondary.sourceDecisionId = decisionId;
            secondary.sourceDecisionTitle = decisionTitle;
            secondary.createdQuarter = currentQuarter;
            secondary.resolveQuarter = currentQuarter + delay + 1;  // One quarter after primary
            secondary.type = ConsequenceType::Regulatory;
            secondary.positive = false;

            if (riskChange > 0.1) {
                secondary.title = "Regulators Take Notice";
                secondary.description = "Your aggressive posture has attracted regulatory attention.";
                secondary.reputationImpact = -3.0;
                secondary.riskImpact = 0.05;
            } else {
                secondary.title = "Competitor Response";
                secondary.description = "Competitors have adjusted their strategy in response to your moves.";
                secondary.moraleImpact = -2.0;
            }
            pending_.push_back(secondary);
        }
    }

    // Resolve all consequences due this quarter. Returns resolved consequences.
    std::vector<Consequence> resolveForQuarter(int quarter) {
        std::vector<Consequence> resolved;
        for (auto& c : pending_) {
            if (!c.resolved && c.resolveQuarter <= quarter) {
                c.resolved = true;
                resolved.push_back(c);
            }
        }
        return resolved;
    }

    // Get all pending (unresolved) consequences
    std::vector<Consequence> pendingConsequences() const {
        std::vector<Consequence> result;
        for (const auto& c : pending_) {
            if (!c.resolved) result.push_back(c);
        }
        return result;
    }

    // Get all resolved consequences (history)
    std::vector<Consequence> resolvedHistory() const {
        std::vector<Consequence> result;
        for (const auto& c : pending_) {
            if (c.resolved) result.push_back(c);
        }
        return result;
    }

    int pendingCount() const {
        int count = 0;
        for (const auto& c : pending_) if (!c.resolved) ++count;
        return count;
    }

    // Cleanup old resolved consequences (keep last 8 quarters)
    void cleanup(int currentQuarter) {
        pending_.erase(
            std::remove_if(pending_.begin(), pending_.end(),
                [&](const Consequence& c) {
                    return c.resolved && (currentQuarter - c.resolveQuarter) > 8;
                }),
            pending_.end()
        );
    }

private:
    math::RandomEngine& rng_;
    std::vector<Consequence> pending_;
    int counter_ = 0;
};

} // namespace stvg
