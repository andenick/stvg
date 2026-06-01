#include <gtest/gtest.h>
#include <stvg/simulation/AICEOEngine.h>

using namespace stvg::simulation;

// ── Player Countermeasure Tests (non-overlapping with test_endgame.cpp) ──

TEST(AICEOCountermeasures, CostToMatchAIScalesWithEfficiency) {
    AICEOEngine engine;
    // Initial efficiency = 1.0
    EXPECT_DOUBLE_EQ(engine.costToMatchAI(), 1.0 * 10e9);

    // Trigger emergence and grow efficiency
    engine.recordAIInvestment(600);
    engine.simulateYear(2025); // Emerge + first efficiency gain
    // efficiency = 1.0 + 0.2 = 1.2
    EXPECT_NEAR(engine.costToMatchAI(), 1.2 * 10e9, 1e6);

    engine.simulateYear(2026); // efficiency = 1.4
    EXPECT_NEAR(engine.costToMatchAI(), 1.4 * 10e9, 1e6);
}

TEST(AICEOCountermeasures, CostToRegulateAIFormula) {
    AICEOEngine engine;
    // Initial efficiency = 1.0: cost = 5.0 + 1.0 * 2.0 = 7.0
    EXPECT_DOUBLE_EQ(engine.costToRegulateAI(), 7.0);

    // After emergence + 5 years: efficiency ~2.0
    engine.recordAIInvestment(600);
    for (int y = 2025; y <= 2029; ++y) engine.simulateYear(y);
    // efficiency = 1.0 + 5*0.2 = 2.0
    double expected = 5.0 + engine.efficiency() * 2.0;
    EXPECT_DOUBLE_EQ(engine.costToRegulateAI(), expected);
}

TEST(AICEOCountermeasures, ApplyPlayerInvestmentUpdatesCapability) {
    AICEOEngine engine;
    EXPECT_DOUBLE_EQ(engine.state().playerAICapability, 0.0);

    engine.applyPlayerInvestment(10.0);
    // Global investment increases by 10, capability by 10*0.1 = 1.0
    EXPECT_DOUBLE_EQ(engine.state().aiInvestmentGlobal, 10.0);
    EXPECT_DOUBLE_EQ(engine.state().playerAICapability, 1.0);

    engine.applyPlayerInvestment(5.0);
    EXPECT_DOUBLE_EQ(engine.state().aiInvestmentGlobal, 15.0);
    EXPECT_DOUBLE_EQ(engine.state().playerAICapability, 1.5);
}

TEST(AICEOCountermeasures, ApplyRegulationCapsAt08) {
    AICEOEngine engine;
    engine.recordAIInvestment(600);
    engine.simulateYear(2025); // Emerge, efficiency = 1.2

    // Apply regulation
    engine.applyRegulation(0.5);
    EXPECT_DOUBLE_EQ(engine.state().regulatoryDrag, 0.5);

    // Apply again — should cap at 0.8
    engine.applyRegulation(0.5);
    EXPECT_DOUBLE_EQ(engine.state().regulatoryDrag, 0.8);

    // Verify efficiency growth is reduced by drag
    double effBefore = engine.efficiency();
    engine.simulateYear(2026);
    double effAfter = engine.efficiency();
    // Gain = 0.2 * (1 - 0.8) = 0.04
    EXPECT_NEAR(effAfter - effBefore, 0.04, 1e-10);
}

TEST(AICEOCountermeasures, PartnershipActivatesAndGeneratesRevenue) {
    AICEOEngine engine;
    engine.recordAIInvestment(600);
    engine.simulateYear(2025); // Emerge

    EXPECT_FALSE(engine.state().partnershipActive);
    EXPECT_DOUBLE_EQ(engine.state().partnershipRevenue, 0.0);

    engine.applyPartnership();
    EXPECT_TRUE(engine.state().partnershipActive);

    // Revenue set on next simulateYear
    engine.simulateYear(2026);
    // efficiency after 2026 = 1.0 + 2*0.2 = 1.4
    // revenue = 1.4 * 0.5e9 = 0.7e9
    EXPECT_NEAR(engine.state().partnershipRevenue, engine.efficiency() * 0.5e9, 1e3);
}
