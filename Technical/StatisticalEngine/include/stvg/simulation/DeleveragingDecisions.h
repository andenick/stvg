#pragma once

#include "../core/BankState.h"
#include "../core/GameConfig.h"
#include "DecisionEngine.h"
#include <algorithm>
#include <string>
#include <vector>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Deleveraging Decisions — three generators that offer the player
// ways to reduce leverage and step back from the regulatory_seizure
// cliff (capitalRatio < 0.02). Without these, revenue-maximizer bots
// have no way to survive — the decision pool only ever offered them
// growth options.
//
// All three are CONDITIONAL — they only appear when triggers fire,
// keeping the decision pool from being cluttered.
// ════════════════════════════════════════════════════════════════════

class DeleveragingDecisions {
public:
    // ── 1a. Special Dividend ────────────────────────────────────────
    //
    // Trigger: capRatio < kBalance.dividendOfferCapRatio OR periodic offer
    // Effect: pay() option immediately reduces capital by the configured
    //         payout fraction, lowering leverage. Reputation gains via
    //         the existing consequence tracker.
    static bool shouldOfferDividend(const Bank& bank, int globalQuarter) {
        double cr = bank.capitalRatio();
        if (cr < kBalance.dividendOfferCapRatio) return true;
        if (cr >= kBalance.survivalReleaseCapRatio - 0.01 && cr <= 0.20
            && (globalQuarter % 12) == 0) return true;
        return false;
    }

    static Decision generateDividendDecision(const Bank& bank, int globalQuarter) {
        Decision d;
        d.id = "dec_dividend_q" + std::to_string(globalQuarter);
        d.title = "Special Dividend Payout?";
        d.description = "Capital ratio is " +
            std::to_string((int)(bank.capitalRatio() * 1000) / 10.0) +
            "%. The board is asking whether to return capital to "
            "shareholders to ease leverage and signal confidence.";
        d.type = DecisionType::Deleveraging;
        d.urgency = DecisionUrgency::Standard;
        d.actionPointCost = 2;

        DecisionOption pay;
        pay.id = d.id + "_dividend_pay";
        pay.title = "Pay Special Dividend";
        pay.description = "Distribute $" +
            std::to_string((long long)(bank.capital * kBalance.dividendPayoutFraction / 1e6)) +
            "M to shareholders. Lowers leverage immediately.";
        pay.financialImpact.immediateCost = bank.capital * kBalance.dividendPayoutFraction;
        pay.financialImpact.recurringCost = 0;
        pay.financialImpact.expectedRevenue = 0;
        pay.financialImpact.timelineQuarters = 1;
        pay.riskImpact.riskChange = -kBalance.dividendPayoutFraction;
        pay.riskImpact.hiddenRiskChange = 0;
        pay.riskImpact.description = "Lower leverage, lower fragility";
        pay.successProbability = 1.0; // dividend always pays
        pay.longTermConsequences = {
            "Capital ratio improves immediately",
            "Shareholders rewarded — reputation ticks up",
            "Less ammunition for future growth"
        };

        DecisionOption skip;
        skip.id = d.id + "_dividend_skip";
        skip.title = "Reinvest — no dividend";
        skip.description = "Keep the capital on the balance sheet for future plays.";
        skip.financialImpact = {0, 0, 0, 1};
        skip.riskImpact = {0, 0, "No change"};
        skip.successProbability = 1.0;

        d.options = {pay, skip};
        return d;
    }

    // ── 1b. Share Buyback ───────────────────────────────────────────
    //
    // Trigger: kBalance.buybackProfitStreak consecutive profitable quarters
    // Effect: capital reduction now (kBalance.buybackPayoutFraction),
    //         expected revenue uplift over 4 quarters via consequence tracker.
    static bool shouldOfferBuyback(int consecutiveProfitableQuarters,
                                    const Bank& bank) {
        return consecutiveProfitableQuarters >= kBalance.buybackProfitStreak
            && bank.capitalRatio() > kBalance.survivalOverrideCapRatio;
    }

