#pragma once

#include "BotStrategy.h"
#include "../core/QuarterlyTurnManager.h"
#include "../core/PersonalityProfile.h"
#include "../math/RandomEngine.h"
#include <array>
#include <string>
#include <cmath>
#include <limits>

namespace stvg::autoplay {

// ════════════════════════════════════════════════════════════════════
// Era-Aware Bots — strategies that understand the 1945-2040 game.
// Each plays the full 95 years with different approaches.
// ════════════════════════════════════════════════════════════════════

// ── Historical Bot: plays like a real bank across eras ─────────────
class HistoricalBot : public BotStrategy {
public:
    explicit HistoricalBot(uint64_t seed = 42) : rng_(seed) {}
    std::string name() const override { return "HistoricalBot"; }

    std::string chooseOption(const QuarterlyTurnManager& game,
                              const simulation::Decision& decision) override {
        const auto& era = game.currentEra();
        const auto& opts = decision.options;
        if (opts.empty()) return "";

        // Era-adaptive strategy (differentiated from ConservativeBot)
        if (era.id == "post_war" || era.id == "expansion") {
            // Cautiously balanced: grow steadily but avoid big risks
            return pickBalanced(game, opts);
        } else if (era.id == "volatility") {
            // Cautious but willing to trade: balanced pick
            return pickBalanced(game, opts);
        } else if (era.id == "big_bang" || era.id == "shadow") {
            // Aggressive: maximize revenue
            return pickHighestRevenue(opts);
        } else if (era.id == "reform") {
            // Compliance-focused: prefer audits and safe options
            return pickLowestRisk(opts);
        } else {
            // Modern: balanced with tech bias
            return pickBalanced(game, opts);
        }
    }

private:
    math::RandomEngine rng_;

    std::string pickLowestRisk(const std::vector<simulation::DecisionOption>& opts) {
        int bestIdx = 0;
        double bestRisk = 1e18;
        for (int i = 0; i < (int)opts.size(); ++i) {
            double risk = opts[i].riskImpact.riskChange;
            if (risk < bestRisk) { bestRisk = risk; bestIdx = i; }
        }
        return opts[bestIdx].id;
    }

    std::string pickHighestRevenue(const std::vector<simulation::DecisionOption>& opts) {
        int bestIdx = 0;
        double bestRev = -1e18;
        for (int i = 0; i < (int)opts.size(); ++i) {
            double rev = opts[i].financialImpact.expectedRevenue - opts[i].financialImpact.immediateCost;
            if (rev > bestRev) { bestRev = rev; bestIdx = i; }
        }
        return opts[bestIdx].id;
    }

    std::string pickBalanced(const QuarterlyTurnManager& game,
                              const std::vector<simulation::DecisionOption>& opts) {
        // Leverage-aware pick: dampens revenue weight when capital ratio is thin.
        const double capRatio = game.bank().capitalRatio();
        const double revenueScale = std::clamp(
            (capRatio - kBalance.regulatorySeizureCapRatio) / kBalance.revenueScaleDenominator,
            0.0, 1.0);
        const double riskScale = (capRatio < kBalance.dangerZoneCapRatio) ? 4.0 : 1.0;

        // Force audit when in danger zone
        if (capRatio < kBalance.dangerZoneCapRatio) {
            for (const auto& o : opts) {
                if (o.id.find("audit") != std::string::npos && o.id != "audit_skip")
                    return o.id;
            }
        }

        int bestIdx = 0;
        double bestEV = -1e18;
        for (int i = 0; i < (int)opts.size(); ++i) {
            double rev = opts[i].financialImpact.expectedRevenue * revenueScale;
            double cost = opts[i].financialImpact.immediateCost;
            double risk = opts[i].riskImpact.riskChange;
            double ev = (rev - cost) * opts[i].successProbability
                      - risk * game.bank().capital * 0.001 * riskScale;
            if (ev > bestEV) { bestEV = ev; bestIdx = i; }
        }
        return opts[bestIdx].id;
    }
};

// ── Speedrun Bot: maximum growth, maximum risk ─────────────────────
class SpeedrunBot : public BotStrategy {
public:
    std::string name() const override { return "SpeedrunBot"; }

