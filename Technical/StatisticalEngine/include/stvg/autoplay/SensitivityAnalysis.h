#pragma once

#include "GameRunner.h"
#include "EraAwareBots.h"
#include "../core/PersonalityProfile.h"
#include "../core/GameConfig.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <future>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <spdlog/spdlog.h>

namespace stvg::autoplay {

struct AxisResult {
    std::string axisName;
    int axisIndex;
    double survivalLow;   // survival % with axis=0.0
    double survivalHigh;  // survival % with axis=1.0
    double scoreLow;      // avg score with axis=0.0
    double scoreHigh;     // avg score with axis=1.0
    double deltaSurv;     // survHigh - survLow
    double deltaScore;    // scoreHigh - scoreLow
    double absImpact;     // |deltaScore|
};

struct SensitivityConfig {
    GameConfig gameConfig;
    int gamesPerExtreme = 20;
    int maxQuarters = 380;
    bool parallel = false;
    std::string outputDir = "data/autoplay_results";
};

// Run batch for one personality profile, return {survivalPct, avgScore}
inline std::pair<double, double> runProfileBatch(
    const std::string& label,
    const PersonalityProfile& profile,
    const SensitivityConfig& cfg) {

    GameRunner runner;
    PersonalityDrivenBot bot(label, profile);
    auto results = runner.runBatch(bot, cfg.gameConfig, cfg.gamesPerExtreme, 1, cfg.maxQuarters);

    int survived = 0;
    double scoreSum = 0;
    for (const auto& r : results) {
        if (r.survived) survived++;
        scoreSum += r.avgScore;
    }

    double survPct = 100.0 * survived / std::max((int)results.size(), 1);
    double avgScore = scoreSum / std::max((int)results.size(), 1);
    return {survPct, avgScore};
}

inline std::vector<AxisResult> runSensitivity(const SensitivityConfig& cfg) {
    std::vector<AxisResult> results;
    results.reserve(PersonalityProfile::kNumAxes);

    spdlog::info("Running personality sensitivity analysis ({} axes, {} games/extreme, {} quarters)",
        PersonalityProfile::kNumAxes, cfg.gamesPerExtreme, cfg.maxQuarters);

    if (cfg.parallel) {
        // Launch all 22 batches in parallel
        struct AsyncJob {
            int axisIdx;
            double value;
            std::future<std::pair<double, double>> future;
        };
        std::vector<AsyncJob> jobs;
        jobs.reserve(PersonalityProfile::kNumAxes * 2);

        for (int i = 0; i < PersonalityProfile::kNumAxes; ++i) {
            for (double val : {0.0, 1.0}) {
                PersonalityProfile profile; // all 0.5
                profile.setAxis(i, val);
                std::string label = std::string(PersonalityProfile::axisName(i))
                    + (val < 0.5 ? "_LOW" : "_HIGH");

                jobs.push_back({i, val, std::async(std::launch::async,
                    [label, profile, &cfg]() {
                        return runProfileBatch(label, profile, cfg);
                    })});
            }
        }

        // Collect results
        std::vector<std::pair<double, double>> lowResults(PersonalityProfile::kNumAxes);
        std::vector<std::pair<double, double>> highResults(PersonalityProfile::kNumAxes);

        for (auto& job : jobs) {
            auto result = job.future.get();
            if (job.value < 0.5)
                lowResults[job.axisIdx] = result;
            else
                highResults[job.axisIdx] = result;
        }

        for (int i = 0; i < PersonalityProfile::kNumAxes; ++i) {
            AxisResult ar;
            ar.axisName = PersonalityProfile::axisName(i);
            ar.axisIndex = i;
            ar.survivalLow = lowResults[i].first;
            ar.survivalHigh = highResults[i].first;
            ar.scoreLow = lowResults[i].second;
            ar.scoreHigh = highResults[i].second;
            ar.deltaSurv = ar.survivalHigh - ar.survivalLow;
            ar.deltaScore = ar.scoreHigh - ar.scoreLow;
            ar.absImpact = std::abs(ar.deltaScore);
            results.push_back(ar);
        }
    } else {
        // Sequential
        for (int i = 0; i < PersonalityProfile::kNumAxes; ++i) {
            spdlog::info("  Axis {}/{}: {}", i + 1, PersonalityProfile::kNumAxes,
                PersonalityProfile::axisName(i));

            PersonalityProfile lowProfile;
            lowProfile.setAxis(i, 0.0);
            auto [survLow, scoreLow] = runProfileBatch(
                std::string(PersonalityProfile::axisName(i)) + "_LOW", lowProfile, cfg);

            PersonalityProfile highProfile;
            highProfile.setAxis(i, 1.0);
            auto [survHigh, scoreHigh] = runProfileBatch(
                std::string(PersonalityProfile::axisName(i)) + "_HIGH", highProfile, cfg);

            AxisResult ar;
            ar.axisName = PersonalityProfile::axisName(i);
            ar.axisIndex = i;
            ar.survivalLow = survLow;
            ar.survivalHigh = survHigh;
            ar.scoreLow = scoreLow;
            ar.scoreHigh = scoreHigh;
            ar.deltaSurv = survHigh - survLow;
            ar.deltaScore = scoreHigh - scoreLow;
            ar.absImpact = std::abs(ar.deltaScore);
            results.push_back(ar);
        }
    }

    // Sort by absolute score impact (highest first)
    std::sort(results.begin(), results.end(),
        [](const AxisResult& a, const AxisResult& b) {
            return a.absImpact > b.absImpact;
        });

    return results;
}

inline void printSensitivityTable(const std::vector<AxisResult>& results) {
    printf("\nPERSONALITY AXIS SENSITIVITY ANALYSIS\n");
    printf("%-22s %8s %8s %10s %10s %10s %10s\n",
        "Axis", "Surv@0", "Surv@1", "Score@0", "Score@1", "dScore", "|Impact|");
    printf("%-22s %8s %8s %10s %10s %10s %10s\n",
        "----", "------", "------", "-------", "-------", "------", "--------");

    for (const auto& r : results) {
        printf("%-22s %7.1f%% %7.1f%% %10.1f %10.1f %+9.1f %10.1f\n",
            r.axisName.c_str(),
            r.survivalLow, r.survivalHigh,
            r.scoreLow, r.scoreHigh,
            r.deltaScore, r.absImpact);
    }
    printf("\n");
}

inline void writeSensitivityMarkdown(const std::vector<AxisResult>& results,
                                      const SensitivityConfig& cfg,
                                      const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) {
        spdlog::warn("Could not write sensitivity report to {}", path);
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    f << "# Personality Axis Sensitivity Analysis\n\n";
    f << "**Generated**: " << std::ctime(&time);
    f << "**Parameters**: " << cfg.gamesPerExtreme << " games per extreme, "
      << cfg.maxQuarters << " quarters per game\n\n";

    f << "## Results (sorted by score impact)\n\n";
    f << "| Axis | Surv@0 | Surv@1 | Score@0 | Score@1 | dScore | |Impact| |\n";
    f << "|------|--------|--------|---------|---------|--------|----------|\n";

    for (const auto& r : results) {
        char line[256];
        snprintf(line, sizeof(line),
            "| %-20s | %5.1f%% | %5.1f%% | %7.1f | %7.1f | %+6.1f | %8.1f |\n",
            r.axisName.c_str(),
            r.survivalLow, r.survivalHigh,
            r.scoreLow, r.scoreHigh,
            r.deltaScore, r.absImpact);
        f << line;
    }

    f << "\n## Interpretation\n\n";

    // Top 3 most impactful
    f << "**Most impactful axes** (highest |dScore|):\n";
    for (int i = 0; i < std::min(3, (int)results.size()); ++i) {
        const auto& r = results[i];
        f << "1. **" << r.axisName << "** — " << std::fixed << std::setprecision(1)
          << r.absImpact << " score delta ("
          << (r.deltaScore > 0 ? "higher is better" : "lower is better") << ")\n";
    }

    // Bottom 3 least impactful (candidates for removal)
    f << "\n**Least impactful axes** (candidates for simplification):\n";
    for (int i = std::max(0, (int)results.size() - 3); i < (int)results.size(); ++i) {
        const auto& r = results[i];
        f << "- **" << r.axisName << "** — " << std::fixed << std::setprecision(1)
          << r.absImpact << " score delta\n";
    }

    f << "\n---\n*Generated by STVG autoplay --sensitivity*\n";
    spdlog::info("Sensitivity report written to {}", path);
}

} // namespace stvg::autoplay
