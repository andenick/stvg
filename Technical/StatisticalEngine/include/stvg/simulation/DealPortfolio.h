#pragma once

#include "../core/Types.h"
#include "../core/BankState.h"
#include "../math/RandomEngine.h"
#include "RiskReturnProfile.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <spdlog/spdlog.h>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Deal Marketplace — gambling-feel opportunities per business line
// Uses RiskReturnProfile for standardized risk/reward mechanics.
// ════════════════════════════════════════════════════════════════════

enum class DealStatus { Available, Active, Succeeded, Failed, Windfall };
NLOHMANN_JSON_SERIALIZE_ENUM(DealStatus, {
    {DealStatus::Available, "available"}, {DealStatus::Active, "active"},
    {DealStatus::Succeeded, "succeeded"}, {DealStatus::Failed, "failed"},
    {DealStatus::Windfall, "windfall"},
})

struct DealOpportunity {
    std::string id;
    std::string title;
    std::string description;
    DivisionType requiredLine;
    RiskReturnProfile risk;
    double investmentRequired;
    std::string clientName;
};

inline void to_json(nlohmann::json& j, const DealOpportunity& d) {
    j = nlohmann::json{
        {"id", d.id}, {"title", d.title}, {"description", d.description},
        {"requiredLine", d.requiredLine}, {"risk", d.risk},
        {"investmentRequired", d.investmentRequired}, {"clientName", d.clientName}
    };
}

struct ActiveDeal {
    DealOpportunity opportunity;
    int startQuarter = 0;
    int resolveQuarter = 0;
    double investedCapital = 0;
    DealStatus status = DealStatus::Active;
    double realizedReturn = 0;
};

inline void to_json(nlohmann::json& j, const ActiveDeal& d) {
    j = nlohmann::json{
        {"opportunity", d.opportunity}, {"startQuarter", d.startQuarter},
        {"resolveQuarter", d.resolveQuarter}, {"investedCapital", d.investedCapital},
        {"status", d.status}, {"realizedReturn", d.realizedReturn},
        {"quartersRemaining", d.resolveQuarter - d.startQuarter}
    };
}

// ════════════════════════════════════════════════════════════════════

class DealPortfolio {
public:
    explicit DealPortfolio(math::RandomEngine& rng) : rng_(rng) {}

    // Generate 0-2 deal opportunities per active business line
    void generateOpportunities(const std::vector<Division>& divisions, int gameYear) {
        availableDeals_.clear();
        loadTemplates(); // Lazy-load JSON templates on first call

        for (const auto& div : divisions) {
            // Each division has 30-60% chance of generating a deal
            if (!rng_.bernoulli(0.4)) continue;

            DealOpportunity deal;
            deal.id = "deal_" + std::to_string(++dealCounter_);
            deal.requiredLine = div.type;
            deal.clientName = generateClientName();

            // 50% chance to use a JSON template if available for this era
            bool usedTemplate = false;
            if (!dealTemplates_.empty() && rng_.bernoulli(0.5)) {
                // Find era-filtered templates matching this division type
                std::vector<const DealTemplate*> eligible;
                for (const auto& t : dealTemplates_) {
                    if (gameYear >= t.eraStart && gameYear <= t.eraEnd) {
                        eligible.push_back(&t);
                    }
                }
                if (!eligible.empty()) {
                    const auto& tmpl = *eligible[(int)(rng_.uniform() * eligible.size()) % eligible.size()];
                    deal.title = tmpl.title;
                    deal.description = tmpl.description;
                    deal.risk = {tmpl.riskLevel, 0.0, 0.0, tmpl.successProbability,
                                 tmpl.durationQuarters, 0.1, 0.05, 2.0};
                    deal.investmentRequired = tmpl.investmentRequired;
                    usedTemplate = true;
                }
            }

            if (!usedTemplate) {
                // Fall back to procedural generation
                generateDealForType(deal, div.type, gameYear);
                deal.investmentRequired = div.budget * (0.05 + rng_.uniform() * 0.15);
                deal.investmentRequired = std::max(deal.investmentRequired, 100000.0);
            }

            availableDeals_.push_back(std::move(deal));
        }

        spdlog::info("Generated {} deal opportunities", availableDeals_.size());
    }

    // Player accepts a deal
    bool acceptDeal(const std::string& dealId, double capitalToInvest, int currentQuarter) {
        auto it = std::find_if(availableDeals_.begin(), availableDeals_.end(),
            [&](const DealOpportunity& d) { return d.id == dealId; });
        if (it == availableDeals_.end()) return false;

        ActiveDeal active;
        active.opportunity = *it;
        active.startQuarter = currentQuarter;
        active.resolveQuarter = currentQuarter + it->risk.durationQuarters;
        active.investedCapital = capitalToInvest;
        active.status = DealStatus::Active;

        activeDeals_.push_back(std::move(active));
        availableDeals_.erase(it);

        spdlog::info("Deal accepted: {} (${:.0f} invested, resolves Q{})",
            activeDeals_.back().opportunity.title,
            capitalToInvest, activeDeals_.back().resolveQuarter);
        return true;
    }

