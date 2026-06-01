#include <gtest/gtest.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <cmath>

using namespace stvg;

static SimulationConfig defaultCfg(uint64_t seed = 42) {
    SimulationConfig cfg;
    cfg.seed = seed;
    cfg.startYear = 1945;
    cfg.startQuarter = 1;
    cfg.quarterDurationDays = 63;
    return cfg;
}

// ── Day-by-day vs batch equivalence ────────────────────────────────
// Running prepareSimulation() + N × tickSimulationDay() + completeSimulationPhase()
// must produce the same final state as the batch runSimulation() path.

TEST(RealtimeSimulation, DayByDayMatchesBatch) {
    auto cfg = defaultCfg(12345);

    // Batch path (realTimeSimulation = false, the default)
    QuarterlyTurnManager batch(cfg);
    batch.advancePhase(); // QuarterStart -> DecisionPhase
    batch.advancePhase(); // DecisionPhase -> Simulation (sets phase only)
    batch.advancePhase(); // Simulation -> QuarterEnd (runs runSimulation)
    EXPECT_EQ(batch.currentPhase(), TurnPhase::QuarterEnd);

    // Day-by-day path
    QuarterlyTurnManager dayByDay(cfg);
    dayByDay.setRealTimeSimulation(true);
    dayByDay.advancePhase(); // QuarterStart -> DecisionPhase
    dayByDay.advancePhase(); // DecisionPhase -> Simulation (no-op since realTimeSimulation)
    EXPECT_EQ(dayByDay.currentPhase(), TurnPhase::Simulation);

    dayByDay.prepareSimulation();
    int tickCount = 0;
    while (!dayByDay.tickSimulationDay()) {
        tickCount++;
    }
    EXPECT_EQ(tickCount, cfg.quarterDurationDays - 1); // last call returns true
    dayByDay.completeSimulationPhase();
    EXPECT_EQ(dayByDay.currentPhase(), TurnPhase::QuarterEnd);

    // Compare final states — same seed should produce identical market prices
    auto batchMarkets = batch.state().markets;
    auto rtMarkets = dayByDay.state().markets;
    ASSERT_EQ(batchMarkets.size(), rtMarkets.size());

    for (size_t i = 0; i < batchMarkets.size(); ++i) {
        EXPECT_NEAR(batchMarkets[i].currentPrice, rtMarkets[i].currentPrice, 1e-6)
            << "Market " << i << " price mismatch";
    }

    EXPECT_NEAR(batch.bank().capital, dayByDay.bank().capital, 1e-6);
    EXPECT_EQ(batch.state().currentYear, dayByDay.state().currentYear);
    EXPECT_EQ(batch.state().currentQuarter, dayByDay.state().currentQuarter);
}

// ── marketTickPayload() structure ──────────────────────────────────

TEST(RealtimeSimulation, MarketTickPayloadHasExpectedFields) {
    auto cfg = defaultCfg(42);
    QuarterlyTurnManager mgr(cfg);
    mgr.setRealTimeSimulation(true);
    mgr.advancePhase(); // -> DecisionPhase
    mgr.advancePhase(); // -> Simulation (stays there)
    mgr.prepareSimulation();
    mgr.tickSimulationDay();

    auto payload = mgr.marketTickPayload();

    EXPECT_TRUE(payload.contains("day"));
    EXPECT_TRUE(payload.contains("year"));
    EXPECT_TRUE(payload.contains("quarter"));
    EXPECT_TRUE(payload.contains("totalDay"));
    EXPECT_TRUE(payload.contains("prices"));
    EXPECT_TRUE(payload.contains("bankCapital"));
    EXPECT_TRUE(payload.contains("bankAssets"));
    EXPECT_TRUE(payload.contains("regime"));

    EXPECT_TRUE(payload["prices"].is_array());
    EXPECT_EQ(payload["prices"].size(), 4); // 4 markets in 1945 (EURUSD/BTC added later)

    for (const auto& p : payload["prices"]) {
        EXPECT_TRUE(p.contains("id"));
        EXPECT_TRUE(p.contains("price"));
        EXPECT_TRUE(p.contains("dailyReturn"));
        EXPECT_GT(p["price"].get<double>(), 0.0);
    }

    EXPECT_GT(payload["bankCapital"].get<double>(), 0.0);
    EXPECT_FALSE(payload["regime"].get<std::string>().empty());
}

// ── Phase gating: advancePhase is no-op for Simulation in real-time mode ──