    std::string chooseOption(const QuarterlyTurnManager& /*game*/,
                              const simulation::Decision& decision) override {
        const auto& opts = decision.options;
        if (opts.empty()) return "";

        // Always pick highest revenue option regardless of risk
        int bestIdx = 0;
        double bestRev = -1e18;
        for (int i = 0; i < (int)opts.size(); ++i) {
            double rev = opts[i].financialImpact.expectedRevenue;
            if (rev > bestRev) { bestRev = rev; bestIdx = i; }
        }
        return opts[bestIdx].id;
    }
};

// ── Survivor Bot: maximum caution, never take risk ─────────────────
class SurvivorBot : public BotStrategy {
public:
    std::string name() const override { return "SurvivorBot"; }

    std::string chooseOption(const QuarterlyTurnManager& game,
                              const simulation::Decision& decision) override {
        const auto& opts = decision.options;
        if (opts.empty()) return "";

        // Audit every other quarter (balance risk management with revenue)
        bool auditQuarter = (game.quartersCompleted() % 2 == 0);
        if (auditQuarter) {
            for (const auto& opt : opts) {
                if (opt.id.find("audit_") != std::string::npos && opt.id != "audit_skip")
                    return opt.id;
            }
        }

        // Non-audit quarters: balanced survival formula with revenue consideration
        int bestIdx = 0;
        double bestScore = -1e18;
        for (int i = 0; i < (int)opts.size(); ++i) {
            double prob = opts[i].successProbability;
            double risk = opts[i].riskImpact.riskChange;
            double cost = opts[i].financialImpact.immediateCost;
            double rev = opts[i].financialImpact.expectedRevenue;
            double score = prob * 1.5 + (rev - cost) / 1e3 - risk * 3.0;
            if (score > bestScore) { bestScore = score; bestIdx = i; }
        }
        return opts[bestIdx].id;
    }
};

// ── Contrary Bot: avoids historical patterns ───────────────────────
class ContraryBot : public BotStrategy {
public:
    explicit ContraryBot(uint64_t seed = 42) : rng_(seed) {}
    std::string name() const override { return "ContraryBot"; }

    std::string chooseOption(const QuarterlyTurnManager& game,
                              const simulation::Decision& decision) override {
        const auto& era = game.currentEra();
        const auto& opts = decision.options;
        if (opts.empty()) return "";

        // Opposite of HistoricalBot (but with some randomness for differentiation)
        if (era.id == "post_war" || era.id == "expansion") {
            // Aggressive when everyone is conservative, but with 30% random for variety
            if (rng_.bernoulli(0.3)) return pickRandom(opts);
            return pickHighestRevenue(opts);
        } else if (era.id == "big_bang" || era.id == "shadow") {
            // Conservative when everyone is aggressive (avoid GFC trap)
            return pickLowestRisk(opts);
        } else {
            return pickRandom(opts);
        }
    }

private:
    math::RandomEngine rng_;

    std::string pickHighestRevenue(const std::vector<simulation::DecisionOption>& opts) {
        int bestIdx = 0;
        double bestRev = -1e18;
        for (int i = 0; i < (int)opts.size(); ++i) {
            if (opts[i].financialImpact.expectedRevenue > bestRev) {
                bestRev = opts[i].financialImpact.expectedRevenue;
                bestIdx = i;
            }
        }
        return opts[bestIdx].id;
    }

    std::string pickLowestRisk(const std::vector<simulation::DecisionOption>& opts) {
        int bestIdx = 0;
        double bestRisk = 1e18;
        for (int i = 0; i < (int)opts.size(); ++i) {
            if (opts[i].riskImpact.riskChange < bestRisk) {
                bestRisk = opts[i].riskImpact.riskChange;
                bestIdx = i;
            }
        }
        return opts[bestIdx].id;
    }

