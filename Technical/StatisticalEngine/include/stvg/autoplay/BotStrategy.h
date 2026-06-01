#pragma once

#include "../core/QuarterlyTurnManager.h"
#include "../simulation/DecisionEngine.h"
#include "../math/RandomEngine.h"
#include <string>
#include <cmath>
#include <algorithm>
#include <limits>

namespace stvg::autoplay {

// Abstract interface for game-playing strategies
class BotStrategy {
public:
    virtual ~BotStrategy() = default;
    virtual std::string name() const = 0;
    virtual std::string chooseOption(const QuarterlyTurnManager& game,
                                      const simulation::Decision& decision) = 0;
};

// ── Random Bot: Pure random choice ──────────────────────────────
class RandomBot : public BotStrategy {
public:
    explicit RandomBot(uint64_t seed = 42) : rng_(seed) {}
    std::string name() const override { return "RandomBot"; }
    std::string chooseOption(const QuarterlyTurnManager&,
                              const simulation::Decision& dec) override {
        if (dec.options.empty()) return "";
        int idx = (int)(rng_.uniform() * dec.options.size()) % dec.options.size();
        return dec.options[idx].id;
    }
private:
    math::RandomEngine rng_;
};

// ── Utility Bot: Weighted score per option ──────────────────────
struct UtilityWeights {
    double revenue = 1.0;
    double cost = -1.0;
    double risk = -2.0;
    double success = 1.5;
    double advisorSupport = 0.5;
};

class UtilityBot : public BotStrategy {
public:
    explicit UtilityBot(const UtilityWeights& w = {}) : weights_(w) {}
    std::string name() const override { return "UtilityBot"; }
    std::string chooseOption(const QuarterlyTurnManager& game,
                              const simulation::Decision& dec) override {
        // Leverage-aware scoring. The always-on regulatory_seizure trigger
        // is capitalRatio() < kBalance.regulatorySeizureCapRatio (50:1 leverage
        // ceiling). UtilityBot previously hit 0% survival because it grew
        // assets without ever considering that ceiling. We make revenue
        // exponentially less attractive as leverage approaches the cliff.
        const auto& bank = game.bank();
        const double capRatio = bank.capitalRatio();

        // revenueScale shrinks as we approach the leverage cliff.
        // At capRatio 0.30 → 1.0 (full revenue weight)
        // At capRatio 0.10 → 0.5
        // At capRatio 0.05 → 0.1 (revenue almost ignored)
        // At capRatio 0.03 → 0.0 (pure preservation mode)
        double revenueScale = std::clamp(
            (capRatio - kBalance.regulatorySeizureCapRatio) / kBalance.revenueScaleDenominator,
            0.0, 1.0);

        // riskWeight grows as the cushion shrinks
        double riskWeight = weights_.risk;
        if (capRatio < kBalance.survivalOverrideCapRatio)      riskWeight *= 8.0;
        else if (capRatio < kBalance.dangerZoneCapRatio)       riskWeight *= 4.0;
        else if (capRatio < kBalance.dangerZoneCapRatio + 0.05) riskWeight *= 2.0;

        // Force audit when leverage is dangerous — proactive de-risking
        const bool dangerZone = (capRatio < kBalance.dangerZoneCapRatio);
        if (dangerZone) {
            for (const auto& opt : dec.options) {
                if (opt.id.find("audit") != std::string::npos && opt.id != "audit_skip")
                    return opt.id;
            }
        }

        double bestScore = -std::numeric_limits<double>::infinity();
        std::string bestId;
        for (const auto& opt : dec.options) {
            double score = weights_.revenue * revenueScale
                                * opt.financialImpact.expectedRevenue / 1e6
                         + weights_.cost * opt.financialImpact.immediateCost / 1e6
                         + riskWeight * opt.riskImpact.riskChange
                         + weights_.success * opt.successProbability;
            int support = 0;
            for (const auto& r : opt.recommendations)
                if (r.recommendation == "Support") support++;
                else if (r.recommendation == "Oppose") support--;
            score += weights_.advisorSupport * support;
            // Regulatory awareness: boost audit/compliance options
            if (opt.id.find("audit") != std::string::npos && opt.id != "audit_skip")
                score += dangerZone ? 5.0 : 1.5;
            if (score > bestScore) { bestScore = score; bestId = opt.id; }
        }
        return bestId;
    }
private:
    UtilityWeights weights_;
};

// ── Conservative Bot: Minimize risk, audit often ────────────────
class ConservativeBot : public BotStrategy {
public:
    std::string name() const override { return "ConservativeBot"; }
    std::string chooseOption(const QuarterlyTurnManager&,
                              const simulation::Decision& dec) override {
        // Always choose audit if available
        for (const auto& opt : dec.options)
            if (opt.id.find("audit") != std::string::npos) return opt.id;
        // Otherwise pick lowest risk
        double bestRisk = std::numeric_limits<double>::infinity();
        std::string bestId = dec.options[0].id;
        for (const auto& opt : dec.options) {
            if (opt.riskImpact.riskChange < bestRisk) {
                bestRisk = opt.riskImpact.riskChange;
                bestId = opt.id;
            }
        }
        return bestId;
    }
};

// ── Aggressive Bot: Maximize revenue, never audit ───────────────
class AggressiveBot : public BotStrategy {
public:
    std::string name() const override { return "AggressiveBot"; }
    std::string chooseOption(const QuarterlyTurnManager&,
                              const simulation::Decision& dec) override {
        // Skip audits
        double bestRev = -std::numeric_limits<double>::infinity();
        std::string bestId = dec.options[0].id;
        for (const auto& opt : dec.options) {
            if (opt.id.find("audit") != std::string::npos) continue;
            if (opt.id.find("skip") != std::string::npos) continue;
            double rev = opt.financialImpact.expectedRevenue - opt.financialImpact.immediateCost;
            if (rev > bestRev) { bestRev = rev; bestId = opt.id; }
        }
        return bestId;
    }
};

// ── Balanced Bot: Maximize expected value ───────────────────────
class BalancedBot : public BotStrategy {
public:
    std::string name() const override { return "BalancedBot"; }
    std::string chooseOption(const QuarterlyTurnManager& game,
                              const simulation::Decision& dec) override {
        double bestEV = -std::numeric_limits<double>::infinity();
        std::string bestId = dec.options[0].id;
        for (const auto& opt : dec.options) {
            double upside = opt.financialImpact.expectedRevenue * opt.successProbability;
            double downside = opt.financialImpact.immediateCost * (1.0 - opt.successProbability);
            double riskPenalty = opt.riskImpact.riskChange * game.bank().capital * 0.01;
            double ev = upside - downside - riskPenalty;
            if (ev > bestEV) { bestEV = ev; bestId = opt.id; }
        }
        return bestId;
    }
};

// ── Gambler Bot: Pick riskiest option, always gamble in crisis ───
class GamblerBot : public BotStrategy {
public:
    std::string name() const override { return "GamblerBot"; }
    std::string chooseOption(const QuarterlyTurnManager&,
                              const simulation::Decision& dec) override {
        // Pick option with lowest success probability (most risky)
        double lowestProb = 2.0;
        std::string bestId = dec.options[0].id;
        for (const auto& opt : dec.options) {
            if (opt.id.find("gamble") != std::string::npos || opt.id.find("wait") != std::string::npos)
                return opt.id; // Always gamble
            if (opt.successProbability < lowestProb) {
                lowestProb = opt.successProbability;
                bestId = opt.id;
            }
        }
        return bestId;
    }
};

// ── Regulator's Pet: Always comply, always audit ────────────────
class RegulatorsPetBot : public BotStrategy {
public:
    std::string name() const override { return "RegulatorsPetBot"; }
    std::string chooseOption(const QuarterlyTurnManager&,
                              const simulation::Decision& dec) override {
        // Prefer audit, then comply, then highest success prob
        for (const auto& opt : dec.options)
            if (opt.id.find("audit") != std::string::npos) return opt.id;
        for (const auto& opt : dec.options)
            if (opt.id.find("comply") != std::string::npos) return opt.id;
        double bestProb = -1;
        std::string bestId = dec.options[0].id;
        for (const auto& opt : dec.options) {
            if (opt.successProbability > bestProb) {
                bestProb = opt.successProbability;
                bestId = opt.id;
            }
        }
        return bestId;
    }
};

} // namespace stvg::autoplay
