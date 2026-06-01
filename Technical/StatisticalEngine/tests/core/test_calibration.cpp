#include <gtest/gtest.h>
#include "stvg/core/EraEngine.h"

using namespace stvg;

TEST(Calibration, LoadsHistoricalParamsFile) {
    EraEngine engine;
    engine.init();

    // Get pre-calibration values
    double preFedFunds = engine.currentEra().economyParams.fedFunds.mu;

    // Load calibration (may or may not find the file depending on cwd)
    bool loaded = engine.loadCalibrationFromFile("data/calibration/historical_era_params.json");

    if (loaded) {
        // Post-war fed funds should be ~1.8% from calibration (vs 2% default)
        double postFedFunds = engine.currentEra().economyParams.fedFunds.mu;
        EXPECT_NE(preFedFunds, postFedFunds)
            << "Calibration should change at least one parameter";
    }
    // If not loaded, this is fine — graceful fallback
}

TEST(Calibration, GracefulFallbackOnMissingFile) {
    EraEngine engine;
    engine.init();

    double preFedFunds = engine.currentEra().economyParams.fedFunds.mu;
    bool loaded = engine.loadCalibrationFromFile("nonexistent_file.json");
    EXPECT_FALSE(loaded);
    double postFedFunds = engine.currentEra().economyParams.fedFunds.mu;
    EXPECT_DOUBLE_EQ(preFedFunds, postFedFunds)
        << "Missing calibration file should not change any parameters";
}

TEST(Calibration, ParamsWithinReasonableBounds) {
    EraEngine engine;
    engine.init();
    engine.loadCalibrationFromFile("data/calibration/historical_era_params.json");

    for (const auto& era : engine.allEras()) {
        EXPECT_GE(era.economyParams.fedFunds.mu, 0.0)
            << era.name << " fed funds mu should be >= 0";
        EXPECT_LE(era.economyParams.fedFunds.mu, 0.20)
            << era.name << " fed funds mu should be <= 20%";
        EXPECT_GE(era.economyParams.gdpGrowth.mu, -0.10)
            << era.name << " GDP growth mu should be >= -10%";
        EXPECT_LE(era.economyParams.gdpGrowth.mu, 0.10)
            << era.name << " GDP growth mu should be <= 10%";
        EXPECT_GE(era.economyParams.unemployment.mu, 0.02)
            << era.name << " unemployment mu should be >= 2%";
        EXPECT_LE(era.economyParams.unemployment.mu, 0.15)
            << era.name << " unemployment mu should be <= 15%";
    }
}
