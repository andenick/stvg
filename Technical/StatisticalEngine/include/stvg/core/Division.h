#pragma once

#include "EmployeeCandidate.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// Division types — 16 business lines spanning 1945-2025
// ════════════════════════════════════════════════════════════════════

enum class DivisionType {
    // Starting (1945)
    CommercialLending,    // Bread and butter — loans, deposits, the 3-6-3 rule
    MortgageLending,      // FHA/VA loans, GI Bill housing boom

    // Unlock 1950s-60s
    TrustAndCustody,      // Wealth preservation, estate management
    CreditCards,          // 1958 BankAmericard
    InternationalBanking, // Eurodollar market, correspondent banking

    // Unlock 1970s-80s
    TradingDesk,          // Proprietary trading, market-making
    AssetManagement,      // Mutual funds, pension management

    // Unlock 1980s-90s (deregulation era)
    InvestmentBanking,    // M&A advisory, underwriting (post Glass-Steagall)
    PrivateEquity,        // LBO/merchant banking
    Restructuring,        // Distressed debt, bankruptcy advisory
    Securitization,       // MBS, CDOs, CLOs

    // Unlock 2000s+
    DerivativesDesk,      // Swaps, options, structured products
    WealthManagement,     // High-net-worth private banking
    VentureCapital,       // Tech/startup investing

    // Unlock 2010s+
    Fintech,              // Digital banking, mobile, APIs
    CryptoCustody,        // Digital asset custody and trading
};

NLOHMANN_JSON_SERIALIZE_ENUM(DivisionType, {
    {DivisionType::CommercialLending, "commercial_lending"},
    {DivisionType::MortgageLending, "mortgage_lending"},
    {DivisionType::TrustAndCustody, "trust_custody"},
    {DivisionType::CreditCards, "credit_cards"},
    {DivisionType::InternationalBanking, "international"},
    {DivisionType::TradingDesk, "trading"},
    {DivisionType::AssetManagement, "asset_management"},
    {DivisionType::InvestmentBanking, "investment_banking"},
    {DivisionType::PrivateEquity, "private_equity"},
    {DivisionType::Restructuring, "restructuring"},
    {DivisionType::Securitization, "securitization"},
    {DivisionType::DerivativesDesk, "derivatives"},
    {DivisionType::WealthManagement, "wealth_management"},
    {DivisionType::VentureCapital, "venture_capital"},
    {DivisionType::Fintech, "fintech"},
    {DivisionType::CryptoCustody, "crypto_custody"},
})

// ════════════════════════════════════════════════════════════════════
// Division head honesty — drives information filtering
// ════════════════════════════════════════════════════════════════════

enum class HeadHonesty { Honest, SelfServing, Mixed };
NLOHMANN_JSON_SERIALIZE_ENUM(HeadHonesty, {
    {HeadHonesty::Honest, "honest"},
    {HeadHonesty::SelfServing, "self_serving"},
    {HeadHonesty::Mixed, "mixed"},
})

// ════════════════════════════════════════════════════════════════════
// Division — a single business line within the bank
// ════════════════════════════════════════════════════════════════════

struct Division {
    std::string id;
    std::string name;
    DivisionType type;

    double autonomyLevel = 0.5;
    double budget = 0.0;
    double revenue = 0.0;
    double costs = 0.0;
    double netIncome = 0.0;
    double reportedRisk = 0.0;
    double actualRisk = 0.0;
    double hiddenRisk = 0.0;
    int employees = 0;
    double morale = 75.0;
    bool inspectedThisQuarter = false;
    int quartersSinceInspection = 0;

    // Division head personality — determines report accuracy
    HeadHonesty headHonesty = HeadHonesty::Honest;
    double headAmbition = 0.5;    // 0-1, drives deception intensity
    double headCompetence = 0.7;  // 0-1, quality of lies (higher = harder to detect)

    // Staff (actual employee objects with RPG stats)
    std::vector<simulation::EmployeeCandidate> staff;

    int staffCount() const { return (int)staff.size(); }

    double staffQualityMultiplier() const {
        if (staff.empty()) return 0.5;
        double totalQ = 0;
        for (const auto& emp : staff) totalQ += emp.expectedSharpe() + 0.5;
        return std::clamp(totalQ / staff.size(), 0.3, 2.0);
    }

    double totalSalaryCost() const {
        double total = 0;
        for (const auto& emp : staff) total += emp.annualSalary / 4.0;
        return total;
    }

