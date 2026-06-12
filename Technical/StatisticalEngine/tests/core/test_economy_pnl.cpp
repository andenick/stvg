#include <gtest/gtest.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <stvg/core/Types.h>

using namespace stvg;

// ════════════════════════════════════════════════════════════════════
// STAR_02 P5 §5 — economy-wide P&L series (v1 proxy).
//
//   economyRevenue = gdpLevel
//   economyProfit  = gdpLevel · margin,  margin ∈ [0.04, 0.16]
// Serialized into EconomicIndicators, streamed in the market_tick econ block,
// and captured in MacroHistory snapshots.
// ════════════════════════════════════════════════════════════════════

TEST(EconomyPnL, IndicatorsDefaultPositive) {
    EconomicIndicators e;
    EXPECT_GT(e.economyRevenue, 0.0);
    EXPECT_GT(e.economyProfit, 0.0);
}

TEST(EconomyPnL, ProfitIsRevenueTimesBoundedMargin) {
    SimulationConfig cfg;
    cfg.seed = 7;
    QuarterlyTurnManager game(cfg);
    // Run a quarter so the EconomicEngine ticks and sets the series.
    game.advancePhase();
    game.advancePhase();
    game.advancePhase();
    game.advancePhase();

    const auto& econ = game.state().economics;
    EXPECT_GT(econ.economyRevenue, 0.0);
    EXPECT_GT(econ.economyProfit, 0.0);
    // economyRevenue == gdpLevel (the v1 proxy).
    EXPECT_NEAR(econ.economyRevenue, econ.gdpLevel, 1e-6);
    // margin = profit / revenue must be inside [0.04, 0.16].
    double margin = econ.economyProfit / econ.economyRevenue;
    EXPECT_GE(margin, 0.04 - 1e-9);
    EXPECT_LE(margin, 0.16 + 1e-9);
}

TEST(EconomyPnL, SerializedRoundTrip) {
    EconomicIndicators e;
    e.economyRevenue = 142.5;
    e.economyProfit = 17.1;
    nlohmann::json j = e;
    auto back = j.get<EconomicIndicators>();
    EXPECT_DOUBLE_EQ(back.economyRevenue, 142.5);
    EXPECT_DOUBLE_EQ(back.economyProfit, 17.1);
}

TEST(EconomyPnL, AppearsInMarketTickEconBlock) {
    SimulationConfig cfg;
    cfg.seed = 7;
    QuarterlyTurnManager game(cfg);
    game.advancePhase();
    game.advancePhase();
    // marketTickPayload is valid mid-quarter; just check the keys exist.
    auto payload = game.marketTickPayload();
    ASSERT_TRUE(payload.contains("econ"));
    EXPECT_TRUE(payload["econ"].contains("economyRevenue"));
    EXPECT_TRUE(payload["econ"].contains("economyProfit"));
}