    std::string pickRandom(const std::vector<simulation::DecisionOption>& opts) {
        return opts[(int)(rng_.uniform() * opts.size()) % opts.size()].id;
    }
};

// ── Political Bot: lobbying focus ──────────────────────────────────
class PoliticalBot : public BotStrategy {
public:
    std::string name() const override { return "PoliticalBot"; }

    std::string chooseOption(const QuarterlyTurnManager& game,
                              const simulation::Decision& decision) override {
        const auto& opts = decision.options;
        if (opts.empty()) return "";

        // Political strategy: audit periodically (connections include compliance)
        if (game.quartersCompleted() % 3 == 0) {
            for (const auto& opt : opts) {
                if (opt.id.find("audit_") != std::string::npos && opt.id != "audit_skip")
                    return opt.id;
            }
        }

        // Otherwise: highest success probability (the lobbyist approach)
        int bestIdx = 0;
        double bestProb = -1;
        for (int i = 0; i < (int)opts.size(); ++i) {
            if (opts[i].successProbability > bestProb) {
                bestProb = opts[i].successProbability;
                bestIdx = i;
            }
        }
        return opts[bestIdx].id;
    }
};

// ════════════════════════════════════════════════════════════════════
// PersonalityDrivenBot — generic bot parameterized by PersonalityProfile.
// Replaces hardcoded heuristic bots with a single scoring formula whose
// weights come from the personality vector. Any preset (or any custom
// CEO personality) can be plugged in to produce differentiated behavior.
// ════════════════════════════════════════════════════════════════════

class PersonalityDrivenBot : public BotStrategy {
public:
    PersonalityDrivenBot(const std::string& label, const PersonalityProfile& p)
        : name_("Personality_" + label), p_(p) {}

    std::string name() const override { return name_; }
    double riskTolerance() const override { return p_.riskTolerance; }

