#include <gtest/gtest.h>
#include <stvg/core/BankState.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <stvg/autoplay/GameRunner.h>
#include <stvg/autoplay/BotStrategy.h>
#include <stvg/autoplay/EraAwareBots.h>

using namespace stvg;
using namespace stvg::autoplay;

TEST(DepositDynamics, DepositsPositiveAtCreation) {
    for (auto pos : {StartingPosition::CommercialBank, StartingPosition::TradingFirm,
                     StartingPosition::InvestmentBank, StartingPosition::CommunityBank,
                     StartingPosition::UniversalBank}) {
        SimulationConfig cfg;
        cfg.seed = 42;
        BankConfig bc = bankConfigForPosition(pos);
        QuarterlyTurnManager mgr(cfg, bc, "", pos);
        EXPECT_GT(mgr.bank().totalDeposits, 0) << "Position " << (int)pos;
    }
}

TEST(DepositDynamics, DepositsStableOver95Years) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    PersonalityDrivenBot bot("Balanced", PersonalityProfile::balanced());
    for (uint64_t seed = 42; seed < 47; ++seed) {
        auto result = runner.runGame(bot, config, seed, 380);
        EXPECT_FALSE(result.hadNaN) << "NaN at seed " << seed;
        EXPECT_GT(result.finalCapital, 0) << "Capital <= 0 at seed " << seed;
    }
}

TEST(DepositDynamics, InterbankBorrowingAbsorbsGap) {
    Bank b = Bank::create(BankConfig{});
    b.totalDeposits = b.totalAssets * 0.3;
    b.endQuarter();
    EXPECT_GT(b.interbankBorrowing, 0)
        << "Interbank should fill the gap when deposits < assets - capital";
}

TEST(DepositDynamics, DepositRateNonNegativeAtCreation) {
    Bank b = Bank::create(BankConfig{});
    EXPECT_GE(b.depositRate, 0.0);
}

TEST(DepositDynamics, DepositFloorHolds) {
    Bank b = Bank::create(BankConfig{});
    b.totalDeposits = 1.0; // force near-zero
    b.endQuarter();
    EXPECT_GT(b.totalDeposits, 0) << "Deposits should not go to zero";
}

TEST(DepositDynamics, DepositToAssetRatioReasonable) {
    for (auto pos : {StartingPosition::CommercialBank, StartingPosition::TradingFirm,
                     StartingPosition::InvestmentBank, StartingPosition::CommunityBank,
                     StartingPosition::UniversalBank}) {
        SimulationConfig cfg;
        cfg.seed = 42;
        BankConfig bc = bankConfigForPosition(pos);
        QuarterlyTurnManager mgr(cfg, bc, "", pos);
        double dtar = mgr.bank().depositToAssetRatio();
        EXPECT_GT(dtar, 0.2) << "DTAR too low at position " << (int)pos;
        EXPECT_LT(dtar, 0.98) << "DTAR too high at position " << (int)pos;
    }
}

TEST(DepositDynamics, MoneyMultiplierGrowsDeposits) {
    Bank b = Bank::create(BankConfig{});
    double initialDeposits = b.totalDeposits;
    for (int i = 0; i < 10; ++i) {
        for (auto& d : b.divisions) {
            d.revenue = d.budget * 0.02;
            d.costs = d.budget * 0.01;
        }
        b.endQuarter();
    }
    EXPECT_GT(b.totalDeposits, initialDeposits)
        << "Deposits should grow from money multiplier effect over 10 quarters";
}
