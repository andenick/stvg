#pragma once

#include "../core/Types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <cmath>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// AI CEO Engine — endgame mechanic. An AI-driven bank emerges when
// global AI investment crosses a threshold, then grows exponentially
// more efficient, capturing market share and driving toward singularity.
// ════════════════════════════════════════════════════════════════════

struct AICEOState {
    std::string hostBankId;             // Which competitor bank hosts the AI
    double aiInvestmentGlobal = 0;      // Cumulative global AI spend ($B)
    double aiInvestmentThreshold = 500; // Emergence threshold ($B)
    double efficiency = 1.0;            // 1.0 = human, grows to 5.0+
    double marketCapture = 0;           // % of total market
    int singularityProgress = 0;        // 0-100 (100 = singularity)
    int emergenceYear = -1;
    bool emerged = false;
    double annualEfficiencyGain = 0.2;  // +0.2/year after emergence
    double playerAICapability = 0;      // Player's own AI capability
    double regulatoryDrag = 0;          // 0-0.8, slows AI efficiency growth
    bool partnershipActive = false;     // Partnered with AI bank?
    double partnershipRevenue = 0;      // Revenue from AI partnership
};

inline void to_json(nlohmann::json& j, const AICEOState& s) {
    j = nlohmann::json{
        {"hostBankId", s.hostBankId},
        {"aiInvestmentGlobal", s.aiInvestmentGlobal},
        {"efficiency", s.efficiency},
        {"marketCapture", s.marketCapture},
        {"singularityProgress", s.singularityProgress},
        {"emergenceYear", s.emergenceYear},
        {"emerged", s.emerged},
        {"playerAICapability", s.playerAICapability},
        {"regulatoryDrag", s.regulatoryDrag},
        {"partnershipActive", s.partnershipActive},
        {"partnershipRevenue", s.partnershipRevenue}
    };
}

class AICEOEngine {
public:
    void recordAIInvestment(double amountBillions) {
        state_.aiInvestmentGlobal += amountBillions;
    }

    void simulateYear(int year) {
        if (year < 2015) return; // AI investment not meaningful before 2015

        // Check emergence
        if (!state_.emerged && state_.aiInvestmentGlobal >= state_.aiInvestmentThreshold) {
            state_.emerged = true;
            state_.emergenceYear = year;
            state_.hostBankId = "novapay"; // Default host
            spdlog::warn("AI CEO EMERGES in {} — global AI investment: ${:.0f}B",
                year, state_.aiInvestmentGlobal);
        }

        if (!state_.emerged) return;

        // Efficiency grows each year (reduced by regulatory drag from player lobbying)
        state_.efficiency += state_.annualEfficiencyGain * (1.0 - state_.regulatoryDrag);

        // Partnership revenue scales with AI efficiency
        if (state_.partnershipActive) {
            state_.partnershipRevenue = state_.efficiency * 0.5e5; // rescaled /10,000
        }

        // Market capture: steals 2% * efficiency per year from all banks
        state_.marketCapture = std::min(0.95,
            state_.marketCapture + 0.02 * state_.efficiency);

        // Singularity progress
        if (state_.efficiency > 5.0 && state_.marketCapture > 0.6) {
            state_.singularityProgress = 100;
        } else {
            state_.singularityProgress = std::min(99,
                (int)(state_.efficiency / 5.0 * 50.0 + state_.marketCapture / 0.6 * 50.0));
        }
    }

    bool hasEmerged() const { return state_.emerged; }
    double efficiency() const { return state_.efficiency; }
    double marketCapture() const { return state_.marketCapture; }
    int singularityProgress() const { return state_.singularityProgress; }
    bool singularityReached() const { return state_.singularityProgress >= 100; }

    double costToMatchAI() const {
        return state_.efficiency * 1e6; // rescaled /10,000 to keep pace
    }
    double costToRegulateAI() const {
        return 5.0 + state_.efficiency * 2.0; // Political capital cost
    }

    // ── Player Countermeasures ────────────────────────────────
    void applyPlayerInvestment(double amountBillions) {
        state_.aiInvestmentGlobal += amountBillions;
        state_.playerAICapability += amountBillions * 0.1;
        spdlog::info("Player AI investment: +${:.0f}B (capability now {:.2f})",
            amountBillions, state_.playerAICapability);
    }

    void applyRegulation(double effectiveness) {
        state_.regulatoryDrag = std::min(0.8, state_.regulatoryDrag + effectiveness);
        spdlog::info("AI regulation applied: drag now {:.2f}", state_.regulatoryDrag);
    }

    void applyPartnership() {
        state_.partnershipActive = true;
        spdlog::info("Partnership with AI bank established");
    }

    const AICEOState& state() const { return state_; }

private:
    AICEOState state_;
};

} // namespace stvg::simulation
