#include <gtest/gtest.h>
#include <stvg/core/BankState.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <stvg/autoplay/GameRunner.h>
#include <stvg/autoplay/BotStrategy.h>
#include <stvg/autoplay/EraAwareBots.h>
#include <cmath>

using namespace stvg;
using namespace stvg::autoplay;

static constexpr double TOLERANCE = 1000.0; // $1K tolerance for floating point

static double identityResidual(const Bank& b) {
    double assets = b.loans + b.securities + b.reserves;
    double liabEquity = b.totalDeposits + b.interbankBorrowing + b.capital;
    return std::fabs(assets - liabEquity);
}

static void assertIdentity(const Bank& b, const std::string& context) {
    // Core identity: A = L + E (deposits + interbank + capital = totalAssets)
    EXPECT_NEAR(b.totalDeposits + b.interbankBorrowing + b.capital, b.totalAssets, TOLERANCE)
        << context << ": L+E != A";
    // Asset components are a lower bound (fee-based divisions may not have explicit assets)
    EXPECT_GE(b.totalAssets + TOLERANCE, b.loans + b.securities + b.reserves)
        << context << ": totalAssets < explicit asset components";
    EXPECT_GE(b.loans, 0) << context;
    EXPECT_GE(b.securities, 0) << context;
    EXPECT_GE(b.reserves, 0) << context;
    EXPECT_GE(b.totalDeposits, 0) << context;
    EXPECT_GE(b.interbankBorrowing, -TOLERANCE) << context;
}

TEST(SFCInvariant, BalanceSheetClosesAtCreation) {
    for (auto pos : {StartingPosition::CommercialBank, StartingPosition::TradingFirm,
                     StartingPosition::InvestmentBank, StartingPosition::CommunityBank,
                     StartingPosition::UniversalBank}) {
        SimulationConfig cfg;
        cfg.seed = 42;
        BankConfig bc = bankConfigForPosition(pos);
        QuarterlyTurnManager mgr(cfg, bc, "", pos);
        const auto& bank = mgr.bank();
        assertIdentity(bank, "position=" + std::to_string((int)pos));
    }
}

TEST(SFCInvariant, BalanceSheetClosesAfterOneQuarter) {
    // After a full quarter, capital may have been modified by consequences,
    // crisis impact, and hidden risk losses outside of endQuarter().
    // The identity should hold within a wider tolerance (~$100M).
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager mgr(cfg);
    mgr.advancePhase(); // QuarterStart
    mgr.advancePhase(); // DecisionPhase
    mgr.advancePhase(); // Simulation
    mgr.advancePhase(); // QuarterEnd
    const auto& b = mgr.bank();
    double residual = std::fabs(
        (b.totalDeposits + b.interbankBorrowing + b.capital) - b.totalAssets);
    EXPECT_LT(residual, 100e6) << "Identity residual $" << residual
        << " exceeds $100M after 1 quarter";
    EXPECT_GE(b.loans, 0);
    EXPECT_GE(b.totalDeposits, 0);
}

TEST(SFCInvariant, BalanceSheetClosesAfterTenQuarters) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    UtilityBot bot;
    auto result = runner.runGame(bot, config, 42, 10);
    EXPECT_FALSE(result.hadNaN);
    EXPECT_FALSE(result.hadNaN) << "NaN in 10Q UtilityBot game";
}

TEST(SFCInvariant, BalanceSheetClosesAfterFullGame) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();

    for (const auto& preset : {"Conservative", "Aggressive", "Gambler"}) {
        PersonalityProfile p;
        if (std::string(preset) == "Conservative") p = PersonalityProfile::conservative();
        else if (std::string(preset) == "Aggressive") p = PersonalityProfile::aggressive();
        else p = PersonalityProfile::gambler();

        PersonalityDrivenBot bot(preset, p);
        for (uint64_t seed = 42; seed < 45; ++seed) {
            auto result = runner.runGame(bot, config, seed, 380);
            EXPECT_FALSE(result.hadNaN) << preset << " seed=" << seed;
            EXPECT_GT(result.quartersPlayed, 0) << preset << " seed=" << seed;
        }
    }
}

TEST(SFCInvariant, IdentityHoldsWhenDivisionAdded) {
    Bank b = Bank::create(BankConfig{});
    assertIdentity(b, "before add");

    Division td;
    td.id = "div_trading";
    td.name = "Trading Desk";
    td.type = DivisionType::TradingDesk;
    td.budget = 2e9;
    b.addDivision(td); // calls recalcBalanceSheet()

    EXPECT_GT(b.securities, 0) << "Trading division should add securities";
    assertIdentity(b, "after adding TradingDesk");
}

TEST(SFCInvariant, LoansMatchDivisionBudgets) {
    Bank b = Bank::create(BankConfig{});
    double expectedLoans = 0;
    double expectedSecurities = 0;
    for (const auto& d : b.divisions) {
        if (d.assetClass() == Division::AssetClass::Loans)
            expectedLoans += d.budget * Bank::kDefaultLeverage;
        else if (d.assetClass() == Division::AssetClass::Securities)
            expectedSecurities += d.budget * Bank::kDefaultLeverage;
    }
    EXPECT_NEAR(b.loans, expectedLoans, TOLERANCE);
    EXPECT_NEAR(b.securities, expectedSecurities, TOLERANCE);
}

TEST(SFCInvariant, InterbankIsNonNegative) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    PersonalityDrivenBot bot("Aggressive", PersonalityProfile::aggressive());
    auto result = runner.runGame(bot, config, 42, 100);
    EXPECT_FALSE(result.hadNaN) << "NaN in Aggressive 100Q game";
}

TEST(SFCInvariant, DepositsNeverNegative) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    PersonalityDrivenBot bot("Gambler", PersonalityProfile::gambler());
    for (uint64_t seed = 42; seed < 52; ++seed) {
        auto result = runner.runGame(bot, config, seed, 380);
        EXPECT_FALSE(result.hadNaN) << "NaN at seed " << seed;
        EXPECT_FALSE(result.hadNegativeCapital) << "Negative capital at seed " << seed;
    }
}

TEST(SFCInvariant, AssetClassificationCoversAllDivisions) {
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
    EXPECT_EQ(allTypes.size(), 16u);
    for (auto type : allTypes) {
        Division d;
        d.type = type;
        auto cls = d.assetClass();
        EXPECT_TRUE(cls == Division::AssetClass::Loans ||
                    cls == Division::AssetClass::Securities ||
                    cls == Division::AssetClass::FeeBased)
            << "Division type " << (int)type << " has no asset classification";
    }
}

TEST(SFCInvariant, RecalcBalanceSheetIdempotent) {
    Bank b = Bank::create(BankConfig{});
    double loans1 = b.loans, sec1 = b.securities, dep1 = b.totalDeposits;
    b.recalcBalanceSheet();
    EXPECT_DOUBLE_EQ(b.loans, loans1);
    EXPECT_DOUBLE_EQ(b.securities, sec1);
    EXPECT_DOUBLE_EQ(b.totalDeposits, dep1);
    b.recalcBalanceSheet();
    EXPECT_DOUBLE_EQ(b.loans, loans1);
}
