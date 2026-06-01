#pragma once

#include "RandomEngine.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <numbers>

namespace stvg::math {

// Base class for statistical distributions
class Distribution {
public:
    virtual ~Distribution() = default;
    virtual double sample(RandomEngine& rng) const = 0;
    virtual double pdf(double x) const = 0;
    virtual double cdf(double x) const = 0;
    virtual double mean() const = 0;
    virtual double variance() const = 0;
};

// ── Normal Distribution ─────────────────────────────────────────────
class NormalDistribution : public Distribution {
public:
    NormalDistribution(double mean = 0.0, double stddev = 1.0)
        : mu_(mean), sigma_(stddev) {}

    double sample(RandomEngine& rng) const override {
        return rng.normalSample(mu_, sigma_);
    }

    double pdf(double x) const override {
        double z = (x - mu_) / sigma_;
        return std::exp(-0.5 * z * z) / (sigma_ * std::sqrt(2.0 * std::numbers::pi));
    }

    double cdf(double x) const override {
        return 0.5 * (1.0 + std::erf((x - mu_) / (sigma_ * std::numbers::sqrt2)));
    }

    double mean() const override { return mu_; }
    double variance() const override { return sigma_ * sigma_; }

    void setParameters(double mean, double stddev) {
        mu_ = mean;
        sigma_ = stddev;
    }

    // Calibrate from data via MLE
    void calibrate(const std::vector<double>& data) {
        if (data.empty()) return;
        mu_ = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
        double var = 0.0;
        for (double x : data) var += (x - mu_) * (x - mu_);
        sigma_ = std::sqrt(std::max(var / data.size(), 1e-12));
    }

private:
    double mu_, sigma_;
};

// ── Student's t Distribution ────────────────────────────────────────
// Used for fat-tailed financial returns (df=4 captures extreme events)
class StudentTDistribution : public Distribution {
public:
    StudentTDistribution(double df = 4.0, double location = 0.0, double scale = 1.0)
        : df_(df), loc_(location), scale_(scale) {}

    double sample(RandomEngine& rng) const override {
        return loc_ + scale_ * rng.studentT(df_);
    }

    double pdf(double x) const override {
        double z = (x - loc_) / scale_;
        double coeff = std::tgamma((df_ + 1.0) / 2.0)
                     / (std::sqrt(df_ * std::numbers::pi) * std::tgamma(df_ / 2.0));
        return (coeff / scale_) * std::pow(1.0 + z * z / df_, -(df_ + 1.0) / 2.0);
    }

    double cdf(double x) const override {
        // Use regularized incomplete beta function approximation
        double z = (x - loc_) / scale_;
        double t = df_ / (df_ + z * z);
        double betaVal = regularizedIncompleteBeta(df_ / 2.0, 0.5, t);
        return z >= 0 ? 1.0 - 0.5 * betaVal : 0.5 * betaVal;
    }

    double mean() const override { return df_ > 1.0 ? loc_ : 0.0; }
    double variance() const override {
        if (df_ > 2.0) return scale_ * scale_ * df_ / (df_ - 2.0);
        return std::numeric_limits<double>::infinity();
    }

    double degreesOfFreedom() const { return df_; }

private:
    double df_, loc_, scale_;

    // Continued fraction approximation for regularized incomplete beta
    static double regularizedIncompleteBeta(double a, double b, double x) {
        if (x <= 0.0) return 0.0;
        if (x >= 1.0) return 1.0;

        double lbeta = std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
        double front = std::exp(std::log(x) * a + std::log(1.0 - x) * b - lbeta);

        // Lentz's algorithm for continued fraction
        double f = 1.0, c = 1.0, d = 1.0 - (a + b) * x / (a + 1.0);
        if (std::abs(d) < 1e-30) d = 1e-30;
        d = 1.0 / d;
        f = d;

        for (int m = 1; m <= 200; ++m) {
            // Even step
            double num = m * (b - m) * x / ((a + 2.0 * m - 1.0) * (a + 2.0 * m));
            d = 1.0 + num * d;
            if (std::abs(d) < 1e-30) d = 1e-30;
            c = 1.0 + num / c;
            if (std::abs(c) < 1e-30) c = 1e-30;
            d = 1.0 / d;
            f *= c * d;

            // Odd step
            num = -(a + m) * (a + b + m) * x / ((a + 2.0 * m) * (a + 2.0 * m + 1.0));
            d = 1.0 + num * d;
            if (std::abs(d) < 1e-30) d = 1e-30;
            c = 1.0 + num / c;
            if (std::abs(c) < 1e-30) c = 1e-30;
            d = 1.0 / d;
            double delta = c * d;
            f *= delta;

            if (std::abs(delta - 1.0) < 1e-10) break;
        }

        return front * f / a;
    }
};

// ── Mixture Distribution ────────────────────────────────────────────
// 95% normal + 5% fat-tailed for realistic financial returns
class MixtureDistribution : public Distribution {
public:
    MixtureDistribution(double normalWeight = 0.95,
                        double normalMean = 0.0, double normalStd = 0.01,
                        double fatMean = 0.0, double fatDf = 4.0, double fatScale = 0.03)
        : weight_(normalWeight)
        , normal_(normalMean, normalStd)
        , fat_(fatDf, fatMean, fatScale) {}

    double sample(RandomEngine& rng) const override {
        return rng.bernoulli(weight_) ? normal_.sample(rng) : fat_.sample(rng);
    }

    double pdf(double x) const override {
        return weight_ * normal_.pdf(x) + (1.0 - weight_) * fat_.pdf(x);
    }

    double cdf(double x) const override {
        return weight_ * normal_.cdf(x) + (1.0 - weight_) * fat_.cdf(x);
    }

    double mean() const override {
        return weight_ * normal_.mean() + (1.0 - weight_) * fat_.mean();
    }

    double variance() const override {
        double m1 = normal_.mean(), m2 = fat_.mean(), m = mean();
        return weight_ * (normal_.variance() + m1 * m1)
             + (1.0 - weight_) * (fat_.variance() + m2 * m2)
             - m * m;
    }

private:
    double weight_;
    NormalDistribution normal_;
    StudentTDistribution fat_;
};

} // namespace stvg::math
