#pragma once

#include "PathEngine.h"
#include "EmployeeCandidate.h"
#include <nlohmann/json.hpp>
#include <array>
#include <string>
#include <vector>
#include <algorithm>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// ReputationLens (STAR_02 P6 — the "Fable mechanic", plan §P6)
//
// Maps the PathEngine's 5 continuous axes → (a) a player-facing reputation
// TAG ("gunslinger shop", "fortress bank", …) the world recognizes you by,
// and (b) generation WEIGHTS that bias the candidate pool, deal flow, and
// poach ask-pricing toward what you've become. Pure function of PathState —
// no RNG, no mutation — so it is lockstep-safe and cheap to call anywhere.
//
// Path axes are on [0,1] (riskCulture defaults 0.2). The plan's "±0.5"
// language is expressed here on that scale via named thresholds that line up
// with the existing PathState::archetype() / canOpenDivision() cutoffs:
//   riskCulture ≥ 0.5  → aggressive tilt  ("gunslinger shop")
//   riskCulture ≤ 0.3  → conservative tilt ("fortress bank")
// internationalFocus / techInvestment get secondary tags when pronounced.
//
// The aggressive families are the directional/fee risk-takers (gunslinger,
// dealmaker, prodigy, quant); the conservative families are the relationship/
// control cluster (lifer, patrician, operator, rainmaker). A tilt multiplies
// the spawn weight of one cluster relative to baseline; the test asserts a
// gunslinger-driven high-riskCulture run shifts the aggressive share ≥1.5×.
// ════════════════════════════════════════════════════════════════════

struct ReputationProfile {
    // The primary public tag ("gunslinger shop" / "fortress bank" / "balanced
    // house" / "global house" / "tech-forward shop").
    std::string tag;
    // Per-family spawn-weight multiplier, indexed by the Archetype enum order
    // (patrician..lifer). 1.0 = no tilt. Applied on top of era spawn weights.
    std::array<double, 8> familyMult{{1, 1, 1, 1, 1, 1, 1, 1}};
    // Deal-risk tilt: >1 biases the deal generator toward riskier prospects,
    // <1 toward safer ones. 1.0 = neutral.
    double dealRiskTilt = 1.0;
    // Poach ask-price tilt: a gunslinger shop pays up for wildcat teams; a
    // fortress bank is quoted gentler. Multiplies the poach askPrice.
    double poachPriceTilt = 1.0;
    // A short list of dialogue tags NPC barks can reference ({repTag} slot).
    std::vector<std::string> dialogueTags;
};

class ReputationLens {
public:
    // Named thresholds (data-driven where cheap; tuned to the existing path
    // cutoffs so the lens agrees with division gating).
    static constexpr double kRiskHigh      = 0.50;  // gunslinger shop
    static constexpr double kRiskLow       = 0.30;  // fortress bank
    static constexpr double kIntlHigh      = 0.45;  // global house
    static constexpr double kTechHigh      = 0.45;  // tech-forward shop
    static constexpr double kAggressiveTilt = 1.6;  // boost aggressive cluster
    static constexpr double kConservTilt    = 1.6;  // boost conservative cluster
    static constexpr double kSuppress       = 0.7;  // damp the opposite cluster

    // Family enum indices for the two clusters (Archetype order:
    // 0 Patrician, 1 Gunslinger, 2 Quant, 3 Dealmaker, 4 Operator,
    // 5 Rainmaker, 6 Prodigy, 7 Lifer).
    static constexpr std::array<int, 4> kAggressiveIdx{{1, 3, 6, 2}}; // gunslinger,dealmaker,prodigy,quant
    static constexpr std::array<int, 4> kConservativeIdx{{7, 0, 4, 5}}; // lifer,patrician,operator,rainmaker

    // Compute the lens for a path state. Pure — no RNG, no side effects.
    static ReputationProfile compute(const PathState& p) {
        ReputationProfile prof;

        // ── riskCulture: the dominant axis (the owner's named mechanic). ──
        if (p.riskCulture >= kRiskHigh) {
            prof.tag = "gunslinger shop";
            for (int i : kAggressiveIdx)   prof.familyMult[i] *= kAggressiveTilt;
            for (int i : kConservativeIdx) prof.familyMult[i] *= kSuppress;
            // The steeper the riskCulture, the riskier the flow + the more you
            // overpay for wildcat teams (scaled within [kRiskHigh,1]).
            double t = std::clamp((p.riskCulture - kRiskHigh) / (1.0 - kRiskHigh), 0.0, 1.0);
            prof.dealRiskTilt  = 1.0 + 0.6 * t;
            prof.poachPriceTilt = 1.0 + 0.5 * t;
            prof.dialogueTags.push_back("gunslinger shop");
        } else if (p.riskCulture <= kRiskLow) {
            prof.tag = "fortress bank";
            for (int i : kConservativeIdx) prof.familyMult[i] *= kConservTilt;
            for (int i : kAggressiveIdx)   prof.familyMult[i] *= kSuppress;
            double t = std::clamp((kRiskLow - p.riskCulture) / kRiskLow, 0.0, 1.0);
            prof.dealRiskTilt  = 1.0 - 0.4 * t;
            prof.poachPriceTilt = 1.0 - 0.3 * t;
            prof.dialogueTags.push_back("fortress bank");
        } else {
            prof.tag = "balanced house";
            prof.dialogueTags.push_back("balanced house");
        }

        // ── Secondary axes: layer extra tags + mild tilts (don't override). ──
        if (p.internationalFocus >= kIntlHigh) {
            prof.dialogueTags.push_back("global house");
            // Dealmaker/Rainmaker do the cross-border flow.
            prof.familyMult[3] *= 1.15; // dealmaker
            prof.familyMult[5] *= 1.15; // rainmaker
            if (prof.tag == "balanced house") prof.tag = "global house";
        }
        if (p.techInvestment >= kTechHigh) {
            prof.dialogueTags.push_back("tech-forward shop");
            prof.familyMult[2] *= 1.20; // quant
            prof.familyMult[6] *= 1.20; // prodigy
            if (prof.tag == "balanced house") prof.tag = "tech-forward shop";
        }

        return prof;
    }

    // Convenience: just the tag (for player-view exposure + bark slots).
    static std::string tagFor(const PathState& p) { return compute(p).tag; }
};

} // namespace stvg
