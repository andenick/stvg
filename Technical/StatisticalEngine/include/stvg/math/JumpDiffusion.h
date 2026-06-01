#pragma once

#include "RandomEngine.h"
#include <cmath>
#include <algorithm>

namespace stvg::math {

// Jump-diffusion parameters calibrated from Robin historical data.
// Models sudden market dislocations (crashes, flash crashes, regime breaks).
// dS/S = mu*dt + sigma*dW + J*dN, where N is Poisson with intensity lambda.
struct JumpParams {
    double intensity = 0.002;    // Daily jump probability (0.2% = ~2 jumps/year)
    double jumpMean = -0.05;     // Average jump size (negative = crash bias)
    double jumpStdDev = 0.08;    // Jump size volatility
    double crisisMultiplier = 3.0; // Intensity multiplier during crisis regime

    static JumpParams Robin() { return {0.002, -0.05, 0.08, 3.0}; }
};

// Composable jump-diffusion process.
// Call step() each tick to check for a jump event.
class JumpDiffusionProcess {
public:
    explicit JumpDiffusionProcess(const JumpParams& params = JumpParams::Robin())
        : params_(params) {}

    // Check for jump this tick. Returns jump magnitude (0.0 if no jump).
    // stressLevel: 0-100, scales intensity toward crisis multiplier
    double step(RandomEngine& rng, double stressLevel = 0.0) {
        // Scale intensity with stress
        double adjustedIntensity = params_.intensity
            * (1.0 + (params_.crisisMultiplier - 1.0) * stressLevel / 100.0);

        if (!rng.bernoulli(adjustedIntensity)) {
            lastJump_ = 0.0;
            jumped_ = false;
            return 0.0;
        }

        // Jump occurred
        double jump = rng.normalSample(params_.jumpMean, params_.jumpStdDev);
        lastJump_ = jump;
        jumped_ = true;
        ++totalJumps_;
        return jump;
    }

    bool jumped() const { return jumped_; }
    double lastJump() const { return lastJump_; }
    int totalJumps() const { return totalJumps_; }

    void setParams(const JumpParams& p) { params_ = p; }
    const JumpParams& params() const { return params_; }

private:
    JumpParams params_;
    double lastJump_ = 0.0;
    bool jumped_ = false;
    int totalJumps_ = 0;
};

} // namespace stvg::math
