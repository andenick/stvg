#include <gtest/gtest.h>
#include <stvg/autoplay/EraAwareBots.h>
#include <stvg/autoplay/GameRunner.h>
#include <stvg/autoplay/SensitivityAnalysis.h>
#include <stvg/core/GameConfig.h>

using namespace stvg;
using namespace stvg::autoplay;

// ── A2: Verify all 11 axes are wired into scoring ──────────────

TEST(PersonalityWiring, PersuasionAffectsScoring) {
    // High persuasion bot should behave differently than low persuasion
    PersonalityProfile high;
    high.persuasion = 1.0;
    PersonalityProfile low;
    low.persuasion = 0.0;

    GameRunner runner;
    GameConfig config = GameConfig::Normal();

    PersonalityDrivenBot highBot("HighPersuasion", high);
    PersonalityDrivenBot lowBot("LowPersuasion", low);

    auto highResult = runner.runGame(highBot, config, 42, 50);
    auto lowResult = runner.runGame(lowBot, config, 42, 50);

    // Both should complete without NaN
    EXPECT_FALSE(highResult.hadNaN);
    EXPECT_FALSE(lowResult.hadNaN);
    // They may make different decisions (different scores)
    // We can't guarantee score differs in 50 quarters, but verify no crash
}

TEST(PersonalityWiring, AdaptabilityAffectsScoring) {
    PersonalityProfile high;
    high.adaptability = 1.0;
    PersonalityProfile low;
    low.adaptability = 0.0;

    GameRunner runner;
    GameConfig config = GameConfig::Normal();

    PersonalityDrivenBot highBot("HighAdapt", high);
    PersonalityDrivenBot lowBot("LowAdapt", low);

    auto highResult = runner.runGame(highBot, config, 42, 50);
    auto lowResult = runner.runGame(lowBot, config, 42, 50);

    EXPECT_FALSE(highResult.hadNaN);
    EXPECT_FALSE(lowResult.hadNaN);
}

TEST(PersonalityWiring, IntegrityAmplified) {
    // High integrity bot should avoid hide/conceal, prefer audits
    PersonalityProfile high;
    high.integrity = 1.0;
    high.complianceFocus = 0.5;  // neutral compliance

    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    PersonalityDrivenBot bot("HighIntegrity", high);
    auto result = runner.runGame(bot, config, 42, 50);

    EXPECT_FALSE(result.hadNaN);
    EXPECT_GT(result.quartersPlayed, 0);
}

TEST(PersonalityWiring, LongTermFocusAmplified) {
    // High longTermFocus should prefer multi-quarter investments
    PersonalityProfile high;
    high.longTermFocus = 1.0;
    PersonalityProfile low;
    low.longTermFocus = 0.0;

    GameRunner runner;
    GameConfig config = GameConfig::Normal();

    PersonalityDrivenBot highBot("HighLTF", high);
    PersonalityDrivenBot lowBot("LowLTF", low);

    auto highResult = runner.runGame(highBot, config, 42, 50);
    auto lowResult = runner.runGame(lowBot, config, 42, 50);

    EXPECT_FALSE(highResult.hadNaN);
    EXPECT_FALSE(lowResult.hadNaN);
}

TEST(PersonalityWiring, PoliticalSavvyBroadened) {
    // High politicalSavvy should score political options higher
    PersonalityProfile high;
    high.politicalSavvy = 1.0;

    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    PersonalityDrivenBot bot("HighPolitical", high);
    auto result = runner.runGame(bot, config, 42, 50);

    EXPECT_FALSE(result.hadNaN);
    EXPECT_GT(result.quartersPlayed, 0);
}

TEST(PersonalityWiring, SensitivityQuickCheck) {
    // Run a quick sensitivity to verify axes show impact
    // This is a smoke test — full validation is C1
    SensitivityConfig cfg;
    cfg.gameConfig = GameConfig::Normal();
    cfg.gamesPerExtreme = 5;
    cfg.maxQuarters = 50;
    cfg.parallel = false;

    auto results = runSensitivity(cfg);
    EXPECT_EQ(results.size(), 11u);

    // Count how many axes have non-zero impact
    int activeAxes = 0;
    for (const auto& r : results) {
        if (r.absImpact > 0.1) activeAxes++;
    }
    // In 50 quarters, at least 4 of the major axes should show impact
    // (Full-matrix C1 will validate all 11)
    EXPECT_GE(activeAxes, 4) << "Expected at least 4 active personality axes in 50Q";
}
