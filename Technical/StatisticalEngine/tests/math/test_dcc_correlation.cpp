#include <gtest/gtest.h>
#include <stvg/math/CorrelationEngine.h>
#include <stvg/math/RandomEngine.h>
#include <cmath>

using namespace stvg::math;

TEST(DCCCorrelation, InitializesAtUnconditional) {
    CorrelationEngine engine;
    auto corr = engine.currentCorrelation();
    // Diagonal should be 1.0
    for (int i = 0; i < NUM_ASSETS; ++i) {
        EXPECT_DOUBLE_EQ(corr[i][i], 1.0);
    }
    // Off-diagonal should match unconditional defaults
    EXPECT_NEAR(corr[0][1], -0.20, 0.01);
}

TEST(DCCCorrelation, ShockIncreasesCorrelation) {
    CorrelationEngine engine;
    CorrMatrix baseline = engine.currentCorrelation();

    // Feed a large simultaneous shock (all assets crash together)
    AssetShocks bigShock = {-3.0, -3.0, -3.0, -3.0, -3.0, -3.0};
    engine.update(bigShock);

    auto after = engine.currentCorrelation();
    // After a simultaneous shock, correlations should increase
    // (off-diagonal moves toward 1)
    double avgCorrBefore = 0, avgCorrAfter = 0;
    int count = 0;
    for (int i = 0; i < NUM_ASSETS; ++i) {
        for (int j = i + 1; j < NUM_ASSETS; ++j) {
            avgCorrBefore += baseline[i][j];
            avgCorrAfter += after[i][j];
            count++;
        }
    }
    avgCorrBefore /= count;
    avgCorrAfter /= count;
    EXPECT_GT(avgCorrAfter, avgCorrBefore);
}

TEST(DCCCorrelation, PersistenceKeepsCorrelationsElevated) {
    CorrelationEngine engine;
    engine.init(engine.unconditionalCorrelation(), 0.10, 0.85); // High persistence

    // Large shock
    AssetShocks shock = {-3.0, -3.0, -3.0, -3.0, -3.0, -3.0};
    engine.update(shock);

    // Small noise for several ticks
    for (int t = 0; t < 5; ++t) {
        AssetShocks small = {0.1, -0.1, 0.05, -0.05, 0.1, -0.1};
        engine.update(small);
    }
    auto afterDecay = engine.pairwiseCorrelation(0, 4);

    // Should have decayed somewhat but still elevated vs unconditional
    double unconditional = engine.unconditionalCorrelation()[0][4];
    EXPECT_GT(afterDecay, unconditional);
}

TEST(DCCCorrelation, HighAlphaCausesRapidChange) {
    CorrelationEngine engineSlow, engineFast;
    auto uncond = engineSlow.unconditionalCorrelation();
    engineSlow.init(uncond, 0.02, 0.95);  // Slow
    engineFast.init(uncond, 0.15, 0.80);  // Fast

    AssetShocks shock = {-2.5, -2.5, -2.5, -2.5, -2.5, -2.5};
    engineSlow.update(shock);
    engineFast.update(shock);

    // Fast engine should show larger correlation change
    double slowChange = std::abs(engineSlow.pairwiseCorrelation(0, 1) - uncond[0][1]);
    double fastChange = std::abs(engineFast.pairwiseCorrelation(0, 1) - uncond[0][1]);
    EXPECT_GT(fastChange, slowChange);
}

TEST(DCCCorrelation, EraTransitionChangesBaseline) {
    CorrelationEngine engine;

    // Simulate era change: set new unconditional with higher base correlations
    CorrMatrix highCorr{};
    for (int i = 0; i < NUM_ASSETS; ++i) {
        for (int j = 0; j < NUM_ASSETS; ++j) {
            highCorr[i][j] = (i == j) ? 1.0 : 0.5;
        }
    }
    engine.setEraParameters(highCorr, 0.07, 0.89);
    EXPECT_DOUBLE_EQ(engine.alpha(), 0.07);

    // After many updates with small noise, should converge to new unconditional
    RandomEngine rng(42);
    for (int t = 0; t < 200; ++t) {
        AssetShocks noise;
        for (int i = 0; i < NUM_ASSETS; ++i) noise[i] = rng.normalSample() * 0.3;
        engine.update(noise);
    }

    // Should be close to the new unconditional baseline
    double offDiag = engine.pairwiseCorrelation(0, 2);
    EXPECT_NEAR(offDiag, 0.5, 0.15); // Within 0.15 of target
}

TEST(DCCCorrelation, CholeskyStaysValid) {
    CorrelationEngine engine;
    RandomEngine rng(99);

    // Run many updates and verify we can always generate valid shocks
    for (int t = 0; t < 100; ++t) {
        AssetShocks noise;
        for (int i = 0; i < NUM_ASSETS; ++i) noise[i] = rng.normalSample();
        engine.update(noise);

        auto shocks = engine.generateCorrelatedShocks(rng);
        for (int i = 0; i < NUM_ASSETS; ++i) {
            EXPECT_TRUE(std::isfinite(shocks[i])) << "NaN at t=" << t << " asset=" << i;
        }
    }
}

TEST(DCCCorrelation, DiagonalAlwaysOne) {
    CorrelationEngine engine;
    RandomEngine rng(123);

    for (int t = 0; t < 50; ++t) {
        AssetShocks noise;
        for (int i = 0; i < NUM_ASSETS; ++i) noise[i] = rng.normalSample() * 2.0;
        engine.update(noise);

        auto corr = engine.currentCorrelation();
        for (int i = 0; i < NUM_ASSETS; ++i) {
            EXPECT_DOUBLE_EQ(corr[i][i], 1.0);
        }
    }
}

TEST(DCCCorrelation, CorrelationsBoundedMinusOneToOne) {
    CorrelationEngine engine;
    engine.init(engine.unconditionalCorrelation(), 0.15, 0.80);
    RandomEngine rng(456);

    // Feed extreme shocks
    for (int t = 0; t < 50; ++t) {
        AssetShocks extreme;
        for (int i = 0; i < NUM_ASSETS; ++i) extreme[i] = rng.normalSample() * 5.0;
        engine.update(extreme);

        auto corr = engine.currentCorrelation();
        for (int i = 0; i < NUM_ASSETS; ++i) {
            for (int j = 0; j < NUM_ASSETS; ++j) {
                EXPECT_GE(corr[i][j], -1.0);
                EXPECT_LE(corr[i][j], 1.0);
            }
        }
    }
}
