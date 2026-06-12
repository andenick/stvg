#pragma once

#include "Types.h"
#include "BankState.h"
#include "ArchetypeRegistry.h"
#include "EmployeeCandidate.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <cmath>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// Path Engine — tracks cumulative player decisions and constrains
// future options. Every major decision shifts the bank's institutional
// DNA along 5 continuous axes. Thresholds gate access to divisions,
// instruments, and conversion options.
// ════════════════════════════════════════════════════════════════════

struct PathDecision {
    int year;
    std::string id;
    std::string description;
    std::map<std::string, double> axisShifts; // e.g., {"riskCulture": +0.05}
};

inline void to_json(nlohmann::json& j, const PathDecision& d) {
    j = nlohmann::json{{"year", d.year}, {"id", d.id}, {"description", d.description}};
}

struct PathState {
    // Continuous axes (0.0 to 1.0)
    double riskCulture = 0.2;       // Conservative → Gunslinger
    double internationalFocus = 0.0; // Domestic → Global
    double techInvestment = 0.0;     // Luddite → Cutting-edge
    double politicalCapital = 0.0;   // Unknown → Connected
    double innovationBias = 0.3;     // Traditional → Innovative

    // Discrete state
    std::set<std::string> unlockedPaths;
    std::set<std::string> closedPaths;
    std::vector<PathDecision> majorDecisions;

    std::string archetype() const {
        if (riskCulture > 0.7 && innovationBias > 0.5) return "trading_firm";
        if (riskCulture < 0.3 && internationalFocus < 0.3) return "conservative_bank";
        if (internationalFocus > 0.6) return "global_bank";
        if (techInvestment > 0.6) return "tech_bank";
        if (riskCulture > 0.5 && innovationBias > 0.5) return "universal_bank";
        return "commercial_bank";
    }
};

inline void to_json(nlohmann::json& j, const PathState& s) {
    j = nlohmann::json{
        {"riskCulture", s.riskCulture},
        {"internationalFocus", s.internationalFocus},
        {"techInvestment", s.techInvestment},
        {"politicalCapital", s.politicalCapital},
        {"innovationBias", s.innovationBias},
        {"archetype", s.archetype()},
        {"unlockedPaths", s.unlockedPaths},
        {"closedPaths", s.closedPaths},
        {"majorDecisions", s.majorDecisions}
    };
}

class PathEngine {
public:
    void recordDecision(const PathDecision& decision) {
        for (const auto& [axis, shift] : decision.axisShifts) {
            applyShift(axis, shift);
        }
        state_.majorDecisions.push_back(decision);
        updatePathFlags();
        spdlog::info("PATH: {} (risk={:.2f} intl={:.2f} tech={:.2f} pol={:.2f} innov={:.2f}) [{}]",
            decision.id, state_.riskCulture, state_.internationalFocus,
            state_.techInvestment, state_.politicalCapital, state_.innovationBias,
            state_.archetype());
    }

    // Hiring effect: certain archetypes shift culture.
    //
    // STAR_02 P5 §3 (NORTH_STAR fix): this used to expect the strings
    // "aggressive"/"rogue"/"conservative"/"quant", but hires emit FAMILY ids
    // ("gunslinger", "lifer", …) — so the culture shift never fired ("hiring
    // gunslingers shifts culture" was silently broken). We now consume the
    // per-family cultureShift vector from archetypes.json keyed by the real
    // family id. A legacy fallback keeps the old string mapping working for any
    // caller still passing the old vocabulary or when the registry is absent.
    void applyHireEffect(const std::string& familyId) {
        const auto& reg = ArchetypeRegistry::instance();
        const ArchetypeFamily* fam = reg.loaded() ? reg.family(familyId) : nullptr;
        if (fam && !fam->cultureShift.empty()) {
            for (const auto& [axis, amount] : fam->cultureShift)
                applyShift(axis, amount);
            updatePathFlags();
            return;
        }
        // Legacy fallback (old vocabulary / registry unavailable).
        if (familyId == "aggressive" || familyId == "rogue" || familyId == "gunslinger") {
            applyShift("riskCulture", 0.03);
        } else if (familyId == "conservative" || familyId == "lifer" || familyId == "patrician") {
            applyShift("riskCulture", -0.02);
        } else if (familyId == "analytical" || familyId == "quant") {
            applyShift("techInvestment", 0.02);
            applyShift("innovationBias", 0.01);
        }
        updatePathFlags();
    }

