#pragma once

#include "../core/Types.h"
#include "../core/GameConfig.h"
#include "../math/RandomEngine.h"
#include <cmath>
#include <algorithm>
#include <deque>
#include <spdlog/spdlog.h>

namespace stvg::simulation {

// Simulates macroeconomic indicators using mean-reverting (Ornstein-Uhlenbeck) processes.
// dX = theta * (mu - X) * dt + sigma * sqrt(dt) * Z
class EconomicEngine {
public:
    explicit EconomicEngine(math::RandomEngine& rng) : rng_(rng) {}

    // Initialize to sensible starting conditions
    void init(const EconomicIndicators& initial) {
        state_ = initial;
    }

    // v3: overwrite state without ticking RNG. Used by the experimental
    // economy-model injection seam — alternative models compute next state
    // externally and push it in here. Behavior-preserving when unused.
    void setState(const EconomicIndicators& s) { state_ = s; }

    // Set era-specific OU parameters (called on era transition)
    void setParams(const GameConfig::EconomyParams& params) {
        params_ = params;
        spdlog::info("EconomicEngine params updated: fed={:.1f}% gdp={:.1f}% vix={:.0f}",
            params_.fedFunds.mu * 100, params_.gdpGrowth.mu * 100, params_.vix.mu);
    }

    // SFC Phase C: credit channel — system-wide lending growth feeds into GDP.
    // P3.2: also mirror into the public indicator so it serializes/streams.
    void setCreditImpulse(double impulse) {
        creditImpulse_ = impulse;
        state_.creditImpulse = impulse;
    }
    double creditImpulse() const { return creditImpulse_; }

    // Advance one trading day (dt = 1/252 year)
    void tick(double dt = 1.0 / 252.0) {
        // ── Fed Funds Rate: Taylor Rule target ──────────────────
        // Taylor: r* + 1.5*(inflation - 2%) + 0.5*(GDP - potential)
        double taylorTarget = params_.fedFunds.mu
            + 1.5 * (state_.cpiInflation - 0.02)
            + 0.5 * (state_.gdpGrowth - params_.gdpGrowth.mu);
        taylorTarget = std::clamp(taylorTarget, params_.fedFunds.floor, params_.fedFunds.ceiling);
        state_.fedFundsRate = ornsteinUhlenbeck(
            state_.fedFundsRate, taylorTarget, params_.fedFunds.theta,
            params_.fedFunds.sigma, dt);
        state_.fedFundsRate = std::clamp(state_.fedFundsRate,
            params_.fedFunds.floor, params_.fedFunds.ceiling);

        // Store in lag buffer for inflation feedback (18-month delay)
        fedFundsHistory_.push_back(state_.fedFundsRate);
        if (static_cast<int>(fedFundsHistory_.size()) > FED_LAG_DAYS)
            fedFundsHistory_.pop_front();

        // ── CPI Inflation: responds to lagged Fed Funds + credit channel ──
        double laggedFed = fedFundsHistory_.size() >= static_cast<size_t>(FED_LAG_DAYS)
            ? fedFundsHistory_.front() : state_.fedFundsRate;
        double inflationTarget = params_.inflation.mu
            - 0.3 * (laggedFed - params_.fedFunds.mu)
            + 0.1 * creditImpulse_;  // SFC Phase C: credit expansion is inflationary
        inflationTarget = std::clamp(inflationTarget, params_.inflation.floor, params_.inflation.ceiling);
        state_.cpiInflation = ornsteinUhlenbeck(
            state_.cpiInflation, inflationTarget, params_.inflation.theta,
            params_.inflation.sigma, dt);
        state_.cpiInflation = std::clamp(state_.cpiInflation,
            params_.inflation.floor, params_.inflation.ceiling);

        // ── GDP Growth: depressed by high VIX (wealth channel) + credit channel ──
        double gdpTarget = params_.gdpGrowth.mu
            - std::max(0.0, (state_.vix - 20.0)) * 0.001
            + 0.5 * creditImpulse_;  // Sprint 4: strengthened credit channel
        state_.gdpGrowth = ornsteinUhlenbeck(
            state_.gdpGrowth, gdpTarget, params_.gdpGrowth.theta,
            params_.gdpGrowth.sigma, dt);
        state_.gdpGrowth = std::clamp(state_.gdpGrowth,
            params_.gdpGrowth.floor, params_.gdpGrowth.ceiling);

        // ── Unemployment: Okun's Law (inversely related to GDP) ─
        double okunTarget = params_.unemployment.mu - 0.5 * (state_.gdpGrowth - params_.gdpGrowth.mu);
        state_.unemployment = ornsteinUhlenbeck(
            state_.unemployment, okunTarget, params_.unemployment.theta,
            params_.unemployment.sigma, dt);
        state_.unemployment = std::clamp(state_.unemployment,
            params_.unemployment.floor, params_.unemployment.ceiling);

        // ── VIX ─────────────────────────────────────────────────
        state_.vix = ornsteinUhlenbeck(
            state_.vix, params_.vix.mu, params_.vix.theta, params_.vix.sigma, dt);
        state_.vix = std::clamp(state_.vix, params_.vix.floor, params_.vix.ceiling);

        // ── 10Y Treasury Yield: Fed Funds + term premium ────────
        double termPremium = 0.01 + 0.5 * state_.cpiInflation;
        state_.treasuryYield10Y = ornsteinUhlenbeck(
            state_.treasuryYield10Y, state_.fedFundsRate + termPremium,
            params_.treasury10Y.theta, params_.treasury10Y.sigma, dt);
        state_.treasuryYield10Y = std::clamp(state_.treasuryYield10Y,
            params_.treasury10Y.floor, params_.treasury10Y.ceiling);

        // ── Credit Spread: VIX + unemployment + recession + leverage ──
        double spreadTarget = params_.creditSpread.mu
            + 0.002 * (state_.vix - params_.vix.mu)
            + 0.5 * (state_.unemployment - params_.unemployment.mu)
            + std::max(0.0, -state_.gdpGrowth) * 5.0
            - 0.1 * creditImpulse_;  // SFC Phase C: credit contraction widens spreads
        state_.creditSpread = ornsteinUhlenbeck(
            state_.creditSpread, std::max(spreadTarget, 0.005),
            params_.creditSpread.theta, params_.creditSpread.sigma, dt);
        state_.creditSpread = std::clamp(state_.creditSpread,
            params_.creditSpread.floor, params_.creditSpread.ceiling);

        // ── S&P 500 daily return: GDP drift + VIX volatility ────
        double equityDrift = state_.gdpGrowth - 0.5 * (state_.vix / 100.0) * (state_.vix / 100.0);
        state_.sp500Return = equityDrift * dt + (state_.vix / 100.0) * std::sqrt(dt) * rng_.normalSample();

        // ── Nominal GDP level index (P3.2): compound the real growth rate ──
        // No RNG draw — deterministic, lockstep-safe. Base 100.0 at 1945.
        state_.gdpLevel *= (1.0 + state_.gdpGrowth * dt);

        // ── Economy-wide revenue/profit (P5 §5, v1 proxy) ──────────────────
        // Revenue = nominal activity index (gdpLevel). Profit = revenue × a
        // corporate-margin cycle that widens in booms and compresses when
        // credit spreads widen. No RNG — deterministic, lockstep-safe.
        double margin = 0.10
            + 0.5 * (state_.gdpGrowth - 0.025)      // pro-cyclical
            - 1.0 * (state_.creditSpread - 0.015);  // squeezed by dear credit
        margin = std::clamp(margin, 0.04, 0.16);
        state_.economyRevenue = state_.gdpLevel;
        state_.economyProfit = state_.gdpLevel * margin;
    }

