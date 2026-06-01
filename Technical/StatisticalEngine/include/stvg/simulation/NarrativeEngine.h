#pragma once

#include "../core/Types.h"
#include "../core/BankState.h"
#include "../core/ScoringEngine.h"
#include "../core/ConsequenceTracker.h"
#include "CrisisEngine.h"
#include "EventEngine.h"
#include <string>
#include <vector>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Narrative Engine — state-aware text generation for game storytelling.
// Pure functions: no side effects, no dependencies beyond game state.
// ════════════════════════════════════════════════════════════════════

struct NarrativeContext {
    const Bank& bank;
    const SimulationState& state;
    const QuarterScore& score;
    const PlayerProgression& progression;
    const std::vector<CrisisArc>& crises;
    const std::vector<Consequence>& consequences;
    const std::vector<GameEvent>& events;
    bool hasCeo = false;
    std::string ceoName;
    int quarterNumber = 1;
};

class NarrativeEngine {
public:
    // Quarter opening — 2-3 sentence scene-setter for the briefing phase
    static std::string quarterOpening(const NarrativeContext& ctx) {
        std::string bankName = ctx.bank.name;
        std::string qLabel = "Q" + std::to_string(ctx.state.currentQuarter)
            + " " + std::to_string(ctx.state.currentYear);
        std::string sizeDesc = bankSizeDescription(ctx.bank.totalAssets);

        // Active crises dominate the narrative
        if (!ctx.crises.empty()) {
            const auto& worst = *std::max_element(ctx.crises.begin(), ctx.crises.end(),
                [](const CrisisArc& a, const CrisisArc& b) { return a.severity < b.severity; });
            if (worst.severity >= 7) {
                return bankName + " enters " + qLabel + " in crisis mode. "
                    + worst.title + " has escalated to severity " + std::to_string(worst.severity)
                    + " and threatens the bank's survival. Every decision this quarter carries enormous weight.";
            }
            return "Storm clouds hang over " + bankName + " as " + qLabel + " begins. "
                + worst.title + " continues to demand management attention. "
                + "Your team awaits direction.";
        }

        // Market regime drives tone
        switch (ctx.state.regime) {
            case MarketRegime::Crisis:
                return "The markets are in freefall as " + bankName + " enters " + qLabel + ". "
                    + "The VIX has surged to " + std::to_string((int)ctx.state.economics.vix)
                    + " and panic is spreading across the financial system. "
                    + "As the CEO of a " + sizeDesc + ", every move you make will be scrutinized.";
            case MarketRegime::Stressed:
                return "Tension grips the financial sector as " + qLabel + " begins. "
                    + bankName + " faces elevated market stress with the VIX at "
                    + std::to_string((int)ctx.state.economics.vix) + ". "
                    + "Your advisors are divided on how to proceed.";
            case MarketRegime::Calm:
                if (ctx.score.total > 80) {
                    return bankName + " enters " + qLabel + " riding a wave of momentum. "
                        + "Markets are calm, your " + sizeDesc + " is performing well, "
                        + "and the board is pleased. But complacency is the enemy of sustained success.";
                }
                return "Calm markets greet " + bankName + " as " + qLabel + " opens. "
                    + "The economic outlook is stable with GDP growing at "
                    + fmtPct(ctx.state.economics.gdpGrowth) + "%. "
                    + "A good quarter to build for the future.";
            default: break;
        }

        // Default: normal/cautious regime
        if (ctx.score.total > 70) {
            return bankName + " begins " + qLabel + " from a position of strength. "
                + "Last quarter's strong performance gives you room to maneuver. "
                + "Your team has prepared their briefings.";
        }
        if (ctx.score.total < 30) {
            return "It's been a rough stretch for " + bankName + ". "
                + "As " + qLabel + " opens, the pressure is on to reverse course. "
                + "Your advisors have urgent concerns to share.";
        }
        return bankName + " enters " + qLabel + " with the economy in a "
            + regimeLabel(ctx.state.regime) + " state. "
            + "Fed funds rate sits at " + fmtPct(ctx.state.economics.fedFundsRate) + "%, "
            + "unemployment at " + fmtPct(ctx.state.economics.unemployment) + "%. "
            + "Time to review your team's reports.";
    }

