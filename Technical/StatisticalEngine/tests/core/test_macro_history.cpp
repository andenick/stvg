#include <gtest/gtest.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <stvg/core/MacroHistory.h>
#include <stvg/simulation/EconomicEngine.h>
#include <stvg/simulation/HistoricalEventLoader.h>
#include <stvg/math/RandomEngine.h>
#include <nlohmann/json.hpp>

using namespace stvg;
using namespace stvg::simulation;

// ════════════════════════════════════════════════════════════════════
// STAR_02 P3E — living-economy plumbing tests
//   1. gdpLevel grows under positive growth
//   2. creditImpulse serialized into EconomicIndicators
//   3. MacroHistory accumulates 4 snapshots/year + save/load roundtrip
//   4. market_tick JSON contains an econ block
//   5. event sign-weighting biases crisis draws in a recession state
// ════════════════════════════════════════════════════════════════════

namespace {
// Run N quarters letting AP expire (mirrors test_save_load helper).
void runQuarters(QuarterlyTurnManager& mgr, int n) {
    for (int q = 0; q < n; ++q) {
        mgr.advancePhase();  // QuarterStart → DecisionPhase
        mgr.advancePhase();  // → Simulation
        mgr.advancePhase();  // → QuarterEnd
        while (mgr.currentPhase() != TurnPhase::QuarterComplete) {
            mgr.advancePhase();
        }
        if (mgr.isGameOver()) break;
        mgr.advancePhase();  // → next QuarterStart
    }
}
}  // namespace

// ── 1. gdpLevel accumulator ───────────────────────────────────────────

TEST(MacroPlumbing, GdpLevelGrowsUnderPositiveGrowth) {
    math::RandomEngine rng(42);
    EconomicEngine engine(rng);
    EconomicIndicators init;            // gdpLevel defaults to 100.0
    init.gdpGrowth = 0.04;              // strong positive growth
    init.vix = 12;
    engine.init(init);
    EXPECT_DOUBLE_EQ(engine.state().gdpLevel, 100.0);

    // One full year of daily ticks.
    for (int i = 0; i < 252; ++i) engine.tick();

    EXPECT_GT(engine.state().gdpLevel, 100.0)
        << "gdpLevel must rise after a year of positive growth";
    // Sanity: a ~3-4% growth path should land roughly in the low 100s, not absurd.
    EXPECT_LT(engine.state().gdpLevel, 120.0);
}

// ── 2. creditImpulse promoted into the indicator + serialized ──────────

TEST(MacroPlumbing, CreditImpulseSerializedInIndicators) {
    math::RandomEngine rng(7);
    EconomicEngine engine(rng);
    EconomicIndicators init;
    engine.init(init);
    engine.setCreditImpulse(0.031);
    EXPECT_DOUBLE_EQ(engine.state().creditImpulse, 0.031);

    // EconomicIndicators NLOHMANN serialization carries the new fields.
    nlohmann::json j = engine.state();
    ASSERT_TRUE(j.contains("creditImpulse"));
    ASSERT_TRUE(j.contains("gdpLevel"));
    EXPECT_DOUBLE_EQ(j["creditImpulse"].get<double>(), 0.031);

    // Roundtrip.
    auto back = j.get<EconomicIndicators>();
    EXPECT_DOUBLE_EQ(back.creditImpulse, 0.031);
    EXPECT_DOUBLE_EQ(back.gdpLevel, engine.state().gdpLevel);
}

// ── 3. MacroHistory: 4 snapshots/year + save/load roundtrip ───────────

TEST(MacroPlumbing, MacroHistoryAccumulatesFourPerYear) {
    SimulationConfig cfg;
    cfg.seed = 9999;
    QuarterlyTurnManager mgr(cfg);
    runQuarters(mgr, 4);  // one full year
    EXPECT_EQ(mgr.macroHistory().size(), 4u)
        << "one snapshot recorded per quarter end";

    // Snapshot content is sane: year set, econ + closes populated.
    const auto& snaps = mgr.macroHistory().snapshots();
    ASSERT_FALSE(snaps.empty());
    EXPECT_GE(snaps.front().year, cfg.startYear);
    EXPECT_FALSE(snaps.front().closes.empty());
}

