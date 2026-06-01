#include <gtest/gtest.h>
#include "stvg/game/DecisionHandler.h"
#include "stvg/core/BankState.h"
#include "stvg/core/GameConfig.h"
#include "stvg/core/ConsequenceTracker.h"
#include "stvg/simulation/TraderAI.h"
#include "stvg/math/RandomEngine.h"

using namespace stvg;
using namespace stvg::game;

class DecisionHandlerTest : public ::testing::Test {
protected:
    math::RandomEngine rng_{42};
    ConsequenceTracker consequenceTracker_{rng_};
    simulation::TraderAIEngine traderAI_{rng_};
    Bank bank_ = Bank::create({});
    ActionPointBudget ap_;
    std::vector<simulation::Trader> traders_;
    std::vector<simulation::Decision> decisions_;
    int recentDividendQuarters_ = 0;
    SimulationConfig config_;
    SimulationState state_;

    void SetUp() override {
        ap_.reset(bank_.totalAssets);
        state_.currentYear = 1945;
        state_.currentQuarter = 1;
        config_.startYear = 1945;

        simulation::Decision d;
        d.id = "dec_test_1";
        d.title = "Test Decision";
        d.type = simulation::DecisionType::Strategic;
        d.actionPointCost = 1;
        d.resolved = false;

        simulation::DecisionOption opt;
        opt.id = "dec_test_1_act";
        opt.title = "Act";
        opt.financialImpact.expectedRevenue = 1e6;
        opt.financialImpact.immediateCost = 0;
        opt.riskImpact.riskChange = 0.01;
        opt.successProbability = 0.8;
        d.options.push_back(opt);

        decisions_.push_back(d);
    }
};

TEST_F(DecisionHandlerTest, SubmitResolvesDecision) {
    bool ok = DecisionHandler::submit(
        "dec_test_1", "dec_test_1_act", decisions_, ap_,
        bank_, consequenceTracker_, traderAI_, traders_,
        recentDividendQuarters_, config_, state_);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(decisions_[0].resolved);
    EXPECT_EQ(decisions_[0].chosenOptionId, "dec_test_1_act");
}

TEST_F(DecisionHandlerTest, SubmitDeductsActionPoints) {
    int before = ap_.remaining();
    DecisionHandler::submit(
        "dec_test_1", "dec_test_1_act", decisions_, ap_,
        bank_, consequenceTracker_, traderAI_, traders_,
        recentDividendQuarters_, config_, state_);
    EXPECT_EQ(ap_.remaining(), before - 1);
}

TEST_F(DecisionHandlerTest, SubmitFailsWhenNotEnoughAP) {
    ap_.spent = ap_.total; // exhaust all AP
    bool ok = DecisionHandler::submit(
        "dec_test_1", "dec_test_1_act", decisions_, ap_,
        bank_, consequenceTracker_, traderAI_, traders_,
        recentDividendQuarters_, config_, state_);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(decisions_[0].resolved);
}

TEST_F(DecisionHandlerTest, SubmitFailsForNonexistentDecision) {
    bool ok = DecisionHandler::submit(
        "dec_nonexistent", "opt_1", decisions_, ap_,
        bank_, consequenceTracker_, traderAI_, traders_,
        recentDividendQuarters_, config_, state_);
    EXPECT_FALSE(ok);
}

TEST_F(DecisionHandlerTest, SubmitGeneratesConsequence) {
    DecisionHandler::submit(
        "dec_test_1", "dec_test_1_act", decisions_, ap_,
        bank_, consequenceTracker_, traderAI_, traders_,
        recentDividendQuarters_, config_, state_);
    EXPECT_GT(consequenceTracker_.pendingCount(), 0);
}

TEST_F(DecisionHandlerTest, LoanSpreadDecisionMutatesBank) {
    simulation::Decision spreadDec;
    spreadDec.id = "dec_loanspread_y1945";
    spreadDec.title = "Set Loan Spread";
    spreadDec.type = simulation::DecisionType::Strategic;
    spreadDec.actionPointCost = 0;
    spreadDec.resolved = false;
    simulation::DecisionOption opt;
    opt.id = "dec_loanspread_y1945_250bps";
    opt.title = "250 bps";
    opt.successProbability = 1.0;
    spreadDec.options.push_back(opt);
    decisions_.push_back(spreadDec);

    DecisionHandler::submit(
        "dec_loanspread_y1945", "dec_loanspread_y1945_250bps", decisions_, ap_,
        bank_, consequenceTracker_, traderAI_, traders_,
        recentDividendQuarters_, config_, state_);
    EXPECT_NEAR(bank_.loanSpread, 0.025, 1e-6);
}

TEST_F(DecisionHandlerTest, CreditStandardsDecisionMutatesBank) {
    simulation::Decision stdDec;
    stdDec.id = "dec_creditstd_y1945";
    stdDec.title = "Set Credit Standards";
    stdDec.type = simulation::DecisionType::Strategic;
    stdDec.actionPointCost = 0;
    stdDec.resolved = false;
    simulation::DecisionOption opt;
    opt.id = "dec_creditstd_y1945_80";
    opt.title = "Tight";
    opt.successProbability = 1.0;
    stdDec.options.push_back(opt);
    decisions_.push_back(stdDec);

    DecisionHandler::submit(
        "dec_creditstd_y1945", "dec_creditstd_y1945_80", decisions_, ap_,
        bank_, consequenceTracker_, traderAI_, traders_,
        recentDividendQuarters_, config_, state_);
    EXPECT_NEAR(bank_.creditStandards, 0.8, 1e-6);
}

TEST_F(DecisionHandlerTest, DividendDecisionReducesCapital) {
    simulation::Decision divDec;
    divDec.id = "dec_dividend_q1";
    divDec.title = "Dividend";
    divDec.type = simulation::DecisionType::Deleveraging;
    divDec.actionPointCost = 0;
    divDec.resolved = false;
    simulation::DecisionOption opt;
    opt.id = "dec_dividend_q1_dividend_pay";
    opt.title = "Pay Dividend";
    opt.successProbability = 1.0;
    divDec.options.push_back(opt);
    decisions_.push_back(divDec);

    double capitalBefore = bank_.capital;
    DecisionHandler::submit(
        "dec_dividend_q1", "dec_dividend_q1_dividend_pay", decisions_, ap_,
        bank_, consequenceTracker_, traderAI_, traders_,
        recentDividendQuarters_, config_, state_);
    EXPECT_LT(bank_.capital, capitalBefore);
    EXPECT_GT(recentDividendQuarters_, 0);
}
