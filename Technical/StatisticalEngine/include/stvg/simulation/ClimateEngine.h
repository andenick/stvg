#pragma once

#include "../core/Types.h"
#include "../core/BankState.h"
#include "../math/RandomEngine.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Climate Engine — parallel endgame threat. Physical risk and
// transition risk grow over time, competing with AI singularity
// for attention and resources.
// ════════════════════════════════════════════════════════════════════

struct ClimateState {
    double globalTemp = 1.1;            // °C above pre-industrial
    double carbonPrice = 0;             // $/ton
    double physicalRiskMult = 1.0;      // Scales weather damage
    double transitionRiskMult = 1.0;    // Scales stranded asset losses
    int tippingPointsReached = 0;       // 0-5
    double annualDamage = 0;            // $ damage this year
    double fossilExposureReduction = 0; // 0-1, how much player reduced exposure
    double esgBondRevenue = 0;          // Quarterly revenue from ESG bonds
};

inline void to_json(nlohmann::json& j, const ClimateState& s) {
    j = nlohmann::json{
        {"globalTemp", s.globalTemp}, {"carbonPrice", s.carbonPrice},
        {"physicalRiskMult", s.physicalRiskMult},
        {"transitionRiskMult", s.transitionRiskMult},
        {"tippingPointsReached", s.tippingPointsReached},
        {"annualDamage", s.annualDamage},
        {"fossilExposureReduction", s.fossilExposureReduction},
        {"esgBondRevenue", s.esgBondRevenue}
    };
}

class ClimateEngine {
public:
    explicit ClimateEngine(math::RandomEngine& rng) : rng_(rng) {}

    void simulateYear(int year) {
        if (year < 2020) return; // Climate effects minimal before 2020

        int yearsFrom2020 = year - 2020;

        // Temperature: rises ~0.03°C/year (accelerating with tipping points)
        double accel = 1.0 + state_.tippingPointsReached * 0.2;
        state_.globalTemp = 1.1 + yearsFrom2020 * 0.03 * accel;

        // Carbon price: ramps up over time
        state_.carbonPrice = std::min(500.0, yearsFrom2020 * 7.5);

        // Physical risk multiplier: exponential with warming
        state_.physicalRiskMult = std::pow(state_.globalTemp / 1.5, 2.0);

        // Transition risk: proportional to carbon price
        state_.transitionRiskMult = 1.0 + state_.carbonPrice / 200.0;

        // Tipping point check (probabilistic, based on temperature)
        if (state_.globalTemp > 1.5 && state_.tippingPointsReached < 5) {
            double tipProb = (state_.globalTemp - 1.5) * 0.05;
            if (rng_.bernoulli(tipProb)) {
                state_.tippingPointsReached++;
                spdlog::warn("CLIMATE TIPPING POINT #{} reached (temp: {:.2f}°C)",
                    state_.tippingPointsReached, state_.globalTemp);
            }
        }
    }

    double physicalDamage(const Bank& bank) const {
        // Damage scales with bank size and physical risk
        double baseDamage = bank.totalAssets * 0.001; // 0.1% base annual damage
        return baseDamage * state_.physicalRiskMult;
    }

    double strandedAssetLoss(const Bank& bank) const {
        // Fossil fuel lending exposure (reduced by player mitigation actions)
        double exposurePct = effectiveFossilExposure();
        double fossilExposure = 0;
        for (const auto& div : bank.divisions) {
            if (div.type == DivisionType::CommercialLending) {
                fossilExposure += div.revenue * exposurePct;
            }
        }
        return fossilExposure * state_.carbonPrice / 100.0;
    }

    double insuranceCostIncrease() const {
        return (state_.physicalRiskMult - 1.0) * 0.01; // % increase in operating costs
    }

    bool isTippingPointYear() const {
        return state_.tippingPointsReached > 0; // Simplified
    }

    // ── Player Mitigation Actions ─────────────────────────────
    void applyGreenLendingShift(double pctShift) {
        state_.fossilExposureReduction = std::min(1.0, state_.fossilExposureReduction + pctShift);
        spdlog::info("Green lending shift: fossil exposure reduced by {:.0f}%", pctShift * 100);
    }

    void applyESGBondProgram(double investment) {
        state_.esgBondRevenue = investment * 0.05 * (1.0 + state_.carbonPrice / 200.0);
        spdlog::info("ESG bond program: generating ${:.0f}M/quarter", state_.esgBondRevenue / 1e6);
    }

    void applyDivestment(double pctDivested) {
        state_.fossilExposureReduction = std::min(1.0, state_.fossilExposureReduction + pctDivested);
        spdlog::info("Fossil divestment: {:.0f}% exposure eliminated", pctDivested * 100);
    }

    double greenBondRevenue() const { return state_.esgBondRevenue; }

    double effectiveFossilExposure() const {
        return std::max(0.0, 0.05 * (1.0 - state_.fossilExposureReduction));
    }

    const ClimateState& state() const { return state_; }

private:
    math::RandomEngine& rng_;
    ClimateState state_;
};

} // namespace stvg::simulation
