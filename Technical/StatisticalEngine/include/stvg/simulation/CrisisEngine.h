#pragma once

#include "../core/Types.h"
#include "../core/BankState.h"
#include "../math/RandomEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <cmath>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Crisis Engine — multi-turn crisis arcs
// Crises unfold over 2-4 quarters with warning signs, peak, and aftermath.
// ════════════════════════════════════════════════════════════════════

struct CrisisArc {
    std::string id;
    CrisisType type;
    std::string title;
    std::string description;
    int severity = 1;           // 1-10, escalates each quarter unresolved
    int startQuarter = 0;
    int quartersActive = 0;
    bool resolved = false;
    bool playerAware = false;   // Has the player been notified?

    // Escalate severity each quarter
    void escalate() {
        if (!resolved) {
            quartersActive++;
            severity = std::min(severity + 2, 10);
        }
    }

    // Financial impact scales linearly with severity AND hidden risk.
    // Severity 1 = 0.5% of capital, severity 10 = 5% of capital.
    // Hidden risk amplifies damage modestly.
    double financialImpact(double bankCapital, double hiddenRiskRatio = 0.0) const {
        double basePct = 0.005 * severity; // Linear: 0.5% to 5%
        double hiddenMult = 1.0 + hiddenRiskRatio * 5.0;
        return bankCapital * basePct * hiddenMult;
    }

    // Reputation damage (scaled down from 1.5 to 0.8 per severity — original was too aggressive,
    // causing 0% 95-year survival across all bot strategies)
    double reputationDamage() const {
        return severity * 0.8; // 0.8-8 points per quarter
    }
};

inline void to_json(nlohmann::json& j, const CrisisArc& c) {
    j = nlohmann::json{
        {"id", c.id}, {"type", c.type}, {"title", c.title},
        {"description", c.description}, {"severity", c.severity},
        {"startQuarter", c.startQuarter},
        {"quartersActive", c.quartersActive}, {"resolved", c.resolved}
    };
}
inline void from_json(const nlohmann::json& j, CrisisArc& c) {
    c.id = j.value("id", ""); c.title = j.value("title", "");
    c.description = j.value("description", "");
    if (j.contains("type")) c.type = j["type"].get<CrisisType>();
    c.severity = j.value("severity", 1);
    c.startQuarter = j.value("startQuarter", 0);
    c.quartersActive = j.value("quartersActive", 0);
    c.resolved = j.value("resolved", false);
}

class CrisisEngine {
public:
    explicit CrisisEngine(math::RandomEngine& rng) : rng_(rng) {}

    // Check if a new crisis triggers this quarter
    // suppressRegulatory: Webb's "Political Cover" blocks regulatory crises
    std::optional<CrisisArc> checkTrigger(const Bank& bank,
                                           const SimulationState& state,
                                           bool suppressRegulatory = false,
                                           double ceoRiskMultiplier = 1.0) {
        double baseProb = baseCrisisProbability(state.regime);
        // hiddenRisk is 0-1 per division — high hidden risk makes crises likely
        double hiddenRiskMod = std::min(bank.totalHiddenRisk() * 0.20, 0.40);
        double autonomyMod = 0;
        for (const auto& d : bank.divisions) {
            double weight = (d.type == DivisionType::TradingDesk || d.type == DivisionType::DerivativesDesk) ? 0.05 : 0.03;
            autonomyMod += d.autonomyLevel * weight;
        }

        // CEO risk personality affects crisis probability
        double triggerProb = (baseProb + hiddenRiskMod + autonomyMod) * ceoRiskMultiplier;
        triggerProb = std::clamp(triggerProb, 0.01, 0.5);

        if (!rng_.bernoulli(triggerProb)) {
            return std::nullopt;
        }

        return generateCrisis(bank, state, suppressRegulatory);
    }

    // Escalate all active crises
    void escalateActive() {
        for (auto& crisis : activeCrises_) {
            if (!crisis.resolved) {
                crisis.escalate();
            }
        }
    }

    // Apply crisis resolution from player decision.
    // ceoStressResponse (0..1) modulates how well aggressive vs. measured plays:
    //   high stressResponse = doubles down well → aggressive cuts harder
    //   low  stressResponse = freezes → measured cuts more (and aggressive backfires)
    void resolveWithResponse(const std::string& crisisId,
                             const std::string& responseType,
                             double ceoStressResponse = 0.5) {
        for (auto& crisis : activeCrises_) {
            if (crisis.id == crisisId) {
                if (responseType == "aggressive") {
                    // base -4, scaled by (0.5 + stressResponse) → -2 to -6
                    int cut = (int)std::round(4.0 * (0.5 + ceoStressResponse));
                    crisis.severity = std::max(crisis.severity - cut, 0);
                    if (crisis.severity <= 1) crisis.resolved = true;
                } else if (responseType == "measured") {
                    // base -2, scaled by (1.5 - stressResponse) → -1 to -3
                    int cut = (int)std::round(2.0 * (1.5 - ceoStressResponse));
                    crisis.severity = std::max(crisis.severity - cut, 0);
                    if (crisis.severity <= 1) crisis.resolved = true;
                } else { // gamble
                    // 30% chance it resolves, 70% it gets worse
                    if (rng_.bernoulli(0.3)) {
                        crisis.severity = std::max(crisis.severity - 3, 0);
                        crisis.resolved = true;
                    } else {
                        crisis.severity = std::min(crisis.severity + 2, 10);
                    }
                }
            }
        }
    }

