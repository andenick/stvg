#pragma once

#include "../core/Types.h"
#include "../core/BankState.h"
#include "../math/RandomEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <numeric>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Event system — Reigns-style weighted event pool
// Events are drawn based on bank state, not pure randomness.
// ════════════════════════════════════════════════════════════════════

enum class EventCategory {
    Market, Regulatory, Personnel, Operational,
    Strategic, Competitive, Crisis, Opportunity
};
NLOHMANN_JSON_SERIALIZE_ENUM(EventCategory, {
    {EventCategory::Market, "market"}, {EventCategory::Regulatory, "regulatory"},
    {EventCategory::Personnel, "personnel"}, {EventCategory::Operational, "operational"},
    {EventCategory::Strategic, "strategic"}, {EventCategory::Competitive, "competitive"},
    {EventCategory::Crisis, "crisis"}, {EventCategory::Opportunity, "opportunity"},
})

struct GameEvent {
    std::string id;
    std::string title;
    std::string description;
    EventCategory category;
    double baseWeight = 1.0;
    bool isHistorical = false;
    std::string historicalNote;
    int eraEarliest = 1945;
    int eraLatest = 2040;

    std::vector<std::string> decisionIds;
};

// ════════════════════════════════════════════════════════════════════
// Event sign-bias (STAR_02 P3.4 Stage A)
//
// A directional read of the current macro/market state, derived from
// EconomicIndicators at the draw call site. Used to bias event-draw weights
// so the narrative matches the engine direction: crisis/bear-flavored events
// are up-weighted when the economy is turning down, boom/opportunity events
// when it is rising. This is weight biasing ONLY — it never adds or removes
// an rng_ draw, so the day-by-day vs batch RNG lockstep contract is preserved.
// ════════════════════════════════════════════════════════════════════

struct EventBias {
    // True when the macro/market picture is risk-off (recession/stress).
    bool bearish = false;
    // True when the macro/market picture is risk-on (boom/expansion).
    bool bullish = false;
    // Strength multiplier applied to the matching flavor (>= 1.0).
    double k = 1.0;

    // Derive a directional bias from the macro state. Cheap, deterministic.
    static EventBias fromEconomics(const EconomicIndicators& e) {
        EventBias b;
        // Risk-off score: negative growth, high vix, falling equities, wide spread.
        double bearScore = 0.0;
        if (e.gdpGrowth < 0.0)     bearScore += 1.0;
        if (e.vix > 28.0)          bearScore += 1.0;
        if (e.sp500Return < 0.0)   bearScore += 0.5;
        if (e.creditSpread > 0.025) bearScore += 0.5;

        double bullScore = 0.0;
        if (e.gdpGrowth > 0.03)    bullScore += 1.0;
        if (e.vix < 15.0)          bullScore += 0.5;
        if (e.sp500Return > 0.0)   bullScore += 0.5;

        if (bearScore > bullScore && bearScore > 0.0) {
            b.bearish = true;
            b.k = 1.0 + std::min(bearScore, 3.0); // up to 4x at full crisis
        } else if (bullScore > 0.0) {
            b.bullish = true;
            b.k = 1.0 + std::min(bullScore, 2.0); // up to 3x in a clear boom
        }
        return b;
    }

    // Multiplier for a given event category under this bias.
    double weightFor(EventCategory cat) const {
        if (bearish) {
            switch (cat) {
                case EventCategory::Crisis:      return k;
                case EventCategory::Market:      return 1.0 + 0.5 * (k - 1.0);
                case EventCategory::Opportunity: return 1.0 / k; // fewer in a downturn
                default:                         return 1.0;
            }
        }
        if (bullish) {
            switch (cat) {
                case EventCategory::Opportunity: return k;
                case EventCategory::Strategic:   return 1.0 + 0.5 * (k - 1.0);
                case EventCategory::Crisis:      return 1.0 / k; // fewer when rising
                default:                         return 1.0;
            }
        }
        return 1.0;
    }
};

inline void to_json(nlohmann::json& j, const GameEvent& e) {
    j = nlohmann::json{
        {"id", e.id}, {"title", e.title}, {"description", e.description},
        {"category", e.category}, {"baseWeight", e.baseWeight},
        {"isHistorical", e.isHistorical}, {"historicalNote", e.historicalNote},
        {"decisionIds", e.decisionIds}
    };
}

// Weighted event pool — bank state determines which events surface
class EventPool {
public:
    EventPool() { buildStarterPool(); }