TEST(MacroPlumbing, MacroHistorySurvivesSaveLoadRoundtrip) {
    SimulationConfig cfg;
    cfg.seed = 2468;
    QuarterlyTurnManager mgr(cfg);
    runQuarters(mgr, 6);
    size_t before = mgr.macroHistory().size();
    ASSERT_GE(before, 1u);

    auto blob = mgr.saveGame();
    ASSERT_TRUE(blob.contains("macroHistory"));
    EXPECT_EQ(blob["version"].get<int>(), QuarterlyTurnManager::kSaveVersion);

    QuarterlyTurnManager restored(cfg);
    ASSERT_TRUE(restored.loadGame(blob));
    EXPECT_EQ(restored.macroHistory().size(), before);

    // Field-level fidelity on the last snapshot.
    const auto& a = mgr.macroHistory().snapshots().back();
    const auto& b = restored.macroHistory().snapshots().back();
    EXPECT_EQ(a.year, b.year);
    EXPECT_EQ(a.quarter, b.quarter);
    EXPECT_DOUBLE_EQ(a.econ.gdpLevel, b.econ.gdpLevel);
    EXPECT_EQ(a.closes.size(), b.closes.size());
}

TEST(MacroPlumbing, MacroHistoryBackwardCompatMissingField) {
    SimulationConfig cfg;
    cfg.seed = 13;
    QuarterlyTurnManager mgr(cfg);
    runQuarters(mgr, 2);

    // Simulate an old (v2) save with no macroHistory key.
    auto blob = mgr.saveGame();
    blob.erase("macroHistory");
    blob["version"] = 2;

    QuarterlyTurnManager restored(cfg);
    ASSERT_TRUE(restored.loadGame(blob));
    EXPECT_TRUE(restored.macroHistory().empty())
        << "missing macroHistory must load as empty, not crash";
}

TEST(MacroPlumbing, MacroHistoryApiShape) {
    SimulationConfig cfg;
    cfg.seed = 55;
    QuarterlyTurnManager mgr(cfg);
    runQuarters(mgr, 4);

    auto api = mgr.macroHistoryJson();
    ASSERT_TRUE(api.contains("quarters"));
    ASSERT_TRUE(api["quarters"].is_array());
    EXPECT_EQ(api["quarters"].size(), 4u);
    const auto& q0 = api["quarters"][0];
    EXPECT_TRUE(q0.contains("year"));
    EXPECT_TRUE(q0.contains("quarter"));
    EXPECT_TRUE(q0.contains("econ"));
    EXPECT_TRUE(q0.contains("closes"));
    EXPECT_TRUE(q0["econ"].contains("gdpLevel"));
    EXPECT_TRUE(q0["econ"].contains("creditImpulse"));
}

// ── 4. market_tick payload contains an econ block ─────────────────────

TEST(MacroPlumbing, MarketTickContainsEconBlock) {
    SimulationConfig cfg;
    cfg.seed = 321;
    QuarterlyTurnManager mgr(cfg);
    // Step into a quarter so the clock/markets are live.
    mgr.advancePhase();  // QuarterStart → DecisionPhase

    auto tick = mgr.marketTickPayload();
    ASSERT_TRUE(tick.contains("econ"));
    const auto& e = tick["econ"];
    for (const char* k : {"gdpLevel", "gdpGrowth", "unemployment",
                          "fedFundsRate", "cpiInflation", "creditSpread"}) {
        EXPECT_TRUE(e.contains(k)) << "econ block missing key " << k;
    }
    EXPECT_TRUE(e["gdpLevel"].is_number());
}