    // ── Filtered reporting (what the CEO sees vs truth) ─────────
    // visibilityPct: bank-wide visibility (100 = small bank, ~5 = megabank)
    double reportedRevenueFiltered(double visibilityPct) const {
        double fogFactor = (100.0 - visibilityPct) / 100.0; // 0 at full vis, ~0.95 at 5% vis
        switch (headHonesty) {
            case HeadHonesty::Honest:
                // Small noise at low visibility (up to +/-5%)
                return revenue * (1.0 + fogFactor * 0.05 * (headCompetence - 0.5));
            case HeadHonesty::SelfServing:
                // Inflate revenue up to 20%, scaled by fog and ambition
                return revenue * (1.0 + fogFactor * 0.20 * headAmbition);
            case HeadHonesty::Mixed:
                // Honest above 60% visibility, then starts shading
                if (visibilityPct > 60.0) return revenue;
                return revenue * (1.0 + fogFactor * 0.12 * headAmbition);
        }
        return revenue;
    }

    double reportedRiskFiltered(double visibilityPct) const {
        double fogFactor = (100.0 - visibilityPct) / 100.0;
        switch (headHonesty) {
            case HeadHonesty::Honest:
                // Reports actual risk (not hidden), small noise at low vis
                return actualRisk * (1.0 + fogFactor * 0.03);
            case HeadHonesty::SelfServing:
                // Suppresses risk by up to 50%, hides hidden risk entirely
                return actualRisk * (1.0 - fogFactor * 0.50 * headAmbition);
            case HeadHonesty::Mixed:
                if (visibilityPct > 60.0) return actualRisk;
                return actualRisk * (1.0 - fogFactor * 0.30 * headAmbition);
        }
        return actualRisk;
    }

    double reportedCostsFiltered(double visibilityPct) const {
        double fogFactor = (100.0 - visibilityPct) / 100.0;
        switch (headHonesty) {
            case HeadHonesty::Honest:
                return costs;
            case HeadHonesty::SelfServing:
                // Underreport costs to look more profitable
                return costs * (1.0 - fogFactor * 0.10 * headAmbition);
            case HeadHonesty::Mixed:
                if (visibilityPct > 60.0) return costs;
                return costs * (1.0 - fogFactor * 0.05 * headAmbition);
        }
        return costs;
    }

    // Asset classification — determines how this division's budget appears on the balance sheet
    enum class AssetClass { Loans, Securities, FeeBased };

    AssetClass assetClass() const {
        switch (type) {
            case DivisionType::CommercialLending:
            case DivisionType::MortgageLending:
            case DivisionType::CreditCards:
            case DivisionType::InternationalBanking:
            case DivisionType::Securitization:
                return AssetClass::Loans;
            case DivisionType::TradingDesk:
            case DivisionType::DerivativesDesk:
            case DivisionType::PrivateEquity:
            case DivisionType::VentureCapital:
            case DivisionType::CryptoCustody:
                return AssetClass::Securities;
            default:
                return AssetClass::FeeBased;
        }
    }

    // Revenue characteristics per division type
    // Revenue rates as % of budget (=capital allocation) per quarter.
    // Targets ~10-15% annual ROE after costs, matching real US bank profitability.
    double baseRevenueRate() const {
        switch (type) {
            case DivisionType::CommercialLending:    return 0.020;  // Net interest margin on loans
            case DivisionType::MortgageLending:      return 0.015;  // Lower margin, high volume
            case DivisionType::TrustAndCustody:      return 0.010;  // Fee-based, stable
            case DivisionType::CreditCards:          return 0.035;  // High yield consumer
            case DivisionType::InternationalBanking: return 0.022;  // FX + trade finance
            case DivisionType::TradingDesk:          return 0.040;  // High but volatile
            case DivisionType::AssetManagement:      return 0.012;  // AUM fees
            case DivisionType::InvestmentBanking:    return 0.050;  // Fee-based, cyclical
            case DivisionType::PrivateEquity:        return 0.025;  // Lumpy carry
            case DivisionType::Restructuring:        return 0.035;  // Counter-cyclical fees
            case DivisionType::Securitization:       return 0.030;  // Origination + servicing
            case DivisionType::DerivativesDesk:      return 0.060;  // Highest return, highest risk
            case DivisionType::WealthManagement:     return 0.015;  // AUM + advisory
            case DivisionType::VentureCapital:       return 0.012;  // Very lumpy
            case DivisionType::Fintech:              return 0.020;  // Platform fees
            case DivisionType::CryptoCustody:        return 0.040;  // Custody + staking
        }
        return 0.020;
    }

