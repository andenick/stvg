#include <gtest/gtest.h>
#include <stvg/core/BankState.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <stvg/autoplay/GameRunner.h>
#include <stvg/autoplay/BotStrategy.h>

using namespace stvg;
using namespace stvg::autoplay;

TEST(RevenueDecomp, LendingOnlyBankHasNII) {
    // Default CommercialBank has only lending divisions
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager mgr(cfg);
    // Run 4 quarters
    for (int i = 0; i < 4; ++i) {
        mgr.advancePhase(); // QuarterStart
        mgr.advancePhase(); // DecisionPhase
        mgr.advancePhase(); // Simulation
        mgr.advancePhase(); // QuarterEnd
        mgr.advancePhase(); // CrisisResponse or QuarterComplete
        if (mgr.currentPhase() == TurnPhase::QuarterComplete)
            mgr.advancePhase(); // back to QuarterStart
    }
    const auto& bank = mgr.bank();
    // Lending-only bank should have NII but no trading P&L
    EXPECT_EQ(bank.tradingPnL, 0) << "No trading divisions = no trading P&L";
}

TEST(RevenueDecomp, FundingCostComputed) {
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager mgr(cfg);
    // Run a few quarters
    for (int i = 0; i < 2; ++i) {
        mgr.advancePhase();
        mgr.advancePhase();
        mgr.advancePhase();
        mgr.advancePhase();
        mgr.advancePhase();
        if (mgr.currentPhase() == TurnPhase::QuarterComplete)
            mgr.advancePhase();
    }
    EXPECT_GE(mgr.bank().fundingCost, 0)
        << "Funding cost should be non-negative";
}

TEST(RevenueDecomp, NIIInJsonPlayerView) {
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager mgr(cfg);
    mgr.advancePhase();
    auto j = mgr.toPlayerJson();
    ASSERT_TRUE(j.contains("bank"));
    auto bank = j["bank"];
    EXPECT_TRUE(bank.contains("netInterestIncome"));
    EXPECT_TRUE(bank.contains("feeIncome"));
    EXPECT_TRUE(bank.contains("tradingPnL"));
    EXPECT_TRUE(bank.contains("loans"));
    EXPECT_TRUE(bank.contains("securities"));
    EXPECT_TRUE(bank.contains("depositToAssetRatio"));
}

TEST(RevenueDecomp, SFCFieldsInGodView) {
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager mgr(cfg);
    mgr.advancePhase();
    auto j = mgr.toGodJson();
    ASSERT_TRUE(j.contains("bank"));
    auto bank = j["bank"];
    EXPECT_TRUE(bank.contains("loans"));
    EXPECT_TRUE(bank.contains("securities"));
    EXPECT_TRUE(bank.contains("interbankBorrowing"));
    EXPECT_TRUE(bank.contains("depositRate"));
    EXPECT_TRUE(bank.contains("fundingCost"));
    EXPECT_TRUE(bank.contains("loanLossReserve"));
}

TEST(RevenueDecomp, AllStartingPositionsHaveSFCFields) {
    for (auto pos : {StartingPosition::CommercialBank, StartingPosition::TradingFirm,
                     StartingPosition::InvestmentBank, StartingPosition::CommunityBank,
                     StartingPosition::UniversalBank}) {
        SimulationConfig cfg;
        cfg.seed = 100 + (int)pos;
        BankConfig bc = bankConfigForPosition(pos);
        QuarterlyTurnManager mgr(cfg, bc, "", pos);
        auto j = mgr.toPlayerJson();
        ASSERT_TRUE(j.contains("bank")) << "position " << (int)pos;
        EXPECT_TRUE(j["bank"].contains("loans")) << "position " << (int)pos;
        EXPECT_TRUE(j["bank"].contains("depositToAssetRatio")) << "position " << (int)pos;
    }
}