    // Resolve mature deals — returns resolved deals with outcomes.
    // ceoPersuasion (0..1): scales realized outcome by ±20%. Persuasive CEOs
    // negotiate better closing terms; analytical-only CEOs underperform on
    // relationship-driven deals.
    std::vector<ActiveDeal> resolveDeals(int currentQuarter, double ceoPersuasion = 0.5) {
        std::vector<ActiveDeal> resolved;

        auto it = activeDeals_.begin();
        while (it != activeDeals_.end()) {
            if (currentQuarter >= it->resolveQuarter) {
                // Generate outcome using RiskReturnProfile
                double outcome = it->opportunity.risk.generateOutcome(rng_);
                // Persuasion modifier: ±20% swing on outcome magnitude
                double persuasionMod = 1.0 + (ceoPersuasion - 0.5) * 0.4;
                outcome *= persuasionMod;
                it->realizedReturn = it->investedCapital * outcome;

                if (outcome > it->opportunity.risk.returnMax * 1.5) {
                    it->status = DealStatus::Windfall;
                } else if (outcome > 0) {
                    it->status = DealStatus::Succeeded;
                } else {
                    it->status = DealStatus::Failed;
                }

                resolved.push_back(*it);
                resolvedHistory_.push_back(*it);
                it = activeDeals_.erase(it);
            } else {
                ++it;
            }
        }

        return resolved;
    }

    const std::vector<DealOpportunity>& availableDeals() const { return availableDeals_; }
    const std::vector<ActiveDeal>& activeDeals() const { return activeDeals_; }
    const std::vector<ActiveDeal>& resolvedHistory() const { return resolvedHistory_; }

    int totalActiveDeals() const { return (int)activeDeals_.size(); }
    int totalResolvedDeals() const { return (int)resolvedHistory_.size(); }

    // ── JSON Template Loading ─────────────────────────────────
    struct DealTemplate {
        std::string title, description, requiredDivision;
        int eraStart, eraEnd;
        int riskLevel;
        double successProbability;
        int durationQuarters;
        double investmentRequired;
    };

    void loadTemplates() {
        if (templatesLoaded_) return;
        for (const auto& path : {"data/deals/post_war_deals.json",
                                  "data/deals/deregulation_deals.json",
                                  "data/deals/modern_deals.json",
                                  "../data/deals/post_war_deals.json",
                                  "../data/deals/deregulation_deals.json",
                                  "../data/deals/modern_deals.json"}) {
            std::ifstream f(path);
            if (!f.is_open()) continue;
            try {
                auto j = nlohmann::json::parse(f);
                for (const auto& d : j) {
                    DealTemplate t;
                    t.title = d.value("title", "");
                    t.description = d.value("description", "");
                    t.requiredDivision = d.value("requiredDivision", "");
                    if (d.contains("eraRange") && d["eraRange"].size() >= 2) {
                        t.eraStart = d["eraRange"][0];
                        t.eraEnd = d["eraRange"][1];
                    }
                    if (d.contains("risk")) {
                        t.riskLevel = d["risk"].value("riskLevel", 5);
                        t.successProbability = d["risk"].value("successProbability", 0.5);
                        t.durationQuarters = d["risk"].value("duration", 4);
                    }
                    t.investmentRequired = d.value("investmentRequired", 50e6);
                    dealTemplates_.push_back(t);
                }
            } catch (...) {}
        }
        templatesLoaded_ = true;
        if (!dealTemplates_.empty()) {
            spdlog::info("Loaded {} deal templates from JSON", dealTemplates_.size());
        }
    }

    int templateCount() const { return (int)dealTemplates_.size(); }

private:
    math::RandomEngine& rng_;
    std::vector<DealOpportunity> availableDeals_;
    std::vector<ActiveDeal> activeDeals_;
    std::vector<ActiveDeal> resolvedHistory_;
    std::vector<DealTemplate> dealTemplates_;
    bool templatesLoaded_ = false;
    int dealCounter_ = 0;

