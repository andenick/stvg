#include <gtest/gtest.h>
#include <stvg/core/GameConfig.h>

using namespace stvg;

TEST(TutorialConfig, HasExpectedDefaults) {
    auto config = GameConfig::Tutorial();

    // Tutorial should be short (20 quarters)
    EXPECT_EQ(config.timing.quartersPerGame, 20);

    // Double starting capital (rescaled from 20e9)
    EXPECT_DOUBLE_EQ(config.bank.startingCapital, 2e6);

    // High starting reputation
    EXPECT_DOUBLE_EQ(config.bank.startingReputation, 80.0);

    // Very low crisis probability
    EXPECT_LT(config.crisis.probCalm, 0.01);
    EXPECT_LT(config.crisis.probNormal, 0.02);
}

TEST(TutorialConfig, DifficultyPresetsExist) {
    // Verify all difficulty presets compile and have sane values
    auto easy = GameConfig::Easy();
    auto normal = GameConfig::Normal();
    auto hard = GameConfig::Hard();
    auto sandbox = GameConfig::Sandbox();
    auto crisis = GameConfig::Crisis();
    auto tutorial = GameConfig::Tutorial();

    // Easy should have lower crisis rates than Hard
    EXPECT_LT(easy.crisis.probNormal, hard.crisis.probNormal);
    EXPECT_LT(easy.crisis.probStressed, hard.crisis.probStressed);

    // Tutorial should be easier than Easy
    EXPECT_LE(tutorial.crisis.probCalm, easy.crisis.probCalm);

    // All should have positive starting capital
    EXPECT_GT(easy.bank.startingCapital, 0);
    EXPECT_GT(normal.bank.startingCapital, 0);
    EXPECT_GT(hard.bank.startingCapital, 0);
    EXPECT_GT(tutorial.bank.startingCapital, 0);
}

TEST(TutorialConfig, RestrictionsStruct) {
    auto config = GameConfig::Normal();

    // Default restrictions should be inactive
    EXPECT_FALSE(config.restrictions.noDeleveraging);
    EXPECT_FALSE(config.restrictions.noCrises);
    EXPECT_TRUE(config.restrictions.forceCeoId.empty());
    EXPECT_FALSE(config.restrictions.hasAny());

    // Setting any flag should make hasAny() true
    config.restrictions.noCrises = true;
    EXPECT_TRUE(config.restrictions.hasAny());
}
