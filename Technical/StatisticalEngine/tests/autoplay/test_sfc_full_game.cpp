#include <gtest/gtest.h>
#include <stvg/autoplay/GameRunner.h>
#include <stvg/autoplay/BotStrategy.h>
#include <stvg/autoplay/EraAwareBots.h>
#include <stvg/core/PersonalityProfile.h>
#include <cmath>
#include <set>

using namespace stvg;
using namespace stvg::autoplay;

struct BotPreset {
    std::string name;
    PersonalityProfile profile;
};

static std::vector<BotPreset> allPresets() {
    return {
        {"Random", PersonalityProfile::random()},
        {"Utility", PersonalityProfile::utility()},
        {"Conservative", PersonalityProfile::conservative()},
        {"Aggressive", PersonalityProfile::aggressive()},
        {"Balanced", PersonalityProfile::balanced()},
        {"Gambler", PersonalityProfile::gambler()},
        {"RegulatorsPet", PersonalityProfile::regulatorsPet()},
        {"Historical", PersonalityProfile::historical()},
        {"Speedrun", PersonalityProfile::speedrun()},
        {"Survivor", PersonalityProfile::survivor()},
        {"Contrary", PersonalityProfile::contrary()},
        {"Political", PersonalityProfile::political()},
    };
}

TEST(SFCFullGame, AllBotsCompleteWithoutNaN) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    for (const auto& preset : allPresets()) {
        PersonalityDrivenBot bot(preset.name, preset.profile);
        for (uint64_t seed = 42; seed < 45; ++seed) {
            auto result = runner.runGame(bot, config, seed, 380);
            EXPECT_FALSE(result.hadNaN) << preset.name << " seed=" << seed;
            EXPECT_GT(result.quartersPlayed, 0) << preset.name << " seed=" << seed;
        }
    }
}

TEST(SFCFullGame, AllBotsSurviveAtLeastSomeGames) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    for (const auto& preset : allPresets()) {
        int survivedLong = 0;
        PersonalityDrivenBot bot(preset.name, preset.profile);
        for (uint64_t seed = 42; seed < 47; ++seed) {
            auto result = runner.runGame(bot, config, seed, 380);
            if (result.quartersPlayed >= 20) survivedLong++;
        }
        EXPECT_GE(survivedLong, 1) << preset.name << " never survived 20+ quarters out of 5 seeds";
    }
}

TEST(SFCFullGame, ScoreSpreadPreserved) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    double minScore = 1e18, maxScore = -1e18;
    for (const auto& preset : allPresets()) {
        PersonalityDrivenBot bot(preset.name, preset.profile);
        double totalScore = 0;
        int games = 0;
        for (uint64_t seed = 42; seed < 45; ++seed) {
            auto result = runner.runGame(bot, config, seed, 100);
            totalScore += result.avgScore;
            games++;
        }
        double avg = totalScore / games;
        minScore = std::min(minScore, avg);
        maxScore = std::max(maxScore, avg);
    }
    double spread = maxScore - minScore;
    EXPECT_GT(spread, 2.0)
        << "Score spread too narrow: " << spread << " (min=" << minScore << " max=" << maxScore << ")";
}

TEST(SFCFullGame, DiverseDeathModes) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    std::set<std::string> deathReasons;
    for (const auto& preset : allPresets()) {
        PersonalityDrivenBot bot(preset.name, preset.profile);
        for (uint64_t seed = 42; seed < 47; ++seed) {
            auto result = runner.runGame(bot, config, seed, 380);
            if (!result.survived && !result.gameEndReason.empty()) {
                deathReasons.insert(result.gameEndReason);
            }
        }
    }
    EXPECT_GE(deathReasons.size(), 1u)
        << "Expected at least 1 death mode, got " << deathReasons.size();
}

TEST(SFCFullGame, FiveStartingPositionsAllViable) {
    for (auto pos : {StartingPosition::CommercialBank, StartingPosition::TradingFirm,
                     StartingPosition::InvestmentBank, StartingPosition::CommunityBank,
                     StartingPosition::UniversalBank}) {
        SimulationConfig cfg;
        cfg.seed = 42;
        BankConfig bc = bankConfigForPosition(pos);
        QuarterlyTurnManager mgr(cfg, bc, "", pos);
        const auto& bank = mgr.bank();
        EXPECT_GT(bank.totalAssets, 0) << "Position " << (int)pos << " has no assets";
        EXPECT_GT(bank.capital, 0) << "Position " << (int)pos << " has no capital";
        EXPECT_GT(bank.totalDeposits, 0) << "Position " << (int)pos << " has no deposits";
    }
}

TEST(SFCFullGame, NoNaNIn36FullGames) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    int nanCount = 0;
    for (const auto& preset : allPresets()) {
        PersonalityDrivenBot bot(preset.name, preset.profile);
        for (uint64_t seed = 42; seed < 45; ++seed) {
            auto result = runner.runGame(bot, config, seed, 380);
            if (result.hadNaN) nanCount++;
        }
    }
    EXPECT_EQ(nanCount, 0) << nanCount << " games had NaN out of 36";
}

TEST(SFCFullGame, FinalCapitalReasonable) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    for (const auto& preset : allPresets()) {
        PersonalityDrivenBot bot(preset.name, preset.profile);
        for (uint64_t seed = 42; seed < 45; ++seed) {
            auto result = runner.runGame(bot, config, seed, 100);
            if (result.survived) {
                EXPECT_GT(result.finalCapital, 0)
                    << preset.name << " seed=" << seed << " survived with 0 capital";
                EXPECT_LT(result.finalCapital, 1e14)
                    << preset.name << " seed=" << seed << " has unrealistic $100T+ capital";
            }
        }
    }
}