    std::string generateClientName() {
        static const std::vector<std::string> prefixes = {
            "Acme", "Pacific", "Continental", "Meridian", "Sterling",
            "Atlas", "Pinnacle", "Monarch", "Northstar", "Vanguard",
            "Apex", "Summit", "Harbor", "Ironclad", "Keystone",
            "Centennial", "Trident", "Golden Gate", "Titan", "Osprey"};
        static const std::vector<std::string> suffixes = {
            "Industries", "Corp", "Holdings", "Group", "Partners",
            "Enterprises", "Capital", "Properties", "Technologies", "Financial",
            "Resources", "Dynamics", "Solutions", "International", "Ventures"};
        return prefixes[(int)(rng_.uniform() * prefixes.size()) % prefixes.size()]
             + " " + suffixes[(int)(rng_.uniform() * suffixes.size()) % suffixes.size()];
    }

    void generateDealForType(DealOpportunity& deal, DivisionType type, int gameYear) {
        switch (type) {
            case DivisionType::CommercialLending:
                if (rng_.bernoulli(0.5)) {
                    deal.title = "Corporate Credit Facility";
                    deal.description = deal.clientName + " needs a $" + std::to_string((int)(rng_.uniform() * 50 + 10)) + "M revolving credit facility.";
                    deal.risk = {3, 0.04, 0.08, 0.85, 4, 0.10, 0.03, 1.5};
                } else {
                    deal.title = "Real Estate Development Loan";
                    deal.description = deal.clientName + " is developing a mixed-use project and needs construction financing.";
                    deal.risk = {5, 0.08, 0.18, 0.65, 6, 0.20, 0.08, 2.0};
                }
                break;

            case DivisionType::MortgageLending:
                deal.title = "Mortgage Pool Origination";
                deal.description = "Originate and hold a pool of " + std::to_string((int)(rng_.uniform() * 200 + 50)) + " residential mortgages.";
                deal.risk = {3, 0.03, 0.07, 0.85, 4, 0.08, 0.05, 1.5};
                break;

            case DivisionType::InvestmentBanking:
                if (rng_.bernoulli(0.5)) {
                    deal.title = "IPO Underwriting";
                    deal.description = deal.clientName + " wants to go public. Underwriting fee 5-7% of offering size.";
                    deal.risk = {6, 0.15, 0.40, 0.55, 2, 0.15, 0.12, 3.0};
                } else {
                    deal.title = "M&A Advisory";
                    deal.description = deal.clientName + " is exploring a strategic acquisition. Advisory fee on close.";
                    deal.risk = {5, 0.10, 0.30, 0.50, 3, 0.08, 0.10, 2.5};
                }
                break;

            case DivisionType::PrivateEquity:
                deal.title = "Leveraged Buyout";
                deal.description = "Take " + deal.clientName + " private via LBO. Restructure and exit in 3-5 years.";
                deal.risk = {8, 0.20, 1.00, 0.45, 6, 0.35, 0.15, 4.0};
                break;

            case DivisionType::VentureCapital:
                deal.title = "Startup Seed Investment";
                deal.description = deal.clientName + " is a pre-revenue startup with a compelling thesis. High risk, transformative upside.";
                deal.risk = {9, -0.80, 5.00, 0.20, 8, 0.80, 0.05, 10.0};
                break;

            case DivisionType::TradingDesk:
                deal.title = "Macro Directional Bet";
                deal.description = "Conviction trade: your desk sees a mispricing in " + std::string(rng_.bernoulli(0.5) ? "rates" : "FX") + ". High leverage opportunity.";
                deal.risk = {8, 0.10, 2.00, 0.40, 1, 0.30, 0.10, 5.0};
                break;

            case DivisionType::Restructuring:
                deal.title = "Distressed Debt Purchase";
                deal.description = "Buy " + deal.clientName + " bonds at " + std::to_string(20 + (int)(rng_.uniform() * 30)) + " cents on the dollar. Restructuring in progress.";
                deal.risk = {7, 0.25, 0.60, 0.50, 4, 0.25, 0.10, 3.0};
                break;

            case DivisionType::Securitization:
                deal.title = "Loan Pool Securitization";
                deal.description = "Package and sell a $" + std::to_string((int)(rng_.uniform() * 500 + 100)) + "M loan pool to institutional investors.";
                deal.risk = {5, 0.05, 0.15, 0.70, 2, 0.10, 0.05, 2.0};
                break;

            case DivisionType::AssetManagement:
                deal.title = "Fund Launch";
                deal.description = "Launch a new fund targeting " + std::string(rng_.bernoulli(0.5) ? "growth equities" : "fixed income") + ". AUM fee revenue.";
                deal.risk = {4, 0.02, 0.12, 0.60, 4, 0.05, 0.08, 2.0};
                break;

            default:
                deal.title = "Business Opportunity";
                deal.description = deal.clientName + " presents an expansion opportunity for " + deal.clientName + ".";
                deal.risk = {4, 0.05, 0.15, 0.60, 3, 0.10, 0.05, 2.0};
                break;
        }
    }
};

} // namespace stvg::simulation
