#pragma once

#include "../core/Types.h"
#include "../core/BankState.h"
#include "../math/RandomEngine.h"
#include "EventEngine.h"
#include "PoliticalEngine.h"
#include "ClimateEngine.h"
#include "AICEOEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// CEO Decision System
// Decisions have visible consequences (Into the Breach model).
// Each quarter, the CEO has an action point budget.
// ════════════════════════════════════════════════════════════════════

enum class DecisionType {
    AutonomyAdjustment, CapitalAllocation, Personnel,
    DealApproval, Investigation, CrisisResponse, Strategic,
    Deleveraging
};
NLOHMANN_JSON_SERIALIZE_ENUM(DecisionType, {
    {DecisionType::AutonomyAdjustment, "autonomy_adjustment"},
    {DecisionType::CapitalAllocation, "capital_allocation"},
    {DecisionType::Personnel, "personnel"},
    {DecisionType::DealApproval, "deal_approval"},
    {DecisionType::Investigation, "investigation"},
    {DecisionType::CrisisResponse, "crisis_response"},
    {DecisionType::Strategic, "strategic"},
    {DecisionType::Deleveraging, "deleveraging"},
})

enum class DecisionUrgency { Standard, Priority, Urgent, Immediate };
NLOHMANN_JSON_SERIALIZE_ENUM(DecisionUrgency, {
    {DecisionUrgency::Standard, "standard"},
    {DecisionUrgency::Priority, "priority"},
    {DecisionUrgency::Urgent, "urgent"},
    {DecisionUrgency::Immediate, "immediate"},
})

struct FinancialImpact {
    double immediateCost = 0;
    double recurringCost = 0;
    double expectedRevenue = 0;
    int timelineQuarters = 1;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FinancialImpact,
    immediateCost, recurringCost, expectedRevenue, timelineQuarters)

struct RiskImpact {
    double riskChange = 0;     // Positive = more risk, negative = less
    double hiddenRiskChange = 0;
    std::string description;
};

inline void to_json(nlohmann::json& j, const RiskImpact& r) {
    j = nlohmann::json{
        {"riskChange", r.riskChange},
        {"hiddenRiskChange", r.hiddenRiskChange},
        {"description", r.description}
    };
}

struct CharacterRecommendation {
    std::string characterId;
    std::string characterName;
    std::string recommendation;  // "Support" or "Oppose" or "Neutral"
    std::string argument;        // Their reasoning
};

inline void to_json(nlohmann::json& j, const CharacterRecommendation& r) {
    j = nlohmann::json{
        {"characterId", r.characterId}, {"characterName", r.characterName},
        {"recommendation", r.recommendation}, {"argument", r.argument}
    };
}

struct DecisionOption {
    std::string id;
    std::string title;
    std::string description;
    FinancialImpact financialImpact;
    RiskImpact riskImpact;
    double successProbability = 0.7;
    std::vector<CharacterRecommendation> recommendations;
    std::vector<std::string> longTermConsequences;
};

inline void to_json(nlohmann::json& j, const DecisionOption& o) {
    j = nlohmann::json{
        {"id", o.id}, {"title", o.title}, {"description", o.description},
        {"financialImpact", o.financialImpact}, {"riskImpact", o.riskImpact},
        {"successProbability", o.successProbability},
        {"recommendations", o.recommendations},
        {"longTermConsequences", o.longTermConsequences}
    };
}

struct Decision {
    std::string id;
    std::string title;
    std::string description;
    DecisionType type;
    DecisionUrgency urgency = DecisionUrgency::Standard;
    int actionPointCost = 1;
    std::string sourceEventId;
    std::vector<DecisionOption> options;
    bool resolved = false;
    std::string chosenOptionId;
};

inline void to_json(nlohmann::json& j, const Decision& d) {
    j = nlohmann::json{
        {"id", d.id}, {"title", d.title}, {"description", d.description},
        {"type", d.type}, {"urgency", d.urgency},
        {"actionPointCost", d.actionPointCost},
        {"sourceEventId", d.sourceEventId},
        {"options", d.options}, {"resolved", d.resolved}
    };
}

// ════════════════════════════════════════════════════════════════════
// Decision Engine — generates decisions from events
// ════════════════════════════════════════════════════════════════════

