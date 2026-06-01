#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <cmath>
#include <spdlog/spdlog.h>

namespace stvg {

// Master configuration struct for the entire game.
// Three-tier system: C++ defaults → JSON file override → REST API override.
// All adjustable game parameters live here.
struct GameConfig {

    // ── Timing & Pacing ───────────────────────────────────────
    struct Timing {
        uint64_t seed = 42;
        int startYear = 1945;
        int startQuarter = 1;
        int quarterDurationDays = 63;  // Trading days per quarter (63 ≈ 252/4)
        double ticksPerSecond = 2.0;   // Simulation ticks per real second
        double daysPerTick = 1.0;      // Simulated days per tick
        int quartersPerGame = 380;     // ~95 years × 4 quarters (1945-2040)
    } timing;

    // ── Initial Economic State ────────────────────────────────
    struct InitialEconomy {
        double fedFundsRate = 0.025;    // 1945: post-war low rates
        double cpiInflation = 0.02;     // 1945: moderate inflation
        double gdpGrowth = 0.04;        // 1945: post-war boom
        double unemployment = 0.035;    // 1945: low unemployment
        double vix = 12.0;             // 1945: calm markets
        double treasuryYield10Y = 0.025; // 1945: low yields
        double creditSpread = 0.005;    // 1945: tight spreads
    } initialEconomy;

    // ── Economic Engine (OU Process Parameters) ───────────────
    struct OUParams {
        double mu, theta, sigma, floor, ceiling;
    };
    struct EconomyParams {
        // Default OU targets — set for Post-War era (1945-1959)
        // EraEngine overrides these when eras change
        OUParams fedFunds    = {0.02,  0.05, 0.003, 0.0,   0.20};
        OUParams inflation   = {0.02,  0.10, 0.002, -0.05, 0.15};
        OUParams gdpGrowth   = {0.04,  0.08, 0.003, -0.10, 0.10};
        OUParams unemployment= {0.035, 0.03, 0.002, 0.02,  0.25};
        OUParams vix         = {12.0,  0.15, 2.0,   8.0,   80.0};
        OUParams treasury10Y = {0.025, 0.05, 0.002, 0.001, 0.15};
        OUParams creditSpread= {0.005, 0.10, 0.001, 0.003, 0.10};

        double stressWeightVix = 0.5;
        double stressWeightSpread = 0.3;
        double stressWeightUnemployment = 0.2;
        double expansionGdpThreshold = 0.03;
        double expansionStressMax = 30.0;
        double contractionGdpThreshold = 0.0;
    } economy;

    // ── Balance Thresholds ────────────────────────────────────
    // All tunable balance knobs in one place. Each value is referenced
    // from at least one of: QuarterlyTurnManager, BotStrategy,
    // EraAwareBots, DeleveragingDecisions, GameRunner.
    struct BalanceThresholds {
        // Capital-ratio cliffs (loss / danger / trigger thresholds)
        double regulatorySeizureCapRatio = 0.02;  // game-over below this
        double survivalOverrideCapRatio  = 0.05;  // bot reflex deleveraging fires
        double survivalReleaseCapRatio   = 0.07;  // hysteresis: drop override above this
        double dangerZoneCapRatio        = 0.10;  // soft "thin reserves" warning
        double dividendOfferCapRatio     = 0.08;  // generator trigger
        double divestitureCapRatio       = 0.10;  // generator trigger
        double revenueScaleDenominator   = 0.20;  // BotStrategy revenue tapering

        // Deleveraging payouts
        double dividendPayoutFraction    = 0.05;
        double buybackPayoutFraction     = 0.10;
        double buybackExpectedReturn     = 0.08;
        int    buybackProfitStreak       = 4;
        int    divestitureMinDivisions   = 4;
        double divestitureSalePriceMult  = 1.2;
        int    dividendDampingQuarters   = 4;     // recentDividendQuarters lifespan

        // Stuck-state detection
        double stuckScoreThreshold       = 0.05;  // GameRunner.h:148 (was 0.01)
        int    stuckMinQuarters          = 6;     // (was 3)
    } balance;

