#include <gtest/gtest.h>
#include <stvg/simulation/AICEOEngine.h>
#include <stvg/simulation/ClimateEngine.h>
#include <stvg/math/RandomEngine.h>
#include <stvg/core/BankState.h>

using namespace stvg;
using namespace stvg::simulation;

// ── AI CEO Tests ────────────────────────────────────────────────────

TEST(AICEOEngine, NotEmergedInitially) {
    AICEOEngine engine;
    EXPECT_FALSE(engine.hasEmerged());
    EXPECT_EQ(engine.singularityProgress(), 0);
}

TEST(AICEOEngine, InvestmentTracking) {
    AICEOEngine engine;
    engine.recordAIInvestment(100);
    engine.recordAIInvestment(200);
    EXPECT_DOUBLE_EQ(engine.state().aiInvestmentGlobal, 300);
}

TEST(AICEOEngine, EmergesAtThreshold) {
    AICEOEngine engine;
    engine.recordAIInvestment(500); // At threshold
    engine.simulateYear(2035);
    EXPECT_TRUE(engine.hasEmerged());
    EXPECT_EQ(engine.state().emergenceYear, 2035);
}

TEST(AICEOEngine, EfficiencyGrowsAfterEmergence) {
    AICEOEngine engine;
    engine.recordAIInvestment(600);
    engine.simulateYear(2035);
    EXPECT_TRUE(engine.hasEmerged());
    double eff1 = engine.efficiency();

    engine.simulateYear(2036);
    EXPECT_GT(engine.efficiency(), eff1);
}

TEST(AICEOEngine, MarketCaptureGrows) {
    AICEOEngine engine;
    engine.recordAIInvestment(600);
    for (int y = 2035; y <= 2040; ++y) {
        engine.simulateYear(y);
    }
    EXPECT_GT(engine.marketCapture(), 0);
}

TEST(AICEOEngine, SingularityCondition) {
    AICEOEngine engine;
    engine.recordAIInvestment(600);
    // Run 30 years to reach singularity
    for (int y = 2035; y <= 2065; ++y) {
        engine.simulateYear(y);
    }
    // After 30 years: efficiency = 1.0 + 30*0.2 = 7.0 (>5.0)
    // marketCapture approaching limit
    EXPECT_GT(engine.efficiency(), 5.0);
    EXPECT_EQ(engine.singularityProgress(), 100);
    EXPECT_TRUE(engine.singularityReached());
}

TEST(AICEOEngine, NoEmergenceBefore2015) {
    AICEOEngine engine;
    engine.recordAIInvestment(1000);
    engine.simulateYear(2010);
    EXPECT_FALSE(engine.hasEmerged());
}

TEST(AICEOEngine, SerializesToJson) {
    AICEOEngine engine;
    engine.recordAIInvestment(100);
    nlohmann::json j = engine.state();
    EXPECT_TRUE(j.contains("aiInvestmentGlobal"));
    EXPECT_TRUE(j.contains("emerged"));
    EXPECT_EQ(j["emerged"], false);
}

// ── Climate Engine Tests ────────────────────────────────────────────

TEST(ClimateEngine, NoEffectBefore2020) {
    math::RandomEngine rng(42);
    ClimateEngine engine(rng);
    engine.simulateYear(2015);
    EXPECT_DOUBLE_EQ(engine.state().globalTemp, 1.1);
    EXPECT_DOUBLE_EQ(engine.state().carbonPrice, 0);
}

TEST(ClimateEngine, TemperatureRises) {
    math::RandomEngine rng(42);
    ClimateEngine engine(rng);
    engine.simulateYear(2025);
    EXPECT_GT(engine.state().globalTemp, 1.1);
}

TEST(ClimateEngine, CarbonPriceRises) {
    math::RandomEngine rng(42);
    ClimateEngine engine(rng);
    engine.simulateYear(2030);
    EXPECT_GT(engine.state().carbonPrice, 0);
}

TEST(ClimateEngine, PhysicalDamageScalesWithSize) {
    math::RandomEngine rng(42);
    ClimateEngine engine(rng);
    engine.simulateYear(2035);

    Bank small = Bank::create({});
    small.totalAssets = 10e9;
    Bank large = Bank::create({});
    large.totalAssets = 100e9;

    EXPECT_GT(engine.physicalDamage(large), engine.physicalDamage(small));
}

TEST(ClimateEngine, TippingPointsPossible) {
    math::RandomEngine rng(42);
    ClimateEngine engine(rng);

    // Run many years — tipping points should be possible
    for (int y = 2020; y <= 2060; ++y) {
        engine.simulateYear(y);
    }
    // Temperature should be high enough for tipping points
    EXPECT_GT(engine.state().globalTemp, 1.5);
}

TEST(ClimateEngine, SerializesToJson) {
    math::RandomEngine rng(42);
    ClimateEngine engine(rng);
    engine.simulateYear(2030);
    nlohmann::json j = engine.state();
    EXPECT_TRUE(j.contains("globalTemp"));
    EXPECT_TRUE(j.contains("carbonPrice"));
    EXPECT_TRUE(j.contains("tippingPointsReached"));
}
