#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <algorithm>
#include <cmath>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// Unified Personality System — drives all AI decision-making
// Used by: CEO profiles, autoplay bots, competitor AI, procedural generation
// ════════════════════════════════════════════════════════════════════

struct PersonalityProfile {
    // Decision-making axes (0.0-1.0)
    double riskTolerance = 0.5;         // 0=extremely cautious, 1=reckless gambler
    double growthAmbition = 0.5;        // 0=preserve capital, 1=maximize growth at all costs
    double complianceFocus = 0.5;       // 0=ignore rules, 1=audit obsessively
    double politicalSavvy = 0.5;        // 0=apolitical, 1=works through connections/lobbying

    // Competence axes (0.0-1.0)
    double analyticalAbility = 0.5;     // 0=gut instinct, 1=quantitative precision
    double persuasion = 0.5;            // 0=no deal skills, 1=master negotiator
    double leadership = 0.5;            // 0=poor morale, 1=inspires loyalty
    double adaptability = 0.5;          // 0=rigid strategy, 1=pivots with conditions

    // Behavioral tendencies (0.0-1.0)
    double integrity = 0.7;             // 0=will cheat/hide, 1=transparent always
    double stressResponse = 0.5;        // 0=freeze/conserve under stress, 1=double down aggressively
    double longTermFocus = 0.5;         // 0=quarterly thinking, 1=decade-scale planning

    // ── Standard bot personality presets ────────────────────────────

    static PersonalityProfile conservative() {
        // Phase 3.3 B1: growthAmbition bumped from 0.10 → 0.25 to fix
        // the regression where conservative couldn't outrun division
        // costs after Phase 2's deleveraging cliff. All other axes
        // unchanged to preserve the cautious decision profile.
        return {0.05, 0.25, 0.95, 0.2, 0.6, 0.3, 0.6, 0.1, 0.9, 0.1, 0.8};
    }
    static PersonalityProfile aggressive() {
        return {0.9, 0.95, 0.05, 0.3, 0.5, 0.6, 0.4, 0.2, 0.4, 0.9, 0.2};
    }
    static PersonalityProfile balanced() {
        return {0.5, 0.5, 0.5, 0.4, 0.8, 0.5, 0.6, 0.5, 0.7, 0.5, 0.6};
    }
    static PersonalityProfile gambler() {
        return {0.95, 0.9, 0.0, 0.1, 0.3, 0.4, 0.3, 0.1, 0.2, 1.0, 0.0};
    }
    static PersonalityProfile survivor() {
        return {0.15, 0.2, 0.7, 0.3, 0.5, 0.3, 0.5, 0.4, 0.8, 0.0, 0.9};
    }
    static PersonalityProfile regulatorsPet() {
        return {0.1, 0.0, 1.0, 0.5, 0.4, 0.3, 0.4, 0.1, 1.0, 0.0, 0.7};
    }
    static PersonalityProfile utility() {
        return {0.4, 0.5, 0.5, 0.3, 0.7, 0.5, 0.5, 0.5, 0.6, 0.4, 0.5};
    }
    static PersonalityProfile speedrun() {
        return {0.8, 1.0, 0.0, 0.1, 0.5, 0.5, 0.3, 0.0, 0.3, 0.8, 0.0};
    }
    static PersonalityProfile historical() {
        return {0.5, 0.5, 0.6, 0.4, 0.7, 0.5, 0.5, 0.9, 0.6, 0.5, 0.7};
    }
    static PersonalityProfile political() {
        return {0.4, 0.5, 0.6, 0.9, 0.4, 0.7, 0.5, 0.5, 0.5, 0.4, 0.5};
    }
    static PersonalityProfile contrary() {
        return {0.5, 0.5, 0.4, 0.3, 0.5, 0.4, 0.4, 0.8, 0.5, 0.6, 0.4};
    }
    static PersonalityProfile random() {
        return {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 1.0, 0.5, 0.5, 0.5};
    }

    // ── Historical CEO personality presets ──────────────────────────

    static PersonalityProfile rockefeller() {
        return {0.3, 0.4, 0.6, 0.95, 0.5, 0.8, 0.8, 0.4, 0.7, 0.3, 0.9};
    }
    static PersonalityProfile wriston() {
        return {0.7, 0.85, 0.2, 0.4, 0.7, 0.6, 0.6, 0.5, 0.5, 0.7, 0.6};
    }
    static PersonalityProfile dimon() {
        return {0.4, 0.6, 0.7, 0.6, 0.9, 0.6, 0.8, 0.6, 0.7, 0.4, 0.8};
    }
    static PersonalityProfile volcker() {
        return {0.2, 0.1, 0.95, 0.3, 0.8, 0.4, 0.9, 0.2, 0.95, 0.2, 0.95};
    }
    static PersonalityProfile milken() {
        return {0.9, 0.9, 0.0, 0.3, 0.8, 0.7, 0.5, 0.4, 0.2, 0.8, 0.3};
    }
    static PersonalityProfile fuld() {
        return {0.95, 0.95, 0.0, 0.2, 0.4, 0.5, 0.6, 0.0, 0.3, 1.0, 0.1};
    }
    static PersonalityProfile blankfein() {
        return {0.7, 0.8, 0.2, 0.5, 0.7, 0.7, 0.5, 0.5, 0.3, 0.7, 0.4};
    }
    static PersonalityProfile greenspan() {
        return {0.6, 0.5, 0.1, 0.8, 0.7, 0.6, 0.5, 0.4, 0.4, 0.6, 0.3};
    }
    static PersonalityProfile born() {
        return {0.1, 0.1, 1.0, 0.3, 0.9, 0.3, 0.6, 0.1, 1.0, 0.1, 1.0};
    }
    static PersonalityProfile warren() {
        return {0.1, 0.0, 1.0, 0.7, 0.7, 0.6, 0.7, 0.3, 1.0, 0.2, 0.9};
    }

