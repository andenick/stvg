#include <gtest/gtest.h>
#include "stvg/simulation/EconomicEngine.h"
#include "stvg/math/RandomEngine.h"

using namespace stvg;
using namespace stvg::simulation;

TEST(EconomicFeedback, TaylorRuleRaisesFedOnHighInflation) {
    math::RandomEngine rng(42);
    EconomicEngine engine(rng);
    EconomicIndicators init;
    init.cpiInflation = 0.06; // 6% inflation (well above 2% target)
    init.fedFundsRate = 0.02; // Low starting rate
    init.gdpGrowth = 0.03;
    init.unemployment = 0.05;
    init.vix = 15;
    init.treasuryYield10Y = 0.04;
    init.creditSpread = 0.02;
    engine.init(init);

    // Run 2 years (504 days) to allow mean-reversion toward Taylor target
    for (int i = 0; i < 504; ++i) engine.tick();

    // Fed should have risen from the 2% starting point (Taylor target is much higher)
    EXPECT_GT(engine.state().fedFundsRate, init.fedFundsRate)
        << "Fed should rise when inflation is well above 2% target";
}

TEST(EconomicFeedback, InflationDeclinesAfterRateHikeLag) {
    math::RandomEngine rng(42);
    EconomicEngine engine(rng);
    EconomicIndicators init;
    init.cpiInflation = 0.04;
    init.fedFundsRate = 0.08; // Already high
    init.gdpGrowth = 0.02;
    init.unemployment = 0.05;
    init.vix = 18;
    init.treasuryYield10Y = 0.05;
    init.creditSpread = 0.02;
    engine.init(init);

    // Run 8 quarters (504 days) to see lag effect
    for (int i = 0; i < 504; ++i) engine.tick();

    EXPECT_LT(engine.state().cpiInflation, init.cpiInflation)
        << "Inflation should decline after sustained high rates (lag effect)";
}

TEST(EconomicFeedback, VIXSpikeDepressesGDP) {
    math::RandomEngine rng(42);
    EconomicEngine engine(rng);
    EconomicIndicators init;
    init.cpiInflation = 0.02;
    init.fedFundsRate = 0.04;
    init.gdpGrowth = 0.03;
    init.unemployment = 0.05;
    init.vix = 60; // Severe crisis-level VIX
    init.treasuryYield10Y = 0.04;
    init.creditSpread = 0.02;
    engine.init(init);

    // Run 2 quarters to let VIX->GDP channel dominate OU noise
    for (int i = 0; i < 126; ++i) engine.tick();

    EXPECT_LT(engine.state().gdpGrowth, init.gdpGrowth)
        << "High VIX should depress GDP growth";
}

TEST(EconomicFeedback, NegativeGDPWidensSpreads) {
    math::RandomEngine rng(42);
    EconomicEngine engine(rng);
    EconomicIndicators init;
    init.cpiInflation = 0.01;
    init.fedFundsRate = 0.02;
    init.gdpGrowth = -0.02; // Recession
    init.unemployment = 0.08;
    init.vix = 30;
    init.treasuryYield10Y = 0.03;
    init.creditSpread = 0.02;
    engine.init(init);

    // Run 1 quarter
    for (int i = 0; i < 63; ++i) engine.tick();

    EXPECT_GT(engine.state().creditSpread, init.creditSpread)
        << "Credit spreads should widen during recession";
}