    static Decision generateBuybackDecision(const Bank& bank, int globalQuarter) {
        Decision d;
        d.id = "dec_buyback_q" + std::to_string(globalQuarter);
        d.title = "Share Buyback Authorization?";
        d.description = "Four straight profitable quarters. The board is debating "
            "an aggressive share buyback to consolidate ownership and boost EPS.";
        d.type = DecisionType::Deleveraging;
        d.urgency = DecisionUrgency::Standard;
        d.actionPointCost = 2;

        DecisionOption execute;
        execute.id = d.id + "_buyback_execute";
        execute.title = "Execute Share Buyback";
        execute.description = "Repurchase shares worth $" +
            std::to_string((long long)(bank.capital * kBalance.buybackPayoutFraction / 1e6)) +
            "M. EPS accretion expected over the next year.";
        execute.financialImpact.immediateCost = bank.capital * kBalance.buybackPayoutFraction;
        execute.financialImpact.recurringCost = 0;
        execute.financialImpact.expectedRevenue = bank.capital * kBalance.buybackExpectedReturn;
        execute.financialImpact.timelineQuarters = 4;
        execute.riskImpact.riskChange = -0.03;
        execute.riskImpact.hiddenRiskChange = 0;
        execute.riskImpact.description = "Slightly lower leverage";
        execute.successProbability = 0.85;
        execute.longTermConsequences = {
            "Concentrated ownership",
            "Lower share count → higher EPS",
            "Less capital cushion if a crisis hits"
        };

        DecisionOption skip;
        skip.id = d.id + "_buyback_skip";
        skip.title = "Hold off";
        skip.description = "Not the right time. Preserve capital for opportunities.";
        skip.financialImpact = {0, 0, 0, 1};
        skip.riskImpact = {0, 0, "No change"};
        skip.successProbability = 1.0;

        d.options = {execute, skip};
        return d;
    }

    // ── 1c. Division Divestiture ────────────────────────────────────
    //
    // Trigger: kBalance.divestitureMinDivisions+ divisions AND capRatio < kBalance.divestitureCapRatio
    // Effect: each option sells one division for kBalance.divestitureSalePriceMult x book value,
    //         removing it from the bank entirely (eliminates ongoing costs and risk).
    static bool shouldOfferDivestiture(const Bank& bank) {
        return (int)bank.divisions.size() >= kBalance.divestitureMinDivisions
            && bank.capitalRatio() < kBalance.divestitureCapRatio;
    }

    static Decision generateDivestitureDecision(const Bank& bank, int globalQuarter) {
        Decision d;
        d.id = "dec_divest_q" + std::to_string(globalQuarter);
        d.title = "Divest a Division?";
        d.description = "Capital is tight. Selling a division would free up cash, "
            "trim costs, and shrink the leverage profile. Activists are circling.";
        d.type = DecisionType::Deleveraging;
        d.urgency = DecisionUrgency::Priority;
        d.actionPointCost = 2;

        // Find up to 3 candidates: lowest-revenue divisions first
        std::vector<const Division*> candidates;
        for (const auto& div : bank.divisions) candidates.push_back(&div);
        std::sort(candidates.begin(), candidates.end(),
            [](const Division* a, const Division* b) { return a->revenue < b->revenue; });
        if (candidates.size() > 3) candidates.resize(3);

        for (const auto* div : candidates) {
            DecisionOption sell;
            sell.id = d.id + "_divest_" + div->id;
            double salePrice = std::max(div->budget * kBalance.divestitureSalePriceMult, 1e8);
            sell.title = "Sell " + div->name;
            sell.description = "Divest " + div->name + " for $" +
                std::to_string((long long)(salePrice / 1e6)) + "M.";
            sell.financialImpact.immediateCost = -salePrice;  // negative = inflow
            sell.financialImpact.recurringCost = -div->costs; // future cost relief
            sell.financialImpact.expectedRevenue = 0;
            sell.financialImpact.timelineQuarters = 1;
            sell.riskImpact.riskChange = -0.10;
            sell.riskImpact.hiddenRiskChange = -div->hiddenRisk;
            sell.riskImpact.description = "Eliminates risk from divested unit";
            sell.successProbability = 0.9;
            sell.longTermConsequences = {
                "Capital injected, costs reduced",
                "Lost optionality on this business line",
                "Activist investors satisfied — for now"
            };
            d.options.push_back(sell);
        }

        DecisionOption hold;
        hold.id = d.id + "_divest_hold";
        hold.title = "Hold all divisions";
        hold.description = "We need every business line. Find another way to deleverage.";
        hold.financialImpact = {0, 0, 0, 1};
        hold.riskImpact = {0, 0, "No change"};
        hold.successProbability = 1.0;
        d.options.push_back(hold);

        return d;
    }
};

} // namespace stvg::simulation