    // ── Restrictions (for --restrict diagnostic mode) ─────────
    struct Restrictions {
        bool noDeleveraging = false;   // Skip all deleveraging decision generators
        bool noCrises = false;         // Block all crisis generation
        std::string forceCeoId;        // Lock all bots to this CEO (empty = none)
        bool hasAny() const {
            return noDeleveraging || noCrises || !forceCeoId.empty();
        }
    } restrictions;

    // ── Bank Setup ────────────────────────────────────────────
    struct BankSetup {
        double startingCapital = 10e9;
        int startingEmployees = 50;
        int startingBranches = 1;
        double startingMarketShare = 0.001;
        double startingReputation = 50.0;
        double leverageMultiplier = 10.0;
        double depositRatio = 0.8;
        double reserveRatio = 0.1;
        double tradingBudgetPct = 0.3;
        double ibBudgetPct = 0.2;
        double cbBudgetPct = 0.5;
    } bank;

    // ── Organizational Complexity ─────────────────────────────
    struct Complexity {
        double visibilityDecayConstant = 200e9;
        double controlDecayConstant = 5000.0;
        double hiddenRiskGrowthBase = 0.02;
        double hiddenRiskGrowthAutonomyMult = 0.08;
        double inspectionRevealBase = 0.5;
        double inspectionRevealBonus = 0.3;
        double reportedRiskHidingFactor = 0.3;
    } complexity;

    // ── Action Points ─────────────────────────────────────────
    struct ActionPointConfig {
        int smallBankAP = 5;
        int mediumBankAP = 7;
        int largeBankAP = 8;
        int megaBankAP = 10;
        double threshold1 = 50e9;
        double threshold2 = 200e9;
        double threshold3 = 500e9;
    } actionPoints;

    // ── GARCH Regime Parameters ───────────────────────────────
    struct GARCHRegime { double alpha, beta; };
    struct GARCHConfig {
        GARCHRegime calm     = {0.15, 0.80};
        GARCHRegime normal   = {0.25, 0.70};
        GARCHRegime cautious = {0.25, 0.70};
        GARCHRegime stressed = {0.30, 0.65};
        GARCHRegime crisis   = {0.35, 0.60};
        double calmMax = 15.0;
        double normalMax = 35.0;
        double cautiousMax = 50.0;
        double stressedMax = 70.0;
    } garch;

    // ── Jump Diffusion ────────────────────────────────────────
    struct JumpConfig {
        double intensity = 0.01773;
        double jumpMean = -0.05;
        double jumpStdDev = 0.08;
        double crisisMultiplier = 3.0;
    } jumps;

    // ── Markets ───────────────────────────────────────────────
    struct MarketsConfig {
        double driftGdpCoeff = 0.5;
        double driftRateCoeff = 0.3;
        double bondDuration = 7.0;
        double stressSpreadCoeff = 0.02;
        double baseVolume = 1e6;
    } markets;

    // ── Trader AI ─────────────────────────────────────────────
    struct TraderConfig {
        double pctConservative = 0.40;
        double pctAnalytical = 0.30;
        double pctAggressive = 0.20;
        double pctRogue = 0.10;
        double conservativeRisk = 0.7, conservativeReturn = 0.9;
        double aggressiveRisk = 1.5, aggressiveReturn = 1.3;
        double rogueRisk = 2.0, rogueReturn = 1.5;
        double analyticalRisk = 1.1, analyticalReturn = 1.2;
        double iqMean = 110.0, iqStd = 15.0;
        double baseSalary = 150000;
        double salaryPerIqPoint = 5000;
        double aggressiveSalaryMult = 1.3;
        double rogueSalaryMult = 1.5;
        double dailyTradeProb = 0.10;
        double kellyFraction = 0.10;
        double baseCapitalPerTrader = 1e6;
        double longBias = 0.60;
        double longBiasStressReduction = 0.20;
        double hideBaseRate = 0.05;
        int initialTraderCount = 5;
    } traders;

