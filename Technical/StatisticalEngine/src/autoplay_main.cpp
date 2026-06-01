#include <stvg/autoplay/GameRunner.h>
#include <stvg/autoplay/BotStrategy.h>
#include <stvg/autoplay/EraAwareBots.h>
#include <stvg/autoplay/Telemetry.h>
#include <stvg/autoplay/SensitivityAnalysis.h>
#include <stvg/core/GameConfig.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <chrono>
#include <future>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <numeric>

using namespace stvg;
using namespace stvg::autoplay;

void printHeader(int totalGames, int numStrategies, double totalTimeSec) {
    std::cout << "\n"
        << "═══════════════════════════════════════════════════════════════════\n"
        << "  STVG AUTOPLAY RESULTS — " << totalGames << " games across " << numStrategies << " strategies\n"
        << "  Total time: " << std::fixed << std::setprecision(1) << totalTimeSec << " seconds ("
        << std::setprecision(0) << (totalGames / totalTimeSec) << " games/second)\n"
        << "═══════════════════════════════════════════════════════════════════\n";
}

void printStrategyTable(const std::vector<BatchResult>& results) {
    std::cout << "\nSTRATEGY COMPARISON:\n";
    std::cout << "┌──────────────────┬────────┬──────────┬──────────┬────────┬─────────┐\n";
    std::cout << "│ Strategy         │ Surv%  │ Avg Score│ Avg Cap  │ Crises │ Avg Turn│\n";
    std::cout << "├──────────────────┼────────┼──────────┼──────────┼────────┼─────────┤\n";

    for (const auto& r : results) {
        std::string name = r.strategyName;
        if (name.length() > 16) name = name.substr(0, 16);
        while (name.length() < 16) name += " ";

        std::cout << "│ " << name << " │ "
                  << std::setw(5) << std::setprecision(1) << (r.survivalRate * 100) << "% │ "
                  << std::setw(8) << std::setprecision(1) << r.avgScore << " │ "
                  << std::setw(4) << std::setprecision(1) << (r.avgCapital / 1e9) << "B"
                  << std::setw(4) << " │ "
                  << std::setw(5) << std::setprecision(1) << r.avgCrises << " │ "
                  << std::setw(5) << std::setprecision(1) << r.avgTurnTimeMs << "ms │\n";
    }
    std::cout << "└──────────────────┴────────┴──────────┴──────────┴────────┴─────────┘\n";
}

// Phase 3.3 B4: where did each bot die?
void printDeathTable(const std::vector<BatchResult>& results) {
    bool anyDeaths = false;
    for (const auto& r : results) if (r.deathCount > 0) { anyDeaths = true; break; }
    if (!anyDeaths) return;

    std::cout << "\nDEATH ANALYSIS (non-survivors only):\n";
    std::cout << "┌──────────────────┬────────┬───────────┬──────────┬──────────────────────┐\n";
    std::cout << "│ Strategy         │ Deaths │ Med Death │ Avg Lev  │ Most common reason   │\n";
    std::cout << "├──────────────────┼────────┼───────────┼──────────┼──────────────────────┤\n";

    for (const auto& r : results) {
        if (r.deathCount == 0) continue;
        std::string name = r.strategyName;
        if (name.length() > 16) name = name.substr(0, 16);
        while (name.length() < 16) name += " ";

        std::string reason = r.mostCommonEndReason;
        if (reason.length() > 20) reason = reason.substr(0, 20);
        while (reason.length() < 20) reason += " ";

        std::cout << "│ " << name << " │ "
                  << std::setw(6) << r.deathCount << " │ "
                  << "Q" << std::setw(8) << std::left << r.medianDeathQuarter << std::right << "│ "
                  << std::setw(7) << std::fixed << std::setprecision(1) << r.avgLeverageAtDeath << "x │ "
                  << reason << " │\n";
    }
    std::cout << "└──────────────────┴────────┴───────────┴──────────┴──────────────────────┘\n";
}

