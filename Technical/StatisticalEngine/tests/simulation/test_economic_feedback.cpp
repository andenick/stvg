#include <gtest/gtest.h>
#include "stvg/simulation/EconomicEngine.h"
#include "stvg/math/RandomEngine.h"

using namespace stvg;
using namespace stvg::simulation;

TEST(EconomicFeedback, TaylorRuleRaisesFedOnHighInflation) {
    // The Taylor rule raises the policy rate when inflation is above target. We
    // assert this COMPARATIVELY (high-inflation vs at-target, identical seed) on
    // the MEAN rate over the window rather than the single noisy endpoint. The
    // single-endpoint form is fragile: the Fed-Funds OU process has day-scale
    // noise of similar magnitude to the 2-year drift, and inflation's own lagged
    // mean-reversion pulls the late-window Taylor target back down, so the final
    // rate can land marginally below the start on some RNG sequences even though
    // the rate is provably higher under high inflation throughout. The
    // comparative-mean form cancels the shared noise (same seed) and isolates the
    // Taylor response. Verified hi>lo on 20/20 seeds with the portable RNG.
    auto meanRate = [](double inflation) {
        math::RandomEngine rng(42);
        EconomicEngine engine(rng);
        EconomicIndicators init;
        init.cpiInflation = inflation;
        init.fedFundsRate = 0.02;
        init.gdpGrowth = 0.03;
        init.unemployment = 0.05;
        init.vix = 15;
        init.treasuryYield10Y = 0.04;
        init.creditSpread = 0.02;
        engine.init(init);
        double sum = 0.0;
        for (int i = 0; i < 504; ++i) { engine.tick(); sum += engine.state().fedFundsRate; }
        return sum / 504.0;
    };

    double meanHigh = meanRate(0.06);    // 6% inflation, well above 2% target
    double meanTarget = meanRate(0.02);  // at target
    EXPECT_GT(meanHigh, meanTarget)
        << "Fed should run a higher rate when inflation is above the 2% target "
        << "(meanHigh=" << meanHigh << " meanTarget=" << meanTarget << ")";
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
