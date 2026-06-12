#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <stvg/core/QuarterlyTurnManager.h>

using namespace stvg;

// ════════════════════════════════════════════════════════════════════
// Endgame Path Tests — verifies that resolveEndgame() routes
// accumulated state into the right one of 6 paths.
// ════════════════════════════════════════════════════════════════════

namespace {
// Helper: build a manager and force its state into a target shape so
// resolveEndgame() can be tested in isolation.
QuarterlyTurnManager makeMgr(uint64_t seed = 500) {
    SimulationConfig cfg;
    cfg.seed = seed;
    return QuarterlyTurnManager(cfg);
}

void setDoomMeters(QuarterlyTurnManager& mgr,
                   double ai, double climate, double tensions, double inequality) {
    auto& state = const_cast<SimulationState&>(mgr.state());
    state.doomMeters.aiDisplacement = ai;
    state.doomMeters.climateCatastrophe = climate;
    state.doomMeters.globalTensions = tensions;
    state.doomMeters.inequality = inequality;
}

void setBank(QuarterlyTurnManager& mgr, double capital, double rep, int divCount) {
    auto& bank = const_cast<Bank&>(mgr.bank());
    bank.capital = capital;
    bank.reputation = rep;
    bank.divisions.clear();
    for (int i = 0; i < divCount; ++i) {
        Division d;
        d.id = "test_div_" + std::to_string(i);
        bank.divisions.push_back(d);
    }
}

void setYear(QuarterlyTurnManager& mgr, int year) {
    auto& state = const_cast<SimulationState&>(mgr.state());
    state.currentYear = year;
}
}  // namespace

// ── Each of the 6 paths ────────────────────────────────────────────

TEST(EndgamePaths, AbolitionRequiresHighRepLowCapitalSmallBank) {
    auto mgr = makeMgr();
    setDoomMeters(mgr, 30, 30, 30, 30);  // all calm
    setBank(mgr, 5e5, 95, 2);             // half starting capital, sky-high rep, 2 divs
    setYear(mgr, 2040);
    auto end = mgr.checkGameEnd();
    EXPECT_EQ(end.reason, GameEndReason::EndgameAbolition);
    EXPECT_TRUE(end.isVictory);
    EXPECT_EQ(end.title, "ABOLITION");
}

TEST(EndgamePaths, TransformationRequiresBigBankAndCleanWorld) {
    auto mgr = makeMgr();
    setDoomMeters(mgr, 30, 30, 30, 30);
    setBank(mgr, 2.5e6, 85, 6);            // 2.5x starting, high rep, normal bank
    setYear(mgr, 2040);
    auto end = mgr.checkGameEnd();
    EXPECT_EQ(end.reason, GameEndReason::EndgameTransformation);
    EXPECT_TRUE(end.isVictory);
}

TEST(EndgamePaths, ReconstructionAfterCrisisFighting) {
    auto mgr = makeMgr();
    setDoomMeters(mgr, 95, 60, 60, 50);   // 1 meter near max
    setBank(mgr, 8e5, 75, 5);              // burned some capital, decent rep
    setYear(mgr, 2040);
    auto end = mgr.checkGameEnd();
    EXPECT_EQ(end.reason, GameEndReason::EndgameReconstruction);
    EXPECT_TRUE(end.isVictory);
}

TEST(EndgamePaths, ManagedDeclineWhenMetersHigh) {
    auto mgr = makeMgr();
    setDoomMeters(mgr, 80, 75, 60, 50);   // 2 meters high, none maxed
    setBank(mgr, 1.2e6, 50, 6);             // healthy bank, mediocre rep
    setYear(mgr, 2040);
    auto end = mgr.checkGameEnd();
    EXPECT_EQ(end.reason, GameEndReason::EndgameManagedDecline);
    EXPECT_FALSE(end.isVictory);
}

TEST(EndgamePaths, FortressWhenBankThrivedButWorldBurned) {
    auto mgr = makeMgr();
    setDoomMeters(mgr, 100, 60, 60, 60);  // one meter maxed
    setBank(mgr, 2e6, 50, 6);             // 2x capital, mediocre rep
    setYear(mgr, 2040);
    auto end = mgr.checkGameEnd();
    EXPECT_EQ(end.reason, GameEndReason::EndgameFortress);
    EXPECT_FALSE(end.isVictory);
}

TEST(EndgamePaths, CollapseAsDefault) {
    auto mgr = makeMgr();
    setDoomMeters(mgr, 100, 100, 80, 80);  // multiple meters maxed
    setBank(mgr, 5e5, 30, 6);                // capital low, rep low
    setYear(mgr, 2040);
    auto end = mgr.checkGameEnd();
    EXPECT_EQ(end.reason, GameEndReason::EndgameCollapse);
    EXPECT_FALSE(end.isVictory);
}

// ── Triggers ───────────────────────────────────────────────────────

TEST(EndgamePaths, MeterAtHundredFiresEndgame) {
    auto mgr = makeMgr();
    setDoomMeters(mgr, 100, 0, 0, 0);
    setBank(mgr, 1e6, 50, 6);
    setYear(mgr, 2010);  // not year 2040
    auto end = mgr.checkGameEnd();
    // Should have hit the endgame branch (any of the 6 paths, not None)
    EXPECT_NE(end.reason, GameEndReason::None);
}

TEST(EndgamePaths, Year2040FiresEndgame) {
    auto mgr = makeMgr();
    setDoomMeters(mgr, 30, 30, 30, 30);
    setBank(mgr, 1e6, 50, 6);
    setYear(mgr, 2040);
    auto end = mgr.checkGameEnd();
    EXPECT_NE(end.reason, GameEndReason::None);
}

TEST(EndgamePaths, NoEndgameBefore2040WithCleanMeters) {
    auto mgr = makeMgr();
    setDoomMeters(mgr, 30, 30, 30, 30);
    setBank(mgr, 1e6, 50, 6);
    setYear(mgr, 2030);
    auto end = mgr.checkGameEnd();
    EXPECT_EQ(end.reason, GameEndReason::None);
}

// ── Save-game compat: legacy enums still serialize ────────────────

TEST(EndgamePaths, LegacyEnumValuesSerialize) {
    nlohmann::json j1 = GameEndReason::AISingularity;
    EXPECT_EQ(j1.get<std::string>(), "ai_singularity");
    nlohmann::json j2 = GameEndReason::ClimateCollapse;
    EXPECT_EQ(j2.get<std::string>(), "climate_collapse");
}

TEST(EndgamePaths, NewEnumValuesSerialize) {
    nlohmann::json j = GameEndReason::EndgameTransformation;
    EXPECT_EQ(j.get<std::string>(), "endgame_transformation");
}