    // ── Scoring & Progression ─────────────────────────────────
    struct ScoringConfig {
        double financialWeight = 0.50;
        double riskWeight = 0.25;
        double stakeholderWeight = 0.25;
        double financialBaseline = 50.0;
        double financialMultiplier = 500.0;
        double riskBaseline = 100.0;
        double riskMultiplier = 200.0;
        int crisisFreeBonus = 20;
        int maxDecisionXP = 30;
        double xpBaseMultiplier = 0.5;
        double levelUpBase = 100.0;
        double levelUpExponent = 1.5;
    } scoring;

    // ── Crisis Engine ─────────────────────────────────────────
    struct CrisisConfig {
        double probCalm = 0.02;
        double probNormal = 0.05;
        double probCautious = 0.10;
        double probStressed = 0.15;
        double probCrisis = 0.30;
        double hiddenRiskCoeff = 0.05;
        double hiddenRiskCap = 0.25;
        double tradingAutonomyWeight = 0.05;
        double ibAutonomyWeight = 0.03;
        double cbAutonomyWeight = 0.03;
        double minProbability = 0.01;
        double maxProbability = 0.50;
        int startSeverityMin = 3;
        int startSeverityRange = 4;
        int escalationRate = 2;
        int maxSeverity = 10;
        double financialImpactCoeff = 0.001;
        double reputationDamagePerSeverity = 0.8;  // Per severity point per quarter (reduced from 1.0)
        double reputationRecoveryRate = 5.0;     // Rep gained per profitable quarter
        double reputationFloorCapitalRatio8 = 15.0;  // Min rep if capital ratio > 8%
        double reputationFloorCapitalRatio12 = 25.0; // Min rep if capital ratio > 12%
        int aggressiveReduction = 4;
        int measuredReduction = 2;
        int gambleReduction = 3;
        int gambleEscalation = 2;
        double gambleSuccessProb = 0.30;
    } crisis;

    // ── Division Financials ───────────────────────────────────
    struct DivisionConfig {
        double tradingSalary = 100000;
        double ibSalary = 80000;
        double cbSalary = 60000;
        double ibRevenueMargin = 0.05;
        double ibGdpLeverage = 10.0;
        double cbRevenueMargin = 0.03;
        double cbRateLeverage = 5.0;
    } divisions;

    // ── Difficulty Presets ─────────────────────────────────────

    // ── Difficulty Presets ──────────────────────────────────────

    static GameConfig Normal() { return {}; } // Medium difficulty (default)

    // Easy — "Learning Banker": forgiving crises, high visibility, fast reputation recovery
    static GameConfig Easy() {
        GameConfig c;
        c.crisis.probCalm = 0.01; c.crisis.probNormal = 0.02;
        c.crisis.probCautious = 0.04; c.crisis.probStressed = 0.08;
        c.crisis.probCrisis = 0.15;
        c.crisis.reputationDamagePerSeverity = 0.5;
        c.crisis.reputationRecoveryRate = 7.0;
        c.complexity.visibilityDecayConstant = 400e9;
        c.complexity.hiddenRiskGrowthBase = 0.01;
        c.traders.pctRogue = 0.03; c.traders.pctAggressive = 0.10;
        c.traders.pctAnalytical = 0.40; c.traders.pctConservative = 0.47;
        return c;
    }

    // Medium — "Career Banker": balanced challenge (same as Normal/default)
    static GameConfig Medium() { return {}; }

    // Hard — "Wall Street Legend": punishing crises, severe fog, slow reputation recovery
    static GameConfig Hard() {
        GameConfig c;
        c.crisis.probCalm = 0.04; c.crisis.probNormal = 0.08;
        c.crisis.probCautious = 0.15; c.crisis.probStressed = 0.25;
        c.crisis.probCrisis = 0.40;
        c.crisis.reputationDamagePerSeverity = 1.2;
        c.crisis.reputationRecoveryRate = 2.5;
        c.complexity.visibilityDecayConstant = 120e9;
        c.complexity.hiddenRiskGrowthBase = 0.03;
        c.complexity.hiddenRiskGrowthAutonomyMult = 0.12;
        c.traders.pctRogue = 0.15; c.traders.pctAggressive = 0.25;
        c.traders.pctAnalytical = 0.25; c.traders.pctConservative = 0.35;
        return c;
    }

    // Legacy presets (kept for backward compatibility)
    static GameConfig Sandbox() { return Easy(); }

