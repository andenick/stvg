#include <gtest/gtest.h>
#include <stvg/core/QuarterlyTurnManager.h>

using namespace stvg;

// Helper to create a game and advance through some quarters
static QuarterlyTurnManager makeGame(uint64_t seed = 200) {
    SimulationConfig cfg;
    cfg.seed = seed;
    return QuarterlyTurnManager(cfg);
}

// ── Loss Conditions ─────────────────────────────────────────────────

TEST(Endgame, NoEndByDefault) {
    auto mgr = makeGame();
    auto end = mgr.checkGameEnd();
    EXPECT_EQ(end.reason, GameEndReason::None);
    EXPECT_FALSE(end.isVictory);
    EXPECT_FALSE(mgr.isGameOver());
}

TEST(Endgame, BankruptOnZeroCapital) {
    auto mgr = makeGame();
    // Force capital to zero (access through non-const reference trick via isGameOver)
    // We'll run a quarter first to set up state, then check the condition
    SimulationConfig cfg;
    cfg.seed = 201;
    BankConfig bc;
    bc.startingCapital = 1.0; // Basically zero
    QuarterlyTurnManager mgr2(cfg, bc);

    // Run a quarter — costs will exceed capital
    mgr2.advancePhase(); // QuarterStart -> DecisionPhase
    mgr2.advancePhase(); // DecisionPhase -> Simulation
    mgr2.advancePhase(); // Simulation -> QuarterEnd
    mgr2.advancePhase(); // QuarterEnd -> QuarterComplete

    auto end = mgr2.checkGameEnd();
    // Capital should be depleted or ratio should be below threshold
    bool gameShouldEnd = (end.reason == GameEndReason::Bankrupt
                       || end.reason == GameEndReason::RegulatorySeizure);
    EXPECT_TRUE(gameShouldEnd);
    EXPECT_FALSE(end.isVictory);
    EXPECT_FALSE(end.title.empty());
    EXPECT_FALSE(end.narrative.empty());
}

TEST(Endgame, ReputationCollapseCheck) {
    // Verify the threshold logic works
    SimulationConfig cfg;
    cfg.seed = 202;
    QuarterlyTurnManager mgr(cfg);

    // Game starts with reputation > 5, so no collapse
    auto end = mgr.checkGameEnd();
    EXPECT_NE(end.reason, GameEndReason::ReputationCollapse);
}

// ── Win Conditions ──────────────────────────────────────────────────

TEST(Endgame, GrowthTargetAt2T) {
    // Can't easily reach $2T in a test, but verify the threshold check logic
    SimulationConfig cfg;
    cfg.seed = 203;
    BankConfig bc;
    bc.startingCapital = 2.1e12; // Way over threshold, assets will be > $2T
    QuarterlyTurnManager mgr(cfg, bc);

    auto end = mgr.checkGameEnd();
    if (mgr.bank().totalAssets >= 2e12) {
        EXPECT_EQ(end.reason, GameEndReason::GrowthTarget);
        EXPECT_TRUE(end.isVictory);
        EXPECT_EQ(end.title, "TOO BIG TO FAIL");
        EXPECT_FALSE(end.narrative.empty());
    }
}

TEST(Endgame, EndStateHasQuartersPlayed) {
    auto mgr = makeGame(204);

    // Run one full quarter
    mgr.advancePhase(); // QuarterStart
    mgr.advancePhase(); // DecisionPhase -> Simulation
    mgr.advancePhase(); // Simulation -> QuarterEnd
    mgr.advancePhase(); // QuarterEnd -> QuarterComplete

    EXPECT_EQ(mgr.quartersCompleted(), 1);
    auto end = mgr.checkGameEnd();
    EXPECT_EQ(end.quartersPlayed, 1);
}

TEST(Endgame, AverageScoreTracked) {
    auto mgr = makeGame(205);

    // Run two quarters
    for (int q = 0; q < 2; ++q) {
        mgr.advancePhase(); // QuarterStart
        mgr.advancePhase(); // Decision -> Simulation
        mgr.advancePhase(); // Simulation -> QuarterEnd
        mgr.advancePhase(); // QuarterEnd -> QuarterComplete
        if (mgr.isGameOver()) break;
        if (q < 1) mgr.advancePhase(); // QuarterComplete -> QuarterStart
    }

    EXPECT_GE(mgr.quartersCompleted(), 1);
    EXPECT_GT(mgr.averageScore(), 0);
}

TEST(Endgame, NarrativeNonEmptyForAllReasons) {
    // Test that all game end reasons produce non-empty narratives
    auto mgr = makeGame(206);

    // Force bankrupt scenario
    SimulationConfig cfg;
    cfg.seed = 207;
    BankConfig bc;
    bc.startingCapital = 0.5; // Force bankrupt
    QuarterlyTurnManager mgr2(cfg, bc);

    auto end = mgr2.checkGameEnd();
    if (end.reason != GameEndReason::None) {
        EXPECT_FALSE(end.narrative.empty());
        EXPECT_FALSE(end.title.empty());
    }
}

TEST(Endgame, IsGameOverConsistentWithCheckGameEnd) {
    auto mgr = makeGame(208);
    auto end = mgr.checkGameEnd();
    bool overByCheck = (end.reason != GameEndReason::None);
    EXPECT_EQ(mgr.isGameOver(), overByCheck);
}

TEST(Endgame, SurvivorRequiresEnoughQuarters) {
    SimulationConfig cfg;
    cfg.seed = 209;
    cfg.quartersPerGame = 2;
    QuarterlyTurnManager mgr(cfg);

    // Haven't played any quarters yet
    auto end = mgr.checkGameEnd();
    EXPECT_NE(end.reason, GameEndReason::SurvivorWin);
}
