#include <gtest/gtest.h>
#include <stvg/core/BankState.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <stvg/autoplay/GameRunner.h>
#include <stvg/autoplay/BotStrategy.h>
#include <stvg/autoplay/EraAwareBots.h>
#include <stvg/simulation/EconomicEngine.h>
#include <cmath>

using namespace stvg;
using namespace stvg::autoplay;

static constexpr double TOLERANCE = 1000.0;

static void assertIdentity(const Bank& b, const std::string& ctx) {
    EXPECT_NEAR(b.totalDeposits + b.interbankBorrowing + b.capital, b.totalAssets, TOLERANCE)
        << ctx << ": L+E != A";
    EXPECT_GE(b.totalAssets + TOLERANCE, b.loans + b.securities + b.reserves)
        << ctx << ": totalAssets < explicit components";
    EXPECT_FALSE(std::isnan(b.totalAssets)) << ctx << ": totalAssets is NaN";
    EXPECT_FALSE(std::isinf(b.totalAssets)) << ctx << ": totalAssets is Inf";
    EXPECT_FALSE(std::isnan(b.capital)) << ctx << ": capital is NaN";
}

TEST(SFCEdge, ZeroCapitalIdentityHolds) {
    Bank b = Bank::create(BankConfig{});
    b.capital = 1.0; // near zero
    b.endQuarter();
    assertIdentity(b, "near-zero capital");
}

TEST(SFCEdge, HugeCapitalIdentityHolds) {
    BankConfig cfg;
    cfg.startingCapital = 1e15;
    Bank b = Bank::create(cfg);
    b.endQuarter();
    assertIdentity(b, "huge capital");
    EXPECT_FALSE(std::isnan(b.loans));
    EXPECT_FALSE(std::isinf(b.totalAssets));
}

TEST(SFCEdge, NoDivisionsIdentityHolds) {
    Bank b;
    b.capital = 1e9;
    b.reserves = 1e8;
    b.totalDeposits = 5e9;
    b.divisions.clear();
    b.endQuarter();
    EXPECT_EQ(b.loans, 0);
    EXPECT_EQ(b.securities, 0);
    assertIdentity(b, "no divisions");
}

TEST(SFCEdge, AllDivisionsAreFeeBasedIdentityHolds) {
    Bank b = Bank::create(BankConfig{});
    b.divisions.clear();
    Division d1;
    d1.id = "trust"; d1.type = DivisionType::TrustAndCustody; d1.budget = 2e9;
    Division d2;
    d2.id = "am"; d2.type = DivisionType::AssetManagement; d2.budget = 3e9;
    b.divisions.push_back(d1);
    b.divisions.push_back(d2);
    b.recalcBalanceSheet();
    EXPECT_EQ(b.loans, 0) << "Fee-based divisions shouldn't create loans";
    EXPECT_EQ(b.securities, 0) << "Fee-based divisions shouldn't create securities";
    assertIdentity(b, "all fee-based");
}

TEST(SFCEdge, AllDivisionsAreSecuritiesIdentityHolds) {
    Bank b = Bank::create(BankConfig{});
    b.divisions.clear();
    Division d1;
    d1.id = "trading"; d1.type = DivisionType::TradingDesk; d1.budget = 3e9;
    Division d2;
    d2.id = "deriv"; d2.type = DivisionType::DerivativesDesk; d2.budget = 2e9;
    b.divisions.push_back(d1);
    b.divisions.push_back(d2);
    b.recalcBalanceSheet();
    EXPECT_EQ(b.loans, 0);
    EXPECT_GT(b.securities, 0);
    assertIdentity(b, "all securities");
}

TEST(SFCEdge, MaximumDivisions) {
    Bank b = Bank::create(BankConfig{});
    b.divisions.clear();
    std::vector<DivisionType> allTypes = {
        DivisionType::CommercialLending, DivisionType::MortgageLending,
        DivisionType::TrustAndCustody, DivisionType::CreditCards,
        DivisionType::InternationalBanking, DivisionType::TradingDesk,
        DivisionType::AssetManagement, DivisionType::InvestmentBanking,
        DivisionType::PrivateEquity, DivisionType::Restructuring,
        DivisionType::Securitization, DivisionType::DerivativesDesk,
        DivisionType::WealthManagement, DivisionType::VentureCapital,
        DivisionType::Fintech, DivisionType::CryptoCustody
    };
    for (int i = 0; i < (int)allTypes.size(); ++i) {
        Division d;
        d.id = "div_" + std::to_string(i);
        d.type = allTypes[i];
        d.budget = 1e9;
        b.divisions.push_back(d);
    }
    b.recalcBalanceSheet();
    EXPECT_GT(b.loans, 0);
    EXPECT_GT(b.securities, 0);
    assertIdentity(b, "all 16 divisions");
}