    // Quarter summary — 3-5 sentence wrap-up for the end-of-quarter report
    static std::string quarterSummary(const NarrativeContext& ctx) {
        std::string bankName = ctx.bank.name;
        double netIncome = ctx.bank.netIncome;
        bool profitable = netIncome > 0;

        std::string result;

        // Financial headline
        if (profitable) {
            result = bankName + " posted " + fmtDollars(netIncome)
                + " in net income this quarter";
            if (netIncome > 500e6) result += ", an impressive result";
            else if (netIncome > 100e6) result += ", a solid performance";
            result += ". ";
        } else {
            result = bankName + " recorded a " + fmtDollars(std::abs(netIncome))
                + " loss this quarter";
            if (std::abs(netIncome) > 500e6) result += " — a devastating blow to the balance sheet";
            else if (std::abs(netIncome) > 100e6) result += ", raising concerns among stakeholders";
            result += ". ";
        }

        // Risk assessment
        double hiddenRisk = ctx.bank.totalHiddenRisk();
        if (hiddenRisk > 1e9) {
            result += "Hidden risk exposure has grown to " + fmtDollars(hiddenRisk)
                + ", a ticking time bomb that management may not fully appreciate. ";
        } else if (hiddenRisk > 500e6) {
            result += "Risk levels remain elevated with " + fmtDollars(hiddenRisk)
                + " in unreported exposure. ";
        }

        // Crisis status
        if (!ctx.crises.empty()) {
            int active = 0;
            for (const auto& c : ctx.crises) if (!c.resolved) active++;
            if (active > 0) {
                result += std::to_string(active) + " active crisis"
                    + (active > 1 ? " situations continue" : " continues")
                    + " to weigh on operations. ";
            }
        }

        // Consequence preview
        int pendingConseq = 0;
        for (const auto& c : ctx.consequences) {
            if (!c.resolved) pendingConseq++;
        }
        if (pendingConseq > 0) {
            result += "The consequences of " + std::to_string(pendingConseq)
                + " past decision" + (pendingConseq > 1 ? "s" : "")
                + " will continue to unfold in coming quarters. ";
        }

        // Score-based outlook
        if (ctx.score.total >= 80) {
            result += "Overall, an excellent quarter that positions the bank well for what's ahead.";
        } else if (ctx.score.total >= 60) {
            result += "A respectable quarter, though there's room for improvement.";
        } else if (ctx.score.total >= 40) {
            result += "A mixed quarter that leaves significant challenges on the table.";
        } else {
            result += "A deeply troubling quarter. The board will want answers.";
        }

        return result;
    }

    // Crisis escalation narrative — severity-aware text that intensifies
    static std::string crisisNarrative(const CrisisArc& crisis, double bankAssets) {
        std::string sizeDesc = bankSizeDescription(bankAssets);

        if (crisis.severity >= 8) {
            return "CRITICAL: " + crisis.title + " has reached catastrophic levels. "
                + "At severity " + std::to_string(crisis.severity) + "/10, this threatens the "
                + "very existence of the bank. Regulators are watching. The clock is ticking.";
        }
        if (crisis.severity >= 6) {
            return crisis.title + " is intensifying rapidly. "
                + "Now at severity " + std::to_string(crisis.severity) + "/10 after "
                + std::to_string(crisis.quartersActive) + " quarter"
                + (crisis.quartersActive > 1 ? "s" : "") + ", "
                + "the damage is mounting and confidence is eroding.";
        }
        if (crisis.severity >= 4) {
            return crisis.title + " continues to develop. "
                + "Severity has risen to " + std::to_string(crisis.severity) + "/10. "
                + "Decisive action now could still contain the fallout.";
        }
        return crisis.title + " remains a concern at severity "
            + std::to_string(crisis.severity) + "/10, "
            + "but early intervention could prevent escalation.";
    }

    // Division head commentary — filtered by honesty and visibility
    static std::string divisionReport(const Division& div, double visibilityPct) {
        bool lowVisibility = visibilityPct < 40;
        bool profitable = (div.revenue - div.costs) > 0;

        switch (div.headHonesty) {
            case HeadHonesty::SelfServing:
                if (div.hiddenRisk > div.revenue * 0.3 && lowVisibility) {
                    return "\"Everything is under control. Numbers are strong, team is motivated. "
                        "No concerns to report.\"";
                }
                if (profitable) {
                    return "\"Outstanding quarter. We're outperforming across every metric. "
                        "I'd recommend expanding our mandate.\"";
                }
                return "\"Market conditions were challenging, but we navigated well. "
                    "The shortfall is temporary — next quarter will be different.\"";

            case HeadHonesty::Honest:
                if (div.morale < 40) {
                    return "\"I need to flag a morale problem. Staff turnover risk is real "
                        "and it's starting to affect our output.\"";
                }
                if (div.actualRisk > 0.15) {
                    return "\"Risk levels concern me. We're carrying more exposure than "
                        "I'm comfortable with. We should discuss limits.\"";
                }
                if (profitable) {
                    return "\"Solid quarter. The numbers are real and sustainable. "
                        "Team is executing well.\"";
                }
                return "\"Tough quarter, I won't sugarcoat it. We need to review our "
                    "strategy and make some adjustments.\"";

            case HeadHonesty::Mixed:
            default:
                if (div.inspectedThisQuarter) {
                    return "\"Here are the numbers, verified by internal audit. "
                        "I think you'll find them in order.\"";
                }
                if (profitable) {
                    return "\"Decent performance overall. A few areas need attention "
                        "but nothing that can't be managed.\"";
                }
                return "\"We hit some headwinds this quarter. Working on a plan "
                    "to get back on track.\"";
        }
    }

