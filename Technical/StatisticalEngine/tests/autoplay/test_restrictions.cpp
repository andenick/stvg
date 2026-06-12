#include <gtest/gtest.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <stvg/core/GameConfig.h>
#include <stvg/autoplay/GameRunner.h>
#include <stvg/autoplay/BotStrategy.h>

using namespace stvg;
using namespace stvg::autoplay;

TEST(Restrictions, DefaultsToNone) {
    GameConfig config = GameConfig::Normal();
    EXPECT_FALSE(config.restrictions.noDeleveraging);
    EXPECT_FALSE(config.restrictions.noCrises);
    EXPECT_TRUE(config.restrictions.forceCeoId.empty());
    EXPECT_FALSE(config.restrictions.hasAny());
}

TEST(Restrictions, ForceCeoSelectsCEO) {
    SimulationConfig simCfg;
    simCfg.seed = 42;
    simCfg.startYear = 1945;
    simCfg.startQuarter = 1;

    BankConfig bankCfg;
    bankCfg.startingCapital = 1e6;   // rescaled from 10e9

    QuarterlyTurnManager mgr(simCfg, bankCfg, "volcker");
    EXPECT_TRUE(mgr.hasCeo());
    EXPECT_EQ(mgr.ceoProfile().id, "volcker");
}

TEST(Restrictions, NoCrisesBlocksAllCrises) {
    // Run a short game with no-crises restriction — should have 0 crises
    GameConfig config = GameConfig::Normal();
    config.restrictions.noCrises = true;

    GameRunner runner;
    RandomBot bot(42);
    auto summary = runner.runGame(bot, config, 42, 40);

    EXPECT_EQ(summary.crisesEncountered, 0)
        << "Expected 0 crises with no-crises restriction";
}

TEST(Restrictions, HasAnyDetectsFlags) {
    GameConfig::Restrictions r;
    EXPECT_FALSE(r.hasAny());

    r.noDeleveraging = true;
    EXPECT_TRUE(r.hasAny());

    r = {};
    r.noCrises = true;
    EXPECT_TRUE(r.hasAny());

    r = {};
    r.forceCeoId = "volcker";
    EXPECT_TRUE(r.hasAny());
}
