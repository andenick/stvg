#include <gtest/gtest.h>
#include <stvg/simulation/ClimateEngine.h>
#include <stvg/core/BankState.h>
#include <stvg/math/RandomEngine.h>
#include <cmath>

using namespace stvg;
using namespace stvg::simulation;
using namespace stvg::math;

// ── Mitigation Action Tests (non-overlapping with test_endgame.cpp) ──

TEST(ClimateMitigation, GreenLendingReducesFossilExposure) {
    RandomEngine rng(42);
    ClimateEngine engine(rng);

    // Initial fossil exposure = 0.05 * (1 - 0) = 0.05
    EXPECT_DOUBLE_EQ(engine.effectiveFossilExposure(), 0.05);

    engine.applyGreenLendingShift(0.3);
    // exposure = 0.05 * (1 - 0.3) = 0.035
    EXPECT_NEAR(engine.effectiveFossilExposure(), 0.035, 1e-10);

    // Apply more — cap at 1.0 reduction
    engine.applyGreenLendingShift(0.8); // total = 1.1, capped at 1.0
    EXPECT_DOUBLE_EQ(engine.effectiveFossilExposure(), 0.0);
}

TEST(ClimateMitigation, ESGBondRevenueFormula) {
    RandomEngine rng(42);
    ClimateEngine engine(rng);

    // Simulate to 2030: carbonPrice = min(500, 10*7.5) = 75
    for (int y = 2020; y <= 2030; ++y) engine.simulateYear(y);
    EXPECT_NEAR(engine.state().carbonPrice, 75.0, 1e-10);

    double investment = 1e9;
    engine.applyESGBondProgram(investment);
    // revenue = 1e9 * 0.05 * (1 + 75/200) = 1e9 * 0.05 * 1.375 = 68.75e6
    double expected = investment * 0.05 * (1.0 + 75.0 / 200.0);
    EXPECT_NEAR(engine.greenBondRevenue(), expected, 1.0);
}

TEST(ClimateMitigation, DivestmentReducesExposure) {
    RandomEngine rng(42);
    ClimateEngine engine(rng);

    engine.applyDivestment(0.5);
    // exposure = 0.05 * (1 - 0.5) = 0.025
    EXPECT_NEAR(engine.effectiveFossilExposure(), 0.025, 1e-10);
}

// ── Formula Verification Tests ──────────────────────────────────

TEST(ClimateFormulas, CarbonPriceCapsAt500) {
    RandomEngine rng(42);
    ClimateEngine engine(rng);

    // At year 2020 + 500/7.5 = 2086.67, so by 2087 it should cap
    for (int y = 2020; y <= 2100; ++y) engine.simulateYear(y);

    EXPECT_DOUBLE_EQ(engine.state().carbonPrice, 500.0);
}

TEST(ClimateFormulas, PhysicalRiskMultiplierAtKnownTemp) {
    RandomEngine rng(42);
    ClimateEngine engine(rng);

    // Simulate only 3 years from 2020 (no tipping possible, temp < 1.5)
    // temp = 1.1 + 3*0.03 = 1.19
    // physicalRisk = (1.19/1.5)^2 = 0.7933^2 = 0.6293
    engine.simulateYear(2020);
    engine.simulateYear(2021);
    engine.simulateYear(2022);

    double expectedTemp = 1.1 + 2 * 0.03; // yearsFrom2020=2 for year 2022
    double expectedRisk = std::pow(expectedTemp / 1.5, 2.0);
    EXPECT_NEAR(engine.state().physicalRiskMult, expectedRisk, 1e-6);
}

TEST(ClimateFormulas, StrandedAssetLossWithCommercialLending) {
    RandomEngine rng(42);
    ClimateEngine engine(rng);

    // Simulate to get a non-zero carbon price
    for (int y = 2020; y <= 2025; ++y) engine.simulateYear(y);
    // carbonPrice = min(500, 5*7.5) = 37.5

    // Create a bank with a CommercialLending division
    Bank bank = Bank::create({});
    // Find or add CommercialLending
    bool hasCommLending = false;
    for (auto& div : bank.divisions) {
        if (div.type == DivisionType::CommercialLending) {
            div.revenue = 1e8; // $100M revenue
            hasCommLending = true;
            break;
        }
    }

    if (hasCommLending) {
        double loss = engine.strandedAssetLoss(bank);
        // loss = revenue * effectiveFossilExposure * carbonPrice / 100
        // = 1e8 * 0.05 * 37.5 / 100 = 1e8 * 0.01875 = 1.875e6
        EXPECT_GT(loss, 0.0);

        // After divestment, loss should decrease
        engine.applyDivestment(0.5);
        double lossAfter = engine.strandedAssetLoss(bank);
        EXPECT_LT(lossAfter, loss);
    }
}