// Forward declarations for acquisition offers
struct AcquisitionOffer {
    std::string targetId;
    std::string targetName;
    double price;
    double assetsGained;
    double hiddenRisk;
    std::string archetype;
};

class DecisionEngine {
public:
    explicit DecisionEngine(math::RandomEngine& rng) : rng_(rng) {}

    void setPoliticalEngine(PoliticalEngine* pe) { politicalEngine_ = pe; }

    // Generate decisions from quarterly events
    std::vector<Decision> generateFromEvents(const std::vector<GameEvent>& events,
                                              const Bank& bank,
                                              const SimulationState& state) {
        std::vector<Decision> decisions;
        int id = 0;

        for (const auto& event : events) {
            Decision d;
            d.id = "dec_" + std::to_string(++id) + "_q" + std::to_string(state.currentQuarter);
            d.sourceEventId = event.id;

            // Generate decision based on event category
            switch (event.category) {
                case EventCategory::Market:
                    d = generateMarketDecision(d, event, bank);
                    break;
                case EventCategory::Regulatory:
                    d = generateRegulatoryDecision(d, event, bank);
                    break;
                case EventCategory::Personnel:
                    d = generatePersonnelDecision(d, event, bank);
                    break;
                case EventCategory::Crisis:
                    d = generateCrisisDecision(d, event, bank);
                    break;
                default:
                    d = generateGenericDecision(d, event, bank);
                    break;
            }

            decisions.push_back(std::move(d));
        }

        // Generate per-division audit options for uninspected divisions
        std::vector<const Division*> uninspected;
        for (const auto& d : bank.divisions)
            if (d.quartersSinceInspection > 0) uninspected.push_back(&d);

        if (!uninspected.empty()) {
            Decision inv;
            inv.id = "dec_inv_q" + std::to_string(state.currentQuarter);
            inv.title = "Launch Division Audit";
            inv.description = "Spend resources to investigate a division for hidden risks. "
                              "Each audit reveals true financial data and drains hidden risk.";
            inv.type = DecisionType::Investigation;
            inv.actionPointCost = 0; // Audits are free — knowledge shouldn't cost action points

            // One audit option per uninspected division
            for (const auto* div : uninspected) {
                DecisionOption opt;
                opt.id = "audit_" + div->id;
                opt.title = "Audit " + div->name;
                double urgency = div->quartersSinceInspection > 3 ? 1.5 : 1.0;
                opt.description = "Full audit of " + div->name + ". "
                    "Last inspected " + std::to_string(div->quartersSinceInspection) + " quarters ago.";
                opt.financialImpact.immediateCost = 2e6 * urgency;
                opt.riskImpact = {0, -0.3, "Reveals hidden risks in " + div->name};
                opt.successProbability = 0.85;
                if (div->quartersSinceInspection > 3) {
                    opt.longTermConsequences = {"Overdue — higher chance of finding problems"};
                }
                inv.options.push_back(opt);
            }

            // Always offer skip option
            DecisionOption skip;
            skip.id = "audit_skip";
            skip.title = "Skip Audit This Quarter";
            skip.description = "Save the cost and focus on other priorities.";
            skip.financialImpact.immediateCost = 0;
            skip.riskImpact = {0.05, 0, "Hidden risks continue to accumulate"};
            skip.successProbability = 1.0;
            inv.options.push_back(skip);

            decisions.push_back(inv);
        }

        return decisions;
    }

private:
    math::RandomEngine& rng_;
    PoliticalEngine* politicalEngine_ = nullptr;

    Decision generateMarketDecision(Decision d, const GameEvent& event, const Bank& bank) {
        d.title = event.title;
        d.description = event.description;
        d.type = DecisionType::CapitalAllocation;
        d.actionPointCost = 1;

        DecisionOption aggressive;
        aggressive.id = d.id + "_aggressive";
        aggressive.title = "Increase Exposure";
        aggressive.description = "Double down on the market opportunity.";
        aggressive.financialImpact = {0, 0, bank.capital * 0.02, 2};
        aggressive.riskImpact = {0.15, 0.05, "Higher market exposure"};
        aggressive.successProbability = 0.6;
        aggressive.longTermConsequences = {"Potential for outsized returns", "Increased concentration risk"};

        DecisionOption conservative;
        conservative.id = d.id + "_conservative";
        conservative.title = "Reduce Exposure";
        conservative.description = "Cut positions and preserve capital.";
        conservative.financialImpact = {bank.capital * 0.005, 0, 0, 1};
        conservative.riskImpact = {-0.1, 0, "Reduced market exposure"};
        conservative.successProbability = 0.9;
        conservative.longTermConsequences = {"Protected capital", "May miss rally"};

        d.options = {aggressive, conservative};
        return d;
    }

