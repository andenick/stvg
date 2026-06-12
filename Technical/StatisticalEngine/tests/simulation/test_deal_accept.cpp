#include <gtest/gtest.h>
#include <stvg/simulation/DealPortfolio.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <stvg/math/RandomEngine.h>

using namespace stvg;
using namespace stvg::simulation;

// ════════════════════════════════════════════════════════════════════
// STAR_02 P5 Item 6 — /deal/accept robustness.
//
// Reproduces the crash/leak surface found during P1 verification: accepting a
// deal by id, then repeat-accepting the same id, and accepting an unknown id.
// Covered at two layers: DealPortfolio directly, and the QuarterlyTurnManager
// acceptDeal() handler the WebSocket route calls.
// ════════════════════════════════════════════════════════════════════

namespace {
// Build a portfolio with one known available deal and return its id.
std::string seedOneDeal(DealPortfolio& port, math::RandomEngine& rng) {
    Division div;
    div.id = "d1";
    div.type = DivisionType::CommercialLending;
    div.budget = 10000.0;
    std::vector<Division> divs{div};
    // Force generation deterministically until we have at least one deal.
    for (int i = 0; i < 50 && port.availableDeals().empty(); ++i)
        port.generateOpportunities(divs, 1960);
    if (port.availableDeals().empty()) return "";
    return port.availableDeals().front().id;
}
} // namespace

TEST(DealAccept, AcceptKnownIdSucceedsAndMovesToActive) {
    math::RandomEngine rng(7);
    DealPortfolio port(rng);
    std::string id = seedOneDeal(port, rng);
    ASSERT_FALSE(id.empty());

    size_t before = port.availableDeals().size();
    EXPECT_TRUE(port.acceptDeal(id, 100.0, 1));
    EXPECT_EQ(port.availableDeals().size(), before - 1);
    EXPECT_EQ(port.totalActiveDeals(), 1);
}

TEST(DealAccept, RepeatAcceptSameIdIsRejectedNotCrash) {
    math::RandomEngine rng(11);
    DealPortfolio port(rng);
    std::string id = seedOneDeal(port, rng);
    ASSERT_FALSE(id.empty());

    EXPECT_TRUE(port.acceptDeal(id, 100.0, 1));
    // Second accept of the now-consumed id must return false, not crash or
    // double-activate (the deal was erased from availableDeals_).
    EXPECT_FALSE(port.acceptDeal(id, 100.0, 1));
    EXPECT_EQ(port.totalActiveDeals(), 1);
}

TEST(DealAccept, UnknownIdRejected) {
    math::RandomEngine rng(13);
    DealPortfolio port(rng);
    seedOneDeal(port, rng);
    EXPECT_FALSE(port.acceptDeal("deal_does_not_exist", 100.0, 1));
}

// ── Handler-level (the route's exact call) ──────────────────────────

TEST(DealAccept, HandlerRejectsUnknownIdWithoutLeakingCapital) {
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager game(cfg);
    double capBefore = game.bank().capital;
    // Unknown id: must return false AND not debit the bank (the pre-fix bug
    // deducted capital before confirming the deal existed).
    EXPECT_FALSE(game.acceptDeal("deal_nope", 50.0));
    EXPECT_DOUBLE_EQ(game.bank().capital, capBefore);
}

TEST(DealAccept, HandlerRejectsNonPositiveAndNonFiniteAmounts) {
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager game(cfg);
    double capBefore = game.bank().capital;
    EXPECT_FALSE(game.acceptDeal("anything", 0.0));
    EXPECT_FALSE(game.acceptDeal("anything", -5.0));
    EXPECT_FALSE(game.acceptDeal("anything",
        std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(game.acceptDeal("anything",
        std::numeric_limits<double>::infinity()));
    EXPECT_DOUBLE_EQ(game.bank().capital, capBefore);
}

TEST(DealAccept, HandlerRejectsOversizedInvestment) {
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager game(cfg);
    double capBefore = game.bank().capital;
    // > 30% of capital is rejected and never debited.
    EXPECT_FALSE(game.acceptDeal("anything", game.bank().capital));
    EXPECT_DOUBLE_EQ(game.bank().capital, capBefore);
}

// Repeat-accept through the handler on a live deal: first accepts, debits
// once; a second accept of the same id is rejected and does NOT double-debit.
TEST(DealAccept, HandlerRepeatAcceptDoesNotDoubleDebit) {
    SimulationConfig cfg;
    cfg.seed = 99;
    QuarterlyTurnManager game(cfg);
    // Drive one quarter so deals are generated, then grab a live deal id.
    game.advancePhase(); // QuarterStart -> DecisionPhase (generates deals)
    std::string id;
    for (const auto& d : game.availableDeals()) { id = d.id; break; }
    if (id.empty()) GTEST_SKIP() << "no deals generated this seed";

    double cap0 = game.bank().capital;
    double amt = cap0 * 0.05;
    ASSERT_TRUE(game.acceptDeal(id, amt));
    double cap1 = game.bank().capital;
    EXPECT_NEAR(cap1, cap0 - amt, 1e-6);
    // Repeat — rejected, no further debit.
    EXPECT_FALSE(game.acceptDeal(id, amt));
    EXPECT_DOUBLE_EQ(game.bank().capital, cap1);
}
