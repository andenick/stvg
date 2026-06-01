#include <gtest/gtest.h>
#include "stvg/simulation/MarketSimulator.h"
#include "stvg/simulation/EconomicEngine.h"
#include "stvg/math/RandomEngine.h"

using namespace stvg;
using namespace stvg::simulation;

// ════════════════════════════════════════════════════════════════════
// Era-Gated Market Tests — markets must respect historical existence
// ════════════════════════════════════════════════════════════════════

class MarketEraTest : public ::testing::Test {
protected:
    math::RandomEngine rng_{42};
    EconomicEngine econ_{rng_};
    MarketSimulator sim_{rng_, econ_};
};

TEST_F(MarketEraTest, NoBTCIn1945) {
    sim_.init(1945);
    EXPECT_FALSE(sim_.hasMarket("BTC")) << "Bitcoin should not exist in 1945";
}

TEST_F(MarketEraTest, NoEURUSDIn1945) {
    sim_.init(1945);
    EXPECT_FALSE(sim_.hasMarket("EURUSD"))
        << "EUR/USD should not exist before Bretton Woods collapse (1971)";
}

TEST_F(MarketEraTest, SP500AlwaysPresent) {
    sim_.init(1945);
    EXPECT_TRUE(sim_.hasMarket("SP500"));
    EXPECT_TRUE(sim_.hasMarket("UST10Y"));
    EXPECT_TRUE(sim_.hasMarket("GOLD"));
    EXPECT_TRUE(sim_.hasMarket("SPXOPT"));
}

TEST_F(MarketEraTest, FourMarketsIn1945) {
    sim_.init(1945);
    EXPECT_EQ(sim_.marketCount(), 4)
        << "1945 should have SP500, UST10Y, GOLD, VIX (no forex, no crypto)";
}

TEST_F(MarketEraTest, FiveMarketsIn1975) {
    sim_.init(1975);
    EXPECT_EQ(sim_.marketCount(), 5)
        << "1975 should add EUR/USD (post-Bretton Woods) but no Bitcoin yet";
    EXPECT_TRUE(sim_.hasMarket("EURUSD"));
    EXPECT_FALSE(sim_.hasMarket("BTC"));
}

TEST_F(MarketEraTest, SixMarketsIn2024) {
    sim_.init(2024);
    EXPECT_EQ(sim_.marketCount(), 6)
        << "2024 should have all 6 markets including Bitcoin";
    EXPECT_TRUE(sim_.hasMarket("BTC"));
    EXPECT_TRUE(sim_.hasMarket("EURUSD"));
}

TEST_F(MarketEraTest, SP500StartsAt15In1945) {
    sim_.init(1945);
    auto markets = sim_.snapshot();
    for (const auto& m : markets) {
        if (m.id == "SP500") {
            EXPECT_NEAR(m.currentPrice, 15.0, 0.01)
                << "S&P 500 should start at ~15 in 1945";
        }
    }
}

TEST_F(MarketEraTest, GoldFixedAt35PreBrettonWoods) {
    sim_.init(1945);
    auto markets = sim_.snapshot();
    for (const auto& m : markets) {
        if (m.id == "GOLD") {
            EXPECT_NEAR(m.currentPrice, 35.0, 0.01)
                << "Gold should be $35 (Bretton Woods peg) in 1945";
        }
    }
}

TEST_F(MarketEraTest, DynamicMarketAddition) {
    sim_.init(1945);
    EXPECT_FALSE(sim_.hasMarket("EURUSD"));

    // Dynamically add EUR/USD as if Bretton Woods just collapsed
    sim_.addMarketDynamic("EURUSD", "EUR/USD", MarketType::Currency,
        "EUR", "USD", 1.08, 0.00, 0.08, 3);
    EXPECT_TRUE(sim_.hasMarket("EURUSD"));
    EXPECT_EQ(sim_.marketCount(), 5);

    // Adding again should not duplicate
    sim_.addMarketDynamic("EURUSD", "EUR/USD", MarketType::Currency,
        "EUR", "USD", 1.08, 0.00, 0.08, 3);
    EXPECT_EQ(sim_.marketCount(), 5) << "Should not add duplicate market";
}
