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

// STAR_02 P7: a player-facing loan-outcome event. Emitted when an accepted deal
// matures and its outcome is rolled (paid_off / defaulted / windfall). The
// frontend uses these to bark the credit officer, fill the Loan Book strip, and
// list recent results in Financials. `realizedPnl` is the net P&L on the stake
// (positive on a win, negative on a default); `quartersHeld` is the holding
// period. These are pure surfacing — the capital move already happened in
// onQuarterEnd when resolveDeals() returned the deal.
struct DealOutcome {
    std::string dealId;
    std::string title;
    std::string clientName;
    std::string outcome;        // "paid_off" | "defaulted" | "windfall"
    double investedCapital = 0;
    double realizedPnl = 0;     // net P&L on the stake (realizedReturn)
    int quartersHeld = 0;
    int resolvedQuarter = 0;    // global quarter index at resolution
};

inline void to_json(nlohmann::json& j, const DealOutcome& o) {
    j = nlohmann::json{
        {"dealId", o.dealId}, {"title", o.title}, {"clientName", o.clientName},
        {"outcome", o.outcome}, {"investedCapital", o.investedCapital},
        {"realizedPnl", o.realizedPnl}, {"quartersHeld", o.quartersHeld},
        {"resolvedQuarter", o.resolvedQuarter}
    };
}

// STAR_02 P7: cumulative loan-book scoreboard (the "loany" heart). Accumulated
// across the whole game as deals resolve. Surfaced in the player view so the
// Loan Book rail's Book strip and the Financials Loan Book card can show the
// running tally without recomputing from history every frame.
struct LoanBookStats {
    int accepted = 0;       // total deals ever accepted
    int paidOff = 0;        // resolved as a clean success
    int defaulted = 0;      // resolved as a loss
    int windfalls = 0;      // resolved as an outsized win
    double realizedPnl = 0; // cumulative net P&L across all resolved deals
};

inline void to_json(nlohmann::json& j, const LoanBookStats& s) {
    j = nlohmann::json{
        {"accepted", s.accepted}, {"paidOff", s.paidOff},
        {"defaulted", s.defaulted}, {"windfalls", s.windfalls},
        {"realizedPnl", s.realizedPnl}
    };
}

// ════════════════════════════════════════════════════════════════════

class DealPortfolio {
public:
    explicit DealPortfolio(math::RandomEngine& rng) : rng_(rng) {}

    // Build a single deal opportunity for a given business line (template or procedural).
    DealOpportunity makeDeal(const Division& div, int gameYear) {
        DealOpportunity deal;
        deal.id = "deal_" + std::to_string(++dealCounter_);
        deal.requiredLine = div.type;
        deal.clientName = generateClientName();

        // Prefer a hand-authored era-appropriate template when available.
        bool usedTemplate = false;
        if (!dealTemplates_.empty() && rng_.bernoulli(0.6)) {
            std::vector<const DealTemplate*> eligible;
            for (const auto& t : dealTemplates_) {
                if (gameYear >= t.eraStart && gameYear <= t.eraEnd) eligible.push_back(&t);
            }
            if (!eligible.empty()) {
                const auto& tmpl = *eligible[(int)(rng_.uniform() * eligible.size()) % eligible.size()];
                deal.title = tmpl.title;
                deal.description = tmpl.description;
                deal.risk = {tmpl.riskLevel, 0.0, 0.0, tmpl.successProbability,
                             tmpl.durationQuarters, 0.1, 0.05, 2.0};
                // Size the deal proportionally to the bank (the template provides
                // flavor + risk, not an absolute dollar amount that breaks at scale).
                deal.investmentRequired = std::max(div.budget * (0.04 + rng_.uniform() * 0.08), 10.0);
                usedTemplate = true;
            }
        }
        if (!usedTemplate) {
            generateDealForType(deal, div.type, gameYear);
            deal.investmentRequired = div.budget * (0.05 + rng_.uniform() * 0.15);
            deal.investmentRequired = std::max(deal.investmentRequired, 10.0);   // floor rescaled /10,000
        }
        // STAR_02 P6 ReputationLens deal-risk tilt (weight-only, no RNG): a
        // gunslinger shop's flow skews riskier (higher upside, lower hit rate);
        // a fortress bank's skews safer. Bounded so a single tilt never makes a
        // deal impossible or risk-free.
        if (dealRiskTilt_ != 1.0) {
            deal.risk.returnMax *= dealRiskTilt_;
            deal.risk.riskLevel = std::clamp(
                (int)std::lround(deal.risk.riskLevel * dealRiskTilt_), 1, 10);
            deal.risk.successProbability = std::clamp(
                deal.risk.successProbability / std::sqrt(dealRiskTilt_), 0.05, 0.97);
        }
        return deal;
    }