    const EconomicIndicators& state() const { return state_; }

    nlohmann::json toSaveJson() const {
        nlohmann::json hist = nlohmann::json::array();
        for (const auto& v : fedFundsHistory_) hist.push_back(v);
        return {{"state", state_}, {"fedFundsHistory", hist}};
    }
    void loadSaveJson(const nlohmann::json& j) {
        if (j.contains("state")) state_ = j["state"].get<EconomicIndicators>();
        if (j.contains("fedFundsHistory")) {
            fedFundsHistory_.clear();
            for (const auto& v : j["fedFundsHistory"]) fedFundsHistory_.push_back(v.get<double>());
        }
    }

    // Compute stress score (0-100) from indicators
    double stressScore() const {
        double vixScore = std::clamp((state_.vix - 12.0) / 28.0 * 100.0, 0.0, 100.0);
        double spreadScore = std::clamp((state_.creditSpread - 0.01) / 0.04 * 100.0, 0.0, 100.0);
        double unempScore = std::clamp((state_.unemployment - 0.04) / 0.06 * 100.0, 0.0, 100.0);
        return 0.5 * vixScore + 0.3 * spreadScore + 0.2 * unempScore;
    }

    // Map current macro state to MarketState (for frontend)
    MarketState toMarketState() const {
        MarketState ms;
        ms.volatility = state_.vix / 100.0;
        ms.interestRate = state_.fedFundsRate;
        ms.inflation = state_.cpiInflation;
        ms.gdpGrowth = state_.gdpGrowth;
        ms.unemployment = state_.unemployment;
        ms.consumerConfidence = std::clamp(100.0 - stressScore(), 10.0, 100.0);

        // Derive cycle from GDP growth + stress
        double stress = stressScore();
        if (state_.gdpGrowth > 0.03 && stress < 30)
            ms.cycle = MarketCycle::Expansion;
        else if (state_.gdpGrowth > 0.02 && stress > 50)
            ms.cycle = MarketCycle::Peak;
        else if (state_.gdpGrowth < 0.0)
            ms.cycle = MarketCycle::Contraction;
        else if (state_.gdpGrowth < 0.01 && stress > 40)
            ms.cycle = MarketCycle::Trough;
        else
            ms.cycle = MarketCycle::Recovery;

        // Derive sentiment from VIX and GDP
        if (state_.vix > 35) ms.marketSentiment = MarketSentiment::Panicked;
        else if (state_.vix > 25) ms.marketSentiment = MarketSentiment::Pessimistic;
        else if (state_.vix < 12 && state_.gdpGrowth > 0.03) ms.marketSentiment = MarketSentiment::Euphoric;
        else if (state_.gdpGrowth > 0.02) ms.marketSentiment = MarketSentiment::Bullish;
        else if (state_.gdpGrowth < 0.0) ms.marketSentiment = MarketSentiment::Bearish;
        else ms.marketSentiment = MarketSentiment::Neutral;

        return ms;
    }

private:
    EconomicIndicators state_;
    math::RandomEngine& rng_;
    GameConfig::EconomyParams params_;
    std::deque<double> fedFundsHistory_;
    static constexpr int FED_LAG_DAYS = 378;
    double creditImpulse_ = 0.0; // SFC Phase C: Δ(system lending) / GDP

    // Ornstein-Uhlenbeck mean-reversion step
    double ornsteinUhlenbeck(double x, double mu, double theta, double sigma, double dt) {
        return x + theta * (mu - x) * dt + sigma * std::sqrt(dt) * rng_.normalSample();
    }
};

} // namespace stvg::simulation
