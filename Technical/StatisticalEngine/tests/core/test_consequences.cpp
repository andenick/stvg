#include <gtest/gtest.h>
#include <stvg/core/ConsequenceTracker.h>
#include <stvg/math/RandomEngine.h>
#include <cmath>

using namespace stvg;
using namespace stvg::math;

// ── Core Mechanics ──────────────────────────────────────────────

TEST(ConsequenceTracker, AddFromDecisionCreatesPending) {
    RandomEngine rng(42);
    ConsequenceTracker tracker(rng);

    EXPECT_EQ(tracker.pendingCount(), 0);

    tracker.addFromDecision("d1", "Test Decision", "opt1", "Option A",
                            1e6, 0.01, 1.0, 1);

    EXPECT_GE(tracker.pendingCount(), 1);
}

TEST(ConsequenceTracker, DelayIsBetween1And4Quarters) {
    for (int seed = 0; seed < 100; ++seed) {
        RandomEngine rng(seed);
        ConsequenceTracker tracker(rng);

        tracker.addFromDecision("d1", "Dec", "opt1", "Opt",
                                1e6, 0.0, 0.5, 10);

        auto pending = tracker.pendingConsequences();
        for (const auto& c : pending) {
            int delay = c.resolveQuarter - c.createdQuarter;
            EXPECT_GE(delay, 1) << "Delay too short at seed " << seed;
            EXPECT_LE(delay, 4) << "Delay too long at seed " << seed;
        }
    }
}

TEST(ConsequenceTracker, ResolveForQuarterMarksResolved) {
    RandomEngine rng(42);
    ConsequenceTracker tracker(rng);

    tracker.addFromDecision("d1", "Dec", "opt1", "Opt",
                            1e6, 0.0, 1.0, 1);

    int initialPending = tracker.pendingCount();
    EXPECT_GE(initialPending, 1);

    // Resolve at quarter 10 (well past any 1-4 delay from quarter 1)
    auto resolved = tracker.resolveForQuarter(10);
    EXPECT_FALSE(resolved.empty());
    EXPECT_LT(tracker.pendingCount(), initialPending);

    // All resolved should be marked positive (successProb=1.0)
    for (const auto& c : resolved) {
        EXPECT_TRUE(c.resolved);
        EXPECT_TRUE(c.positive);
    }
}

TEST(ConsequenceTracker, CleanupRemovesOldResolved) {
    RandomEngine rng(42);
    ConsequenceTracker tracker(rng);

    // Add at quarter 1, resolve, then cleanup
    tracker.addFromDecision("d1", "Dec", "opt1", "Opt",
                            1e6, 0.0, 1.0, 1);

    // Resolve at quarter 5
    tracker.resolveForQuarter(5);
    auto resolved = tracker.resolvedHistory();
    EXPECT_FALSE(resolved.empty());

    // Cleanup at quarter 14 (14-5=9 > 8, should remove)
    tracker.cleanup(14);
    auto afterCleanup = tracker.resolvedHistory();
    EXPECT_TRUE(afterCleanup.empty());
}

TEST(ConsequenceTracker, SecondaryConsequence30PctChance) {
    // Secondary consequences fire at 30% when |riskChange| > 0.05
    int secondaryCount = 0;
    const int N = 1000;

    for (int i = 0; i < N; ++i) {
        RandomEngine rng(i + 1000);
        ConsequenceTracker tracker(rng);

        tracker.addFromDecision("d1", "Dec", "opt1", "Opt",
                                1e6, 0.2, 0.5, 1);  // riskChange=0.2 > 0.05

        if (tracker.pendingCount() > 1) {
            secondaryCount++;
        }
    }

    double rate = (double)secondaryCount / N;
    // Expect ~30%, allow 15%-45% tolerance for stochastic variation
    EXPECT_GT(rate, 0.15) << "Secondary rate too low: " << rate;
    EXPECT_LT(rate, 0.45) << "Secondary rate too high: " << rate;
}

TEST(ConsequenceTracker, SuccessFailureDeterminedByProbability) {
    // successProb=1.0 → all positive
    {
        RandomEngine rng(42);
        ConsequenceTracker tracker(rng);
        for (int i = 0; i < 10; ++i) {
            tracker.addFromDecision("d" + std::to_string(i), "Dec", "opt1", "Opt",
                                    1e6, 0.0, 1.0, i + 1);
        }
        auto pending = tracker.pendingConsequences();
        for (const auto& c : pending) {
            EXPECT_TRUE(c.positive) << "Expected positive with successProb=1.0";
        }
    }

    // successProb=0.0 → all negative
    {
        RandomEngine rng(42);
        ConsequenceTracker tracker(rng);
        for (int i = 0; i < 10; ++i) {
            tracker.addFromDecision("d" + std::to_string(i), "Dec", "opt1", "Opt",
                                    1e6, 0.0, 0.0, i + 1);
        }
        auto pending = tracker.pendingConsequences();
        for (const auto& c : pending) {
            EXPECT_FALSE(c.positive) << "Expected negative with successProb=0.0";
        }
    }
}