    // Apply financial impact of all active crises (scaled by hidden risk)
    double totalFinancialImpact(double bankCapital, double hiddenRiskRatio = 0.0) const {
        double total = 0;
        for (const auto& c : activeCrises_) {
            if (!c.resolved) total += c.financialImpact(bankCapital, hiddenRiskRatio);
        }
        return total;
    }

    // Apply reputation damage from all active crises
    double totalReputationDamage() const {
        double total = 0;
        for (const auto& c : activeCrises_) {
            if (!c.resolved) total += c.reputationDamage();
        }
        return total;
    }

    const std::vector<CrisisArc>& activeCrises() const { return activeCrises_; }
    std::vector<CrisisArc>& activeCrises() { return activeCrises_; }

    nlohmann::json toSaveJson() const { return {{"activeCrises", activeCrises_}}; }
    void loadSaveJson(const nlohmann::json& j) {
        activeCrises_.clear();
        if (j.contains("activeCrises")) {
            for (const auto& c : j["activeCrises"]) activeCrises_.push_back(c.get<CrisisArc>());
        }
    }

    bool hasActiveCrisis() const {
        for (const auto& c : activeCrises_) {
            if (!c.resolved) return true;
        }
        return false;
    }

    // Phase 4 A1: max severity of active crises (for variable loss caps)
    int maxSeverity() const {
        int max = 0;
        for (const auto& c : activeCrises_) {
            if (!c.resolved && c.severity > max) max = c.severity;
        }
        return max;
    }

    // Clean up resolved crises older than 2 quarters
    void cleanup(int currentQuarter) {
        activeCrises_.erase(
            std::remove_if(activeCrises_.begin(), activeCrises_.end(),
                [&](const CrisisArc& c) {
                    return c.resolved && (currentQuarter - c.startQuarter) > 2;
                }),
            activeCrises_.end()
        );
    }

private:
    math::RandomEngine& rng_;
    std::vector<CrisisArc> activeCrises_;
    int crisisCounter_ = 0;

    // Empirically calibrated: real crisis ~1 per 7-10 years in normal times
    double baseCrisisProbability(MarketRegime regime) const {
        switch (regime) {
            case MarketRegime::Calm:     return 0.005; // 0.5% per quarter
            case MarketRegime::Normal:   return 0.01;  // 1% per quarter (~4%/year)
            case MarketRegime::Cautious: return 0.03;  // 3% per quarter
            case MarketRegime::Stressed: return 0.05;  // 5% per quarter
            case MarketRegime::Crisis:   return 0.15;  // 15% per quarter
        }
        return 0.01;
    }

    CrisisArc generateCrisis(const Bank& bank, const SimulationState& state,
                             bool suppressRegulatory = false) {
        CrisisArc crisis;
        crisis.id = "crisis_" + std::to_string(++crisisCounter_);
        crisis.startQuarter = state.currentQuarter;
        crisis.severity = 3 + (int)(rng_.uniform() * 4); // Start at 3-6

        // Choose crisis type based on bank vulnerabilities
        double roll = rng_.uniform();
        const auto* tradingDiv = bank.getDivisionByType(DivisionType::TradingDesk);
        const auto* commercialDiv = bank.getDivisionByType(DivisionType::CommercialLending);
        if (tradingDiv && tradingDiv->hiddenRisk > 0.5e9 && roll < 0.3) {
            crisis.type = CrisisType::Concentration;
            crisis.title = "Trading Position Blowup";
            crisis.description = "Hidden concentrated positions in the trading book are unwinding. "
                "Losses are accelerating and counterparties are demanding margin.";
        } else if (commercialDiv && commercialDiv->actualRisk > 0.3 && roll < 0.5) {
            crisis.type = CrisisType::Credit;
            crisis.title = "Loan Portfolio Deterioration";
            crisis.description = "Non-performing loans have spiked. Several major commercial borrowers "
                "are in distress. Provisions may need to increase significantly.";
        } else if (state.economics.vix > 30 && roll < 0.7) {
            crisis.type = CrisisType::Liquidity;
            crisis.title = "Liquidity Squeeze";
            crisis.description = "Wholesale funding markets have tightened. Your treasury desk "
                "is struggling to roll short-term obligations. Depositors are getting nervous.";
        } else if (roll < 0.85 && !suppressRegulatory) {
            crisis.type = CrisisType::Regulatory;
            crisis.title = "Regulatory Enforcement Action";
            crisis.description = "Regulators have identified significant compliance deficiencies. "
                "A consent order is being drafted that will restrict certain business activities.";
        } else {
            // Confidence crisis (also fallback when regulatory is suppressed)
            crisis.type = CrisisType::Confidence;
            crisis.title = "Confidence Crisis";
            crisis.description = "Media reports questioning the bank's risk management practices "
                "have gone viral. Depositors are lining up at branches. Stock price is falling.";
        }

        activeCrises_.push_back(crisis);
        return crisis;
    }
};

} // namespace stvg::simulation
