#include <gtest/gtest.h>
#include <stvg/math/RandomEngine.h>
#include <stvg/math/Distributions.h>
#include <stvg/math/GARCH.h>
#include <cmath>
#include <numeric>
#include <algorithm>

using namespace stvg::math;

// ── RandomEngine Tests ──────────────────────────────────────────────

TEST(RandomEngine, DeterministicWithSameSeed) {
    RandomEngine rng1(123);
    RandomEngine rng2(123);
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(rng1.uniform(), rng2.uniform());
    }
}

TEST(RandomEngine, DifferentSeeds) {
    RandomEngine rng1(1);
    RandomEngine rng2(2);
    // At least one of 10 samples should differ
    bool anyDifferent = false;
    for (int i = 0; i < 10; ++i) {
        if (rng1.uniform() != rng2.uniform()) anyDifferent = true;
    }
    EXPECT_TRUE(anyDifferent);
}

TEST(RandomEngine, NormalSampleMeanApproxZero) {
    RandomEngine rng(42);
    double sum = 0;
    int N = 100000;
    for (int i = 0; i < N; ++i) sum += rng.normalSample();
    double mean = sum / N;
    EXPECT_NEAR(mean, 0.0, 0.02); // Within ~2 SE
}

// ── Normal Distribution Tests ───────────────────────────────────────

TEST(NormalDistribution, PDFAtMean) {
    NormalDistribution n(0, 1);
    double expected = 1.0 / std::sqrt(2.0 * M_PI);
    EXPECT_NEAR(n.pdf(0.0), expected, 1e-10);
}

TEST(NormalDistribution, CDFSymmetry) {
    NormalDistribution n(0, 1);
    EXPECT_NEAR(n.cdf(0.0), 0.5, 1e-10);
    EXPECT_NEAR(n.cdf(-1.96) + (1.0 - n.cdf(1.96)), 0.05, 0.001);
}

TEST(NormalDistribution, SampleMeanAndVariance) {
    RandomEngine rng(42);
    NormalDistribution n(5.0, 2.0);
    int N = 100000;
    std::vector<double> samples(N);
    for (auto& s : samples) s = n.sample(rng);

    double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / N;
    double var = 0;
    for (auto s : samples) var += (s - mean) * (s - mean);
    var /= N;

    EXPECT_NEAR(mean, 5.0, 0.05);
    EXPECT_NEAR(var, 4.0, 0.1);
}

TEST(NormalDistribution, Calibrate) {
    RandomEngine rng(42);
    NormalDistribution n(0, 1);
    std::vector<double> data;
    for (int i = 0; i < 10000; ++i) data.push_back(rng.normalSample(3.0, 1.5));

    n.calibrate(data);
    EXPECT_NEAR(n.mean(), 3.0, 0.05);
    EXPECT_NEAR(std::sqrt(n.variance()), 1.5, 0.05);
}

// ── Student-t Distribution Tests ────────────────────────────────────

TEST(StudentT, FatterTailsThanNormal) {
    RandomEngine rng(42);
    StudentTDistribution t(4.0);
    NormalDistribution n(0, 1);
    int N = 100000;

    int tExtremes = 0, nExtremes = 0;
    for (int i = 0; i < N; ++i) {
        if (std::abs(t.sample(rng)) > 3.0) ++tExtremes;
        if (std::abs(n.sample(rng)) > 3.0) ++nExtremes;
    }

    // Student-t(4) should have significantly more extreme events
    EXPECT_GT(tExtremes, nExtremes * 2);
}

TEST(StudentT, CDFAtZero) {
    StudentTDistribution t(4.0);
    EXPECT_NEAR(t.cdf(0.0), 0.5, 0.01);
}

// ── GARCH(1,1) Tests ────────────────────────────────────────────────

TEST(GARCH, StationarityCheck) {
    EXPECT_TRUE(GARCHParams::Normal().isStationary());
    EXPECT_TRUE(GARCHParams::Crisis().isStationary());
    EXPECT_TRUE(GARCHParams::Stressed().isStationary());

    GARCHParams unstable{0.001, 0.5, 0.6}; // alpha+beta=1.1
    EXPECT_FALSE(unstable.isStationary());
}

TEST(GARCH, VolatilityClustering) {
    RandomEngine rng(42);
    GARCHProcess garch(GARCHParams::Normal());

    // Run 1000 days
    std::vector<double> vols;
    for (int i = 0; i < 1000; ++i) {
        garch.step(rng);
        vols.push_back(garch.currentVolatility());
    }

    // Volatility should show autocorrelation (clustering)
    // Simple test: correlation between consecutive vols should be high
    double meanVol = std::accumulate(vols.begin(), vols.end(), 0.0) / vols.size();
    double cov = 0, var = 0;
    for (size_t i = 1; i < vols.size(); ++i) {
        cov += (vols[i] - meanVol) * (vols[i-1] - meanVol);
        var += (vols[i] - meanVol) * (vols[i] - meanVol);
    }
    double autocorr = cov / var;
    EXPECT_GT(autocorr, 0.8); // Strong persistence
}

TEST(GARCH, CrisisParamsHigherVol) {
    RandomEngine rng1(42), rng2(42);
    GARCHProcess normal(GARCHParams::Normal());
    GARCHProcess crisis(GARCHParams::Crisis());

    double sumVolNormal = 0, sumVolCrisis = 0;
    for (int i = 0; i < 500; ++i) {
        normal.step(rng1);
        crisis.step(rng2);
        sumVolNormal += normal.currentVolatility();
        sumVolCrisis += crisis.currentVolatility();
    }

    // Crisis parameters should produce higher average volatility
    EXPECT_GT(sumVolCrisis / 500, sumVolNormal / 500);
}

// ── Mixture Distribution Tests ──────────────────────────────────────

TEST(Mixture, HasFatTails) {
    RandomEngine rng(42);
    // Default: 95% Normal(0, 0.01) + 5% t(4, 0, 0.03)
    MixtureDistribution mix;
    int N = 100000;
    int extremes = 0;
    for (int i = 0; i < N; ++i) {
        if (std::abs(mix.sample(rng)) > 0.03) ++extremes;
    }
    // With fat-tailed component, should have significant extreme events
    EXPECT_GT(extremes, N / 50); // >2000 events beyond 3 sigma of normal component
}