    // ── Additional historical CEO presets (Tier 2-5 fill-ins) ───────

    // Sandy Weill — empire builder via acquisition, bent rules until they broke
    static PersonalityProfile weill() {
        return {0.7, 0.95, 0.2, 0.7, 0.6, 0.85, 0.7, 0.6, 0.3, 0.7, 0.5};
    }
    // Ken Griffin — quant market-maker, ruthless, technologist
    static PersonalityProfile griffin() {
        return {0.7, 0.85, 0.4, 0.5, 0.95, 0.6, 0.5, 0.7, 0.6, 0.8, 0.7};
    }
    // Siegmund Warburg — refugee innovator, regulatory-arbitrage architect
    static PersonalityProfile warburg() {
        return {0.6, 0.7, 0.3, 0.6, 0.8, 0.8, 0.7, 0.9, 0.7, 0.4, 0.9};
    }
    // Thomas Lamont — Morgan diplomat-partner, geopolitical financier
    static PersonalityProfile lamont() {
        return {0.5, 0.6, 0.4, 0.95, 0.6, 0.9, 0.7, 0.6, 0.5, 0.4, 0.8};
    }
    // Robert McNamara — systems analysis technocrat, volume over quality
    static PersonalityProfile mcnamara() {
        return {0.4, 0.8, 0.5, 0.6, 0.95, 0.5, 0.6, 0.4, 0.6, 0.3, 0.7};
    }
    // Robert Rubin — revolving-door insider, conflict-of-interest sophisticate
    static PersonalityProfile rubin() {
        return {0.6, 0.7, 0.4, 0.95, 0.85, 0.8, 0.7, 0.6, 0.4, 0.5, 0.6};
    }
    // Hank Paulson — Goldman alpha, crisis manager
    static PersonalityProfile paulson() {
        return {0.6, 0.7, 0.5, 0.7, 0.7, 0.7, 0.8, 0.8, 0.6, 0.8, 0.6};
    }
    // John Meriwether — bond arbitrage genius, twice-blown-up
    static PersonalityProfile meriwether() {
        return {0.85, 0.7, 0.3, 0.3, 0.95, 0.6, 0.6, 0.2, 0.6, 0.9, 0.4};
    }
    // Ray Dalio — systematic macro, radical transparency, framework-driven
    static PersonalityProfile dalio() {
        return {0.5, 0.6, 0.6, 0.5, 0.95, 0.6, 0.5, 0.7, 0.8, 0.4, 0.9};
    }
    // ── Axis accessors (for sensitivity analysis) ───────────────────

    static constexpr int kNumAxes = 11;

    static constexpr const char* axisName(int idx) {
        constexpr const char* names[] = {
            "riskTolerance", "growthAmbition", "complianceFocus", "politicalSavvy",
            "analyticalAbility", "persuasion", "leadership", "adaptability",
            "integrity", "stressResponse", "longTermFocus"
        };
        return (idx >= 0 && idx < kNumAxes) ? names[idx] : "unknown";
    }

    void setAxis(int idx, double val) {
        switch(idx) {
            case 0: riskTolerance = val; break;
            case 1: growthAmbition = val; break;
            case 2: complianceFocus = val; break;
            case 3: politicalSavvy = val; break;
            case 4: analyticalAbility = val; break;
            case 5: persuasion = val; break;
            case 6: leadership = val; break;
            case 7: adaptability = val; break;
            case 8: integrity = val; break;
            case 9: stressResponse = val; break;
            case 10: longTermFocus = val; break;
        }
    }

    double getAxis(int idx) const {
        switch(idx) {
            case 0: return riskTolerance;
            case 1: return growthAmbition;
            case 2: return complianceFocus;
            case 3: return politicalSavvy;
            case 4: return analyticalAbility;
            case 5: return persuasion;
            case 6: return leadership;
            case 7: return adaptability;
            case 8: return integrity;
            case 9: return stressResponse;
            case 10: return longTermFocus;
            default: return 0.5;
        }
    }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PersonalityProfile,
    riskTolerance, growthAmbition, complianceFocus, politicalSavvy,
    analyticalAbility, persuasion, leadership, adaptability,
    integrity, stressResponse, longTermFocus)

} // namespace stvg