    Decision generateRegulatoryDecision(Decision d, const GameEvent& event, const Bank& bank) {
        d.title = event.title;
        d.description = event.description;
        d.type = DecisionType::Strategic;
        d.urgency = DecisionUrgency::Priority;
        d.actionPointCost = 2;

        DecisionOption comply;
        comply.id = d.id + "_comply";
        comply.title = "Full Compliance";
        comply.description = "Invest in compliance and cooperate fully.";
        comply.financialImpact = {bank.capital * 0.01, bank.capital * 0.002, 0, 4};
        comply.riskImpact = {-0.1, 0, "Improved regulatory standing"};
        comply.successProbability = 0.95;

        DecisionOption lobby;
        lobby.id = d.id + "_lobby";
        lobby.title = "Lobby to Reduce Requirements";
        // Dynamic success probability from PoliticalEngine
        double lobbyProb = 0.45;
        if (politicalEngine_) {
            double capMod = std::min(politicalEngine_->currentCapital() / 50.0, 1.5);
            lobbyProb = std::clamp(0.45 * politicalEngine_->effectiveness() * capMod, 0.05, 0.90);
        }
        lobby.description = "Use political connections to soften the regulation. Success: "
            + std::to_string((int)(lobbyProb * 100)) + "%";
        lobby.financialImpact = {2e6, 0, bank.capital * 0.005, 2};
        lobby.riskImpact = {0.05, 0.1, "Regulatory risk if lobbying fails"};
        lobby.successProbability = lobbyProb;

        d.options = {comply, lobby};
        return d;
    }

    Decision generatePersonnelDecision(Decision d, const GameEvent& event, const Bank& bank) {
        d.title = event.title;
        d.description = event.description;
        d.type = DecisionType::Personnel;
        d.actionPointCost = 1;

        DecisionOption retain;
        retain.id = d.id + "_retain";
        retain.title = "Match Offer / Retain Talent";
        retain.description = "Counter-offer to keep the employee.";
        retain.financialImpact = {500000, 200000, 0, 4};
        retain.riskImpact = {0, 0, "Maintains team stability"};
        retain.successProbability = 0.65;

        DecisionOption release;
        release.id = d.id + "_release";
        release.title = "Let Them Go";
        release.description = "Accept the departure and promote from within.";
        release.financialImpact = {100000, -150000, 0, 2};
        release.riskImpact = {0.05, 0, "Temporary skill gap"};
        release.successProbability = 0.8;

        d.options = {retain, release};
        return d;
    }

    Decision generateCrisisDecision(Decision d, const GameEvent& event, const Bank& bank) {
        d.title = event.title;
        d.description = event.description;
        d.type = DecisionType::CrisisResponse;
        d.urgency = DecisionUrgency::Immediate;
        d.actionPointCost = 3;

        DecisionOption aggressive;
        aggressive.id = d.id + "_aggressive";
        aggressive.title = "Aggressive Response";
        aggressive.description = "Immediately cut risk, fire responsible parties, full disclosure.";
        aggressive.financialImpact = {bank.capital * 0.03, 0, 0, 1};
        aggressive.riskImpact = {-0.3, -0.5, "Rapid risk reduction"};
        aggressive.successProbability = 0.75;
        aggressive.longTermConsequences = {"Fast recovery", "Morale damage", "Regulatory appreciation"};

        DecisionOption measured;
        measured.id = d.id + "_measured";
        measured.title = "Measured Response";
        measured.description = "Hire advisors, restructure over 2-3 quarters.";
        measured.financialImpact = {bank.capital * 0.01, bank.capital * 0.005, 0, 3};
        measured.riskImpact = {-0.1, -0.2, "Gradual de-risking"};
        measured.successProbability = 0.6;
        measured.longTermConsequences = {"Slower but steadier recovery", "Risk of escalation"};

        DecisionOption gamble;
        gamble.id = d.id + "_gamble";
        gamble.title = "Wait and Hope";
        gamble.description = "Minimal intervention. Hope conditions improve naturally.";
        gamble.financialImpact = {0, 0, 0, 1};
        gamble.riskImpact = {0.1, 0.2, "Risk continues to accumulate"};
        gamble.successProbability = 0.3;
        gamble.longTermConsequences = {"Cheap if it works", "Catastrophic if it doesn't"};

        d.options = {aggressive, measured, gamble};
        return d;
    }

