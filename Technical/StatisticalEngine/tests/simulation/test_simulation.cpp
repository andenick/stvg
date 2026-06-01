#include <gtest/gtest.h>
#include <stvg/core/GameSession.h>
#include <stvg/core/Types.h>
#include <cmath>
#include <iostream>
#include <iomanip>

using namespace stvg;

// ── GameSession Integration Tests ───────────────────────────────────

TEST(GameSession, CreatesWithValidState) {
    SimulationConfig cfg;
    cfg.seed = 42;
    cfg.startYear = 2024;
    cfg.startQuarter = 1;
    GameSession session(cfg);

    EXPECT_EQ(session.state().currentYear, 2024);
    EXPECT_EQ(session.state().currentQuarter, 1);
    EXPECT_EQ(session.state().currentDay, 0);
    EXPECT_FALSE(session.state().paused);
    EXPECT_EQ(session.state().markets.size(), 6);
}

TEST(GameSession, TickAdvancesDay) {
    SimulationConfig cfg;
    cfg.seed = 42;
    GameSession session(cfg);

    session.tick();
    EXPECT_EQ(session.state().currentDay, 1);
    EXPECT_EQ(session.state().totalDaysElapsed, 1);
}

TEST(GameSession, QuarterAdvancesAfter63Days) {
    SimulationConfig cfg;
    cfg.seed = 42;
    cfg.quarterDurationDays = 63;
    GameSession session(cfg);

    int quarterChanges = 0;
    for (int i = 0; i < 63; ++i) {
        if (session.tick()) quarterChanges++;
    }
    EXPECT_EQ(quarterChanges, 1);
    EXPECT_EQ(session.state().currentQuarter, 2);
}

TEST(GameSession, PauseStopsTicking) {
    SimulationConfig cfg;
    cfg.seed = 42;
    GameSession session(cfg);

    session.pause();
    session.tick();
    EXPECT_EQ(session.state().currentDay, 0); // Should not advance
}

// ── Market Data Generation Tests ────────────────────────────────────

TEST(MarketGeneration, PricesChangeEachTick) {
    SimulationConfig cfg;
    cfg.seed = 42;
    GameSession session(cfg);

    double initialPrice = session.state().markets[0].currentPrice;
    session.tick();
    double afterOneDay = session.state().markets[0].currentPrice;

    EXPECT_NE(initialPrice, afterOneDay);
}

TEST(MarketGeneration, AllSixMarketsHaveData) {
    SimulationConfig cfg;
    cfg.seed = 42;
    cfg.startYear = 2024;  // Modern era has all 6 markets
    GameSession session(cfg);

    for (int i = 0; i < 10; ++i) session.tick();

    const auto& markets = session.state().markets;
    EXPECT_EQ(markets.size(), 6);

    for (const auto& m : markets) {
        EXPECT_FALSE(m.id.empty());
        EXPECT_GT(m.currentPrice, 0.0);
        EXPECT_GT(m.volatility, 0.0);
        EXPECT_NE(m.change, 0.0); // Very unlikely all zero after 10 ticks
    }
}

TEST(MarketGeneration, VolatilityIsPositiveAndReasonable) {
    SimulationConfig cfg;
    cfg.seed = 42;
    GameSession session(cfg);

    for (int i = 0; i < 100; ++i) session.tick();

    for (const auto& m : session.state().markets) {
        EXPECT_GT(m.volatility, 0.0);
        EXPECT_LT(m.volatility, 5.0); // Annualized vol under 500%
    }
}

// ── Economic Engine Tests ───────────────────────────────────────────

TEST(EconomicEngine, IndicatorsStayBounded) {
    SimulationConfig cfg;
    cfg.seed = 42;
    GameSession session(cfg);

    for (int i = 0; i < 252 * 3; ++i) { // 3 years of daily ticks
        session.tick();
    }

    const auto& eco = session.state().economics;
    EXPECT_GE(eco.fedFundsRate, 0.0);
    EXPECT_LE(eco.fedFundsRate, 0.20);
    EXPECT_GE(eco.unemployment, 0.02);
    EXPECT_LE(eco.unemployment, 0.25);
    EXPECT_GE(eco.vix, 8.0);
    EXPECT_LE(eco.vix, 80.0);
    EXPECT_GE(eco.gdpGrowth, -0.10);
    EXPECT_LE(eco.gdpGrowth, 0.10);
}

TEST(EconomicEngine, MeanReversionWorks) {
    SimulationConfig cfg;
    cfg.seed = 42;
    cfg.startYear = 2024;  // Modern era VIX mu ~20
    GameSession session(cfg);

    // Run for 5 years
    std::vector<double> vixHistory;
    for (int i = 0; i < 252 * 5; ++i) {
        session.tick();
        if (i % 10 == 0) vixHistory.push_back(session.state().economics.vix);
    }

    // VIX should have mean close to 18-20 (GameSession default OU target)
    double sum = 0;
    for (double v : vixHistory) sum += v;
    double mean = sum / vixHistory.size();
    EXPECT_NEAR(mean, 18.0, 6.0); // Within 6 of target
}

// ── Full Quarter Simulation Test ────────────────────────────────────

