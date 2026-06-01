#include <gtest/gtest.h>
#include "stvg/simulation/DurationRisk.h"

using namespace stvg::simulation;

TEST(DurationTarget, ShortDurationSetsProfile) {
    DurationRiskState state;
    state.target = DurationTarget::Short;
    state.applyTarget();
    EXPECT_NEAR(state.profile.shortPct, 0.60, 0.01);
    EXPECT_NEAR(state.profile.longPct, 0.10, 0.01);
    EXPECT_LT(state.profile.portfolioDuration(), 2.1);
}

TEST(DurationTarget, LongDurationSetsProfile) {
    DurationRiskState state;
    state.target = DurationTarget::Long;
    state.applyTarget();
    EXPECT_NEAR(state.profile.shortPct, 0.10, 0.01);
    EXPECT_NEAR(state.profile.longPct, 0.50, 0.01);
    EXPECT_GT(state.profile.portfolioDuration(), 5.0);
}

TEST(DurationTarget, BalancedIsDefault) {
    DurationRiskState state;
    EXPECT_EQ(state.target, DurationTarget::Balanced);
    state.applyTarget();
    EXPECT_NEAR(state.profile.shortPct, 0.30, 0.01);
    EXPECT_NEAR(state.profile.mediumPct, 0.50, 0.01);
}

TEST(DurationTarget, LongDurationAmplifiesRateRisk) {
    DurationRiskState shortState, longState;
    shortState.target = DurationTarget::Short;
    longState.target = DurationTarget::Long;

    double yieldRise = 0.05; // 50bps rise
    double securities = 20e9;
    shortState.updateQuarterly(shortState.prevYield10Y + yieldRise, securities);
    longState.updateQuarterly(longState.prevYield10Y + yieldRise, securities);

    // Long duration should lose more from rate increases
    EXPECT_LT(longState.unrealizedGainsLosses, shortState.unrealizedGainsLosses);
}
