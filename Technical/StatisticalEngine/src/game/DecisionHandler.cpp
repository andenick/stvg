#include "stvg/game/DecisionHandler.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace stvg::game {

bool DecisionHandler::submit(
    const std::string& decisionId,
    const std::string& optionId,
    std::vector<simulation::Decision>& pendingDecisions,
    ActionPointBudget& actionPoints,
    Bank& bank,
    ConsequenceTracker& consequenceTracker,
    simulation::TraderAIEngine& traderAI,
    std::vector<simulation::Trader>& traders,
    int& recentDividendQuarters,
    const SimulationConfig& config,
    const SimulationState& state)
{
    for (auto& d : pendingDecisions) {
        if (d.id == decisionId && !d.resolved) {
            if (!actionPoints.spend(d.actionPointCost)) {
                spdlog::warn("Not enough action points for decision {}", decisionId);
                return false;
            }
            d.resolved = true;
            d.chosenOptionId = optionId;

            // Apply investigation if applicable — per-division audits
            if (d.type == simulation::DecisionType::Investigation
                && optionId.find("audit_") != std::string::npos
                && optionId != "audit_skip") {
                std::string divId = optionId.substr(6); // strip "audit_"
                if (auto* div = bank.getDivision(divId)) {
                    div->inspectedThisQuarter = true;
                    spdlog::info("Audit of {} revealed: actualRisk={:.4f}, hiddenRisk={:.4f}",
                        div->name, div->actualRisk, div->hiddenRisk);
                }
                if (divId == "div_trading" || divId.find("trading") != std::string::npos) {
                    if (!traders.empty()) {
                        double revealed = traderAI.investigateTrader(traders[0]);
                        spdlog::info("Trading audit revealed ${:.0f} in hidden exposure", std::abs(revealed));
                    }
                }
            }

            // Deleveraging decisions: direct state mutations
            if (d.type == simulation::DecisionType::Deleveraging) {
                if (optionId.find("dividend_pay") != std::string::npos) {
                    double payout = bank.capital * kBalance.dividendPayoutFraction;
                    bank.capital -= payout;
                    recentDividendQuarters = kBalance.dividendDampingQuarters;
                    spdlog::info("Special dividend paid: ${:.0f}M, new capital ${:.0f}M",
                        payout / 1e6, bank.capital / 1e6);
                } else if (optionId.find("buyback_execute") != std::string::npos) {
                    double cost = bank.capital * kBalance.buybackPayoutFraction;
                    bank.capital -= cost;
                    spdlog::info("Share buyback executed: ${:.0f}M, new capital ${:.0f}M",
                        cost / 1e6, bank.capital / 1e6);
                } else if (optionId.find("_divest_") != std::string::npos
                           && optionId.find("divest_hold") == std::string::npos) {
                    size_t pos = optionId.rfind("_divest_");
                    if (pos != std::string::npos) {
                        std::string divId = optionId.substr(pos + 8);
                        auto it = std::find_if(bank.divisions.begin(), bank.divisions.end(),
                            [&](const Division& dv) { return dv.id == divId; });
                        if (it != bank.divisions.end()) {
                            double salePrice = std::max(it->budget * kBalance.divestitureSalePriceMult, 1e8);
                            bank.capital += salePrice;
                            spdlog::info("Divested {}: +${:.0f}M, new capital ${:.0f}M",
                                it->name, salePrice / 1e6, bank.capital / 1e6);
                            bank.divisions.erase(it);
                        }
                    }
                }
            }

            // Banking policy decisions — direct state mutations
            if (decisionId.find("loanspread") != std::string::npos) {
                auto bpsPos = optionId.rfind('_');
                if (bpsPos != std::string::npos) {
                    try {
                        int bps = std::stoi(optionId.substr(bpsPos + 1));
                        bank.loanSpread = bps / 10000.0;
                        spdlog::info("Loan spread set to {}bps", bps);
                    } catch (...) {}
                }
            }
            if (decisionId.find("creditstd") != std::string::npos) {
                auto valPos = optionId.rfind('_');
                if (valPos != std::string::npos) {
                    try {
                        int pct = std::stoi(optionId.substr(valPos + 1));
                        bank.creditStandards = pct / 100.0;
                        spdlog::info("Credit standards set to {:.0f}%", bank.creditStandards * 100);
                    } catch (...) {}
                }
            }

            // Generate delayed consequences from this decision
            for (const auto& opt : d.options) {
                if (opt.id == optionId) {
                    int currentQ = state.currentQuarter + (state.currentYear - config.startYear) * 4;
                    consequenceTracker.addFromDecision(
                        decisionId, d.title, optionId, opt.title,
                        opt.financialImpact.expectedRevenue - opt.financialImpact.immediateCost,
                        opt.riskImpact.riskChange,
                        opt.successProbability,
                        currentQ
                    );
                    break;
                }
            }

            spdlog::info("Decision {} resolved with option {}", decisionId, optionId);
            return true;
        }
    }
    return false;
}

} // namespace stvg::game
