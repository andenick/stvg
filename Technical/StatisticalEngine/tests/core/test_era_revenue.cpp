#include <gtest/gtest.h>
#include "stvg/core/BankState.h"

using namespace stvg;

// ════════════════════════════════════════════════════════════════════
// Era-Variable Revenue Rate Tests
// ════════════════════════════════════════════════════════════════════

TEST(EraRevenue, CreditCardsLowBefore1970) {
    Division div;
    div.type = DivisionType::CreditCards;
    double base = div.baseRevenueRate();
    double era1960 = div.baseRevenueRate(1960);
    EXPECT_NEAR(era1960, base * 0.3, 0.001)
        << "Credit cards should earn 30% of base rate before 1970";
}

TEST(EraRevenue, CreditCardsPeakIn2000s) {
    Division div;
    div.type = DivisionType::CreditCards;
    double base = div.baseRevenueRate();
    double era2005 = div.baseRevenueRate(2005);
    EXPECT_NEAR(era2005, base * 1.2, 0.001)
        << "Credit cards should earn 120% of base rate in 2000s";
}

TEST(EraRevenue, TradingLowBefore1980) {
    Division div;
    div.type = DivisionType::TradingDesk;
    double base = div.baseRevenueRate();
    double era1970 = div.baseRevenueRate(1970);
    EXPECT_NEAR(era1970, base * 0.5, 0.001)
        << "Trading should earn 50% of base rate before 1980";
}

TEST(EraRevenue, TradingPeakInDeregulation) {
    Division div;
    div.type = DivisionType::TradingDesk;
    double base = div.baseRevenueRate();
    double era1995 = div.baseRevenueRate(1995);
    EXPECT_NEAR(era1995, base * 1.5, 0.001)
        << "Trading should earn 150% in deregulation era (1980-2000)";
}

TEST(EraRevenue, SecuritizationPeakBefore2007) {
    Division div;
    div.type = DivisionType::Securitization;
    double base = div.baseRevenueRate();
    double era2005 = div.baseRevenueRate(2005);
    EXPECT_NEAR(era2005, base * 2.0, 0.001)
        << "Securitization should earn 200% during CDO boom (1995-2007)";
}

TEST(EraRevenue, SecuritizationLowPostGFC) {
    Division div;
    div.type = DivisionType::Securitization;
    double base = div.baseRevenueRate();
    double era2015 = div.baseRevenueRate(2015);
    EXPECT_NEAR(era2015, base * 0.7, 0.001)
        << "Securitization should earn 70% post-GFC";
}

TEST(EraRevenue, MortgageBoomIn2005) {
    Division div;
    div.type = DivisionType::MortgageLending;
    double base = div.baseRevenueRate();
    double era2005 = div.baseRevenueRate(2005);
    EXPECT_NEAR(era2005, base * 1.8, 0.001)
        << "Mortgage lending should earn 180% during subprime boom (2003-2007)";
}

TEST(EraRevenue, DefaultDivisionsUnchanged) {
    Division div;
    div.type = DivisionType::CommercialLending;
    double base = div.baseRevenueRate();
    double era1950 = div.baseRevenueRate(1950);
    double era2020 = div.baseRevenueRate(2020);
    EXPECT_DOUBLE_EQ(era1950, base)
        << "CommercialLending should not change with era";
    EXPECT_DOUBLE_EQ(era2020, base)
        << "CommercialLending should not change with era";
}

TEST(EraRevenue, InvestmentBankingLowBeforeGlassSteagall) {
    Division div;
    div.type = DivisionType::InvestmentBanking;
    double base = div.baseRevenueRate();
    double era1980 = div.baseRevenueRate(1980);
    EXPECT_NEAR(era1980, base * 0.5, 0.001)
        << "IB should earn 50% before Glass-Steagall repeal";
}
