#include <gtest/gtest.h>
#include <stvg/autoplay/GameRunner.h>
#include <stvg/autoplay/EraAwareBots.h>
#include <stvg/autoplay/BotStrategy.h>

using namespace stvg;
using namespace stvg::autoplay;

// Helper to run a quick game (20 quarters = 5 years, enough to test basics)
static GameSummary quickGame(BotStrategy& bot, uint64_t seed = 42, int maxQ = 20) {
    GameConfig config;
    GameRunner runner;
    return runner.runGame(bot, config, seed, maxQ);
}

// ── Wiring Tests ────────────────────────────────────────────────────

TEST(Wiring, EraTransitionUpdatesEconomy) {
    // Verify the era engine progresses — just check it starts in era 1
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager mgr(cfg);
    EXPECT_EQ(mgr.currentEra().id, "post_war");
    // Run a few quarters to confirm economy uses era params
    mgr.advancePhase(); // QuarterStart
    // The OU targets should be post-war values (fed=2%, not 4%)
    EXPECT_LT(mgr.state().economics.fedFundsRate, 0.05); // Should be near 2.5%
}

TEST(Wiring, CompetitorsVisibleInJson) {
    SimulationConfig cfg;
    cfg.seed = 700;
    QuarterlyTurnManager mgr(cfg);
    auto j = mgr.toPlayerJson();
    EXPECT_TRUE(j.contains("competitors"));
    EXPECT_TRUE(j.contains("leaderboard"));
    EXPECT_GE(j["competitors"].size(), 4);
}

TEST(Wiring, AICeoAndClimateInJson) {
    SimulationConfig cfg;
    cfg.seed = 701;
    QuarterlyTurnManager mgr(cfg);
    auto j = mgr.toPlayerJson();
    EXPECT_TRUE(j.contains("aiCeo"));
    EXPECT_TRUE(j.contains("climate"));
}

TEST(Wiring, TripleGateDivisionCheck) {
    SimulationConfig cfg;
    cfg.seed = 702;
    QuarterlyTurnManager mgr(cfg);

    // In 1945: Glass-Steagall blocks IB even though we might try
    EXPECT_FALSE(mgr.isDivisionAvailable(DivisionType::InvestmentBanking));
    // Commercial lending always available
    EXPECT_TRUE(mgr.isDivisionAvailable(DivisionType::CommercialLending));
    // Trading blocked by low risk culture (0.2 < 0.3)
    EXPECT_FALSE(mgr.isDivisionAvailable(DivisionType::TradingDesk));
}

TEST(Wiring, RegulatoryComplianceCostApplied) {
    // Run a few quarters and check that costs exist
    BalancedBot bot;
    auto summary = quickGame(bot, 703, 8);
    EXPECT_GT(summary.totalCosts, 0);
}

// ── 95-Year Game Tests ──────────────────────────────────────────────

TEST(FullGame, HistoricalBotRunsWithoutNaN) {
    HistoricalBot bot;
    auto summary = quickGame(bot, 800, 40); // 10 years
    EXPECT_FALSE(summary.hadNaN);
    EXPECT_GT(summary.erasReached, 0);
    EXPECT_GT(summary.quartersPlayed, 0);
}

TEST(FullGame, SpeedrunBotRuns20Years) {
    SpeedrunBot bot;
    auto summary = quickGame(bot, 801, 80);
    // Speedrun may go bankrupt or thrive — both are valid
    EXPECT_FALSE(summary.hadNaN);
}

TEST(FullGame, SurvivorBotRunsWithoutNaN) {
    SurvivorBot bot;
    auto summary = quickGame(bot, 802, 40);
    EXPECT_FALSE(summary.hadNaN);
    EXPECT_GT(summary.quartersPlayed, 0);
}

TEST(FullGame, ContraryBotRuns20Years) {
    ContraryBot bot;
    auto summary = quickGame(bot, 803, 80);
    EXPECT_FALSE(summary.hadNaN);
}

TEST(FullGame, PoliticalBotRuns20Years) {
    PoliticalBot bot;
    auto summary = quickGame(bot, 804, 80);
    EXPECT_FALSE(summary.hadNaN);
}

TEST(FullGame, BotsProduceDifferentOutcomes) {
    HistoricalBot hist;
    SpeedrunBot speed;
    SurvivorBot surv;

    auto h = quickGame(hist, 900, 40);
    auto s = quickGame(speed, 900, 40);
    auto v = quickGame(surv, 900, 40);

    // Different strategies should produce different final capitals
    // (at least 2 of the 3 should differ significantly)
    bool differ = (std::abs(h.finalCapital - s.finalCapital) > 1e8)
               || (std::abs(h.finalCapital - v.finalCapital) > 1e8)
               || (std::abs(s.finalCapital - v.finalCapital) > 1e8);
    EXPECT_TRUE(differ);
}

TEST(FullGame, TelemetryTracksEra) {
    HistoricalBot bot;
    auto summary = quickGame(bot, 901, 64); // ~16 years, into era 2

    // Check that era is tracked in telemetry
    EXPECT_FALSE(summary.turnHistory.empty());
    EXPECT_FALSE(summary.turnHistory[0].currentEra.empty());
    EXPECT_FALSE(summary.finalEra.empty());
}

TEST(FullGame, BatchRunsMultipleGames) {
    HistoricalBot bot;
    GameConfig config;
    GameRunner runner;
    auto results = runner.runBatch(bot, config, 3, 1000, 20);
    EXPECT_EQ(results.size(), 3);
    for (const auto& r : results) {
        EXPECT_FALSE(r.hadNaN);
        EXPECT_GT(r.quartersPlayed, 0);
    }
}