// ── 5. Event sign-weighting (Stage A) ─────────────────────────────────

TEST(MacroPlumbing, RecessionBiasRaisesCrisisDrawProbability) {
    // Build a recession state vs a boom state, draw many times with a fixed
    // seed each side, and assert crisis-flavored events surface more often in
    // the recession. EventBias is weight-only so this is purely the bias's
    // effect on draw probabilities.
    EconomicIndicators recession;
    recession.gdpGrowth = -0.03;
    recession.vix = 38.0;
    recession.sp500Return = -0.02;
    recession.creditSpread = 0.05;

    EconomicIndicators baseline;  // benign defaults
    baseline.gdpGrowth = 0.025;
    baseline.vix = 16.0;

    EventBias bearBias = EventBias::fromEconomics(recession);
    EventBias neutralBias = EventBias::fromEconomics(baseline);
    EXPECT_TRUE(bearBias.bearish);
    EXPECT_GT(bearBias.k, 1.0);

    // Crisis events get up-weighted; opportunity down-weighted under bearish.
    EXPECT_GT(bearBias.weightFor(EventCategory::Crisis),
              neutralBias.weightFor(EventCategory::Crisis));
    EXPECT_LT(bearBias.weightFor(EventCategory::Opportunity),
              neutralBias.weightFor(EventCategory::Opportunity));

    // End-to-end through the historical loader: count crisis draws.
    HistoricalEventLoader loader;
    // Synthetic in-memory pool would require file IO; instead exercise the
    // EventPool draw which has a built-in starter pool with crisis + opportunity.
    //
    // Robustness note (D1): the empirical-count form below is a Monte-Carlo
    // confirmation of the weight property already asserted deterministically
    // above (weightFor(Crisis): bear > neutral). The bank is given explicit
    // hidden risk so the crisis category weight (0.2 + hiddenRisk/1e9) is solidly
    // non-trivial and crises reliably surface in a 4-of-pool draw; without it the
    // crisis weight is a bare 0.2 and crisis draws are so rare that the count can
    // degenerate to a 0-vs-0 tie depending on suite execution order (a fresh
    // EventPool/Bank are state-free, so this is purely small-sample fragility).
    // Trials are raised to 400 to keep the separation comfortably significant.
    EventPool pool;
    Bank bank;
    {
        Division d;
        d.id = "ib"; d.type = DivisionType::InvestmentBanking;
        d.hiddenRisk = 3.0e9;  // material hidden risk → crisis category is live
        bank.divisions.push_back(d);
    }
    SimulationState st;
    st.currentYear = 1985;

    auto countCrisis = [&](const EventBias& bias) {
        int crisisCount = 0;
        for (int trial = 0; trial < 400; ++trial) {
            math::RandomEngine rng(1000 + trial);
            auto drawn = pool.drawEvents(bank, st, rng, 4, bias);
            for (const auto& e : drawn)
                if (e.category == EventCategory::Crisis) crisisCount++;
        }
        return crisisCount;
    };

    int crisisBear = countCrisis(bearBias);
    int crisisNeutral = countCrisis(neutralBias);
    EXPECT_GT(crisisBear, crisisNeutral)
        << "crisis events should surface more often in a recession state "
        << "(bear=" << crisisBear << " neutral=" << crisisNeutral << ")";

    (void)loader;
}

TEST(MacroPlumbing, BoomBiasRaisesOpportunityDrawProbability) {
    EconomicIndicators boom;
    boom.gdpGrowth = 0.05;
    boom.vix = 11.0;
    boom.sp500Return = 0.03;

    EventBias bull = EventBias::fromEconomics(boom);
    EventBias neutral = EventBias::fromEconomics(EconomicIndicators{});
    EXPECT_TRUE(bull.bullish);
    EXPECT_GT(bull.weightFor(EventCategory::Opportunity),
              neutral.weightFor(EventCategory::Opportunity));
}
