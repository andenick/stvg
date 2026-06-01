#include <gtest/gtest.h>
#include <stvg/core/PersonalityProfile.h>
#include <stvg/autoplay/SensitivityAnalysis.h>

using namespace stvg;
using namespace stvg::autoplay;

TEST(PersonalitySensitivity, SetAxisModifiesCorrectField) {
    PersonalityProfile p;

    for (int i = 0; i < PersonalityProfile::kNumAxes; ++i) {
        PersonalityProfile test;
        test.setAxis(i, 0.0);
        EXPECT_DOUBLE_EQ(test.getAxis(i), 0.0)
            << "setAxis/getAxis mismatch at index " << i
            << " (" << PersonalityProfile::axisName(i) << ")";

        // All other axes should still be at defaults
        for (int j = 0; j < PersonalityProfile::kNumAxes; ++j) {
            if (j == i) continue;
            EXPECT_DOUBLE_EQ(test.getAxis(j), p.getAxis(j))
                << "Axis " << j << " (" << PersonalityProfile::axisName(j)
                << ") changed when setting axis " << i;
        }
    }
}

TEST(PersonalitySensitivity, AxisNameCoversAll11) {
    EXPECT_EQ(PersonalityProfile::kNumAxes, 11);

    for (int i = 0; i < PersonalityProfile::kNumAxes; ++i) {
        const char* name = PersonalityProfile::axisName(i);
        EXPECT_NE(name, nullptr) << "Null name at index " << i;
        EXPECT_GT(strlen(name), 0u) << "Empty name at index " << i;
        EXPECT_STRNE(name, "unknown") << "Unknown name at index " << i;
    }

    // Out of bounds returns "unknown"
    EXPECT_STREQ(PersonalityProfile::axisName(-1), "unknown");
    EXPECT_STREQ(PersonalityProfile::axisName(11), "unknown");
}

TEST(PersonalitySensitivity, SensitivitySmokeTest) {
    SensitivityConfig cfg;
    cfg.gameConfig = GameConfig::Normal();
    cfg.gamesPerExtreme = 2;
    cfg.maxQuarters = 10;
    cfg.parallel = false;

    auto results = runSensitivity(cfg);

    // Should produce exactly 11 axis results
    EXPECT_EQ(results.size(), 11u);

    // All results should have valid data
    for (const auto& r : results) {
        EXPECT_FALSE(r.axisName.empty());
        EXPECT_GE(r.survivalLow, 0.0);
        EXPECT_LE(r.survivalLow, 100.0);
        EXPECT_GE(r.survivalHigh, 0.0);
        EXPECT_LE(r.survivalHigh, 100.0);
        EXPECT_FALSE(std::isnan(r.scoreLow));
        EXPECT_FALSE(std::isnan(r.scoreHigh));
    }

    // Results should be sorted by |impact| descending
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_GE(results[i-1].absImpact, results[i].absImpact);
    }
}