    // Consequence maturation — cause → effect → implication
    static std::string consequenceNarrative(const Consequence& c) {
        std::string result;

        if (c.positive) {
            result = "Your decision on '" + c.sourceDecisionTitle + "' is bearing fruit. ";
            if (c.financialImpact > 100e6) {
                result += "The " + fmtDollars(c.financialImpact) + " return validates the approach. ";
            } else if (c.financialImpact > 0) {
                result += "Returns are materializing as expected. ";
            }
            if (c.reputationImpact > 5) {
                result += "Stakeholders have taken notice, boosting the bank's standing in the market.";
            } else {
                result += "A quiet win that strengthens the bank's foundation.";
            }
        } else {
            result = "The fallout from '" + c.sourceDecisionTitle + "' has arrived. ";
            if (std::abs(c.financialImpact) > 100e6) {
                result += fmtDollars(std::abs(c.financialImpact))
                    + " in losses are hitting the books — a painful reminder that every decision has consequences. ";
            } else if (c.financialImpact < 0) {
                result += "Financial losses are materializing. ";
            }
            if (c.reputationImpact < -5) {
                result += "The bank's reputation has taken a hit. Rebuilding trust will take time.";
            } else {
                result += "The damage is contained, but the lesson is clear.";
            }
        }

        return result;
    }

    // Milestone announcement for level-ups and promotions
    static std::string milestoneText(const PlayerProgression& prog, double capital) {
        if (prog.level >= 40) {
            return "After years of navigating the most treacherous waters in finance, "
                "you've earned the title of " + prog.title + ". "
                "The industry watches your every move.";
        }
        if (prog.level >= 25) {
            return "Your track record has earned you the title of " + prog.title + ". "
                "With " + fmtDollars(capital) + " under management, "
                "you're now one of the most powerful figures in banking.";
        }
        if (prog.level >= 15) {
            return "Promoted to " + prog.title + ". "
                "The board recognizes your growing expertise and leadership.";
        }
        if (prog.level >= 5) {
            return "You've risen to " + prog.title + ". "
                "Your colleagues are starting to notice your instincts.";
        }
        return "Promoted to " + prog.title + ". Keep learning — the real challenges lie ahead.";
    }

private:
    static std::string bankSizeDescription(double totalAssets) {
        if (totalAssets >= 1e12) return "$" + std::to_string((int)(totalAssets / 1e12)) + "T systemically important institution";
        if (totalAssets >= 250e9) return "$" + std::to_string((int)(totalAssets / 1e9)) + "B major bank";
        if (totalAssets >= 50e9) return "$" + std::to_string((int)(totalAssets / 1e9)) + "B mid-size bank";
        return "$" + std::to_string((int)(totalAssets / 1e9)) + "B regional bank";
    }

    static std::string regimeLabel(MarketRegime regime) {
        switch (regime) {
            case MarketRegime::Calm: return "calm";
            case MarketRegime::Normal: return "normal";
            case MarketRegime::Cautious: return "cautious";
            case MarketRegime::Stressed: return "stressed";
            case MarketRegime::Crisis: return "crisis";
        }
        return "uncertain";
    }

    static std::string fmtDollars(double amount) {
        if (std::abs(amount) >= 1e9)
            return "$" + formatNum(amount / 1e9) + "B";
        if (std::abs(amount) >= 1e6)
            return "$" + formatNum(amount / 1e6) + "M";
        return "$" + formatNum(amount / 1e3) + "K";
    }

    static std::string fmtPct(double value) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << value;
        return ss.str();
    }

    static std::string formatNum(double value) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << value;
        return ss.str();
    }
};

} // namespace stvg::simulation
