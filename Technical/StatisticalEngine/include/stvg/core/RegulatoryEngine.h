#pragma once

#include "Types.h"
#include "BankState.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// Regulatory Engine — historical regulations as hard gameplay gates.
// Glass-Steagall, Bretton Woods, Basel, Dodd-Frank — these are not
// flavor text, they are mechanical constraints.
// ════════════════════════════════════════════════════════════════════

struct Regulation {
    std::string id;
    std::string name;
    std::string description;
    int enactedYear;
    int repealedYear;               // -1 if still active

    // Hard constraints
    std::vector<DivisionType> prohibitedDivisions;
    double capitalRequirement = 0;  // Min capital ratio (0 = no requirement)
    double leverageLimit = 0;       // Max leverage ratio (0 = no limit)
    double depositRateCap = -1;     // Interest rate cap on deposits (-1 = no cap)
    bool fixedExchangeRates = false;
    double complianceCostPct = 0;   // Annual cost as % of revenue
    bool restrictsProprietaryTrading = false;

    bool isActive(int year) const {
        return year >= enactedYear && (repealedYear < 0 || year < repealedYear);
    }
};

struct RegulatoryState {
    double effectiveCapitalReq = 0;
    double effectiveLeverageLimit = 0;
    double totalComplianceCostPct = 0;
    int violations = 0;
    bool underInvestigation = false;
    bool glassWallActive = false;
    bool brettonWoodsActive = false;
    bool volckerRuleActive = false;
    std::vector<std::string> activeRegulationIds;
};

inline void to_json(nlohmann::json& j, const RegulatoryState& s) {
    j = nlohmann::json{
        {"effectiveCapitalReq", s.effectiveCapitalReq},
        {"effectiveLeverageLimit", s.effectiveLeverageLimit},
        {"totalComplianceCostPct", s.totalComplianceCostPct},
        {"violations", s.violations},
        {"underInvestigation", s.underInvestigation},
        {"glassWallActive", s.glassWallActive},
        {"brettonWoodsActive", s.brettonWoodsActive},
        {"volckerRuleActive", s.volckerRuleActive},
        {"activeRegulationIds", s.activeRegulationIds}
    };
}

class RegulatoryEngine {
public:
    void init() {
        buildRegulations();
        spdlog::info("RegulatoryEngine initialized with {} regulations", regulations_.size());
    }

    void updateForYear(int year) {
        state_.activeRegulationIds.clear();
        state_.effectiveCapitalReq = 0;
        state_.effectiveLeverageLimit = 0;
        state_.totalComplianceCostPct = 0;
        state_.glassWallActive = false;
        state_.brettonWoodsActive = false;
        state_.volckerRuleActive = false;

        for (const auto& reg : regulations_) {
            if (!reg.isActive(year)) continue;

            state_.activeRegulationIds.push_back(reg.id);
            state_.effectiveCapitalReq = std::max(state_.effectiveCapitalReq, reg.capitalRequirement);
            if (reg.leverageLimit > 0) {
                state_.effectiveLeverageLimit = (state_.effectiveLeverageLimit > 0)
                    ? std::min(state_.effectiveLeverageLimit, reg.leverageLimit)
                    : reg.leverageLimit;
            }
            state_.totalComplianceCostPct += reg.complianceCostPct;

            if (reg.id == "glass_steagall") state_.glassWallActive = true;
            if (reg.id == "bretton_woods") state_.brettonWoodsActive = true;
            if (reg.id == "dodd_frank") state_.volckerRuleActive = true;
        }

        state_.underInvestigation = (state_.violations >= 3);
    }

    bool isDivisionProhibited(DivisionType type) const {
        for (const auto& regId : state_.activeRegulationIds) {
            for (const auto& reg : regulations_) {
                if (reg.id != regId) continue;
                if (std::find(reg.prohibitedDivisions.begin(),
                              reg.prohibitedDivisions.end(), type)
                    != reg.prohibitedDivisions.end()) {
                    return true;
                }
            }
        }
        return false;
    }

    double effectiveCapitalRequirement() const { return state_.effectiveCapitalReq; }
    double effectiveLeverageLimit() const { return state_.effectiveLeverageLimit; }

    double annualComplianceCost(double revenue) const {
        return revenue * state_.totalComplianceCostPct;
    }

    void recordViolation(const std::string& reason) {
        state_.violations++;
        state_.underInvestigation = (state_.violations >= 3);
        spdlog::warn("REGULATORY VIOLATION (#{}) : {}", state_.violations, reason);
    }

