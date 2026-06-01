#pragma once

#include "RandomEngine.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace stvg::math {

// Number of correlated asset classes
static constexpr int NUM_ASSETS = 6;
// Order: Equities, FixedIncome, Commodities, Currencies, Credit, Mortgages

using CorrMatrix = std::array<std::array<double, NUM_ASSETS>, NUM_ASSETS>;
using CholeskyL  = std::array<std::array<double, NUM_ASSETS>, NUM_ASSETS>;
using AssetShocks = std::array<double, NUM_ASSETS>;

// ════════════════════════════════════════════════════════════════════
// DCC-GARCH Correlation Engine
//
// Dynamic Conditional Correlations: correlations evolve over time,
// spiking during crises and slowly mean-reverting to baseline.
//
//   Q_t = (1 - alpha - beta) * Q_bar + alpha * e_{t-1} * e'_{t-1} + beta * Q_{t-1}
//   R_t = diag(Q_t)^{-1/2} * Q_t * diag(Q_t)^{-1/2}
//
// alpha = shock sensitivity (how fast correlations spike on stress)
// beta  = persistence (how long elevated correlations linger)
// ════════════════════════════════════════════════════════════════════

class CorrelationEngine {
public:
    CorrelationEngine() {
        // Default unconditional correlations (post-war calm)
        unconditionalCorr_ = {{
            {{ 1.00, -0.20,  0.30,  0.10,  0.40,  0.25}},
            {{-0.20,  1.00, -0.10, -0.05, -0.15,  0.30}},
            {{ 0.30, -0.10,  1.00,  0.20,  0.15,  0.10}},
            {{ 0.10, -0.05,  0.20,  1.00,  0.10,  0.05}},
            {{ 0.40, -0.15,  0.15,  0.10,  1.00,  0.35}},
            {{ 0.25,  0.30,  0.10,  0.05,  0.35,  1.00}}
        }};

        Qt_ = unconditionalCorr_;
        conditionalCorr_ = unconditionalCorr_;
    }

    // Initialize with era-specific parameters
    void init(const CorrMatrix& unconditional, double alpha, double beta) {
        alpha_ = alpha;
        beta_ = beta;
        unconditionalCorr_ = unconditional;
        Qt_ = unconditional;
        conditionalCorr_ = unconditional;
    }

    // Switch era parameters (smooth transition)
    void setEraParameters(const CorrMatrix& unconditional, double alpha, double beta) {
        alpha_ = alpha;
        beta_ = beta;
        unconditionalCorr_ = unconditional;
        // Qt_ carries forward — it will mean-revert to new unconditional over time
    }

    // DCC update step — call after each simulation tick with standardized residuals
    void update(const AssetShocks& residuals) {
        double oneMinusAB = 1.0 - alpha_ - beta_;

        // Q_t = (1-a-b)*Q_bar + a*e_{t-1}*e'_{t-1} + b*Q_{t-1}
        for (int i = 0; i < NUM_ASSETS; ++i) {
            for (int j = 0; j < NUM_ASSETS; ++j) {
                Qt_[i][j] = oneMinusAB * unconditionalCorr_[i][j]
                           + alpha_ * residuals[i] * residuals[j]
                           + beta_ * Qt_[i][j];
            }
        }

        // Normalize: R_t = diag(Q_t)^{-1/2} * Q_t * diag(Q_t)^{-1/2}
        for (int i = 0; i < NUM_ASSETS; ++i) {
            for (int j = 0; j < NUM_ASSETS; ++j) {
                double denom = std::sqrt(std::max(Qt_[i][i], 1e-12))
                             * std::sqrt(std::max(Qt_[j][j], 1e-12));
                conditionalCorr_[i][j] = std::clamp(Qt_[i][j] / denom, -1.0, 1.0);
            }
            conditionalCorr_[i][i] = 1.0; // Enforce unit diagonal
        }
    }

    // Generate correlated shocks using CURRENT dynamic correlations
    // stressLevel parameter maintained for backward compatibility but DCC
    // handles dynamics internally — high stress feeds large residuals which
    // spike correlations via the DCC update.
    AssetShocks generateCorrelatedShocks(RandomEngine& rng, double /*stressLevel*/ = 0) {
        AssetShocks independent;
        for (int i = 0; i < NUM_ASSETS; ++i) {
            independent[i] = rng.normalSample();
        }

        CholeskyL L = cholesky(conditionalCorr_);

        AssetShocks correlated{};
        for (int i = 0; i < NUM_ASSETS; ++i) {
            for (int j = 0; j <= i; ++j) {
                correlated[i] += L[i][j] * independent[j];
            }
        }

        return correlated;
    }

    // Accessors
    const CorrMatrix& currentCorrelation() const { return conditionalCorr_; }
    const CorrMatrix& unconditionalCorrelation() const { return unconditionalCorr_; }
    double pairwiseCorrelation(int i, int j) const { return conditionalCorr_[i][j]; }
    double alpha() const { return alpha_; }
    double beta() const { return beta_; }

    // Legacy setters (for backward compatibility)
    void setNormalCorrelation(const CorrMatrix& m) {
        unconditionalCorr_ = m;
        // Don't reset Qt_ — let it evolve
    }
    void setCrisisCorrelation(const CorrMatrix& /*m*/) {
        // No-op in DCC model — crisis correlations emerge dynamically
    }

private:
    double alpha_ = 0.05;      // Shock sensitivity
    double beta_ = 0.90;       // Persistence

    CorrMatrix unconditionalCorr_;  // Q_bar (era-specific baseline)
    CorrMatrix conditionalCorr_;    // R_t (current dynamic correlations)
    CorrMatrix Qt_;                 // DCC intermediate matrix

    static CholeskyL cholesky(const CorrMatrix& A) {
        CholeskyL L{};
        for (int i = 0; i < NUM_ASSETS; ++i) {
            for (int j = 0; j <= i; ++j) {
                double sum = 0.0;
                for (int k = 0; k < j; ++k) {
                    sum += L[i][k] * L[j][k];
                }
                if (i == j) {
                    double val = A[i][i] - sum;
                    L[i][j] = std::sqrt(std::max(val, 1e-12));
                } else {
                    L[i][j] = (A[i][j] - sum) / L[j][j];
                }
            }
        }
        return L;
    }
};

} // namespace stvg::math
