#pragma once

#include <random>
#include <mutex>
#include <cstdint>
#include <cmath>
#include <sstream>
#include <nlohmann/json.hpp>

namespace stvg::math {

// Thread-safe, deterministically-seeded random number generator.
// Each game session gets a master seed; all randomness derives from it.
//
// CROSS-PLATFORM DETERMINISM (D1 deployment requirement)
// ──────────────────────────────────────────────────────
// std::mt19937_64 is specified by the C++ standard to produce an identical bit
// stream on every conforming implementation (MSVC, libstdc++, libc++). The
// *distributions* in <random> (std::normal_distribution, std::uniform_real_-
// distribution, std::student_t_distribution, std::uniform_int_distribution) are
// NOT — each STL is free to choose its own algorithm, so the same engine bits
// map to different samples on MSVC vs. libstdc++. Saves move between the dev PC
// (MSVC) and the Carson box (libstdc++) and tests assert day-by-day==batch and
// same-seed==same-run, so the sampling algorithms must be platform-independent.
//
// All sampling below is therefore HAND-ROLLED on top of mt19937_64's raw 64-bit
// output. Given the same engine state every platform now yields the SAME
// sequence of doubles. The mt19937_64 text serialization used by toSaveJson()
// is itself standard-mandated and portable, so a save written on either OS
// resumes identically on the other.
class RandomEngine {
public:
    explicit RandomEngine(uint64_t seed = 42) : gen_(seed), seed_(seed) {}

    // Canonical uniform on [0, 1) from the top 53 bits of one engine draw.
    // Portable: std::mt19937_64::operator() is standard-fixed across STLs and
    // the bit→double mapping here is fixed too. (Replaces std::uniform_real_-
    // distribution, whose algorithm is implementation-defined.)
    double uniform() {
        std::lock_guard lock(mtx_);
        return canonicalUniform();
    }

    // Uniform [a, b)
    double uniform(double a, double b) {
        return a + (b - a) * uniform();
    }

    // Standard normal sample N(0,1) via the basic (trig) Box–Muller transform.
    // Deterministic and platform-independent. One cached spare value per two
    // draws keeps it efficient. (Replaces std::normal_distribution.)
    double normalSample() {
        std::lock_guard lock(mtx_);
        return normalLocked();
    }

    // Normal sample N(mean, stddev)
    double normalSample(double mean, double stddev) {
        return mean + stddev * normalSample();
    }

    // Student-t with given degrees of freedom (df may be non-integer).
    // Bailey's (1994) polar rejection method generates a t-variate directly
    // from two uniforms with no chi-squared / gamma dependency — fully portable.
    // (Replaces std::student_t_distribution.)
    double studentT(double df) {
        std::lock_guard lock(mtx_);
        double u, v, w;
        do {
            u = 2.0 * canonicalUniform() - 1.0;   // U(-1,1)
            v = 2.0 * canonicalUniform() - 1.0;   // U(-1,1)
            w = u * u + v * v;
        } while (w > 1.0 || w == 0.0);
        double c2 = u * u / w;
        double r2 = df * (std::pow(w, -2.0 / df) - 1.0);
        double t = std::sqrt(r2 * c2);
        return (v < 0.0) ? -t : t;
    }

    // Bernoulli trial with probability p
    bool bernoulli(double p) {
        return uniform() < p;
    }

    // Reset to original seed for deterministic replay
    void reset() {
        std::lock_guard lock(mtx_);
        gen_.seed(seed_);
        hasSpare_ = false;
        spare_ = 0.0;
    }

    // Get a derived child seed for sub-systems. Uses the engine's raw 64-bit
    // output directly (full range, uniform, portable) — replaces
    // std::uniform_int_distribution<uint64_t>.
    uint64_t deriveSeed() {
        std::lock_guard lock(mtx_);
        return gen_();
    }

    uint64_t seed() const { return seed_; }

    nlohmann::json toSaveJson() const {
        std::lock_guard lock(mtx_);
        std::stringstream ss;
        ss << gen_;
        return {{"seed", seed_}, {"state", ss.str()},
                {"hasSpare", hasSpare_}, {"spare", spare_}};
    }
    void loadSaveJson(const nlohmann::json& j) {
        std::lock_guard lock(mtx_);
        seed_ = j.value("seed", seed_);
        if (j.contains("state")) {
            std::stringstream ss(j["state"].get<std::string>());
            ss >> gen_;
        }
        hasSpare_ = j.value("hasSpare", false);
        spare_ = j.value("spare", 0.0);
    }

private:
    // [0,1) from the top 53 bits of one engine word. Caller holds mtx_.
    double canonicalUniform() {
        // 2^-53; multiplying a 53-bit integer by this gives a value in [0,1).
        constexpr double kInv53 = 1.0 / 9007199254740992.0; // 1 / 2^53
        return static_cast<double>(gen_() >> 11) * kInv53;
    }

    // Box–Muller N(0,1). Caller holds mtx_.
    double normalLocked() {
        if (hasSpare_) {
            hasSpare_ = false;
            return spare_;
        }
        // Guard u1 away from 0 so log() is finite.
        double u1, u2;
        do {
            u1 = canonicalUniform();
        } while (u1 <= 0.0);
        u2 = canonicalUniform();
        double mag = std::sqrt(-2.0 * std::log(u1));
        constexpr double kTwoPi = 6.283185307179586476925286766559;
        spare_ = mag * std::sin(kTwoPi * u2);
        hasSpare_ = true;
        return mag * std::cos(kTwoPi * u2);
    }

    std::mt19937_64 gen_;
    uint64_t seed_;
    bool hasSpare_ = false;   // Box–Muller spare-value cache
    double spare_ = 0.0;
    mutable std::mutex mtx_;
};

} // namespace stvg::math
