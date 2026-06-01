#pragma once

#include "Types.h"
#include "SimulationClock.h"
#include "../math/RandomEngine.h"
#include "../simulation/MarketSimulator.h"
#include "../simulation/EconomicEngine.h"
#include <spdlog/spdlog.h>
#include <string>
#include <functional>

namespace stvg {

// Owns all simulation state for one game session.
// Tick() advances the simulation by one trading day.
class GameSession {
public:
    explicit GameSession(const SimulationConfig& config)
        : config_(config)
        , rng_(config.seed)
        , clock_(config.startYear, config.startQuarter, config.quarterDurationDays)
        , econ_(rng_)
        , marketSim_(rng_, econ_)
    {
        state_.sessionId = generateSessionId();
        state_.currentYear = config.startYear;
        state_.currentQuarter = config.startQuarter;

        // Initialize economic state
        EconomicIndicators initial;
        initial.fedFundsRate = 0.05;
        initial.cpiInflation = 0.025;
        initial.gdpGrowth = 0.025;
        initial.unemployment = 0.04;
        initial.vix = 18.0;
        initial.treasuryYield10Y = 0.045;
        initial.creditSpread = 0.015;
        econ_.init(initial);

        // Initialize markets (era-gated)
        marketSim_.init(config.startYear);

        // Populate initial state so markets are available before first tick()
        state_.economics = econ_.state();
        state_.marketState = econ_.toMarketState();
        state_.markets = marketSim_.snapshot();
        state_.regime = marketSim_.currentRegime(econ_.stressScore());

        spdlog::info("GameSession {} created (seed={}, year={} Q{})",
            state_.sessionId, config.seed, config.startYear, config.startQuarter);
    }

    // Advance simulation by one trading day
    // Returns true if a quarter boundary was crossed
    bool tick() {
        if (state_.paused) return false;

        double dt = config_.daysPerTick / 252.0; // Convert days to year fraction

        // 1. Advance economic indicators
        econ_.tick(dt);

        // 2. Advance markets
        marketSim_.tick(dt);

        // 3. Advance clock
        bool quarterEnd = clock_.advanceDay();

        // 4. Update state
        state_.currentYear = clock_.year();
        state_.currentQuarter = clock_.quarter();
        state_.currentDay = clock_.dayInQuarter();
        state_.totalDaysElapsed = clock_.totalDays();
        state_.economics = econ_.state();
        state_.marketState = econ_.toMarketState();
        state_.markets = marketSim_.snapshot();
        state_.regime = marketSim_.currentRegime(econ_.stressScore());

        if (quarterEnd) {
            spdlog::info("Session {}: Quarter ended -> {} Q{}",
                state_.sessionId, state_.currentYear, state_.currentQuarter);
        }

        return quarterEnd;
    }

    // Generate snapshot for WebSocket broadcast
    nlohmann::json marketUpdatePayload() const {
        return nlohmann::json{
            {"interestRates", state_.economics.fedFundsRate},
            {"marketVolatility", state_.economics.vix / 100.0},
            {"economicIndicators", {
                {"fedFundsRate", state_.economics.fedFundsRate},
                {"cpiInflation", state_.economics.cpiInflation},
                {"gdpGrowth", state_.economics.gdpGrowth},
                {"unemployment", state_.economics.unemployment},
                {"vix", state_.economics.vix},
                {"treasuryYield10Y", state_.economics.treasuryYield10Y},
                {"creditSpread", state_.economics.creditSpread},
                {"sp500Return", state_.economics.sp500Return}
            }}
        };
    }

    // Full state snapshot for REST API
    const SimulationState& state() const { return state_; }
    SimulationState& state() { return state_; }

    void pause() { state_.paused = true; }
    void resume() { state_.paused = false; }
    void setSpeed(double multiplier) { state_.speedMultiplier = multiplier; }

    const std::string& sessionId() const { return state_.sessionId; }

private:
    SimulationConfig config_;
    math::RandomEngine rng_;
    SimulationClock clock_;
    simulation::EconomicEngine econ_;
    simulation::MarketSimulator marketSim_;
    SimulationState state_;

    std::string generateSessionId() {
        static int counter = 0;
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        return "sess_" + std::to_string(ms) + "_" + std::to_string(++counter);
    }
};

} // namespace stvg