    // Typed overload: accept the engine Archetype enum directly.
    void applyHireEffect(simulation::Archetype a) {
        nlohmann::json j = a;          // enum -> family id string
        applyHireEffect(j.get<std::string>());
    }

    // Division gating by path (independent of era/regulatory checks)
    bool canOpenDivision(DivisionType type) const {
        switch (type) {
            case DivisionType::TradingDesk:
                return state_.riskCulture >= 0.3;
            case DivisionType::InternationalBanking:
                return state_.internationalFocus >= 0.15 || state_.riskCulture >= 0.4;
            case DivisionType::DerivativesDesk:
                return state_.riskCulture >= 0.5 && state_.innovationBias >= 0.4;
            case DivisionType::Fintech:
                return state_.techInvestment >= 0.4;
            case DivisionType::CryptoCustody:
                return state_.techInvestment >= 0.5 && state_.innovationBias >= 0.5;
            case DivisionType::VentureCapital:
                return state_.innovationBias >= 0.4 && state_.riskCulture >= 0.4;
            default:
                return true; // Most divisions have no path requirement
        }
    }

    bool canConvertToTradingFirm() const {
        return state_.riskCulture >= 0.7
            && state_.unlockedPaths.count("trading_capability") > 0;
    }

    bool canConvertToUniversalBank(int year) const {
        // Requires Glass-Steagall to be repealed (1999+) and diversified capabilities
        return year >= 1999
            && state_.riskCulture >= 0.3
            && state_.innovationBias >= 0.3;
    }

    // Revenue/risk multipliers based on path alignment
    double revenueMultiplier(DivisionType type) const {
        double mult = 1.0;
        switch (type) {
            case DivisionType::TradingDesk:
            case DivisionType::DerivativesDesk:
                mult += (state_.riskCulture - 0.5) * 0.4; // Risk-takers trade better
                break;
            case DivisionType::InternationalBanking:
                mult += state_.internationalFocus * 0.3;    // International expertise
                break;
            case DivisionType::Fintech:
            case DivisionType::CryptoCustody:
                mult += state_.techInvestment * 0.4;        // Tech investment compounds
                break;
            case DivisionType::CommercialLending:
            case DivisionType::MortgageLending:
                mult += (0.5 - state_.riskCulture) * 0.2;  // Conservatives lend better
                break;
            default: break;
        }
        return std::clamp(mult, 0.5, 2.0);
    }

    double riskMultiplier(DivisionType type) const {
        double mult = 1.0;
        switch (type) {
            case DivisionType::TradingDesk:
            case DivisionType::DerivativesDesk:
                mult += (state_.riskCulture - 0.5) * 0.6; // High risk culture = more risk
                break;
            case DivisionType::InternationalBanking:
                mult += state_.internationalFocus * 0.3;
                break;
            default: break;
        }
        return std::clamp(mult, 0.3, 3.0);
    }

    const PathState& state() const { return state_; }

private:
    PathState state_;

    void applyShift(const std::string& axis, double amount) {
        if (axis == "riskCulture") state_.riskCulture = std::clamp(state_.riskCulture + amount, 0.0, 1.0);
        else if (axis == "internationalFocus") state_.internationalFocus = std::clamp(state_.internationalFocus + amount, 0.0, 1.0);
        else if (axis == "techInvestment") state_.techInvestment = std::clamp(state_.techInvestment + amount, 0.0, 1.0);
        else if (axis == "politicalCapital") state_.politicalCapital = std::clamp(state_.politicalCapital + amount, 0.0, 1.0);
        else if (axis == "innovationBias") state_.innovationBias = std::clamp(state_.innovationBias + amount, 0.0, 1.0);
    }

    void updatePathFlags() {
        // Unlock paths based on thresholds
        if (state_.riskCulture >= 0.5) state_.unlockedPaths.insert("trading_capability");
        if (state_.internationalFocus >= 0.4) state_.unlockedPaths.insert("global_operations");
        if (state_.techInvestment >= 0.5) state_.unlockedPaths.insert("tech_pioneer");
        if (state_.politicalCapital >= 0.5) state_.unlockedPaths.insert("political_influence");

        // Close paths based on thresholds
        if (state_.riskCulture >= 0.7) state_.closedPaths.insert("conservative_reputation");
        if (state_.riskCulture <= 0.2) state_.closedPaths.insert("gunslinger_conversion");
    }
};

} // namespace stvg
