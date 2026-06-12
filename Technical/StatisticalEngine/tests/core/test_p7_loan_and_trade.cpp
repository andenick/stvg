// STAR_02 P7 — loan outcome feedback loop + personal trading book.
//
// Covers:
//   • An accepted deal resolves within its duration; loan-book stats accumulate
//     and a player-facing outcome event is emitted.
//   • The personal trading book: buy → price-move → sell realizes the right P&L;
//     limit rejection; unknown market; no-shorts rejection.
//   • Lockstep: trades are exogenous and do NOT perturb the day-by-day-vs-batch
//     RNG sequence (markets + bank capital stay identical with trades injected).

#include <gtest/gtest.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <cmath>

using namespace stvg;

static SimulationConfig cfg(uint64_t seed = 2025) {
    SimulationConfig c;
    c.seed = seed;
    c.startYear = 1945;
    c.startQuarter = 1;
    c.quarterDurationDays = 63;
    return c;
}

// Run one full quarter through the batch path.
static void runQuarter(QuarterlyTurnManager& m) {
    m.advancePhase(); // QuarterStart -> DecisionPhase
    m.advancePhase(); // DecisionPhase -> Simulation
    m.advancePhase(); // Simulation -> QuarterEnd (runs runSimulation + onQuarterEnd? no: QuarterEnd runs onQuarterEnd on next advance)
    m.advancePhase(); // QuarterEnd -> CrisisResponse|QuarterComplete (runs onQuarterEnd)
    if (m.currentPhase() == TurnPhase::CrisisResponse) m.advancePhase();
    m.advancePhase(); // -> QuarterStart
}

// ── Loan outcome feedback loop ─────────────────────────────────────────────

TEST(P7LoanOutcomes, AcceptedDealResolvesAndStatsAccumulate) {
    QuarterlyTurnManager m(cfg(777));

    // Deals are generated at quarter start with a per-line probability, so a
    // single quarter may produce none. Advance quarter starts until at least one
    // prospect appears (deterministic for the fixed seed; bounded retries).
    std::string dealId;
    int dur = 4;
    for (int attempt = 0; attempt < 12 && dealId.empty(); ++attempt) {
        m.advancePhase(); // QuarterStart -> DecisionPhase (deals generated)
        const auto& deals = m.availableDeals();
        if (!deals.empty()) {
            // Pick the shortest-duration deal so it resolves quickly.
            int best = 99;
            for (const auto& d : deals) {
                if (d.risk.durationQuarters < best) { best = d.risk.durationQuarters; dealId = d.id; }
            }
            dur = best;
            break;
        }
        // No deals this quarter — finish it and loop to the next quarter start.
        m.advancePhase(); // DecisionPhase -> Simulation
        m.advancePhase(); // Simulation -> QuarterEnd
        m.advancePhase(); // QuarterEnd -> QuarterComplete|CrisisResponse
        if (m.currentPhase() == TurnPhase::CrisisResponse) m.advancePhase();
        m.advancePhase(); // -> QuarterStart
    }
    ASSERT_FALSE(dealId.empty()) << "no deal opportunity appeared within 12 quarters";

    double stake = m.bank().capital * 0.05;
    ASSERT_TRUE(m.acceptDeal(dealId, stake));

    // loanBookStats.accepted bumps immediately on accept.
    auto view0 = m.toPlayerJson();
    ASSERT_TRUE(view0.contains("loanBookStats"));
    EXPECT_EQ(view0["loanBookStats"]["accepted"].get<int>(), 1);
    EXPECT_TRUE(view0.contains("dealOutcomes"));

    // Finish the current quarter (we are mid-DecisionPhase).
    m.advancePhase(); // DecisionPhase -> Simulation
    m.advancePhase(); // Simulation -> QuarterEnd
    m.advancePhase(); // QuarterEnd -> (resolves) QuarterComplete|CrisisResponse
    if (m.currentPhase() == TurnPhase::CrisisResponse) m.advancePhase();
    m.advancePhase(); // -> QuarterStart

    // Run enough additional quarters to exceed the deal's duration.
    for (int q = 0; q <= dur + 1; ++q) runQuarter(m);

    auto view = m.toPlayerJson();
    const auto& stats = view["loanBookStats"];
    int resolved = stats["paidOff"].get<int>() + stats["defaulted"].get<int>()
                 + stats["windfalls"].get<int>();
    EXPECT_GE(resolved, 1) << "the accepted deal should have resolved by now";

    // An outcome event for our deal should be present with a valid outcome label.
    const auto& outcomes = view["dealOutcomes"];
    ASSERT_TRUE(outcomes.is_array());
    bool found = false;
    for (const auto& o : outcomes) {
        if (o["dealId"].get<std::string>() == dealId) {
            found = true;
            std::string oc = o["outcome"].get<std::string>();
            EXPECT_TRUE(oc == "paid_off" || oc == "defaulted" || oc == "windfall");
            EXPECT_GT(o["quartersHeld"].get<int>(), 0);
        }
    }
    EXPECT_TRUE(found) << "expected an outcome event for the accepted deal";
}