    static GameConfig Crisis() {
        GameConfig c = Hard();
        c.initialEconomy.fedFundsRate = 0.02;
        c.initialEconomy.gdpGrowth = -0.02;
        c.initialEconomy.unemployment = 0.08;
        c.initialEconomy.vix = 35.0;
        c.initialEconomy.creditSpread = 0.04;
        c.crisis.probCalm = 0.10; c.crisis.probNormal = 0.20;
        c.crisis.probCautious = 0.30; c.crisis.probStressed = 0.40;
        c.crisis.probCrisis = 0.50;
        c.complexity.visibilityDecayConstant = 100e9;
        c.complexity.hiddenRiskGrowthBase = 0.05;
        c.traders.pctRogue = 0.20; c.traders.pctAggressive = 0.30;
        c.traders.pctAnalytical = 0.20; c.traders.pctConservative = 0.30;
        return c;
    }

    // Tutorial — guided 20-quarter intro with safety nets
    static GameConfig Tutorial() {
        GameConfig c = Easy();
        c.timing.quartersPerGame = 20;
        c.bank.startingCapital = 20e9;     // 2x normal to prevent early death
        c.bank.startingReputation = 80.0;  // High starting rep
        c.crisis.probCalm = 0.005;         // Almost no crises
        c.crisis.probNormal = 0.01;
        c.crisis.probCautious = 0.02;
        c.crisis.probStressed = 0.04;
        c.crisis.probCrisis = 0.10;
        return c;
    }

    // ── JSON Serialization ────────────────────────────────────

    nlohmann::json toJson() const {
        return nlohmann::json{
            {"timing", {
                {"seed", timing.seed}, {"startYear", timing.startYear},
                {"startQuarter", timing.startQuarter},
                {"quarterDurationDays", timing.quarterDurationDays},
                {"ticksPerSecond", timing.ticksPerSecond},
                {"daysPerTick", timing.daysPerTick},
                {"quartersPerGame", timing.quartersPerGame}
            }},
            {"initialEconomy", {
                {"fedFundsRate", initialEconomy.fedFundsRate},
                {"cpiInflation", initialEconomy.cpiInflation},
                {"gdpGrowth", initialEconomy.gdpGrowth},
                {"unemployment", initialEconomy.unemployment},
                {"vix", initialEconomy.vix},
                {"treasuryYield10Y", initialEconomy.treasuryYield10Y},
                {"creditSpread", initialEconomy.creditSpread}
            }},
            {"bank", {
                {"startingCapital", bank.startingCapital},
                {"startingEmployees", bank.startingEmployees},
                {"startingBranches", bank.startingBranches},
                {"leverageMultiplier", bank.leverageMultiplier}
            }},
            {"complexity", {
                {"visibilityDecayConstant", complexity.visibilityDecayConstant},
                {"controlDecayConstant", complexity.controlDecayConstant},
                {"hiddenRiskGrowthBase", complexity.hiddenRiskGrowthBase}
            }},
            {"traders", {
                {"pctConservative", traders.pctConservative},
                {"pctAnalytical", traders.pctAnalytical},
                {"pctAggressive", traders.pctAggressive},
                {"pctRogue", traders.pctRogue},
                {"dailyTradeProb", traders.dailyTradeProb},
                {"kellyFraction", traders.kellyFraction},
                {"initialTraderCount", traders.initialTraderCount}
            }},
            {"scoring", {
                {"financialWeight", scoring.financialWeight},
                {"riskWeight", scoring.riskWeight},
                {"stakeholderWeight", scoring.stakeholderWeight},
                {"levelUpBase", scoring.levelUpBase},
                {"levelUpExponent", scoring.levelUpExponent}
            }},
            {"crisis", {
                {"probNormal", crisis.probNormal},
                {"probStressed", crisis.probStressed},
                {"probCrisis", crisis.probCrisis},
                {"maxProbability", crisis.maxProbability}
            }},
            {"divisions", {
                {"tradingSalary", divisions.tradingSalary},
                {"ibSalary", divisions.ibSalary},
                {"cbSalary", divisions.cbSalary}
            }}
        };
    }