void printHealthCheck(const std::vector<BatchResult>& results, int totalGames) {
    int totalNaN = 0, totalStuck = 0, totalBankrupt = 0;
    for (const auto& r : results) {
        totalNaN += r.nanCount;
        totalStuck += r.stuckCount;
        totalBankrupt += r.bankruptcyCount;
    }

    std::cout << "\nHEALTH CHECK:\n";
    std::cout << "  NaN/Inf detected: " << totalNaN << " / " << totalGames << " games "
              << (totalNaN == 0 ? "✓" : "✗ ISSUE") << "\n";
    std::cout << "  Stuck states:     " << totalStuck << " / " << totalGames << " ("
              << std::setprecision(2) << (100.0 * totalStuck / totalGames) << "%) "
              << (totalStuck < totalGames * 0.05 ? "✓" : "⚠") << "\n";
    std::cout << "  Bankruptcies:     " << totalBankrupt << " / " << totalGames << " ("
              << std::setprecision(1) << (100.0 * totalBankrupt / totalGames) << "%)\n";
}

void printBalanceAnalysis(const std::vector<BatchResult>& results) {
    if (results.empty()) return;

    // Find dominant strategy
    auto best = std::max_element(results.begin(), results.end(),
        [](const BatchResult& a, const BatchResult& b) { return a.avgScore < b.avgScore; });

    double secondBest = 0;
    for (const auto& r : results) {
        if (&r != &(*best) && r.avgScore > secondBest) secondBest = r.avgScore;
    }

    double advantage = best->avgScore - secondBest;
    bool dominant = advantage > 2.0 * best->stdDevScore;

    std::cout << "\nBALANCE ANALYSIS:\n";
    std::cout << "  Dominant strategy: " << best->strategyName
              << " (" << std::setprecision(1) << best->avgScore
              << " avg, +" << std::setprecision(1) << advantage << " vs next)\n";
    std::cout << "  Dominance: " << (dominant ? "STRONG ✗ (> 2 std devs)" : "MARGINAL ✓ (< 2 std devs)") << "\n";
}

void printPerformance(const std::vector<BatchResult>& results) {
    double maxP95 = 0, maxP99 = 0;
    for (const auto& r : results) {
        if (r.p95TurnTimeMs > maxP95) maxP95 = r.p95TurnTimeMs;
        if (r.maxTurnTimeMs > maxP99) maxP99 = r.maxTurnTimeMs;
    }

    std::cout << "\nPERFORMANCE:\n";
    std::cout << "  Avg turn time: " << std::setprecision(1) << results[0].avgTurnTimeMs << "ms"
              << " (P95: " << maxP95 << "ms, Max: " << maxP99 << "ms)\n";
}

void writeTimeSeries(const std::vector<GameSummary>& games, const std::string& strategyName,
                     const std::string& outputDir) {
    for (const auto& g : games) {
        std::string path = outputDir + "/ts_" + strategyName + "_seed" + std::to_string(g.seed) + ".csv";
        std::ofstream f(path);
        f << "quarter,year,era,capital,totalAssets,revenue,costs,netIncome,reputation,"
             "capitalRatio,hiddenRisk,visibilityPct,score,xp,spxPrice,vix,"
             "fedFundsRate,activeCrises,decisionsResolved,competitorCount,eraTransitioned\n";
        for (const auto& t : g.turnHistory) {
            f << t.quarter << "," << t.year << ",\"" << t.currentEra << "\","
              << t.capital << "," << t.totalAssets << ","
              << t.revenue << "," << t.costs << "," << t.netIncome << ","
              << t.reputation << "," << t.capitalRatio << "," << t.hiddenRisk << ","
              << t.visibilityPct << "," << t.score << "," << t.xpEarned << ","
              << t.spxPrice << "," << t.vix << "," << t.fedFundsRate << ","
              << t.activeCrises << "," << t.decisionsResolved << ","
              << t.competitorCount << "," << (t.eraTransitioned ? 1 : 0) << "\n";
        }
    }
}

void writeCSV(const std::vector<BatchResult>& results, const std::string& path) {
    std::ofstream f(path);
    f << "strategy,games,survived,survivalRate,avgScore,medianScore,stdDev,avgCapital,"
         "avgLevel,avgCrises,avgTurnMs,maxTurnMs,nanCount,stuckCount,bankruptcies\n";
    for (const auto& r : results) {
        f << r.strategyName << "," << r.gamesPlayed << "," << r.gamesSurvived << ","
          << r.survivalRate << "," << r.avgScore << "," << r.medianScore << ","
          << r.stdDevScore << "," << r.avgCapital << "," << r.avgLevel << ","
          << r.avgCrises << "," << r.avgTurnTimeMs << "," << r.maxTurnTimeMs << ","
          << r.nanCount << "," << r.stuckCount << "," << r.bankruptcyCount << "\n";
    }
}

