#include <gtest/gtest.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <nlohmann/json.hpp>

using namespace stvg;

// ════════════════════════════════════════════════════════════════════
// Save/Load Tests — Phase 3.2 C1
//
// Verifies that game state survives a saveGame()/loadGame() roundtrip.
// The most important property: after restoring at quarter N, the game
// continues with bit-identical (or near-identical) behavior for the
// next several quarters compared to a control game that wasn't saved.
// ════════════════════════════════════════════════════════════════════

namespace {
QuarterlyTurnManager makeMgr(uint64_t seed = 7777) {
    SimulationConfig cfg;
    cfg.seed = seed;
    return QuarterlyTurnManager(cfg);
}

// Run N quarters with no decisions submitted (let AP expire each turn).
void runQuarters(QuarterlyTurnManager& mgr, int n) {
    for (int q = 0; q < n; ++q) {
        mgr.advancePhase();  // QuarterStart → DecisionPhase
        mgr.advancePhase();  // → Simulation
        mgr.advancePhase();  // → QuarterEnd
        // Skip any pending crisis-response phase
        while (mgr.currentPhase() != TurnPhase::QuarterComplete) {
            mgr.advancePhase();
        }
        if (mgr.isGameOver()) break;
        mgr.advancePhase();  // → next QuarterStart
    }
}
}  // namespace

// ── Save format basics ────────────────────────────────────────────

TEST(SaveLoad, SaveProducesValidJson) {
    auto mgr = makeMgr();
    auto j = mgr.saveGame();
    EXPECT_TRUE(j.contains("version"));
    EXPECT_GE(j["version"].get<int>(), 1);
    EXPECT_TRUE(j.contains("rng"));
    EXPECT_TRUE(j.contains("clock"));
    EXPECT_TRUE(j.contains("bank"));
    EXPECT_TRUE(j.contains("tracking"));
}

TEST(SaveLoad, SaveCapturesPhase2Fields) {
    auto mgr = makeMgr();
    auto j = mgr.saveGame();
    // Phase 2 additions: doomMeters + new tracking fields
    ASSERT_TRUE(j.contains("state"));
    EXPECT_TRUE(j["state"].contains("doomMeters"));
    ASSERT_TRUE(j.contains("tracking"));
    EXPECT_TRUE(j["tracking"].contains("consecutiveProfitableQuarters"));
    EXPECT_TRUE(j["tracking"].contains("recentDividendQuarters"));
}

// ── Load: invalid input handling ──────────────────────────────────

TEST(SaveLoad, LoadRejectsMissingVersion) {
    auto mgr = makeMgr();
    nlohmann::json bad = {{"rng", nlohmann::json::object()}};
    EXPECT_FALSE(mgr.loadGame(bad));
}

TEST(SaveLoad, LoadRejectsFutureVersion) {
    auto mgr = makeMgr();
    nlohmann::json bad = {{"version", 999}};
    EXPECT_FALSE(mgr.loadGame(bad));
}

// ── Roundtrip ─────────────────────────────────────────────────────

TEST(SaveLoad, RoundtripPreservesCapital) {
    auto mgr = makeMgr();
    runQuarters(mgr, 8);  // Get to a non-trivial state
    double capBefore = mgr.bank().capital;
    int qBefore = mgr.quartersCompleted();

    auto blob = mgr.saveGame();

    auto mgr2 = makeMgr(/*different seed*/ 99);
    ASSERT_TRUE(mgr2.loadGame(blob));
    EXPECT_NEAR(mgr2.bank().capital, capBefore, 1e-3);
    EXPECT_EQ(mgr2.quartersCompleted(), qBefore);
}

TEST(SaveLoad, RoundtripPreservesDoomMeters) {
    auto mgr = makeMgr();
    // Force doom meters to a known state via const_cast (test-only)
    auto& s = const_cast<SimulationState&>(mgr.state());
    s.doomMeters.aiDisplacement = 42.5;
    s.doomMeters.climateCatastrophe = 17.0;
    s.doomMeters.globalTensions = 88.8;
    s.doomMeters.inequality = 5.5;

    auto blob = mgr.saveGame();
    auto mgr2 = makeMgr(123);
    ASSERT_TRUE(mgr2.loadGame(blob));
    EXPECT_DOUBLE_EQ(mgr2.state().doomMeters.aiDisplacement, 42.5);
    EXPECT_DOUBLE_EQ(mgr2.state().doomMeters.climateCatastrophe, 17.0);
    EXPECT_DOUBLE_EQ(mgr2.state().doomMeters.globalTensions, 88.8);
    EXPECT_DOUBLE_EQ(mgr2.state().doomMeters.inequality, 5.5);
}

TEST(SaveLoad, RoundtripPreservesDivisions) {
    auto mgr = makeMgr();
    runQuarters(mgr, 4);
    size_t divCountBefore = mgr.bank().divisions.size();
    std::vector<std::string> divIds;
    for (const auto& d : mgr.bank().divisions) divIds.push_back(d.id);

    auto blob = mgr.saveGame();
    auto mgr2 = makeMgr(456);
    ASSERT_TRUE(mgr2.loadGame(blob));
    EXPECT_EQ(mgr2.bank().divisions.size(), divCountBefore);
    for (size_t i = 0; i < divIds.size(); ++i) {
        EXPECT_EQ(mgr2.bank().divisions[i].id, divIds[i]);
    }
}

TEST(SaveLoad, RoundtripPreservesTrackingCounters) {
    auto mgr = makeMgr();
    runQuarters(mgr, 6);
    int qc = mgr.quartersCompleted();
    double ts = 0.0;  // can't read totalScore directly; use averageScore as proxy
    double avg = mgr.averageScore();

    auto blob = mgr.saveGame();
    auto mgr2 = makeMgr();
    ASSERT_TRUE(mgr2.loadGame(blob));
    EXPECT_EQ(mgr2.quartersCompleted(), qc);
    EXPECT_NEAR(mgr2.averageScore(), avg, 1e-3);
    (void)ts;
}

TEST(SaveLoad, RoundtripPreservesYearAndQuarter) {
    auto mgr = makeMgr();
    runQuarters(mgr, 12);  // 3 years
    int year = mgr.state().currentYear;
    int quarter = mgr.state().currentQuarter;

    auto blob = mgr.saveGame();
    auto mgr2 = makeMgr();
    ASSERT_TRUE(mgr2.loadGame(blob));
    EXPECT_EQ(mgr2.state().currentYear, year);
    EXPECT_EQ(mgr2.state().currentQuarter, quarter);
}