    // Era-adjusted revenue rate: historical multiplier based on game year
    double baseRevenueRate(int year) const {
        double base = baseRevenueRate();
        double eraMult = 1.0;
        switch (type) {
            case DivisionType::CreditCards:
                if (year < 1970) eraMult = 0.3;
                else if (year < 1990) eraMult = 0.7;
                else if (year < 2010) eraMult = 1.2;
                break;
            case DivisionType::TradingDesk:
                if (year < 1980) eraMult = 0.5;
                else if (year < 2000) eraMult = 1.5;
                else if (year < 2010) eraMult = 1.3;
                else eraMult = 0.8;
                break;
            case DivisionType::Securitization:
                if (year < 1995) eraMult = 0.3;
                else if (year < 2007) eraMult = 2.0;
                else eraMult = 0.7;
                break;
            case DivisionType::InvestmentBanking:
                if (year < 1987) eraMult = 0.5;
                else if (year < 2008) eraMult = 1.5;
                break;
            case DivisionType::DerivativesDesk:
                if (year < 1990) eraMult = 0.4;
                else if (year < 2008) eraMult = 1.5;
                else eraMult = 0.8;
                break;
            case DivisionType::Fintech:
                if (year < 2015) eraMult = 0.3;
                else eraMult = 1.5;
                break;
            case DivisionType::CryptoCustody:
                if (year < 2020) eraMult = 0.3;
                else eraMult = 1.5;
                break;
            case DivisionType::InternationalBanking:
                if (year < 1971) eraMult = 0.6;
                else if (year < 2000) eraMult = 1.3;
                break;
            case DivisionType::MortgageLending:
                if (year < 1960) eraMult = 1.3;
                else if (year >= 2003 && year <= 2007) eraMult = 1.8;
                else if (year > 2008) eraMult = 0.8;
                break;
            default: break;
        }
        return base * eraMult;
    }

    // Loss Given Default: what fraction of a defaulted loan is lost
    double lossGivenDefault() const {
        switch (type) {
            case DivisionType::MortgageLending: return 0.25;  // collateralized
            case DivisionType::CommercialLending: return 0.40;
            case DivisionType::CreditCards: return 0.60;      // unsecured
            case DivisionType::InternationalBanking: return 0.45;
            case DivisionType::Securitization: return 0.35;   // tranched
            default: return 0.40;
        }
    }

    // Risk profile per type
    double baseRiskRate() const {
        switch (type) {
            case DivisionType::CommercialLending:    return 0.03;
            case DivisionType::MortgageLending:      return 0.04;
            case DivisionType::TrustAndCustody:      return 0.01;
            case DivisionType::CreditCards:          return 0.05;
            case DivisionType::InternationalBanking: return 0.04;
            case DivisionType::TradingDesk:          return 0.10;
            case DivisionType::AssetManagement:      return 0.02;
            case DivisionType::InvestmentBanking:    return 0.06;
            case DivisionType::PrivateEquity:        return 0.12;
            case DivisionType::Restructuring:        return 0.05;
            case DivisionType::Securitization:       return 0.08;
            case DivisionType::DerivativesDesk:      return 0.15;
            case DivisionType::WealthManagement:     return 0.02;
            case DivisionType::VentureCapital:       return 0.20;
            case DivisionType::Fintech:              return 0.06;
            case DivisionType::CryptoCustody:        return 0.12;
        }
        return 0.05;
    }

    void accumulateHiddenRisk() {
        if (!inspectedThisQuarter) {
            quartersSinceInspection++;
            double growthRate = std::min(0.02 + 0.06 * autonomyLevel, 0.05);
            hiddenRisk = std::min(hiddenRisk + actualRisk * growthRate * quartersSinceInspection, 1.0);
        } else {
            // Audit: reveal small portion, remove MORE hidden risk than revealed
            double revealPct = 0.15 + 0.1 * (1.0 - autonomyLevel);
            actualRisk += hiddenRisk * revealPct;
            hiddenRisk *= (1.0 - revealPct * 2.0); // Remove double what's revealed
            hiddenRisk = std::max(hiddenRisk, 0.0);
            quartersSinceInspection = 0;
            // Strong mitigation from inspection (institutional learning)
            actualRisk *= 0.80;
        }
        // Natural risk decay (markets normalize, positions unwind)
        actualRisk *= 0.98;
        actualRisk = std::min(actualRisk, 1.0);
        inspectedThisQuarter = false;
    }
};

