#pragma once

#include <random>
#include <mutex>
#include <cstdint>
#include <sstream>
#include <nlohmann/json.hpp>

namespace stvg::math {

// Thread-safe, deterministically-seeded random number generator.
// Each game session gets a master seed; all randomness derives from it.
class RandomEngine {
public:
    explicit RandomEngine(uint64_t seed = 42) : gen_(seed), seed_(seed) {}

    // Standard normal sample N(0,1)
    double normalSample() {
        std::lock_guard lock(mtx_);
        return normal_(gen_);
    }

    // Normal sample N(mean, stddev)
    double normalSample(double mean, double stddev) {
        return mean + stddev * normalSample();
    }

    // Uniform [0, 1)
    double uniform() {
        std::lock_guard lock(mtx_);
        return uniform_(gen_);
    }

    // Uniform [a, b)
    double uniform(double a, double b) {
        return a + (b - a) * uniform();
    }

    // Student-t with given degrees of freedom
    double studentT(double df) {
        std::lock_guard lock(mtx_);
        std::student_t_distribution<double> dist(df);
        return dist(gen_);
    }

    // Bernoulli trial with probability p
    bool bernoulli(double p) {
        return uniform() < p;
    }

    // Reset to original seed for deterministic replay
    void reset() {
        std::lock_guard lock(mtx_);
        gen_.seed(seed_);
    }

    // Get a derived child seed for sub-systems
    uint64_t deriveSeed() {
        std::lock_guard lock(mtx_);
        std::uniform_int_distribution<uint64_t> dist;
        return dist(gen_);
    }

    uint64_t seed() const { return seed_; }

    nlohmann::json toSaveJson() const {
        std::stringstream ss;
        ss << gen_;
        return {{"seed", seed_}, {"state", ss.str()}};
    }
    void loadSaveJson(const nlohmann::json& j) {
        seed_ = j.value("seed", seed_);
        if (j.contains("state")) {
            std::stringstream ss(j["state"].get<std::string>());
            ss >> gen_;
        }
    }

private:
    std::mt19937_64 gen_;
    uint64_t seed_;
    std::normal_distribution<double> normal_{0.0, 1.0};
    std::uniform_real_distribution<double> uniform_{0.0, 1.0};
    std::mutex mtx_;
};

} // namespace stvg::math