    std::string chooseOption(const QuarterlyTurnManager& game,
                              const simulation::Decision& decision) override {
        const auto& opts = decision.options;
        if (opts.empty()) return "";

        // Sprint 1 + Phase 4: Banking policy decisions — personality + era-driven
        if (decision.id.find("loanspread") != std::string::npos) {
            int targetBps = 200;
            if (p_.growthAmbition > 0.7)       targetBps = 100;
            else if (p_.growthAmbition > 0.5)  targetBps = 150;
            else if (p_.complianceFocus > 0.7) targetBps = 300;
            else if (p_.riskTolerance > 0.6)   targetBps = 150;
            else if (p_.riskTolerance < 0.3)   targetBps = 250;
            // ±50bps noise for variety among similar personalities
            targetBps += (int)((p_.persuasion - 0.5) * 100);
            targetBps = std::clamp(targetBps, 100, 300);
            // Snap to nearest available option
            int bestDist = 999;
            std::string bestId;
            for (const auto& opt : opts) {
                for (int bps : {100, 150, 200, 250, 300}) {
                    if (opt.id.find(std::to_string(bps) + "bps") != std::string::npos) {
                        if (std::abs(bps - targetBps) < bestDist) {
                            bestDist = std::abs(bps - targetBps);
                            bestId = opt.id;
                        }
                    }
                }
            }
            return bestId.empty() ? opts[opts.size() / 2].id : bestId;
        }
        if (decision.id.find("creditstd") != std::string::npos) {
            // Era-aware: deregulation eras push toward loose, reform toward tight
            const auto& era = game.currentEra();
            double looseBias = 0;
            if (era.id == "big_bang" || era.id == "shadow") looseBias = 0.2;
            if (era.id == "reform") looseBias = -0.2;

            double aggressionScore = p_.riskTolerance * 0.5 + p_.growthAmbition * 0.3
                                   - p_.complianceFocus * 0.4 + looseBias;
            if (aggressionScore > 0.3)
                return opts.front().id;   // loose
            if (aggressionScore < -0.1)
                return opts.back().id;    // tight
            return opts[opts.size() / 2].id; // moderate
        }
        if (decision.id.find("duration") != std::string::npos) {
            if (p_.riskTolerance > 0.7)
                return opts.back().id;    // long (yield-seeking)
            if (p_.riskTolerance < 0.3 || p_.complianceFocus > 0.7)
                return opts.front().id;   // short (safe)
            return opts[opts.size() / 2].id; // balanced
        }

        // SURVIVAL OVERRIDE with hysteresis (Phase 3.4 B2b).
        // Enter survival mode when capRatio < survivalOverrideCapRatio (0.05);
        // stay in survival mode until capRatio rises above survivalReleaseCapRatio (0.07).
        // Prevents the cliff-snap-back-cliff oscillation that compresses scores.
        // Phase 3.4 C3: thresholds read via balanceOverride to support --sweep.
        const double capRatio = game.bank().capitalRatio();
        const double releaseAt = balanceOverride::survivalReleaseCapRatio();
        const double enterAt = balanceOverride::survivalOverrideCapRatio();
        if (inSurvivalMode_ && capRatio >= releaseAt) {
            inSurvivalMode_ = false;
        } else if (!inSurvivalMode_ && capRatio < enterAt) {
            inSurvivalMode_ = true;
        }
        if (inSurvivalMode_) {
            return pickDeleveragingOption(opts);
        }

        // Audit cadence driven by complianceFocus.
        // High compliance audits every 2 quarters; medium every 4; low never.
        int auditPeriod = 0;
        if (p_.complianceFocus > 0.7)      auditPeriod = 2;
        else if (p_.complianceFocus > 0.4) auditPeriod = 4;
        if (auditPeriod > 0 && (game.quartersCompleted() % auditPeriod) == 0) {
            for (const auto& opt : opts) {
                if (opt.id.find("audit_") != std::string::npos && opt.id != "audit_skip")
                    return opt.id;
            }
        }

        // Stress-aware scoring under crisis.
        const bool inCrisis = game.crisisEngine().hasActiveCrisis();
        const bool nearMin = capRatio < kBalance.dangerZoneCapRatio;

        int bestIdx = 0;
        double bestScore = -std::numeric_limits<double>::infinity();
        for (int i = 0; i < (int)opts.size(); ++i) {
            const auto& o = opts[i];

            // Normalized financial signal (revenue net of cost, scaled).
            double net = (o.financialImpact.expectedRevenue - o.financialImpact.immediateCost) / 1e3;

            // Risk aversion scales with (1-riskTolerance). Doubled when capital is near minimum.
            double riskAversion = (1.0 - p_.riskTolerance) * 3.0;
            if (nearMin) riskAversion *= 2.0;

            double score = 0.0;
            score += p_.growthAmbition * net;
            score -= riskAversion * o.riskImpact.riskChange;
            score += p_.analyticalAbility * o.successProbability * 1.5;

            // Phase 4 A2: longTermFocus amplified 10x + multi-quarter bonus
            score += p_.longTermFocus * (o.financialImpact.expectedRevenue / 1e3);
            score += p_.longTermFocus * o.financialImpact.timelineQuarters * 0.5;

            // Phase 4 A2: persuasion scales deal value for revenue-positive options
            if (o.financialImpact.expectedRevenue > 0)
                score += p_.persuasion * net * 0.4;

            // Phase 4 A2: adaptability modulates crisis-vs-normal scoring
            if (inCrisis)
                score *= (0.85 + p_.adaptability * 0.3);  // range: 0.85x-1.15x

            // Compliance bias toward audit / comply options
            if (o.id.find("audit") != std::string::npos && o.id != "audit_skip")
                score += p_.complianceFocus * 1.5;
            if (o.id.find("comply") != std::string::npos)
                score += p_.complianceFocus * 1.0;

            // Phase 4 A2: politicalSavvy amplified 2.5x + broader string matching
            if (o.id.find("lobby") != std::string::npos ||
                o.id.find("political") != std::string::npos ||
                o.id.find("regulation") != std::string::npos ||
                o.id.find("coalition") != std::string::npos ||
                o.id.find("government") != std::string::npos)
                score += p_.politicalSavvy * 5.0;

            // Crisis behavior: stressResponse > 0.5 doubles down (favors revenue);
            // stressResponse < 0.5 conserves (favors low risk).
            if (inCrisis) {
                if (p_.stressResponse > 0.5)
                    score += (p_.stressResponse - 0.5) * 2.0 * net;
                else
                    score -= (0.5 - p_.stressResponse) * 4.0 * o.riskImpact.riskChange;
            }

            // Advisor recommendations weighted by leadership (high leaders listen to staff)
            int support = 0;
            for (const auto& r : o.recommendations) {
                if (r.recommendation == "Support") support++;
                else if (r.recommendation == "Oppose") support--;
            }
            score += p_.leadership * support * 0.5;

            // Phase 4 A2: integrity amplified 2x + audit bonus for high-integrity bots
            if (o.id.find("hide") != std::string::npos ||
                o.id.find("conceal") != std::string::npos ||
                o.id.find("defer") != std::string::npos) {
                score -= (p_.integrity - 0.5) * 6.0;  // was 3.0
            }
            if (p_.integrity > 0.7 &&
                (o.id.find("audit") != std::string::npos && o.id != "audit_skip"))
                score += p_.integrity * 2.0;

            if (score > bestScore) { bestScore = score; bestIdx = i; }
        }
        return opts[bestIdx].id;
    }