// Full truth (for god view / autoplay telemetry)
inline void to_json(nlohmann::json& j, const Division& d) {
    j = nlohmann::json{
        {"id", d.id}, {"name", d.name}, {"type", d.type},
        {"autonomyLevel", d.autonomyLevel},
        {"budget", d.budget}, {"revenue", d.revenue},
        {"costs", d.costs}, {"netIncome", d.netIncome},
        {"reportedRisk", d.reportedRisk},
        {"actualRisk", d.actualRisk}, {"hiddenRisk", d.hiddenRisk},
        {"employees", d.employees},
        {"morale", d.morale}, {"inspectedThisQuarter", d.inspectedThisQuarter},
        {"quartersSinceInspection", d.quartersSinceInspection},
        {"headHonesty", d.headHonesty},
        {"headAmbition", d.headAmbition},
        {"headCompetence", d.headCompetence},
        {"staffCount", d.staffCount()},
        {"staffQualityMultiplier", d.staffQualityMultiplier()},
        {"totalSalaryCost", d.totalSalaryCost()}
    };
}

// Phase 3.2 save/load support: restore a Division from JSON. Skips computed
// fields (staffQualityMultiplier, totalSalaryCost) and the staff vector
// (acceptable trade-off — staff regen on next hiring cycle).
inline void loadDivisionFromJson(Division& d, const nlohmann::json& j) {
    d.id          = j.value("id", d.id);
    d.name        = j.value("name", d.name);
    if (j.contains("type")) d.type = j["type"].get<DivisionType>();
    d.autonomyLevel        = j.value("autonomyLevel", d.autonomyLevel);
    d.budget               = j.value("budget", d.budget);
    d.revenue              = j.value("revenue", 0.0);
    d.costs                = j.value("costs", 0.0);
    d.netIncome            = j.value("netIncome", 0.0);
    d.reportedRisk         = j.value("reportedRisk", 0.0);
    d.actualRisk           = j.value("actualRisk", 0.0);
    d.hiddenRisk           = j.value("hiddenRisk", 0.0);
    d.employees            = j.value("employees", 0);
    d.morale               = j.value("morale", 75.0);
    d.inspectedThisQuarter = j.value("inspectedThisQuarter", false);
    d.quartersSinceInspection = j.value("quartersSinceInspection", 0);
    if (j.contains("headHonesty")) d.headHonesty = j["headHonesty"].get<HeadHonesty>();
    d.headAmbition         = j.value("headAmbition", 0.5);
    d.headCompetence       = j.value("headCompetence", 0.7);
}

// Filtered view (what the CEO actually sees)
inline nlohmann::json divisionToPlayerJson(const Division& d, double visibilityPct) {
    nlohmann::json j = {
        {"id", d.id}, {"name", d.name}, {"type", d.type},
        {"autonomyLevel", d.autonomyLevel},
        {"budget", d.budget},
        {"revenue", d.reportedRevenueFiltered(visibilityPct)},
        {"costs", d.reportedCostsFiltered(visibilityPct)},
        {"netIncome", d.reportedRevenueFiltered(visibilityPct) - d.reportedCostsFiltered(visibilityPct)},
        {"reportedRisk", d.reportedRiskFiltered(visibilityPct)},
        {"employees", d.employees},
        {"morale", d.morale},
        {"quartersSinceInspection", d.quartersSinceInspection},
        {"staffCount", d.staffCount()},
        {"staffQualityMultiplier", d.staffQualityMultiplier()},
        {"totalSalaryCost", d.totalSalaryCost()}
    };

    // Only show hidden risk if inspected this quarter
    if (d.inspectedThisQuarter) {
        j["hiddenRisk"] = d.hiddenRisk;
        j["actualRisk"] = d.actualRisk;
        j["inspectionRevealed"] = true;
    }

    // Filter staff: hide hidden stats (potential, integrity) unless revealed
    nlohmann::json staffJson = nlohmann::json::array();
    for (const auto& emp : d.staff) {
        nlohmann::json ej;
        ej["id"] = emp.id;
        ej["name"] = emp.name;
        ej["archetype"] = emp.archetype;
        ej["level"] = emp.level;
        ej["annualSalary"] = emp.annualSalary;
        ej["quartersEmployed"] = emp.quartersEmployed;
        ej["yearsExperience"] = emp.yearsExperience;
        ej["stats"] = emp.stats;
        // Traits are visible
        nlohmann::json traits = nlohmann::json::array();
        for (const auto& t : emp.traits) {
            traits.push_back({{"name", t.name}, {"description", t.description}, {"positive", t.positive}});
        }
        ej["traits"] = traits;
        // Hidden stats only if potential revealed (after 4 quarters)
        if (emp.potentialRevealed) {
            ej["hiddenPotential"] = emp.hiddenPotential;
            ej["hiddenIntegrity"] = emp.hiddenIntegrity;
        }
        staffJson.push_back(ej);
    }
    j["staff"] = staffJson;

    return j;
}

} // namespace stvg