    // STAR_02 P6 ReputationLens: bias the deal flow's risk. >1 tilts toward
    // riskier prospects (gunslinger shop), <1 toward safer (fortress bank).
    // Weight-only on the GENERATED deal's risk profile — no extra RNG draw, so
    // it does not change the deal-generation RNG sequence. 1.0 = neutral.
    void setDealRiskTilt(double tilt) { dealRiskTilt_ = std::clamp(tilt, 0.5, 2.0); }

    // Generate 0-2 deal opportunities per active business line (quarter-start seed).
    void generateOpportunities(const std::vector<Division>& divisions, int gameYear) {
        availableDeals_.clear();
        loadTemplates(); // Lazy-load JSON templates on first call

        for (const auto& div : divisions) {
            if (!rng_.bernoulli(0.4)) continue;        // 40% chance per line
            if ((int)availableDeals_.size() >= maxAvailable_) break;
            availableDeals_.push_back(makeDeal(div, gameYear));
        }
        spdlog::info("Generated {} deal opportunities", availableDeals_.size());
    }

    // Intra-quarter trickle: append ONE fresh opportunity, capped, so deals
    // flow into the loan book continuously (~every 15-30s of real time) rather
    // than only at quarter boundaries. Called from the daily simulation tick.
    void addTrickleDeal(const std::vector<Division>& divisions, int gameYear) {
        if (divisions.empty()) return;
        if ((int)availableDeals_.size() >= maxAvailable_) return;
        loadTemplates();
        const auto& div = divisions[(size_t)(rng_.uniform() * divisions.size()) % divisions.size()];
        availableDeals_.push_back(makeDeal(div, gameYear));
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

        // P7: count every accepted deal into the loan-book scoreboard.
        loanBookStats_.accepted++;

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

                if (outcome > it->opportunity.risk.returnMax * 1.5
                    && outcome > 0.0) {
                    it->status = DealStatus::Windfall;
                } else if (outcome >= 0.0) {
                    // A non-negative resolution PAID OFF. Only a genuine loss is a
                    // default — a flat (zero-return) close on a template deal whose
                    // returnMin==returnMax==0 is NOT a default (it would otherwise
                    // surface as "defaulted, $0 loss", which is nonsense).
                    it->status = DealStatus::Succeeded;
                } else {
                    it->status = DealStatus::Failed;
                }

                // P7: emit a player-facing outcome event + accumulate the
                // loan-book scoreboard. realizedPnl is the net P&L on the stake
                // (positive on a win, negative on a default). This is pure
                // surfacing — the capital move happens in onQuarterEnd from the
                // returned `resolved` vector; we don't touch capital here.
                DealOutcome ev;
                ev.dealId = it->opportunity.id;
                ev.title = it->opportunity.title;
                ev.clientName = it->opportunity.clientName;
                ev.outcome = it->status == DealStatus::Windfall ? "windfall"
                           : it->status == DealStatus::Failed   ? "defaulted"
                                                                : "paid_off";
                ev.investedCapital = it->investedCapital;
                ev.realizedPnl = it->realizedReturn;
                ev.quartersHeld = it->resolveQuarter - it->startQuarter;
                ev.resolvedQuarter = currentQuarter;
                dealOutcomes_.push_back(ev);
                if ((int)dealOutcomes_.size() > kMaxOutcomes_)
                    dealOutcomes_.erase(dealOutcomes_.begin());
                loanBookStats_.realizedPnl += it->realizedReturn;
                if (it->status == DealStatus::Windfall)      loanBookStats_.windfalls++;
                else if (it->status == DealStatus::Failed)   loanBookStats_.defaulted++;
                else                                         loanBookStats_.paidOff++;

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

    // P7: player-facing loan-outcome feed (most-recent-last, capped) + the
    // cumulative loan-book scoreboard. Both surfaced in the player view.
    const std::vector<DealOutcome>& dealOutcomes() const { return dealOutcomes_; }
    const LoanBookStats& loanBookStats() const { return loanBookStats_; }

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
    std::vector<DealOutcome> dealOutcomes_;     // P7: player-facing outcome feed (capped)
    LoanBookStats loanBookStats_;               // P7: cumulative loan-book scoreboard
    static constexpr int kMaxOutcomes_ = 50;    // keep the feed bounded
    std::vector<DealTemplate> dealTemplates_;
    bool templatesLoaded_ = false;
    int dealCounter_ = 0;
    int maxAvailable_ = 8;          // cap on simultaneously-shown prospects
    double dealRiskTilt_ = 1.0;     // STAR_02 P6 ReputationLens deal-risk bias

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
