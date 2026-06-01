#include <gtest/gtest.h>
#include <stvg/simulation/RiskReturnProfile.h>
#include <stvg/math/RandomEngine.h>
#include <cmath>

using namespace stvg::simulation;
using namespace stvg::math;

// ── Formula Tests ───────────────────────────────────────────────

TEST(RiskReturnProfile, ExpectedValueFormula) {
    RiskReturnProfile r;
    // Defaults: windfallProb=0.05, returnMax=0.20, windfallMult=3.0
    //           successProb=0.6, returnMin=-0.10, failureLoss=0.15
    // windfallEV  = 0.05 * 0.20 * 3.0 = 0.03
    // successEV   = 0.6 * (1-0.05) * (-0.10+0.20)/2 = 0.6*0.95*0.05 = 0.0285
    // failureEV   = (1-0.6) * (-0.15) = -0.06
    // total = 0.03 + 0.0285 - 0.06 = -0.0015
    EXPECT_NEAR(r.expectedValue(), -0.0015, 1e-10);
}

TEST(RiskReturnProfile, RiskScoreFormula) {
    RiskReturnProfile r;
    // range = returnMax - returnMin + failureLoss = 0.20 - (-0.10) + 0.15 = 0.45
    // riskScore = 0.45 * (1-0.6) * 5/10 = 0.45 * 0.4 * 0.5 = 0.09
    EXPECT_NEAR(r.riskScore(), 0.09, 1e-10);
}

TEST(RiskReturnProfile, GenerateOutcomeDistribution) {
    RandomEngine rng(42);
    RiskReturnProfile r;

    int windfallCount = 0, successCount = 0, failureCount = 0;
    const int N = 10000;

    for (int i = 0; i < N; ++i) {
        double outcome = r.generateOutcome(rng);
        // Windfall: returnMax * windfallMult * noise = 0.20*3.0*(0.8-1.2) = [0.48, 0.72]
        if (outcome > 0.20) {
            windfallCount++;
        } else if (outcome >= -0.15) {
            // Success range: returnMin to returnMax = [-0.10, 0.20]
            // But could overlap with small failures. Use positive as success proxy.
            successCount++;
        } else {
            failureCount++;
        }
    }

    double windfallPct = (double)windfallCount / N;
    // Windfall ~5% (tolerance 2-9%)
    EXPECT_GT(windfallPct, 0.02);
    EXPECT_LT(windfallPct, 0.09);

    // Failure outcomes exist
    EXPECT_GT(failureCount, 0);

    // Success is the majority of remaining outcomes
    EXPECT_GT(successCount, failureCount);
}

TEST(RiskReturnProfile, OutcomesNeverNaN) {
    RandomEngine rng(123);
    RiskReturnProfile r;

    for (int i = 0; i < 1000; ++i) {
        double outcome = r.generateOutcome(rng);
        EXPECT_FALSE(std::isnan(outcome)) << "NaN at iteration " << i;
        EXPECT_FALSE(std::isinf(outcome)) << "Inf at iteration " << i;
    }
}

TEST(RiskReturnProfile, ZeroSuccessAllFail) {
    RandomEngine rng(42);
    RiskReturnProfile r;
    r.successProbability = 0.0;
    r.windfallProbability = 0.0;

    for (int i = 0; i < 100; ++i) {
        double outcome = r.generateOutcome(rng);
        EXPECT_LT(outcome, 0.0) << "Expected negative outcome at iteration " << i;
    }
}

TEST(RiskReturnProfile, JsonRoundtrip) {
    RiskReturnProfile original;
    original.riskLevel = 7;
    original.returnMin = -0.05;
    original.returnMax = 0.30;
    original.successProbability = 0.7;
    original.durationQuarters = 3;
    original.failureLoss = 0.20;
    original.windfallProbability = 0.08;
    original.windfallMultiplier = 4.0;

    nlohmann::json j;
    to_json(j, original);

    RiskReturnProfile loaded;
    from_json(j, loaded);

    EXPECT_EQ(loaded.riskLevel, original.riskLevel);
    EXPECT_NEAR(loaded.returnMin, original.returnMin, 1e-10);
    EXPECT_NEAR(loaded.returnMax, original.returnMax, 1e-10);
    EXPECT_NEAR(loaded.successProbability, original.successProbability, 1e-10);
    EXPECT_EQ(loaded.durationQuarters, original.durationQuarters);
    EXPECT_NEAR(loaded.failureLoss, original.failureLoss, 1e-10);
    EXPECT_NEAR(loaded.windfallProbability, original.windfallProbability, 1e-10);
    EXPECT_NEAR(loaded.windfallMultiplier, original.windfallMultiplier, 1e-10);
}
