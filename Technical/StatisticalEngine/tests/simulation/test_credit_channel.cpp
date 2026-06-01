#include <gtest/gtest.h>
#include <stvg/simulation/EconomicEngine.h>
#include <stvg/simulation/CompetitorEngine.h>
#include <stvg/math/RandomEngine.h>
#include <stvg/core/GameConfig.h>
#include <stvg/autoplay/GameRunner.h>
#include <stvg/autoplay/EraAwareBots.h>
#include <cmath>

using namespace stvg;
using namespace stvg::simulation;
using namespace stvg::autoplay;

TEST(CreditChannel, CreditImpulseStartsAtZero) {
    math::RandomEngine rng(42);
    EconomicEngine engine(rng);
    EXPECT_DOUBLE_EQ(engine.creditImpulse(), 0.0);
}

TEST(CreditChannel, SetCreditImpulseWorks) {
    math::RandomEngine rng(42);
    EconomicEngine engine(rng);
    engine.setCreditImpulse(0.03);
    EXPECT_DOUBLE_EQ(engine.creditImpulse(), 0.03);
}

TEST(CreditChannel, PositiveImpulseBoostsGDP) {
    EconomicIndicators init;
    init.gdpGrowth = 0.025;
    init.fedFundsRate = 0.02;
    init.cpiInflation = 0.02;
    init.unemployment = 0.05;
    init.vix = 15;
    init.treasuryYield10Y = 0.03;
    init.creditSpread = 0.015;

    // Control: no credit impulse
    math::RandomEngine rng1(42);
    EconomicEngine control(rng1);
    control.init(init);
    for (int i = 0; i < 126; ++i) control.tick();
    double controlGDP = control.state().gdpGrowth;

    // Boosted: +5% credit impulse
    math::RandomEngine rng2(42);
    EconomicEngine boosted(rng2);
    boosted.init(init);
    boosted.setCreditImpulse(0.05);
    for (int i = 0; i < 126; ++i) boosted.tick();
    double boostedGDP = boosted.state().gdpGrowth;

    EXPECT_GT(boostedGDP, controlGDP)
        << "Credit expansion should boost GDP (control=" << controlGDP
        << ", boosted=" << boostedGDP << ")";
}

TEST(CreditChannel, NegativeImpulseDepressesGDP) {
    EconomicIndicators init;
    init.gdpGrowth = 0.025;
    init.fedFundsRate = 0.02;
    init.cpiInflation = 0.02;
    init.unemployment = 0.05;
    init.vix = 15;
    init.treasuryYield10Y = 0.03;
    init.creditSpread = 0.015;

    math::RandomEngine rng1(42);
    EconomicEngine control(rng1);
    control.init(init);
    for (int i = 0; i < 126; ++i) control.tick();

    math::RandomEngine rng2(42);
    EconomicEngine contracted(rng2);
    contracted.init(init);
    contracted.setCreditImpulse(-0.05);
    for (int i = 0; i < 126; ++i) contracted.tick();

    EXPECT_LT(contracted.state().gdpGrowth, control.state().gdpGrowth)
        << "Credit contraction should depress GDP";
}

TEST(CreditChannel, CreditExpansionIsInflationary) {
    EconomicIndicators init;
    init.gdpGrowth = 0.025;
    init.fedFundsRate = 0.02;
    init.cpiInflation = 0.02;
    init.unemployment = 0.05;
    init.vix = 15;
    init.treasuryYield10Y = 0.03;
    init.creditSpread = 0.015;

    math::RandomEngine rng1(42);
    EconomicEngine control(rng1);
    control.init(init);
    for (int i = 0; i < 252; ++i) control.tick();

    math::RandomEngine rng2(42);
    EconomicEngine expanded(rng2);
    expanded.init(init);
    expanded.setCreditImpulse(0.05);
    for (int i = 0; i < 252; ++i) expanded.tick();

    EXPECT_GT(expanded.state().cpiInflation, control.state().cpiInflation)
        << "Credit expansion should be inflationary";
}

TEST(CreditChannel, CompetitorLoansInitialized) {
    math::RandomEngine rng(42);
    CompetitorEngine ce(rng);
    ce.init(1945);
    EXPECT_GT(ce.sumCompetitorLoans(), 0) << "System should have loans after init";
    for (const auto& c : ce.competitors()) {
        if (c.alive && 1945 >= c.foundedYear) {
            EXPECT_GT(c.totalLoans, 0) << c.name << " should have loans";
            EXPECT_NEAR(c.totalLoans, c.totalAssets * 0.70, c.totalAssets * 0.05)
                << c.name << " loan ratio should be ~70% of assets";
        }
    }
}

TEST(CreditChannel, CompetitorLoansTrackAssets) {
    math::RandomEngine rng(42);
    CompetitorEngine ce(rng);
    ce.init(1945);
    Bank dummyBank = Bank::create(BankConfig{});
    for (int year = 1945; year < 1955; ++year) {
        ce.simulateYear(year, MarketRegime::Normal, 20.0, dummyBank);
    }
    for (const auto& c : ce.competitors()) {
        if (c.alive && c.totalAssets > 0) {
            double ratio = c.totalLoans / c.totalAssets;
            EXPECT_NEAR(ratio, 0.70, 0.05)
                << c.name << " loan/asset ratio drifted to " << ratio;
        }
    }
}

TEST(CreditChannel, FeedbackLoopDoesNotDiverge) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    PersonalityDrivenBot bot("Aggressive", PersonalityProfile::aggressive());
    for (uint64_t seed = 42; seed < 52; ++seed) {
        auto result = runner.runGame(bot, config, seed, 380);
        EXPECT_FALSE(result.hadNaN) << "NaN at seed " << seed;
    }
}

TEST(CreditChannel, CreditImpulseExtremeValues) {
    EconomicIndicators init;
    init.gdpGrowth = 0.025;
    init.fedFundsRate = 0.02;
    init.cpiInflation = 0.02;
    init.unemployment = 0.05;
    init.vix = 15;
    init.treasuryYield10Y = 0.03;
    init.creditSpread = 0.015;

    for (double impulse : {-0.05, 0.05}) {
        math::RandomEngine rng(42);
        EconomicEngine engine(rng);
        engine.init(init);
        engine.setCreditImpulse(impulse);
        for (int i = 0; i < 252; ++i) engine.tick();
        EXPECT_FALSE(std::isnan(engine.state().gdpGrowth))
            << "GDP NaN at impulse=" << impulse;
        EXPECT_FALSE(std::isinf(engine.state().gdpGrowth))
            << "GDP Inf at impulse=" << impulse;
        EXPECT_FALSE(std::isnan(engine.state().cpiInflation))
            << "CPI NaN at impulse=" << impulse;
    }
}