    std::vector<GameEvent> drawEvents(const Bank& bank,
                                       const SimulationState& simState,
                                       math::RandomEngine& rng,
                                       int count = 5,
                                       const EventBias& bias = {}) {
        int year = simState.currentYear;

        std::vector<double> weights;
        weights.reserve(pool_.size());

        for (const auto& event : pool_) {
            if (year < event.eraEarliest || year > event.eraLatest) {
                weights.push_back(0.0);
                continue;
            }
            double w = event.baseWeight;
            w *= categoryWeight(event.category, bank, simState);
            // P3.4 Stage A: bias by current macro/market direction. Weight-only,
            // no extra rng draw → lockstep-safe.
            w *= bias.weightFor(event.category);
            weights.push_back(std::max(w, 0.01));
        }

        std::vector<GameEvent> drawn;
        std::vector<bool> used(pool_.size(), false);

        for (int i = 0; i < count && i < (int)pool_.size(); ++i) {
            double totalWeight = 0;
            for (size_t j = 0; j < weights.size(); ++j) {
                if (!used[j]) totalWeight += weights[j];
            }
            if (totalWeight <= 0) break;

            double r = rng.uniform() * totalWeight;
            double cumulative = 0;
            for (size_t j = 0; j < pool_.size(); ++j) {
                if (used[j]) continue;
                cumulative += weights[j];
                if (cumulative >= r) {
                    drawn.push_back(pool_[j]);
                    used[j] = true;
                    break;
                }
            }
        }

        return drawn;
    }

    size_t poolSize() const { return pool_.size(); }

private:
    std::vector<GameEvent> pool_;

    // Weight multiplier per category based on bank state
    double categoryWeight(EventCategory cat, const Bank& bank,
                          const SimulationState& simState) const {
        double stress = 0;
        if (simState.economics.vix > 25) stress += 1.0;
        if (simState.economics.unemployment > 0.06) stress += 0.5;

        switch (cat) {
            case EventCategory::Market:
                return 1.0 + stress; // More market events during volatile times
            case EventCategory::Regulatory:
                // More regulatory events with larger banks and low compliance
                return 0.5 + (bank.totalAssets / 500e9) + (100.0 - bank.reputation) / 100.0;
            case EventCategory::Personnel:
                // More personnel events when morale is low
                return 0.5 + (100.0 - bank.averageMorale()) / 100.0;
            case EventCategory::Operational:
                return 0.5 + bank.employees / 10000.0;
            case EventCategory::Strategic:
                return 1.0;
            case EventCategory::Competitive:
                return 0.5 + bank.marketShare * 20.0; // Bigger = more competition
            case EventCategory::Crisis:
                // Crisis events weighted by hidden risk
                return 0.2 + bank.totalHiddenRisk() / 1e9;
            case EventCategory::Opportunity:
                return 1.0 - stress * 0.3; // Fewer opportunities during stress
        }
        return 1.0;
    }