TEST(SFCEdge, DepositRateZero) {
    Bank b = Bank::create(BankConfig{});
    b.depositRate = 0.0;
    for (int i = 0; i < 10; ++i) b.endQuarter();
    EXPECT_FALSE(std::isnan(b.fundingCost));
    assertIdentity(b, "zero deposit rate");
}

TEST(SFCEdge, DepositRateVeryHigh) {
    Bank b = Bank::create(BankConfig{});
    b.depositRate = 0.20;
    for (int i = 0; i < 10; ++i) b.endQuarter();
    EXPECT_FALSE(std::isnan(b.fundingCost));
    EXPECT_FALSE(std::isinf(b.fundingCost));
    assertIdentity(b, "20% deposit rate");
}

TEST(SFCEdge, NegativeNetIncomePreservesIdentity) {
    Bank b = Bank::create(BankConfig{});
    for (auto& d : b.divisions) {
        d.revenue = 0;
        d.costs = d.budget * 0.1;
    }
    for (int i = 0; i < 10; ++i) b.endQuarter();
    EXPECT_LT(b.capital, 10e9) << "Capital should have shrunk from losses";
    assertIdentity(b, "negative net income");
}

TEST(SFCEdge, CommunityBankSmallScale) {
    SimulationConfig cfg;
    cfg.seed = 42;
    BankConfig bc = bankConfigForPosition(StartingPosition::CommunityBank);
    QuarterlyTurnManager mgr(cfg, bc, "", StartingPosition::CommunityBank);
    assertIdentity(mgr.bank(), "CommunityBank at creation");
    EXPECT_GT(mgr.bank().totalAssets, 0);
}

TEST(SFCEdge, UniversalBankLargeScale) {
    SimulationConfig cfg;
    cfg.seed = 42;
    BankConfig bc = bankConfigForPosition(StartingPosition::UniversalBank);
    QuarterlyTurnManager mgr(cfg, bc, "", StartingPosition::UniversalBank);
    assertIdentity(mgr.bank(), "UniversalBank at creation");
    EXPECT_GT(mgr.bank().totalAssets, 1e8) << "Universal bank should have > $100M assets (rescaled)";
}

TEST(SFCEdge, CreditImpulseExtremeValues) {
    EconomicIndicators init;
    init.gdpGrowth = 0.025; init.fedFundsRate = 0.02;
    init.cpiInflation = 0.02; init.unemployment = 0.05;
    init.vix = 15; init.treasuryYield10Y = 0.03; init.creditSpread = 0.015;

    for (double impulse : {-0.05, 0.05}) {
        math::RandomEngine rng(42);
        simulation::EconomicEngine engine(rng);
        engine.init(init);
        engine.setCreditImpulse(impulse);
        for (int i = 0; i < 252; ++i) engine.tick();
        auto s = engine.state();
        EXPECT_FALSE(std::isnan(s.gdpGrowth)) << "impulse=" << impulse;
        EXPECT_FALSE(std::isnan(s.cpiInflation)) << "impulse=" << impulse;
        EXPECT_FALSE(std::isnan(s.vix)) << "impulse=" << impulse;
        EXPECT_FALSE(std::isinf(s.gdpGrowth)) << "impulse=" << impulse;
    }
}

TEST(SFCEdge, RecalcBalanceSheetIdempotent) {
    Bank b = Bank::create(BankConfig{});
    double l1 = b.loans, s1 = b.securities, d1 = b.totalDeposits, a1 = b.totalAssets;
    b.recalcBalanceSheet();
    EXPECT_DOUBLE_EQ(b.loans, l1);
    EXPECT_DOUBLE_EQ(b.securities, s1);
    EXPECT_DOUBLE_EQ(b.totalDeposits, d1);
    EXPECT_DOUBLE_EQ(b.totalAssets, a1);
}