// Phase 3.4 C3: parse a --sweep spec like "balance.survivalOverrideCapRatio=0.04,0.05,0.06"
// into (key, [values]). Returns false on parse error.
static bool parseSweepSpec(const std::string& spec,
                           std::string& outKey,
                           std::vector<double>& outValues) {
    size_t eq = spec.find('=');
    if (eq == std::string::npos) return false;
    std::string lhs = spec.substr(0, eq);
    // Strip optional "balance." prefix
    size_t dot = lhs.find('.');
    outKey = (dot != std::string::npos) ? lhs.substr(dot + 1) : lhs;
    std::stringstream ss(spec.substr(eq + 1));
    std::string val;
    while (std::getline(ss, val, ',')) {
        if (!val.empty()) {
            try { outValues.push_back(std::stod(val)); }
            catch (...) { return false; }
        }
    }
    return !outValues.empty();
}

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::warn); // Quiet — only warnings/errors

    int gamesPerStrategy = 200;
    int maxQuarters = 12;
    std::string outputDir = "data/autoplay_results";
    bool fullMatrix = false;
    bool quickMatrix = false;
    bool timeSeries = false;
    bool parallel = false;
    bool sensitivity = false;
    std::string sweepKey;
    std::vector<double> sweepValues;
    std::string restrictLabel;
    GameConfig config = GameConfig::Normal();

    // Simple arg parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--games" && i + 1 < argc) gamesPerStrategy = std::stoi(argv[++i]);
        else if (arg == "--quarters" && i + 1 < argc) maxQuarters = std::stoi(argv[++i]);
        else if (arg == "--output" && i + 1 < argc) outputDir = argv[++i];
        else if (arg == "--full-matrix") { fullMatrix = true; gamesPerStrategy = 10; maxQuarters = 380; }
        else if (arg == "--quick-matrix") { quickMatrix = true; gamesPerStrategy = 3; maxQuarters = 100; }
        else if (arg == "--time-series") { timeSeries = true; }
        else if (arg == "--parallel") { parallel = true; }
        else if (arg == "--sweep" && i + 1 < argc) {
            if (!parseSweepSpec(argv[++i], sweepKey, sweepValues)) {
                std::cerr << "ERROR: --sweep requires format key=v1,v2,v3\n";
                return 1;
            }
        }
        else if (arg == "--restrict" && i + 1 < argc) {
            std::string rval = argv[++i];
            if (rval == "no-deleveraging") { config.restrictions.noDeleveraging = true; restrictLabel += "no-deleveraging "; }
            else if (rval == "no-crises") { config.restrictions.noCrises = true; restrictLabel += "no-crises "; }
            else if (rval.rfind("ceo=", 0) == 0) { config.restrictions.forceCeoId = rval.substr(4); restrictLabel += rval + " "; }
            else { std::cerr << "ERROR: unknown --restrict value: " << rval << "\n"; return 1; }
        }
        else if (arg == "--sensitivity") { sensitivity = true; }
        else if (arg == "--help") {
            std::cout << "Usage: stvg_autoplay [--games N] [--quarters N] [--output dir] [--full-matrix] [--quick-matrix] [--sweep KEY=v1,v2,v3] [--time-series]\n";
            std::cout << "  --full-matrix:  12 bots x 10 seeds x 380 quarters (full 95-year games, ~10 min)\n";
            std::cout << "  --quick-matrix: 12 bots x 3 seeds x 100 quarters (~40 sec, for tuning)\n";
            std::cout << "  --sweep:        run matrix once per parameter value, e.g.\n";
            std::cout << "                  --sweep balance.survivalOverrideCapRatio=0.04,0.05,0.06 --quick-matrix\n";
            std::cout << "                  Tunable keys: survivalOverrideCapRatio, survivalReleaseCapRatio,\n";
            std::cout << "                                dangerZoneCapRatio, dividendOfferCapRatio\n";
            std::cout << "  --parallel:     run bot batches concurrently via std::async (4-6x speedup)\n";
            std::cout << "  --time-series:  Export per-turn telemetry CSV for each game\n";
            std::cout << "  --restrict X:   diagnostic mode — run baseline then restricted, print delta\n";
            std::cout << "                  no-deleveraging: skip all deleveraging decisions\n";
            std::cout << "                  no-crises:       block all crisis generation\n";
            std::cout << "                  ceo=KEY:         force all bots to play as CEO (e.g., ceo=volcker)\n";
            std::cout << "  --sensitivity:  sweep each personality axis through 0/1, report score impact\n";
            return 0;
        }
    }

    // ── Sensitivity mode: run and exit early ─────────────────────
    if (sensitivity) {
        std::cout << "STVG Autoplay Engine v1.1 — SENSITIVITY ANALYSIS\n\n";

        SensitivityConfig sensCfg;
        sensCfg.gameConfig = config;
        sensCfg.gamesPerExtreme = gamesPerStrategy;
        sensCfg.maxQuarters = maxQuarters;
        sensCfg.parallel = parallel;
        sensCfg.outputDir = outputDir;

        auto results = runSensitivity(sensCfg);
        printSensitivityTable(results);

        // Write markdown report
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time);
        char dateBuf[32];
        std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d", &tm);
        std::string mdPath = outputDir + "/PERSONALITY_SENSITIVITY_" + std::string(dateBuf) + ".md";
        writeSensitivityMarkdown(results, sensCfg, mdPath);

        return 0;
    }

    std::cout << "STVG Autoplay Engine v1.1\n";
    std::cout << "Games per strategy: " << gamesPerStrategy << " | Quarters: " << maxQuarters << "\n\n";

    // Create strategies — all bots are now PersonalityDrivenBot variants.
    // The legacy hardcoded BotStrategy/EraAwareBot classes are still defined
    // (used by tests) but autoplay drives every variant from a personality vector.
    std::vector<std::unique_ptr<BotStrategy>> strategies;
    auto add = [&](const std::string& name, const PersonalityProfile& p) {
        strategies.push_back(std::make_unique<PersonalityDrivenBot>(name, p));
    };
    add("Random",       PersonalityProfile::random());
    add("Utility",      PersonalityProfile::utility());
    add("Conservative", PersonalityProfile::conservative());
    add("Aggressive",   PersonalityProfile::aggressive());
    add("Balanced",     PersonalityProfile::balanced());
    add("Gambler",      PersonalityProfile::gambler());
    add("RegulatorsPet",PersonalityProfile::regulatorsPet());

    if (fullMatrix || quickMatrix || maxQuarters > 40) {
        add("Historical", PersonalityProfile::historical());
        add("Speedrun",   PersonalityProfile::speedrun());
        add("Survivor",   PersonalityProfile::survivor());
        add("Contrary",   PersonalityProfile::contrary());
        add("Political",  PersonalityProfile::political());
    }

    GameRunner runner;
    std::vector<BatchResult> allResults;

    auto totalStart = std::chrono::high_resolution_clock::now();
    int totalGames = 0;

    // Store all game results for optional time-series/full-matrix export
    std::vector<std::pair<std::string, std::vector<GameSummary>>> allGames;

    // Helper: run all strategies once with the current balance config.
    // Used both by single-config runs and by the sweep loop.
    // Phase 3.4 C4: --parallel runs each bot batch concurrently via std::async.
    // Each batch is independent (stateless GameRunner, separate BotStrategy
    // instances, kBalance is read-only) so no locking is needed for the games
    // themselves. We only collect results after all futures complete.
    auto runMatrix = [&](std::vector<BatchResult>& outResults,
                         std::vector<std::pair<std::string, std::vector<GameSummary>>>& outAllGames) {
        if (parallel) {
            std::cout << "Running " << strategies.size() << " strategies in parallel...\n";
            auto start = std::chrono::high_resolution_clock::now();

            std::vector<std::future<std::vector<GameSummary>>> futures;
            futures.reserve(strategies.size());
            for (auto& strategy : strategies) {
                BotStrategy* strat = strategy.get();
                futures.push_back(std::async(std::launch::async, [strat, &runner, &config, gamesPerStrategy, maxQuarters]() {
                    return runner.runBatch(*strat, config, gamesPerStrategy, 1, maxQuarters);
                }));
            }
            for (size_t i = 0; i < futures.size(); ++i) {
                auto games = futures[i].get();
                auto result = BatchResult::compute(games);
                outResults.push_back(result);
                totalGames += gamesPerStrategy;
                std::cout << "  " << strategies[i]->name() << ": "
                          << std::fixed << std::setprecision(1)
                          << "Surv " << (result.survivalRate * 100) << "% "
                          << "Score " << result.avgScore << "\n";
                if (timeSeries || fullMatrix) {
                    outAllGames.push_back({strategies[i]->name(), std::move(games)});
                }
            }

            auto end = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(end - start).count();
            std::cout << "Parallel matrix completed in " << std::fixed << std::setprecision(1)
                      << elapsed << "s\n";
        } else {
            for (auto& strategy : strategies) {
                std::cout << "Running " << strategy->name() << " (" << gamesPerStrategy << " games)... ";
                std::cout.flush();

                auto start = std::chrono::high_resolution_clock::now();
                auto games = runner.runBatch(*strategy, config, gamesPerStrategy, 1, maxQuarters);
                auto end = std::chrono::high_resolution_clock::now();
                double elapsed = std::chrono::duration<double>(end - start).count();

                auto result = BatchResult::compute(games);
                outResults.push_back(result);
                totalGames += gamesPerStrategy;

                std::cout << std::fixed << std::setprecision(1)
                          << elapsed << "s (" << (gamesPerStrategy / elapsed) << " games/sec) "
                          << "Surv: " << (result.survivalRate * 100) << "% "
                          << "Score: " << result.avgScore << "\n";

                if (timeSeries || fullMatrix) {
                    outAllGames.push_back({strategy->name(), std::move(games)});
                }
            }
        }
    };

    // Phase 3.4 C3: Sweep loop. If --sweep was given, run the matrix once per
    // override value and aggregate. Otherwise, run once normally.
    struct SweepRow {
        double value;
        double avgSurvival;
        double avgScore;
        double scoreSpread;     // max - min across bots
        int    medianDeathQ;
    };
    std::vector<SweepRow> sweepRows;

    // ── Restriction delta mode ─────────────────────────────────────
    // When --restrict is active, run baseline (no restrictions) then restricted,
    // print both tables plus a delta comparison.
    std::vector<BatchResult> baselineResults;
    if (config.restrictions.hasAny()) {
        std::cout << "RESTRICTION MODE: " << restrictLabel << "\n";
        std::cout << "Running baseline (no restrictions)...\n";

        // Save restrictions, run baseline without them
        auto savedRestrictions = config.restrictions;
        config.restrictions = {};
        std::vector<std::pair<std::string, std::vector<GameSummary>>> baseGames;
        runMatrix(baselineResults, baseGames);
        config.restrictions = savedRestrictions;

        std::cout << "\nRunning restricted (" << restrictLabel << ")...\n";
    }

    if (!sweepValues.empty()) {
        std::cout << "SWEEP MODE: " << sweepKey << " ∈ {";
        for (size_t i = 0; i < sweepValues.size(); ++i) {
            std::cout << sweepValues[i] << (i + 1 < sweepValues.size() ? ", " : "");
        }
        std::cout << "}\n\n";

        for (double v : sweepValues) {
            balanceOverride::values[sweepKey] = v;
            std::cout << "─── " << sweepKey << " = " << v << " ───\n";

            std::vector<BatchResult> rowResults;
            std::vector<std::pair<std::string, std::vector<GameSummary>>> rowGames;
            runMatrix(rowResults, rowGames);

            // Aggregate row stats
            double sumSurv = 0, sumScore = 0;
            double minScore = 1e18, maxScore = -1e18;
            std::vector<int> deathQuarters;
            for (const auto& r : rowResults) {
                sumSurv += r.survivalRate;
                sumScore += r.avgScore;
                if (r.avgScore < minScore) minScore = r.avgScore;
                if (r.avgScore > maxScore) maxScore = r.avgScore;
                if (r.medianDeathQuarter >= 0) deathQuarters.push_back(r.medianDeathQuarter);
            }
            int medQ = -1;
            if (!deathQuarters.empty()) {
                std::sort(deathQuarters.begin(), deathQuarters.end());
                medQ = deathQuarters[deathQuarters.size() / 2];
            }
            sweepRows.push_back({
                v,
                rowResults.empty() ? 0.0 : sumSurv / rowResults.size(),
                rowResults.empty() ? 0.0 : sumScore / rowResults.size(),
                maxScore - minScore,
                medQ
            });

            // Use the LAST sweep iteration's results for the printed tables below
            allResults = std::move(rowResults);
            allGames = std::move(rowGames);
        }
        balanceOverride::clear();
    } else {
        runMatrix(allResults, allGames);
    }

    auto totalEnd = std::chrono::high_resolution_clock::now();
    double totalSec = std::chrono::duration<double>(totalEnd - totalStart).count();

    // Print full report
    printHeader(totalGames, (int)strategies.size(), totalSec);
    printStrategyTable(allResults);
    printDeathTable(allResults);
    printBalanceAnalysis(allResults);
    printHealthCheck(allResults, totalGames);
    printPerformance(allResults);

    // Sweep summary table (Phase 3.4 C3)
    if (!sweepRows.empty()) {
        std::cout << "\nSWEEP SUMMARY (" << sweepKey << "):\n";
        std::cout << "┌────────────┬──────────┬───────────┬────────────┬───────────┐\n";
        std::cout << "│ Value      │ Avg Surv │ Avg Score │ Score Spread│ Med Death │\n";
        std::cout << "├────────────┼──────────┼───────────┼────────────┼───────────┤\n";
        for (const auto& row : sweepRows) {
            std::cout << "│ " << std::setw(10) << std::fixed << std::setprecision(4) << row.value << " │ "
                      << std::setw(7) << std::setprecision(1) << (row.avgSurvival * 100) << "% │ "
                      << std::setw(9) << std::setprecision(1) << row.avgScore << " │ "
                      << std::setw(10) << std::setprecision(2) << row.scoreSpread << " │ "
                      << "Q" << std::setw(8) << std::left << row.medianDeathQ << std::right << " │\n";
        }
        std::cout << "└────────────┴──────────┴───────────┴────────────┴───────────┘\n";
    }

    // Restriction delta table
    if (!baselineResults.empty() && baselineResults.size() == allResults.size()) {
        std::cout << "\nRESTRICTION DELTA (" << restrictLabel << "vs baseline):\n";
        printf("%-18s %10s %10s %10s\n", "Strategy", "dSurv%", "dScore", "dCrises");
        printf("%-18s %10s %10s %10s\n", "--------", "------", "------", "-------");
        for (size_t i = 0; i < allResults.size(); ++i) {
            double dSurv = (allResults[i].survivalRate - baselineResults[i].survivalRate) * 100;
            double dScore = allResults[i].avgScore - baselineResults[i].avgScore;
            double dCrises = allResults[i].avgCrises - baselineResults[i].avgCrises;
            std::string name = allResults[i].strategyName;
            if (name.length() > 16) name = name.substr(0, 16);
            printf("%-18s %+9.1f%% %+9.1f %+9.1f\n",
                name.c_str(), dSurv, dScore, dCrises);
        }
        std::cout << "\n";
    }

    // Write CSV
    writeCSV(allResults, outputDir + "/strategy_comparison.csv");
    std::cout << "\nCSV written to: " << outputDir << "/strategy_comparison.csv\n";

    // Full matrix: write per-game detail CSV (uses stored games — no re-run)
    if (fullMatrix && !allGames.empty()) {
        std::ofstream detail(outputDir + "/full_matrix.csv");
        detail << "strategy,seed,quartersPlayed,survived,finalCapital,peakCapital,"
                  "finalScore,avgScore,erasReached,finalEra,gameEndReason,"
                  "crisesEncountered,aiSingularity,climateTippingPoints\n";
        std::cout << "\nWriting per-game detail...\n";
        for (const auto& [name, games] : allGames) {
            for (const auto& g : games) {
                detail << name << "," << g.seed << ","
                       << g.quartersPlayed << "," << (g.survived ? 1 : 0) << ","
                       << g.finalCapital << "," << g.peakCapital << ","
                       << g.finalScore << "," << g.avgScore << ","
                       << g.erasReached << ",\"" << g.finalEra << "\","
                       << "\"" << g.gameEndReason << "\","
                       << g.crisesEncountered << ","
                       << (g.aiSingularityOccurred ? 1 : 0) << ","
                       << g.climateTippingPoints << "\n";
            }
        }
        std::cout << "Full matrix CSV written to: " << outputDir << "/full_matrix.csv\n";
    }

    // Time series: write per-turn telemetry CSV for each game
    if (timeSeries && !allGames.empty()) {
        std::cout << "\nWriting time-series CSVs...\n";
        int tsCount = 0;
        for (const auto& [name, games] : allGames) {
            writeTimeSeries(games, name, outputDir);
            tsCount += (int)games.size();
        }
        std::cout << "Wrote " << tsCount << " time-series CSVs to: " << outputDir << "/\n";
    }

    return 0;
}