    // ceoRegulatoryBonus is in percentage points (e.g. 20.0 = 2% capital ratio relief).
    // It relaxes the effective requirements as if the CEO's connections / reputation
    // bought breathing room — never below a 2% capital floor.
    bool checkCompliance(const Bank& bank, double ceoRegulatoryBonus = 0.0) const {
        double capitalRelief = std::max(0.0, ceoRegulatoryBonus) * 0.01; // pp → ratio
        double effectiveCapReq = std::max(0.02, state_.effectiveCapitalReq - capitalRelief);
        if (state_.effectiveCapitalReq > 0 && bank.capitalRatio() < effectiveCapReq) {
            return false;
        }
        // Leverage limit is also relaxed proportionally (each pp of bonus adds 1x leverage headroom).
        double leverageRelief = std::max(0.0, ceoRegulatoryBonus) * 1.0;
        double effectiveLevLimit = state_.effectiveLeverageLimit + leverageRelief;
        if (state_.effectiveLeverageLimit > 0 && bank.leverageRatio() > effectiveLevLimit) {
            return false;
        }
        return true;
    }

    bool isGlassWallActive() const { return state_.glassWallActive; }
    bool isBrettonWoodsActive() const { return state_.brettonWoodsActive; }
    bool isVolckerRuleActive() const { return state_.volckerRuleActive; }

    const RegulatoryState& state() const { return state_; }
    const std::vector<Regulation>& allRegulations() const { return regulations_; }

    const Regulation* findRegulation(const std::string& id) const {
        for (const auto& r : regulations_) {
            if (r.id == id) return &r;
        }
        return nullptr;
    }

private:
    std::vector<Regulation> regulations_;
    RegulatoryState state_;

    void buildRegulations() {
        regulations_.clear();

        // Glass-Steagall Act (1933-1999)
        {
            Regulation r;
            r.id = "glass_steagall";
            r.name = "Glass-Steagall Act";
            r.description = "Separates commercial banking from securities underwriting. "
                "Banks cannot engage in investment banking, securitization, or derivatives.";
            r.enactedYear = 1933;
            r.repealedYear = 1999; // Gramm-Leach-Bliley
            r.prohibitedDivisions = {
                DivisionType::InvestmentBanking,
                DivisionType::Securitization,
                DivisionType::DerivativesDesk,
                DivisionType::PrivateEquity
            };
            r.complianceCostPct = 0.005; // 0.5%
            regulations_.push_back(std::move(r));
        }

        // Regulation Q (1933-2011)
        {
            Regulation r;
            r.id = "regulation_q";
            r.name = "Regulation Q";
            r.description = "Caps interest rates on savings deposits, limiting deposit competition.";
            r.enactedYear = 1933;
            r.repealedYear = 2011;
            r.depositRateCap = 0.025; // 2.5% cap
            r.complianceCostPct = 0.001;
            regulations_.push_back(std::move(r));
        }

        // Bretton Woods (1944-1971)
        {
            Regulation r;
            r.id = "bretton_woods";
            r.name = "Bretton Woods System";
            r.description = "Fixed exchange rate system pegged to gold. Forex trading is not possible.";
            r.enactedYear = 1944;
            r.repealedYear = 1971;
            r.fixedExchangeRates = true;
            regulations_.push_back(std::move(r));
        }

        // Basel I (1988-2004)
        {
            Regulation r;
            r.id = "basel_i";
            r.name = "Basel I Accord";
            r.description = "First international capital standard. Minimum 8% capital-to-risk-weighted-assets.";
            r.enactedYear = 1988;
            r.repealedYear = 2004;
            r.capitalRequirement = 0.08;
            r.complianceCostPct = 0.003;
            regulations_.push_back(std::move(r));
        }

        // Basel II (2004-2010)
        {
            Regulation r;
            r.id = "basel_ii";
            r.name = "Basel II Accord";
            r.description = "Risk-sensitive capital requirements with three pillars: capital, supervision, disclosure.";
            r.enactedYear = 2004;
            r.repealedYear = 2010;
            r.capitalRequirement = 0.08;
            r.complianceCostPct = 0.005;
            regulations_.push_back(std::move(r));
        }

        // Basel III (2010+)
        {
            Regulation r;
            r.id = "basel_iii";
            r.name = "Basel III Accord";
            r.description = "Strengthened capital requirements post-GFC. 10.5% minimum including buffers.";
            r.enactedYear = 2010;
            r.repealedYear = -1; // Still active
            r.capitalRequirement = 0.105;
            r.leverageLimit = 33.3; // 3% leverage ratio = max 33x
            r.complianceCostPct = 0.008;
            regulations_.push_back(std::move(r));
        }

        // Dodd-Frank (2010+)
        {
            Regulation r;
            r.id = "dodd_frank";
            r.name = "Dodd-Frank Act";
            r.description = "Comprehensive financial reform. Volcker Rule restricts proprietary trading. "
                "Too-big-to-fail designation. Stress testing requirements.";
            r.enactedYear = 2010;
            r.repealedYear = -1;
            r.restrictsProprietaryTrading = true;
            r.complianceCostPct = 0.02; // 2% — massive compliance burden
            regulations_.push_back(std::move(r));
        }
    }
};

} // namespace stvg