    Decision generateGenericDecision(Decision d, const GameEvent& event, const Bank& bank) {
        d.title = event.title;
        d.description = event.description;
        d.type = DecisionType::Strategic;
        d.actionPointCost = 1;

        DecisionOption act;
        act.id = d.id + "_act";
        act.title = "Take Action";
        act.description = "Invest resources to capitalize on this.";
        act.financialImpact = {bank.capital * 0.005, 0, bank.capital * 0.01, 2};
        act.riskImpact = {0.05, 0, "Minor risk increase"};
        act.successProbability = 0.65;

        DecisionOption pass;
        pass.id = d.id + "_pass";
        pass.title = "Pass";
        pass.description = "Focus on existing priorities.";
        pass.financialImpact = {0, 0, 0, 0};
        pass.riskImpact = {0, 0, "No change"};
        pass.successProbability = 1.0;

        d.options = {act, pass};
        return d;
    }

public:
    // ── Climate Decision (annual, from 2020+) ────────────────────
    Decision generateClimateDecision(const Bank& bank, const ClimateState& climate) {
        static int climateId = 0;
        Decision d;
        d.id = "dec_climate_" + std::to_string(++climateId);
        d.title = "Climate Strategy";
        d.description = "Global warming is " + std::to_string((int)(climate.globalTemp * 100) / 100.0).substr(0,4)
            + "C above pre-industrial. Carbon price: $" + std::to_string((int)climate.carbonPrice)
            + "/ton. How should the bank respond?";
        d.type = DecisionType::Strategic;
        d.urgency = DecisionUrgency::Priority;
        d.actionPointCost = 2;

        DecisionOption green;
        green.id = d.id + "_green";
        green.title = "Green Lending Initiative";
        green.description = "Shift 20% of lending to green projects. Reduces fossil exposure.";
        green.financialImpact = {500e6, 0, bank.capital * 0.003, 2};
        green.riskImpact = {-0.05, 0, "Reduced climate exposure"};
        green.successProbability = 0.80;

        DecisionOption esg;
        esg.id = d.id + "_esg";
        esg.title = "ESG Bond Program";
        esg.description = "Launch ESG bond desk. Generates new revenue proportional to carbon price.";
        esg.financialImpact = {1e9, 0, bank.capital * 0.005, 4};
        esg.riskImpact = {0, 0, "New revenue stream"};
        esg.successProbability = 0.75;

        DecisionOption divest;
        divest.id = d.id + "_divest";
        divest.title = "Divest Fossil Assets";
        divest.description = "Immediate write-down but eliminates future stranded asset risk.";
        divest.financialImpact = {2e9, 0, 0, 1};
        divest.riskImpact = {-0.15, 0, "Eliminated fossil exposure"};
        divest.successProbability = 0.95;

        DecisionOption ignore;
        ignore.id = d.id + "_ignore";
        ignore.title = "Ignore Climate Risk";
        ignore.description = "Focus on short-term profitability.";
        ignore.financialImpact = {0, 0, 0, 0};
        ignore.riskImpact = {0.10, 0.05, "Growing climate exposure"};
        ignore.successProbability = 1.0;

        d.options = {green, esg, divest, ignore};
        return d;
    }

