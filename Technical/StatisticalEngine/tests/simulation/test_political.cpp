#include <gtest/gtest.h>
#include "stvg/simulation/PoliticalEngine.h"
#include "stvg/math/RandomEngine.h"

using namespace stvg;
using namespace stvg::simulation;

class PoliticalTest : public ::testing::Test {
protected:
    math::RandomEngine rng_{42};
    PoliticalEngine engine_{rng_};
};

TEST_F(PoliticalTest, InitialStateIsNeutral) {
    EXPECT_DOUBLE_EQ(engine_.currentCapital(), 0.0);
    EXPECT_DOUBLE_EQ(engine_.publicOpinion(), 0.0);
    EXPECT_DOUBLE_EQ(engine_.effectiveness(), 1.0);
}

TEST_F(PoliticalTest, EarnAndSpendCapital) {
    engine_.earnCapital(30.0, "PAC contribution");
    EXPECT_NEAR(engine_.currentCapital(), 30.0, 0.01);

    engine_.spendCapital(10.0);
    EXPECT_NEAR(engine_.currentCapital(), 20.0, 0.01);
}

TEST_F(PoliticalTest, CapitalCappedAt100) {
    engine_.earnCapital(200.0);
    EXPECT_DOUBLE_EQ(engine_.currentCapital(), 100.0);
}

TEST_F(PoliticalTest, CapitalDoesntGoBelowZero) {
    engine_.spendCapital(50.0);
    EXPECT_DOUBLE_EQ(engine_.currentCapital(), 0.0);
}

TEST_F(PoliticalTest, EraModifierAffectsEffectiveness) {
    engine_.setEraModifier("big_bang");
    EXPECT_NEAR(engine_.effectiveness(), 1.5, 0.01)
        << "Peak deregulation era should have 1.5x lobbying effectiveness";

    engine_.setEraModifier("reform");
    EXPECT_NEAR(engine_.effectiveness(), 0.5, 0.01)
        << "Post-crisis reform era should have 0.5x lobbying effectiveness";
}

TEST_F(PoliticalTest, LobbyingCostsCapital) {
    engine_.earnCapital(50.0);
    double before = engine_.currentCapital();
    engine_.lobbyRegulation("dodd_frank", 1e6);
    EXPECT_LT(engine_.currentCapital(), before)
        << "Lobbying should consume political capital";
}

TEST_F(PoliticalTest, LobbyingSuccessWithHighCapital) {
    // With high capital + favorable era, success rate should be reasonable
    engine_.earnCapital(80.0);
    engine_.setEraModifier("big_bang"); // 1.5x effectiveness

    int successes = 0;
    int trials = 100;
    for (int i = 0; i < trials; i++) {
        math::RandomEngine trialRng(i);
        PoliticalEngine trial(trialRng);
        trial.earnCapital(80.0);
        trial.setEraModifier("big_bang");
        if (trial.lobbyRegulation("test_reg", 5e6)) successes++;
    }
    double successRate = (double)successes / trials;
    EXPECT_GT(successRate, 0.3) << "With high capital in deregulation era, success > 30%";
    EXPECT_LT(successRate, 0.95) << "Lobbying should never be guaranteed";
}

TEST_F(PoliticalTest, PoliticalCapitalDecaysAnnually) {
    engine_.earnCapital(50.0);
    engine_.simulateYear(1990, 60.0, false);
    EXPECT_LT(engine_.currentCapital(), 50.0)
        << "Political capital should decay annually";
    EXPECT_GT(engine_.currentCapital(), 40.0)
        << "Decay should be moderate (10%)";
}

TEST_F(PoliticalTest, CrisisReducesPublicOpinion) {
    engine_.simulateYear(2008, 50.0, true);  // Crisis active
    EXPECT_LT(engine_.publicOpinion(), 0.0)
        << "Banking crises should reduce public support for banks";
}

TEST_F(PoliticalTest, CanEvadeRegulationRequiresCapital) {
    EXPECT_FALSE(engine_.canEvadeRegulation("test", 15.0))
        << "Cannot evade regulation without political capital";

    engine_.earnCapital(20.0);
    EXPECT_TRUE(engine_.canEvadeRegulation("test", 15.0))
        << "Should be able to attempt evasion with sufficient capital";
}

TEST_F(PoliticalTest, CostToRegulateAIScales) {
    EXPECT_NEAR(engine_.costToRegulateAI(1.0), 7.0, 0.01);
    EXPECT_NEAR(engine_.costToRegulateAI(5.0), 15.0, 0.01);
}

TEST_F(PoliticalTest, StateSerializesToJson) {
    engine_.earnCapital(25.0);
    engine_.setEraModifier("big_bang");
    auto j = nlohmann::json(engine_.state());
    EXPECT_NEAR(j["politicalCapital"].get<double>(), 25.0, 0.01);
    EXPECT_EQ(j["currentEra"].get<std::string>(), "big_bang");
    EXPECT_NEAR(j["lobbyingEffectiveness"].get<double>(), 1.5, 0.01);
}
