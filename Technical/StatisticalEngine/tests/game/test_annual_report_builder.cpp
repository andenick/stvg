#include <gtest/gtest.h>
#include "stvg/game/AnnualReportBuilder.h"
#include "stvg/core/BankState.h"
#include "stvg/core/RegulatoryEngine.h"

using namespace stvg;
using namespace stvg::game;

class AnnualReportBuilderTest : public ::testing::Test {
protected:
    Bank bank_ = Bank::create({});
    PlayerProgression progression_;
    RegulatoryEngine regEngine_;
    AnnualReportBuilder::AnnualAccumulator acc_;

    void SetUp() override {
        regEngine_.init();
        acc_.yearStartCapital = bank_.capital;

        // Simulate 4 quarters of data
        acc_.accumulate(1e9, 0.7e9, 75.0, {"Q1 profit"});
        acc_.accumulate(1.2e9, 0.8e9, 80.0, {"Q2 growth"});
        acc_.accumulate(0.9e9, 0.6e9, 70.0, {"Q3 steady"});
        acc_.accumulate(1.1e9, 0.75e9, 77.0, {"Q4 close"});

        bank_.capital = acc_.yearStartCapital + 1e9; // net growth
        bank_.netInterestIncome = 0.5e9;
        bank_.feeIncome = 0.2e9;
        bank_.tradingPnL = 0.1e9;
        bank_.fundingCost = 0.15e9;
        bank_.quarterlyProvisions = 0.05e9;
        bank_.loans = 50e9;
        bank_.securities = 20e9;
        bank_.retailDeposits = 40e9;
        bank_.wholesaleDeposits = 20e9;
        bank_.totalDeposits = 60e9;
        bank_.interbankBorrowing = 5e9;
    }
};

TEST_F(AnnualReportBuilderTest, GeneratesCorrectTotals) {
    AnnualReport report;
    AnnualReportBuilder::generate(report, 1946, bank_, progression_,
        regEngine_.state(), "Post-War Stability", acc_);

    EXPECT_EQ(report.year, 1946);
    EXPECT_NEAR(report.totalRevenue, 4.2e9, 1e6);
    EXPECT_NEAR(report.totalCosts, 2.85e9, 1e6);
    EXPECT_NEAR(report.netIncome, 1.35e9, 1e6);
    EXPECT_EQ(report.quartersPlayed, 4);
}

TEST_F(AnnualReportBuilderTest, CapitalGrowthCalculated) {
    AnnualReport report;
    AnnualReportBuilder::generate(report, 1946, bank_, progression_,
        regEngine_.state(), "Post-War Stability", acc_);

    EXPECT_EQ(report.capitalStart, acc_.yearStartCapital);
    EXPECT_EQ(report.capitalEnd, bank_.capital);
    EXPECT_GT(report.capitalGrowthPct, 0);
}

TEST_F(AnnualReportBuilderTest, AvgScoreCalculated) {
    AnnualReport report;
    AnnualReportBuilder::generate(report, 1946, bank_, progression_,
        regEngine_.state(), "Post-War Stability", acc_);

    EXPECT_NEAR(report.avgQuarterScore, (75 + 80 + 70 + 77) / 4.0, 0.1);
}

TEST_F(AnnualReportBuilderTest, FinancialStatementsAnnualized) {
    AnnualReport report;
    AnnualReportBuilder::generate(report, 1946, bank_, progression_,
        regEngine_.state(), "Post-War Stability", acc_);

    EXPECT_NEAR(report.netInterestIncome, bank_.netInterestIncome * 4, 1e3);
    EXPECT_NEAR(report.feeIncome, bank_.feeIncome * 4, 1e3);
    EXPECT_NEAR(report.tradingPnL, bank_.tradingPnL * 4, 1e3);
    EXPECT_NEAR(report.loanLossProvisions, bank_.quarterlyProvisions * 4, 1e3);
}

TEST_F(AnnualReportBuilderTest, BalanceSheetSnapshot) {
    AnnualReport report;
    AnnualReportBuilder::generate(report, 1946, bank_, progression_,
        regEngine_.state(), "Post-War Stability", acc_);

    EXPECT_EQ(report.loans, bank_.loans);
    EXPECT_EQ(report.securities, bank_.securities);
    EXPECT_EQ(report.retailDeposits, bank_.retailDeposits);
    EXPECT_EQ(report.wholesaleDeposits, bank_.wholesaleDeposits);
}

TEST_F(AnnualReportBuilderTest, RatiosComputed) {
    AnnualReport report;
    AnnualReportBuilder::generate(report, 1946, bank_, progression_,
        regEngine_.state(), "Post-War Stability", acc_);

    EXPECT_GT(report.nim, 0);
    EXPECT_GT(report.roe, 0);
    EXPECT_GT(report.roa, 0);
    EXPECT_GT(report.efficiencyRatio, 0);
    EXPECT_GT(report.loanToDepositRatio, 0);
}

TEST_F(AnnualReportBuilderTest, HeadlinesAccumulated) {
    AnnualReport report;
    AnnualReportBuilder::generate(report, 1946, bank_, progression_,
        regEngine_.state(), "Post-War Stability", acc_);

    EXPECT_EQ(report.headlines.size(), 4u);
    EXPECT_EQ(report.headlines[0], "Q1 profit");
    EXPECT_EQ(report.headlines[3], "Q4 close");
}

TEST_F(AnnualReportBuilderTest, AccumulatorResets) {
    acc_.reset(50e9);
    EXPECT_EQ(acc_.yearStartCapital, 50e9);
    EXPECT_EQ(acc_.yearTotalRevenue, 0);
    EXPECT_EQ(acc_.yearQuarterCount, 0);
    EXPECT_TRUE(acc_.yearHeadlines.empty());
}