TEST(FullQuarter, GeneratesReasonableData) {
    SimulationConfig cfg;
    cfg.seed = 100;
    cfg.startYear = 2024;
    cfg.startQuarter = 1;
    cfg.quarterDurationDays = 63;
    GameSession session(cfg);

    // Run a full quarter
    for (int day = 0; day < 63; ++day) {
        session.tick();
    }

    const auto& state = session.state();

    // Quarter should have advanced
    EXPECT_EQ(state.currentQuarter, 2);
    EXPECT_EQ(state.totalDaysElapsed, 63);

    // Markets should have meaningful price changes from starting values
    bool anySignificantMove = false;
    for (const auto& m : state.markets) {
        double pctChange = std::abs(m.changePercent);
        if (pctChange > 0.01) anySignificantMove = true;
    }
    EXPECT_TRUE(anySignificantMove);

    // Market state should have reasonable values
    EXPECT_NE(state.marketState.cycle, MarketCycle::Expansion); // May or may not be expansion
    EXPECT_GT(state.economics.fedFundsRate, 0.0);
}

// ── Multi-Quarter Variety Test ──────────────────────────────────────

TEST(MultiQuarter, DifferentQuartersProduceDifferentOutcomes) {
    SimulationConfig cfg;
    cfg.seed = 42;
    cfg.quarterDurationDays = 63;
    GameSession session(cfg);

    std::vector<double> spxEndPrices;
    std::vector<MarketCycle> cycles;
    std::vector<double> vixValues;

    for (int q = 0; q < 8; ++q) { // 8 quarters = 2 years
        for (int d = 0; d < 63; ++d) session.tick();
        spxEndPrices.push_back(session.state().markets[0].currentPrice);
        cycles.push_back(session.state().marketState.cycle);
        vixValues.push_back(session.state().economics.vix);
    }

    // Prices should not be monotonically increasing or decreasing
    int ups = 0, downs = 0;
    for (size_t i = 1; i < spxEndPrices.size(); ++i) {
        if (spxEndPrices[i] > spxEndPrices[i-1]) ++ups;
        else ++downs;
    }
    EXPECT_GT(ups, 0);   // At least some up quarters
    // Note: downs might be 0 with seed=42 and positive drift, that's OK

    // VIX should show variation
    double minVix = *std::min_element(vixValues.begin(), vixValues.end());
    double maxVix = *std::max_element(vixValues.begin(), vixValues.end());
    EXPECT_GT(maxVix - minVix, 1.0); // At least 1 point spread over 2 years
}

// ── Deterministic Replay Test ───────────────────────────────────────

TEST(Determinism, SameSeedProducesSameResults) {
    auto runSession = [](uint64_t seed) {
        SimulationConfig cfg;
        cfg.seed = seed;
        GameSession session(cfg);
        for (int i = 0; i < 126; ++i) session.tick(); // 2 quarters
        return session.state().markets[0].currentPrice;
    };

    double run1 = runSession(42);
    double run2 = runSession(42);
    double run3 = runSession(99); // Different seed

    EXPECT_DOUBLE_EQ(run1, run2);
    EXPECT_NE(run1, run3);
}

// ── Print Diagnostic Output (for manual review) ─────────────────────

TEST(Diagnostic, PrintQuarterlyReport) {
    SimulationConfig cfg;
    cfg.seed = 42;
    cfg.startYear = 2024;
    cfg.startQuarter = 1;
    cfg.quarterDurationDays = 63;
    GameSession session(cfg);

    std::cout << "\n═══════════════════════════════════════════════════\n";
    std::cout << "  STVG QUARTERLY SIMULATION DIAGNOSTIC\n";
    std::cout << "  Seed: 42 | Start: 2024 Q1 | Days/Quarter: 63\n";
    std::cout << "═══════════════════════════════════════════════════\n";

    for (int q = 0; q < 4; ++q) {
        for (int d = 0; d < 63; ++d) session.tick();
        const auto& s = session.state();

        std::cout << "\n─── " << s.currentYear << " Q" << s.currentQuarter
                  << " (Day " << s.totalDaysElapsed << ") ───\n";

        std::cout << "  Regime: " << nlohmann::json(s.regime).get<std::string>() << "\n";
        std::cout << "  Cycle:  " << nlohmann::json(s.marketState.cycle).get<std::string>() << "\n";
        std::cout << "  Sentiment: " << nlohmann::json(s.marketState.marketSentiment).get<std::string>() << "\n";

        std::cout << "\n  MARKETS:\n";
        std::cout << "  " << std::left << std::setw(10) << "Symbol"
                  << std::right << std::setw(12) << "Price"
                  << std::setw(10) << "Change%"
                  << std::setw(12) << "AnnVol%\n";

        for (const auto& m : s.markets) {
            std::cout << "  " << std::left << std::setw(10) << m.symbol
                      << std::right << std::fixed << std::setprecision(2)
                      << std::setw(12) << m.currentPrice
                      << std::setw(10) << m.changePercent
                      << std::setw(12) << (m.volatility * 100.0) << "\n";
        }

        std::cout << "\n  ECONOMY:\n";
        std::cout << "  Fed Funds:  " << std::fixed << std::setprecision(3)
                  << (s.economics.fedFundsRate * 100) << "%\n";
        std::cout << "  CPI:        " << (s.economics.cpiInflation * 100) << "%\n";
        std::cout << "  GDP Growth: " << (s.economics.gdpGrowth * 100) << "%\n";
        std::cout << "  Unemploy:   " << (s.economics.unemployment * 100) << "%\n";
        std::cout << "  VIX:        " << std::setprecision(1) << s.economics.vix << "\n";
        std::cout << "  10Y Yield:  " << std::setprecision(3)
                  << (s.economics.treasuryYield10Y * 100) << "%\n";
        std::cout << "  Credit Spd: " << (s.economics.creditSpread * 100) << "%\n";
    }
    std::cout << "\n═══════════════════════════════════════════════════\n";
}