    // ── AI Counter-Decision (annual, after AI emergence) ─────────
    Decision generateAICounterDecision(const Bank& bank, const AICEOState& aiState) {
        static int aiDecId = 0;
        Decision d;
        d.id = "dec_ai_" + std::to_string(++aiDecId);
        d.title = "AI CEO Response";
        d.description = "The AI-driven bank has reached " + std::to_string((int)(aiState.efficiency * 10) / 10.0).substr(0,3)
            + "x efficiency and captured " + std::to_string((int)(aiState.marketCapture * 100))
            + "% of the market. How do you respond?";
        d.type = DecisionType::Strategic;
        d.urgency = DecisionUrgency::Urgent;
        d.actionPointCost = 2;

        double matchCost = aiState.efficiency * 10e9;

        DecisionOption invest;
        invest.id = d.id + "_invest";
        invest.title = "Match AI Investment";
        invest.description = "Invest $" + std::to_string((long long)(matchCost / 1e9)) + "B in AI capabilities.";
        invest.financialImpact = {matchCost, 0, matchCost * 0.3, 4};
        invest.riskImpact = {0.05, 0, "Heavy AI investment"};
        invest.successProbability = 0.70;

        DecisionOption regulate;
        regulate.id = d.id + "_regulate";
        regulate.title = "Lobby for AI Regulation";
        regulate.description = "Use political capital to slow AI advancement.";
        double regulateCost = 5e6; // Financial cost (political cost handled separately)
        regulate.financialImpact = {regulateCost, 0, 0, 2};
        regulate.riskImpact = {0, 0, "Regulatory approach to AI"};
        double regulateProb = 0.50;
        if (politicalEngine_) {
            regulateProb = std::clamp(0.5 * politicalEngine_->effectiveness(), 0.15, 0.85);
        }
        regulate.successProbability = regulateProb;

        DecisionOption acquire;
        acquire.id = d.id + "_acquire";
        acquire.title = "Acquire AI Startup";
        acquire.description = "High risk, high reward: buy cutting-edge AI talent.";
        acquire.financialImpact = {5e9, 0, 2e9, 4};
        acquire.riskImpact = {0.10, 0, "Startup integration risk"};
        acquire.successProbability = 0.40;

        DecisionOption partner;
        partner.id = d.id + "_partner";
        partner.title = "Partner with AI Bank";
        partner.description = "Share in AI efficiency gains at cost of some autonomy.";
        partner.financialImpact = {2e9, 0, aiState.efficiency * 1e9, 4};
        partner.riskImpact = {0.03, 0, "Dependency on AI partner"};
        partner.successProbability = 0.65;

        DecisionOption ignore;
        ignore.id = d.id + "_ignore_ai";
        ignore.title = "Ignore AI Threat";
        ignore.description = "Stick to traditional banking. Hope it works out.";
        ignore.financialImpact = {0, 0, 0, 0};
        ignore.riskImpact = {0, 0.05, "Falling behind in AI"};
        ignore.successProbability = 1.0;

        d.options = {invest, regulate, acquire, partner, ignore};
        return d;
    }

    // ── Acquisition Decision (when competitor fails) ─────────────
    Decision generateAcquisitionDecision(const AcquisitionOffer& offer) {
        Decision d;
        d.id = "dec_acquire_" + offer.targetId;
        d.title = "Acquire " + offer.targetName + "?";
        d.description = offer.targetName + " (" + offer.archetype + ") has failed. "
            "Assets available at $" + std::to_string((long long)(offer.price / 1e9)) + "B "
            "(15 cents on the dollar). Some assets may be toxic.";
        d.type = DecisionType::DealApproval;
        d.urgency = DecisionUrgency::Immediate;
        d.actionPointCost = 3;

        DecisionOption buy;
        buy.id = d.id + "_buy";
        buy.title = "Acquire " + offer.targetName;
        buy.description = "Pay $" + std::to_string((long long)(offer.price / 1e9))
            + "B for $" + std::to_string((long long)(offer.assetsGained / 1e9)) + "B in assets.";
        buy.financialImpact = {offer.price, 0, offer.assetsGained * 0.03, 4};
        buy.riskImpact = {0.10, offer.hiddenRisk / 1e9 * 0.05, "Inherited risk from failed bank"};
        buy.successProbability = 0.70;

        DecisionOption pass;
        pass.id = d.id + "_pass";
        pass.title = "Pass on " + offer.targetName;
        pass.description = "Let another bank pick up the pieces.";
        pass.financialImpact = {0, 0, 0, 0};
        pass.riskImpact = {0, 0, "No change"};
        pass.successProbability = 1.0;

        d.options = {buy, pass};
        return d;
    }

};

} // namespace stvg::simulation