    // Load partial overrides from JSON (only specified keys override defaults)
    void mergeFromJson(const nlohmann::json& j) {
        auto get = [](const nlohmann::json& j, const std::string& section,
                      const std::string& key, auto& target) {
            if (j.contains(section) && j[section].contains(key))
                target = j[section][key].get<std::remove_reference_t<decltype(target)>>();
        };

        // Timing
        get(j, "timing", "seed", timing.seed);
        get(j, "timing", "startYear", timing.startYear);
        get(j, "timing", "startQuarter", timing.startQuarter);
        get(j, "timing", "quarterDurationDays", timing.quarterDurationDays);
        get(j, "timing", "ticksPerSecond", timing.ticksPerSecond);
        get(j, "timing", "daysPerTick", timing.daysPerTick);
        get(j, "timing", "quartersPerGame", timing.quartersPerGame);

        // Initial economy
        get(j, "initialEconomy", "fedFundsRate", initialEconomy.fedFundsRate);
        get(j, "initialEconomy", "cpiInflation", initialEconomy.cpiInflation);
        get(j, "initialEconomy", "gdpGrowth", initialEconomy.gdpGrowth);
        get(j, "initialEconomy", "unemployment", initialEconomy.unemployment);
        get(j, "initialEconomy", "vix", initialEconomy.vix);
        get(j, "initialEconomy", "treasuryYield10Y", initialEconomy.treasuryYield10Y);
        get(j, "initialEconomy", "creditSpread", initialEconomy.creditSpread);

        // Bank
        get(j, "bank", "startingCapital", bank.startingCapital);
        get(j, "bank", "startingEmployees", bank.startingEmployees);
        get(j, "bank", "startingBranches", bank.startingBranches);
        get(j, "bank", "leverageMultiplier", bank.leverageMultiplier);

        // Complexity
        get(j, "complexity", "visibilityDecayConstant", complexity.visibilityDecayConstant);
        get(j, "complexity", "controlDecayConstant", complexity.controlDecayConstant);
        get(j, "complexity", "hiddenRiskGrowthBase", complexity.hiddenRiskGrowthBase);

        // Traders
        get(j, "traders", "pctConservative", traders.pctConservative);
        get(j, "traders", "pctAnalytical", traders.pctAnalytical);
        get(j, "traders", "pctAggressive", traders.pctAggressive);
        get(j, "traders", "pctRogue", traders.pctRogue);
        get(j, "traders", "dailyTradeProb", traders.dailyTradeProb);
        get(j, "traders", "kellyFraction", traders.kellyFraction);
        get(j, "traders", "initialTraderCount", traders.initialTraderCount);

        // Scoring
        get(j, "scoring", "financialWeight", scoring.financialWeight);
        get(j, "scoring", "riskWeight", scoring.riskWeight);
        get(j, "scoring", "stakeholderWeight", scoring.stakeholderWeight);
        get(j, "scoring", "levelUpBase", scoring.levelUpBase);
        get(j, "scoring", "levelUpExponent", scoring.levelUpExponent);

        // Crisis
        get(j, "crisis", "probNormal", crisis.probNormal);
        get(j, "crisis", "probStressed", crisis.probStressed);
        get(j, "crisis", "probCrisis", crisis.probCrisis);
        get(j, "crisis", "maxProbability", crisis.maxProbability);

        // Divisions
        get(j, "divisions", "tradingSalary", divisions.tradingSalary);
        get(j, "divisions", "ibSalary", divisions.ibSalary);
        get(j, "divisions", "cbSalary", divisions.cbSalary);
    }

    // Load from JSON file
    bool loadFromFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        try {
            auto j = nlohmann::json::parse(f);
            mergeFromJson(j);
            spdlog::info("Loaded config from {}", path);
            return true;
        } catch (const std::exception& e) {
            spdlog::warn("Failed to parse config {}: {}", path, e.what());
            return false;
        }
    }

    // Save to JSON file
    void saveToFile(const std::string& path) const {
        std::ofstream f(path);
        f << toJson().dump(2);
    }

    // ── Validation ────────────────────────────────────────────