    const PersonalityProfile& personality() const { return p_; }

private:
    std::string name_;
    PersonalityProfile p_;
    // Phase 3.4 B2b: hysteresis flag — true while we're in the danger zone
    // and force-deleveraging. Cleared when capRatio rises above release threshold.
    bool inSurvivalMode_ = false;

    // Phase 3.4 B2a: personality-aware deleveraging cascade.
    // Returns the best deleveraging option for this personality from the
    // current option pool. The preference order varies by dominant trait:
    //   high growthAmbition  → buyback first (preserves future EPS)
    //   high complianceFocus → dividend first (clean balance sheet, signals)
    //   high politicalSavvy  → dividend first (shareholder/regulator signal)
    //   low  riskTolerance   → divestiture first (eliminates riskiest unit)
    //   default              → divest > buyback > dividend > audit > skip
    std::string pickDeleveragingOption(const std::vector<simulation::DecisionOption>& opts) const {
        std::array<const char*, 5> prefs;
        if (p_.growthAmbition > 0.6) {
            prefs = {"buyback_execute", "dividend_pay", "_divest_", "audit_", "skip"};
        } else if (p_.complianceFocus > 0.7) {
            prefs = {"dividend_pay", "audit_", "buyback_execute", "_divest_", "skip"};
        } else if (p_.politicalSavvy > 0.7) {
            prefs = {"dividend_pay", "buyback_execute", "audit_", "_divest_", "skip"};
        } else if (p_.riskTolerance < 0.3) {
            prefs = {"_divest_", "audit_", "dividend_pay", "buyback_execute", "skip"};
        } else {
            // Moderate profiles (no dominant trait): audit-first is safer than
            // the aggressive divest-first default. Lets moderate bots discover
            // hidden risk before panic-selling. Phase 4 A4.
            prefs = {"audit_", "dividend_pay", "buyback_execute", "_divest_", "skip"};
        }

        for (const auto* needle : prefs) {
            for (const auto& opt : opts) {
                // _divest_ matches divest_hold (the "hold all" option) — exclude it
                if (std::string(needle) == "_divest_"
                    && opt.id.find("divest_hold") != std::string::npos) {
                    continue;
                }
                // audit_ matches audit_skip — exclude it
                if (std::string(needle) == "audit_" && opt.id == "audit_skip") {
                    continue;
                }
                if (opt.id.find(needle) != std::string::npos) {
                    return opt.id;
                }
            }
        }
        // Nothing matched — fall back to first option
        return opts.empty() ? "" : opts[0].id;
    }
};

} // namespace stvg::autoplay