TEST(RealtimeSimulation, AdvancePhaseNoOpDuringRealTimeSimulation) {
    auto cfg = defaultCfg(42);
    QuarterlyTurnManager mgr(cfg);
    mgr.setRealTimeSimulation(true);
    mgr.advancePhase(); // -> DecisionPhase
    mgr.advancePhase(); // -> should stay at Simulation

    EXPECT_EQ(mgr.currentPhase(), TurnPhase::Simulation);

    // Calling advancePhase again should NOT change the phase
    auto phase = mgr.advancePhase();
    EXPECT_EQ(phase, TurnPhase::Simulation);

    // Only completeSimulationPhase() should transition
    mgr.prepareSimulation();
    while (!mgr.tickSimulationDay()) {}
    mgr.completeSimulationPhase();
    EXPECT_EQ(mgr.currentPhase(), TurnPhase::QuarterEnd);
}

// ── Intra-day state updates ────────────────────────────────────────
// state_.currentDay and totalDaysElapsed should update every tick

TEST(RealtimeSimulation, IntraDayStateUpdates) {
    auto cfg = defaultCfg(42);
    QuarterlyTurnManager mgr(cfg);
    mgr.setRealTimeSimulation(true);
    mgr.advancePhase(); // -> DecisionPhase
    mgr.advancePhase(); // -> Simulation

    mgr.prepareSimulation();

    int prevDay = mgr.state().currentDay;
    mgr.tickSimulationDay();
    EXPECT_GT(mgr.state().currentDay, prevDay);
    EXPECT_GT(mgr.state().totalDaysElapsed, 0);

    mgr.tickSimulationDay();
    EXPECT_EQ(mgr.state().totalDaysElapsed, prevDay + 2);
}

// ── Determinism: same seed produces identical results ──────────────

TEST(RealtimeSimulation, SameSeedIsDeterministic) {
    auto cfg = defaultCfg(99999);

    // Run 1
    QuarterlyTurnManager run1(cfg);
    run1.setRealTimeSimulation(true);
    run1.advancePhase();
    run1.advancePhase();
    run1.prepareSimulation();
    while (!run1.tickSimulationDay()) {}
    run1.completeSimulationPhase();

    // Run 2 (same seed)
    QuarterlyTurnManager run2(cfg);
    run2.setRealTimeSimulation(true);
    run2.advancePhase();
    run2.advancePhase();
    run2.prepareSimulation();
    while (!run2.tickSimulationDay()) {}
    run2.completeSimulationPhase();

    auto m1 = run1.state().markets;
    auto m2 = run2.state().markets;
    ASSERT_EQ(m1.size(), m2.size());

    for (size_t i = 0; i < m1.size(); ++i) {
        EXPECT_DOUBLE_EQ(m1[i].currentPrice, m2[i].currentPrice)
            << "Market " << i << " not deterministic";
    }

    EXPECT_DOUBLE_EQ(run1.bank().capital, run2.bank().capital);
}

// ── Multiple quarters via day-by-day ───────────────────────────────

TEST(RealtimeSimulation, MultipleQuartersProgress) {
    auto cfg = defaultCfg(7777);
    QuarterlyTurnManager mgr(cfg);
    mgr.setRealTimeSimulation(true);

    for (int q = 0; q < 4; ++q) {
        // QuarterStart -> DecisionPhase
        mgr.advancePhase();
        EXPECT_EQ(mgr.currentPhase(), TurnPhase::DecisionPhase);

        // DecisionPhase -> Simulation
        mgr.advancePhase();
        EXPECT_EQ(mgr.currentPhase(), TurnPhase::Simulation);

        // Day-by-day simulation
        mgr.prepareSimulation();
        while (!mgr.tickSimulationDay()) {}
        mgr.completeSimulationPhase();
        EXPECT_EQ(mgr.currentPhase(), TurnPhase::QuarterEnd);

        // QuarterEnd -> CrisisResponse or QuarterComplete
        mgr.advancePhase();
        auto phase = mgr.currentPhase();
        if (phase == TurnPhase::CrisisResponse) {
            mgr.advancePhase(); // -> QuarterComplete
            mgr.advancePhase(); // -> QuarterStart (next quarter)
        } else {
            EXPECT_EQ(phase, TurnPhase::QuarterComplete);
            mgr.advancePhase(); // -> QuarterStart
        }

        EXPECT_EQ(mgr.currentPhase(), TurnPhase::QuarterStart);
    }

    // After 4 quarters, should be in year 1946
    EXPECT_EQ(mgr.state().currentYear, 1946);
}
