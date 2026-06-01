#pragma once

#include "RandomEngine.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace stvg::math {

// GJR-GARCH(1,1) parameters:
// sigma^2_t = omega + alpha * eps^2_{t-1} + gamma * eps^2_{t-1} * I(eps<0) + beta * sigma^2_{t-1}
// gamma > 0 captures the leverage effect (negative returns spike vol more than positive)
struct GARCHParams {
    double omega = 0.000252;    // Long-run variance intercept
    double alpha = 0.25;        // Shock reaction coefficient
    double beta  = 0.70;        // Volatility persistence coefficient
    double gamma = 0.12;        // Leverage effect (asymmetric response to negative returns)

    // Persistence = alpha + gamma/2 + beta; must be < 1.0 for stationarity
    double persistence() const { return alpha + gamma / 2.0 + beta; }

    // Long-run (unconditional) variance = omega / (1 - alpha - beta)
    double longRunVariance() const {
        double p = persistence();
        if (p >= 1.0) return omega / 0.01; // fallback for near-unit-root
        return omega / (1.0 - p);
    }

    // Long-run (unconditional) volatility
    double longRunVolatility() const {
        return std::sqrt(longRunVariance());
    }

    bool isStationary() const { return persistence() < 1.0; }

    // Named presets calibrated from Robin VIX data (1990-2025)
    // Format: {omega, alpha, beta, gamma}
    // Presets ensure persistence = alpha + gamma/2 + beta < 1.0
    static GARCHParams Normal()  { return {0.000252, 0.18, 0.70, 0.12}; } // persist=0.94
    static GARCHParams Crisis()  { return {0.000400, 0.22, 0.60, 0.15}; } // persist=0.895
    static GARCHParams Stressed(){ return {0.000300, 0.20, 0.65, 0.12}; } // persist=0.91
    static GARCHParams Bubble()  { return {0.000200, 0.15, 0.75, 0.08}; } // persist=0.94
    static GARCHParams Calm()    { return {0.000150, 0.10, 0.82, 0.06}; } // persist=0.95
};

// GARCH(1,1) volatility process
// Produces time-varying volatility that clusters: high-vol periods persist,
// low-vol periods persist, matching real market behavior.
class GARCHProcess {
public:
    explicit GARCHProcess(const GARCHParams& params = GARCHParams::Normal())
        : params_(params)
        , currentVariance_(params.longRunVariance())
        , previousReturn_(0.0) {}

    // Advance one time step: updates volatility based on previous return,
    // then generates a new return using the updated volatility.
    // dt: time step in years (e.g., 1/252 for daily)
    // Returns: the simulated return for this period
    double step(RandomEngine& rng, double drift = 0.0, double dt = 1.0 / 252.0) {
        // Update conditional variance via GJR-GARCH(1,1) with leverage effect
        double leverageTerm = (previousReturn_ < 0) ? params_.gamma * previousReturn_ * previousReturn_ : 0.0;
        currentVariance_ = params_.omega
                         + params_.alpha * previousReturn_ * previousReturn_
                         + leverageTerm
                         + params_.beta * currentVariance_;

        // Floor variance to avoid numerical issues
        currentVariance_ = std::max(currentVariance_, 1e-10);

        double vol = std::sqrt(currentVariance_);

        // Generate return: drift * dt + vol * sqrt(dt) * Z, Z ~ t(4) for fat tails
        double z = rng.studentT(4.0);
        double ret = drift * dt + vol * std::sqrt(dt) * z;

        previousReturn_ = ret;
        return ret;
    }

    double currentVolatility() const { return std::sqrt(currentVariance_); }
    double currentVariance() const { return currentVariance_; }
    double previousReturn() const { return previousReturn_; }
    void loadState(double variance, double prevReturn) {
        currentVariance_ = variance;
        previousReturn_ = prevReturn;
    }

    void setParams(const GARCHParams& p) {
        params_ = p;
        // Optionally re-anchor variance to new long-run level
    }

    const GARCHParams& params() const { return params_; }

    // Feed an externally-generated return into the GARCH state.
    // Use this when returns come from correlated shocks rather than garch.step().
    void feedReturn(double ret) {
        double leverageTerm = (ret < 0) ? params_.gamma * ret * ret : 0.0;
        currentVariance_ = params_.omega
                         + params_.alpha * ret * ret
                         + leverageTerm
                         + params_.beta * currentVariance_;
        currentVariance_ = std::max(currentVariance_, 1e-10);
        previousReturn_ = ret;
    }

    // Reset to unconditional variance
    void reset() {
        currentVariance_ = params_.longRunVariance();
        previousReturn_ = 0.0;
    }

private:
    GARCHParams params_;
    double currentVariance_;
    double previousReturn_;
};

} // namespace stvg::math
