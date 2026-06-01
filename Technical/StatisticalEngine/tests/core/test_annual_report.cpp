#include <gtest/gtest.h>
#include <stvg/core/QuarterlyTurnManager.h>

using namespace stvg;

// Helper: run N full quarters
static void runQuarters(QuarterlyTurnManager& mgr, int n) {
    for (int i = 0; i < n; ++i) {
        mgr.advancePhase(); // QuarterStart -> DecisionPhase
        mgr.advancePhase(); // DecisionPhase -> Simulation
        mgr.advancePhase(); // Simulation -> QuarterEnd
        mgr.advancePhase(); // QuarterEnd -> QuarterComplete
        if (mgr.isGameOver()) break;
        if (i < n - 1) mgr.advancePhase(); // QuarterComplete -> QuarterStart
    }
}

TEST(AnnualReport, GeneratedAfterFourQuarters) {
    SimulationConfig cfg;
    cfg.seed = 500;
    QuarterlyTurnManager mgr(cfg);

    runQuarters(mgr, 4);

    const auto& report = mgr.lastAnnualReport();
    EXPECT_EQ(report.year, 1945);
    EXPECT_GT(report.quartersPlayed, 0);
}

TEST(AnnualReport, ContainsFinancials) {
    SimulationConfig cfg;
    cfg.seed = 501;
    QuarterlyTurnManager mgr(cfg);

    runQuarters(mgr, 4);

    const auto& report = mgr.lastAnnualReport();
    // Revenue and costs should be non-zero after a year
    EXPECT_NE(report.totalRevenue, 0);
    EXPECT_NE(report.totalCosts, 0);
}

TEST(AnnualReport, TracksCapitalGrowth) {
    SimulationConfig cfg;
    cfg.seed = 502;
    QuarterlyTurnManager mgr(cfg);

    runQuarters(mgr, 4);

    const auto& report = mgr.lastAnnualReport();
    EXPECT_GT(report.capitalStart, 0);
    EXPECT_GT(report.capitalEnd, 0);
    // Growth pct should be finite
    EXPECT_TRUE(std::isfinite(report.capitalGrowthPct));
}

TEST(AnnualReport, IncludesEraName) {
    SimulationConfig cfg;
    cfg.seed = 503;
    QuarterlyTurnManager mgr(cfg);

    runQuarters(mgr, 4);

    const auto& report = mgr.lastAnnualReport();
    EXPECT_EQ(report.eraName, "Post-War Stability");
}

TEST(AnnualReport, SerializesToJson) {
    SimulationConfig cfg;
    cfg.seed = 504;
    QuarterlyTurnManager mgr(cfg);

    runQuarters(mgr, 4);

    nlohmann::json j = mgr.lastAnnualReport();
    EXPECT_EQ(j["year"], 1945);
    EXPECT_TRUE(j.contains("totalRevenue"));
    EXPECT_TRUE(j.contains("capitalGrowthPct"));
    EXPECT_TRUE(j.contains("eraName"));
    EXPECT_TRUE(j.contains("regulatoryStatus"));
}