// ── Personal trading book ──────────────────────────────────────────────────

TEST(P7TradeBook, BuyPriceMoveSellRealizesPnl) {
    QuarterlyTurnManager m(cfg(123));
    m.advancePhase(); // -> DecisionPhase (markets initialized at construction)

    // Buy $5k of S&P at the current price.
    double price0 = m.marketPriceFor("SP500");
    ASSERT_GT(price0, 0.0);
    auto r = m.trade("SP500", "buy", 5000.0);
    ASSERT_TRUE(r.ok) << r.err;
    double qty = r.qtyDelta;
    EXPECT_GT(qty, 0.0);

    // Run a quarter so the market price moves.
    m.advancePhase(); m.advancePhase();
    if (m.currentPhase() == TurnPhase::CrisisResponse) m.advancePhase();
    m.advancePhase(); m.advancePhase();
    if (m.currentPhase() == TurnPhase::QuarterStart) {} // landed at QuarterStart

    double price1 = m.marketPriceFor("SP500");
    // Sell the whole position ($ notional = qty * current price).
    auto s = m.trade("SP500", "sell", qty * price1);
    ASSERT_TRUE(s.ok) << s.err;
    // Realized P&L ≈ qty * (price1 - price0).
    double expected = qty * (price1 - price0);
    EXPECT_NEAR(s.realizedPnl, expected, std::abs(expected) * 1e-6 + 1e-6);
}

TEST(P7TradeBook, LimitRejection) {
    QuarterlyTurnManager m(cfg(55));
    m.advancePhase();
    // Phase-1 limit is 20% of capital; seed cash is only 5%. A buy beyond the
    // smaller of (cash, limit) is rejected. Try to buy 50% of capital → rejected.
    double huge = m.bank().capital * 0.5;
    auto r = m.trade("SP500", "buy", huge);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.err.empty());
}

TEST(P7TradeBook, UnknownMarketRejected) {
    QuarterlyTurnManager m(cfg(55));
    m.advancePhase();
    auto r = m.trade("NOPE", "buy", 100.0);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.err, "unknown market");
}

TEST(P7TradeBook, NoShorts) {
    QuarterlyTurnManager m(cfg(55));
    m.advancePhase();
    // Selling with no position is rejected (no shorts in v1).
    auto r = m.trade("SP500", "sell", 100.0);
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.err.empty());
}

TEST(P7TradeBook, BankCapitalUnaffectedByTrades) {
    QuarterlyTurnManager m(cfg(909));
    m.advancePhase();
    double capBefore = m.bank().capital;
    auto r = m.trade("SP500", "buy", 2000.0);
    ASSERT_TRUE(r.ok) << r.err;
    // Off-balance-sheet: the bank's capital must NOT move on a personal trade.
    EXPECT_DOUBLE_EQ(m.bank().capital, capBefore);
}

// ── Lockstep: trades do not perturb the RNG sequence ───────────────────────

TEST(P7TradeBook, TradesDoNotBreakLockstep) {
    auto c = cfg(31337);

    // Reference run: batch, no trades.
    QuarterlyTurnManager ref(c);
    ref.advancePhase(); ref.advancePhase(); ref.advancePhase();
    EXPECT_EQ(ref.currentPhase(), TurnPhase::QuarterEnd);

    // Day-by-day with a personal trade injected mid-quarter.
    QuarterlyTurnManager dd(c);
    dd.setRealTimeSimulation(true);
    dd.advancePhase(); dd.advancePhase();
    dd.prepareSimulation();
    int tick = 0;
    while (!dd.tickSimulationDay()) {
        // Inject a buy on day 10 and a sell on day 30 — exogenous player input.
        if (tick == 10) dd.trade("SP500", "buy", 1000.0);
        if (tick == 30) dd.trade("SP500", "sell", 200.0);
        tick++;
    }
    dd.completeSimulationPhase();

    auto rm = ref.state().markets;
    auto dm = dd.state().markets;
    ASSERT_EQ(rm.size(), dm.size());
    for (size_t i = 0; i < rm.size(); ++i) {
        EXPECT_NEAR(rm[i].currentPrice, dm[i].currentPrice, 1e-6)
            << "market " << i << " perturbed by personal trades — lockstep broken";
    }
    // Bank capital is off-balance-sheet from the personal book → identical too.
    EXPECT_NEAR(ref.bank().capital, dd.bank().capital, 1e-6);
}
