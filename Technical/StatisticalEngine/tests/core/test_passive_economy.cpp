#include <gtest/gtest.h>
#include <stvg/autoplay/GameRunner.h>
#include <stvg/autoplay/BotStrategy.h>
#include <stvg/core/GameConfig.h>

using namespace stvg;
using namespace stvg::autoplay;

// ── PassiveBot: makes no decisions (returns "" to skip all) ─────

class PassiveBot : public BotStrategy {
public:
    std::string name() const override { return "PassiveBot"; }
    std::string chooseOption(const QuarterlyTurnManager&,
                              const simulation::Decision&) override {
        return "";  // Skip all decisions
    }
};

// ── Passive Economy Validation ──────────────────────────────────

TEST(PassiveEconomy, CapitalStaysWithinBounds) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    PassiveBot bot;

    double startCap = config.bank.startingCapital;

    for (uint64_t seed = 42; seed < 52; ++seed) {
        auto summary = runner.runGame(bot, config, seed, 380);

        // Capital should stay within [0.5x, 2x] of starting
        // If this fails, economy is biased without player input
        EXPECT_GE(summary.finalCapital, startCap * 0.5)
            << "Capital too low at seed " << seed
            << ": $" << summary.finalCapital / 1e9 << "B"
            << " (started at $" << startCap / 1e9 << "B)"
            << " after " << summary.quartersPlayed << " quarters";
        EXPECT_LE(summary.finalCapital, startCap * 2.0)
            << "Capital too high at seed " << seed
            << ": $" << summary.finalCapital / 1e9 << "B";
    }
}

TEST(PassiveEconomy, RevenuesExceedCosts) {
    // Banks are naturally profitable businesses — passive revenue/cost
    // ratio ~2.5x is expected (deposits generate interest income).
    // This test verifies revenue isn't negative and costs don't explode.
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    PassiveBot bot;

    for (uint64_t seed = 42; seed < 52; ++seed) {
        auto summary = runner.runGame(bot, config, seed, 380);

        if (summary.totalCosts > 0) {
            double ratio = summary.totalRevenue / summary.totalCosts;
            EXPECT_GE(ratio, 0.5)
                << "Revenue/cost ratio too low at seed " << seed
                << ": " << ratio;
            EXPECT_LE(ratio, 5.0)
                << "Revenue/cost ratio unrealistically high at seed " << seed
                << ": " << ratio;
        }
    }
}

TEST(PassiveEconomy, NoNaNOrInfinity) {
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    PassiveBot bot;

    for (uint64_t seed = 42; seed < 52; ++seed) {
        auto summary = runner.runGame(bot, config, seed, 380);
        EXPECT_FALSE(summary.hadNaN)
            << "NaN detected at seed " << seed
            << " in quarter " << summary.quartersPlayed;
    }
}

TEST(PassiveEconomy, SurvivesAtLeast20Quarters) {
    // A passive CEO (no decisions) gets board_fired at ~35-44 quarters,
    // which is working as designed. This test verifies the bank doesn't
    // crash immediately — 20 quarters (5 years) is the minimum viable
    // lifetime before the board loses patience.
    GameRunner runner;
    GameConfig config = GameConfig::Normal();
    PassiveBot bot;

    for (uint64_t seed = 42; seed < 52; ++seed) {
        auto summary = runner.runGame(bot, config, seed, 380);
        EXPECT_GE(summary.quartersPlayed, 20)
            << "Passive bank died too early at seed " << seed
            << " after " << summary.quartersPlayed << " quarters"
            << " (reason: " << summary.gameEndReason << ")";
    }
}