    struct ValidationResult {
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        bool valid() const { return errors.empty(); }
    };

    ValidationResult validate() const {
        ValidationResult r;

        // Timing bounds
        if (timing.quarterDurationDays < 10 || timing.quarterDurationDays > 252)
            r.errors.push_back("quarterDurationDays must be 10-252");
        if (timing.ticksPerSecond < 0.1 || timing.ticksPerSecond > 100.0)
            r.errors.push_back("ticksPerSecond must be 0.1-100.0");
        if (timing.quartersPerGame < 1 || timing.quartersPerGame > 100)
            r.errors.push_back("quartersPerGame must be 1-100");

        // GARCH stationarity
        auto checkGarch = [&](const char* name, const GARCHRegime& g) {
            if (g.alpha + g.beta >= 1.0)
                r.errors.push_back(std::string(name) + " GARCH: alpha+beta must be < 1.0");
        };
        checkGarch("calm", garch.calm); checkGarch("normal", garch.normal);
        checkGarch("stressed", garch.stressed); checkGarch("crisis", garch.crisis);

        // Scoring weights sum
        double wsum = scoring.financialWeight + scoring.riskWeight + scoring.stakeholderWeight;
        if (std::abs(wsum - 1.0) > 0.01)
            r.errors.push_back("Scoring weights must sum to 1.0 (got " + std::to_string(wsum) + ")");

        // Trader personality sum
        double psum = traders.pctConservative + traders.pctAnalytical + traders.pctAggressive + traders.pctRogue;
        if (std::abs(psum - 1.0) > 0.01)
            r.errors.push_back("Trader personality pcts must sum to 1.0 (got " + std::to_string(psum) + ")");

        // Probabilities in range
        if (crisis.maxProbability < 0 || crisis.maxProbability > 1.0)
            r.errors.push_back("crisis.maxProbability must be 0-1");

        // OU floor < ceiling
        auto checkOU = [&](const char* name, const OUParams& p) {
            if (p.floor >= p.ceiling)
                r.errors.push_back(std::string(name) + " OU: floor must be < ceiling");
        };
        checkOU("fedFunds", economy.fedFunds); checkOU("inflation", economy.inflation);
        checkOU("gdpGrowth", economy.gdpGrowth); checkOU("vix", economy.vix);

        // Semantic warnings
        if (timing.quarterDurationDays < 20)
            r.warnings.push_back("Short quarters (<20 days) may feel rushed");
        if (crisis.probCrisis > 0.5)
            r.warnings.push_back("Crisis probability >50% may frustrate players");
        if (economy.vix.theta < 0.01)
            r.warnings.push_back("VIX theta < 0.01: indicator barely moves");

        return r;
    }
};

// Namespace-level default balance thresholds — accessible without a GameConfig
// instance (used by bots and stateless decision generators).
inline const GameConfig::BalanceThresholds kBalance{};

// Phase 3.4 C3: Runtime balance override map for parameter sweeps.
// Sweep harness mutates this between matrix runs to A/B test thresholds.
// Empty map = use kBalance defaults. Single-threaded use only (autoplay sweeps).
namespace balanceOverride {
    inline std::unordered_map<std::string, double> values;

    // Look up an override; fall back to the provided default.
    inline double get(const std::string& key, double fallback) {
        auto it = values.find(key);
        return it != values.end() ? it->second : fallback;
    }

    // Convenience accessors for the actively-tuned thresholds. Bots and
    // generators that want to honor sweeps call these instead of kBalance.X.
    inline double survivalOverrideCapRatio() {
        return get("survivalOverrideCapRatio", kBalance.survivalOverrideCapRatio);
    }
    inline double survivalReleaseCapRatio() {
        return get("survivalReleaseCapRatio", kBalance.survivalReleaseCapRatio);
    }
    inline double dangerZoneCapRatio() {
        return get("dangerZoneCapRatio", kBalance.dangerZoneCapRatio);
    }
    inline double dividendOfferCapRatio() {
        return get("dividendOfferCapRatio", kBalance.dividendOfferCapRatio);
    }

    // Reset all overrides between sweep iterations.
    inline void clear() { values.clear(); }
}

} // namespace stvg