    void buildStarterPool() {
        int id = 0;
        auto add = [&](const std::string& title, const std::string& desc,
                       EventCategory cat, double weight = 1.0,
                       int earliest = 1945, int latest = 2040) {
            GameEvent e;
            e.id = "evt_" + std::to_string(++id);
            e.title = title;
            e.description = desc;
            e.category = cat;
            e.baseWeight = weight;
            e.eraEarliest = earliest;
            e.eraLatest = latest;
            pool_.push_back(std::move(e));
        };

        // Market events
        add("Fed Rate Decision Pending", "The Federal Reserve meets next week. Markets are pricing in a rate change.", EventCategory::Market, 1.5);
        add("Market Flash Crash", "A sudden liquidity event caused a 3% drop in equities within minutes.", EventCategory::Market, 0.8);
        add("Bond Market Rally", "Treasury yields have fallen sharply. Your fixed income portfolio is up.", EventCategory::Market, 1.0);
        add("Commodity Supercycle", "Commodity markets are surging. Gold and oil prices climbing rapidly.", EventCategory::Market, 0.9);
        add("Currency Volatility Spike", "The dollar moved sharply overnight on policy surprise.", EventCategory::Market, 0.7, 1971);
        add("Crypto Market Turbulence", "Bitcoin dropped 15% after major exchange insolvency rumors.", EventCategory::Market, 0.6, 2009);
        add("Housing Market Cooling", "New home sales fell 12% month-over-month. Mortgage applications declining.", EventCategory::Market, 1.0);
        add("IPO Window Opens", "Market conditions favor new equity issuance. Several companies preparing to list.", EventCategory::Opportunity, 1.2);

        // Regulatory events
        add("Regulatory Audit Notice", "OCC has scheduled a comprehensive examination of your lending practices.", EventCategory::Regulatory, 1.0);
        add("New Capital Requirements", "New regulatory framework requires additional capital buffer.", EventCategory::Regulatory, 0.8);
        add("Stress Test Results Due", "Annual stress test submissions deadline is approaching.", EventCategory::Regulatory, 1.0, 2011);
        add("Consumer Protection Complaint", "Regulators received complaints about your fee practices.", EventCategory::Regulatory, 0.7);
        add("Anti-Money Laundering Alert", "Suspicious activity detected in several commercial accounts.", EventCategory::Regulatory, 0.9);
        add("Lobbying Opportunity", "A sympathetic senator chairs the banking committee hearings.", EventCategory::Opportunity, 0.6);

        // Personnel events
        add("Star Trader Poached", "A competitor is offering your best trader 3x their current compensation.", EventCategory::Personnel, 1.2);
        add("Division Head Resignation", "Your head of commercial banking is leaving for a competitor.", EventCategory::Personnel, 0.8);
        add("Whistleblower Report", "An anonymous employee reported irregularities in the trading division.", EventCategory::Personnel, 1.0);
        add("Talent Acquisition Opportunity", "A top quantitative team was laid off. Available to hire.", EventCategory::Opportunity, 1.0, 1970);
        add("Employee Morale Crisis", "Annual survey shows 40% of staff considering leaving. Bonus season looming.", EventCategory::Personnel, 0.9);
        add("Training Program Results", "Your analyst development program produced three promotion-ready candidates.", EventCategory::Personnel, 0.5);

        // Operational events
        add("Technology System Failure", "Core banking platform experienced 4-hour outage. Customer complaints spiking.", EventCategory::Operational, 0.8, 1960);
        add("Cybersecurity Incident", "Attempted breach detected on customer database. No data compromised yet.", EventCategory::Operational, 0.7, 1995);
        add("Process Automation Proposal", "Management proposes investment in automated risk monitoring systems.", EventCategory::Operational, 0.6, 1990);
        add("Branch Consolidation Debate", "CFO suggests closing 3 underperforming branches to cut costs.", EventCategory::Operational, 0.7);

        // Strategic events
        add("Acquisition Target Identified", "A struggling regional bank with strong deposit base is available.", EventCategory::Strategic, 1.0);
        add("New Product Launch Decision", "Team proposes entering the wealth management market.", EventCategory::Strategic, 0.8);
        add("Geographic Expansion Opportunity", "Real estate available in a growing suburban market.", EventCategory::Strategic, 0.7);
        add("ESG Investment Mandate", "Board proposes committing to ESG lending standards.", EventCategory::Strategic, 0.6, 2000);

        // Competitive events
        add("Competitor Launches Rate War", "A rival bank cut their savings rate by 50bps. Depositors may move.", EventCategory::Competitive, 1.0);
        add("Fintech Disruption", "A startup is offering instant business loans at half your rate.", EventCategory::Competitive, 0.9, 2010);
        add("Major Client Defection", "Your third-largest commercial client is moving to a competitor.", EventCategory::Competitive, 1.0);
        add("Industry Award Nomination", "Your bank nominated for excellence — reputation boost if you win.", EventCategory::Opportunity, 0.5);

        // Crisis events
        add("Rogue Trader Suspected", "Internal audit flagged unusual position concentrations in the trading book.", EventCategory::Crisis, 0.5);
        add("Loan Portfolio Deterioration", "Non-performing loans jumped 30% this quarter. Commercial RE exposure concerning.", EventCategory::Crisis, 0.7);
        add("Liquidity Strain Warning", "Treasury desk reports difficulty funding short-term obligations.", EventCategory::Crisis, 0.6);
        add("Counterparty Default Risk", "A major derivatives counterparty missed a margin call.", EventCategory::Crisis, 0.5, 1970);

        // Additional Market Events
        add("Emerging Market Contagion", "Currency crisis in a developing nation is spreading to global markets. Your international exposure is at risk.", EventCategory::Market, 0.7, 1960);
        add("Oil Price Shock", "OPEC announces surprise production cuts. Energy sector and inflation expectations are shifting rapidly.", EventCategory::Market, 0.8, 1973);
        add("Treasury Auction Failure", "A Treasury auction received weak demand. Yields are spiking and bond portfolios are under pressure.", EventCategory::Market, 0.6);
        add("Sector Rotation Underway", "Investors are fleeing growth stocks for value. Your equity portfolio composition matters.", EventCategory::Market, 0.9, 1960);
        add("Credit Downgrade Cascade", "Rating agencies downgraded three major corporate borrowers overnight. Credit spreads widening.", EventCategory::Market, 0.7, 1970);
        add("Real Estate Boom", "Commercial real estate values have risen 15% this quarter. Lending opportunities abound, but is it a bubble?", EventCategory::Opportunity, 1.0);
        add("M&A Frenzy", "Corporate America is in deal-making mode. Companies seeking advisory, financing, and bridge loans.", EventCategory::Opportunity, 1.1, 1960);

        // Additional Personnel Events
        add("Key Client Relationship at Risk", "Your top commercial client's primary banker is considering retirement. The relationship may not survive the transition.", EventCategory::Personnel, 1.0);
        add("Trading Floor Morale Crisis", "Three traders submitted resignations this week. Compensation concerns are spreading across the desk.", EventCategory::Personnel, 0.9);
        add("Executive Poaching Attempt", "A competitor is systematically recruiting your senior talent with aggressive compensation packages.", EventCategory::Personnel, 1.1);
        add("Discrimination Lawsuit Filed", "A former employee has filed a discrimination suit. Media attention is growing.", EventCategory::Personnel, 0.7, 1965);
        add("Star Analyst Publishes Report", "Your top analyst published a contrarian market call that's attracting significant media attention.", EventCategory::Opportunity, 0.8);

        // Additional Regulatory Events
        add("Congressional Banking Hearing", "Your bank has been called to testify before the Senate Banking Committee on lending practices.", EventCategory::Regulatory, 0.8);
        add("FDIC Examination Surprise", "FDIC examiners have arrived unannounced for a comprehensive safety and soundness examination.", EventCategory::Regulatory, 0.9);
        add("New Data Privacy Regulation", "Sweeping new customer data protection rules require immediate compliance investment.", EventCategory::Regulatory, 0.6, 2000);
        add("Community Reinvestment Audit", "Regulators are reviewing whether your bank meets CRA obligations in underserved communities.", EventCategory::Regulatory, 0.5, 1977);

        // Additional Operational Events
        add("Core System Modernization Needed", "Your banking platform is increasingly unreliable. Technology debt is accumulating and competitors are pulling ahead.", EventCategory::Operational, 0.8, 1975);
        add("Natural Disaster Impact", "A hurricane/earthquake has damaged several branches and affected thousands of customers. Recovery costs mounting.", EventCategory::Operational, 0.5);
        add("Fraud Ring Discovered", "Internal investigation uncovered a sophisticated check fraud ring operating through your branches.", EventCategory::Operational, 0.7);
        add("Third-Party Vendor Failure", "A critical technology vendor has gone bankrupt. You need to find a replacement or build in-house capability.", EventCategory::Operational, 0.6, 1980);

        // Additional Strategic Events
        add("Failed Bank Available", "The FDIC is offering a failed bank for acquisition. Good assets but unknown liabilities. Due diligence period: 48 hours.", EventCategory::Strategic, 1.2);
        add("International Expansion Opportunity", "A correspondent bank in London/Tokyo wants to establish a partnership. Significant capital required.", EventCategory::Strategic, 0.9);
        add("Product Innovation Pressure", "Customers are demanding services you don't offer. Invest now or lose market share.", EventCategory::Strategic, 0.8);
        add("Board Succession Crisis", "Your board chair is stepping down unexpectedly. The vacancy creates both risk and opportunity.", EventCategory::Strategic, 0.7);

        // Additional Competitive Events
        add("Competitor Acquisition Announced", "A rival bank just acquired a major player in your market. Competitive dynamics are shifting.", EventCategory::Competitive, 1.0);
        add("Price War Escalation", "Three competitors have cut lending rates below cost. Market discipline is collapsing.", EventCategory::Competitive, 0.8);
        add("Fintech Disruption Accelerates", "A well-funded startup is offering instant small business loans at half your rate and twice the speed.", EventCategory::Competitive, 0.9, 2010);
        add("Client Poaching Campaign", "A competitor's relationship team is systematically targeting your top 20 commercial clients.", EventCategory::Competitive, 1.0);

        // Windfall / High-Drama Events
        add("Government Contract Windfall", "The federal government selects your bank as a primary lending partner for a major infrastructure program.", EventCategory::Opportunity, 0.5);
        add("Celebrity Client Signs On", "A high-profile individual wants to bring their entire portfolio to your bank. Prestige and assets.", EventCategory::Opportunity, 0.4);
        add("Whistleblower Goes Public", "An employee went directly to the press about alleged misconduct. Damage control is urgent.", EventCategory::Crisis, 0.6);
        add("Market Manipulation Allegations", "Regulators allege your traders engaged in market manipulation. Criminal investigation possible.", EventCategory::Crisis, 0.4, 1960);
    }
};

} // namespace stvg::simulation
