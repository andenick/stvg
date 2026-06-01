#pragma once

#include "../core/Types.h"
#include "../core/BankState.h"
#include "../math/RandomEngine.h"
#include "DecisionEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cmath>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Character System — advisors with personalities and biases
// Characters give conflicting advice, colored by their traits.
// ════════════════════════════════════════════════════════════════════

enum class CharacterBias { RiskAverse, ProfitSeeking, RuleFocused, Political, ShareholderFocused };

struct Character {
    std::string id;
    std::string name;
    std::string role;
    CharacterBias bias;
    double trustworthiness = 70;  // 0-100 (hidden from player)
    double competence = 70;       // 0-100
    double loyalty = 70;          // 0-100
    std::string catchphrase;      // Signature reaction

    // Generate recommendation for a decision option
    CharacterRecommendation recommend(const DecisionOption& option) const {
        CharacterRecommendation rec;
        rec.characterId = id;
        rec.characterName = name;

        // Recommendation based on bias + option's risk/financial profile
        double riskScore = option.riskImpact.riskChange;
        double revenueScore = option.financialImpact.expectedRevenue;
        double costScore = option.financialImpact.immediateCost;

        switch (bias) {
            case CharacterBias::RiskAverse:
                if (riskScore > 0.05) {
                    rec.recommendation = "Oppose";
                    rec.argument = "The risk profile is concerning. " + option.riskImpact.description;
                } else if (riskScore < -0.05) {
                    rec.recommendation = "Support";
                    rec.argument = "This reduces our risk exposure. Prudent move.";
                } else {
                    rec.recommendation = "Neutral";
                    rec.argument = "Risk-neutral option. Acceptable.";
                }
                break;

            case CharacterBias::ProfitSeeking:
                if (revenueScore > costScore * 1.5) {
                    rec.recommendation = "Support";
                    rec.argument = "The return potential justifies the investment.";
                } else if (revenueScore < costScore * 0.5) {
                    rec.recommendation = "Oppose";
                    rec.argument = "The numbers don't work. We're leaving money on the table.";
                } else {
                    rec.recommendation = "Neutral";
                    rec.argument = "Marginal opportunity. Could go either way.";
                }
                break;

            case CharacterBias::RuleFocused:
                if (riskScore > 0.1 || option.successProbability < 0.5) {
                    rec.recommendation = "Oppose";
                    rec.argument = "This may violate our compliance framework. Regulatory scrutiny is high.";
                } else {
                    rec.recommendation = "Support";
                    rec.argument = "Within acceptable compliance boundaries.";
                }
                break;

            case CharacterBias::Political:
                if (option.successProbability > 0.6) {
                    rec.recommendation = "Support";
                    rec.argument = "I can make this work with the right connections.";
                } else {
                    rec.recommendation = "Neutral";
                    rec.argument = "Let me see what I can do behind the scenes.";
                }
                break;

            case CharacterBias::ShareholderFocused:
                if (revenueScore > 0) {
                    rec.recommendation = "Support";
                    rec.argument = "Shareholders will appreciate the growth initiative.";
                } else if (costScore > 0 && revenueScore <= 0) {
                    rec.recommendation = "Oppose";
                    rec.argument = "The board won't approve spending without clear ROI.";
                } else {
                    rec.recommendation = "Neutral";
                    rec.argument = "Present both sides to the board. Let them weigh in.";
                }
                break;
        }

        return rec;
    }
};

inline void to_json(nlohmann::json& j, const Character& c) {
    j = nlohmann::json{
        {"id", c.id}, {"name", c.name}, {"role", c.role},
        {"competence", c.competence}, {"loyalty", c.loyalty},
        {"catchphrase", c.catchphrase}
        // trustworthiness is hidden from player
    };
}

// Quarterly message from a character about their area
struct CharacterMessage {
    std::string characterId;
    std::string characterName;
    std::string role;
    std::string content;
    std::string tone;       // "worried", "confident", "neutral", "urgent"
    double sentiment;       // -1 to 1 (negative = worried)
};

inline void to_json(nlohmann::json& j, const CharacterMessage& m) {
    j = nlohmann::json{
        {"characterId", m.characterId}, {"characterName", m.characterName},
        {"role", m.role}, {"content", m.content},
        {"tone", m.tone}, {"sentiment", m.sentiment}
    };
}

// ════════════════════════════════════════════════════════════════════
// Character Engine — manages the 5 core team members
// ════════════════════════════════════════════════════════════════════

class CharacterEngine {
public:
    CharacterEngine() { initCoreTeam(); }

    // Add character recommendations to all decision options
    void addRecommendations(std::vector<Decision>& decisions) {
        for (auto& decision : decisions) {
            for (auto& option : decision.options) {
                for (const auto& character : coreTeam_) {
                    option.recommendations.push_back(character.recommend(option));
                }
            }
        }
    }

    // Generate quarterly briefing messages
    std::vector<CharacterMessage> generateBriefings(const Bank& bank,
                                                     const SimulationState& state) {
        std::vector<CharacterMessage> messages;

        // CRO — always worried about risk
        {
            CharacterMessage msg;
            msg.characterId = coreTeam_[0].id;
            msg.characterName = coreTeam_[0].name;
            msg.role = coreTeam_[0].role;
            double riskLevel = bank.totalReportedRisk();
            if (riskLevel > 0.5 || state.economics.vix > 25) {
                msg.content = "I'm deeply concerned about our exposure. VIX is at "
                    + std::to_string((int)state.economics.vix)
                    + " and our risk metrics are elevated. We need to reduce positions immediately.";
                msg.tone = "urgent";
                msg.sentiment = -0.8;
            } else {
                msg.content = "Risk levels are within acceptable bounds this quarter, "
                    "but I'd recommend building additional reserves. Markets feel fragile.";
                msg.tone = "worried";
                msg.sentiment = -0.3;
            }
            messages.push_back(msg);
        }

        // Chief Trader — always optimistic about profits
        {
            CharacterMessage msg;
            msg.characterId = coreTeam_[1].id;
            msg.characterName = coreTeam_[1].name;
            msg.role = coreTeam_[1].role;
            const auto* tradingDiv = bank.getDivisionByType(DivisionType::TradingDesk);
            double tradingPnL = tradingDiv ? (tradingDiv->revenue - tradingDiv->costs) : 0.0;
            if (tradingPnL > 0) {
                msg.content = "Trading desk is on fire! We're up $"
                    + std::to_string((int)(tradingPnL / 1e6))
                    + "M this quarter. The team is performing. Let's increase our limits.";
                msg.tone = "confident";
                msg.sentiment = 0.8;
            } else {
                msg.content = "We had a tough quarter, but the thesis is intact. "
                    "Markets will turn. Give me more capital and we'll make it back.";
                msg.tone = "confident";
                msg.sentiment = 0.2;
            }
            // Low-trustworthiness trader may underreport issues
            if (coreTeam_[1].trustworthiness < 60 && tradingDiv && tradingDiv->hiddenRisk > 0) {
                msg.content += " Everything else is under control."; // Hiding problems
            }
            messages.push_back(msg);
        }

        // Compliance Officer — focuses on regulations
        {
            CharacterMessage msg;
            msg.characterId = coreTeam_[2].id;
            msg.characterName = coreTeam_[2].name;
            msg.role = coreTeam_[2].role;
            if (bank.totalAssets > 100e9) {
                msg.content = "At our current asset size, we're subject to enhanced supervision. "
                    "I need additional staff to maintain compliance. Several reporting deadlines are approaching.";
                msg.tone = "neutral";
                msg.sentiment = -0.2;
            } else {
                msg.content = "Compliance is clean this quarter. All filings current. "
                    "One minor documentation gap in commercial lending — I've flagged it.";
                msg.tone = "neutral";
                msg.sentiment = 0.1;
            }
            messages.push_back(msg);
        }

        // Lobbyist — political angle
        {
            CharacterMessage msg;
            msg.characterId = coreTeam_[3].id;
            msg.characterName = coreTeam_[3].name;
            msg.role = coreTeam_[3].role;
            msg.content = "I've been working my contacts on the Hill. "
                "The regulatory environment is " +
                std::string(state.economics.fedFundsRate > 0.05 ? "tightening" : "loosening")
                + ". I can arrange some meetings if you want to get ahead of it.";
            msg.tone = "confident";
            msg.sentiment = 0.3;
            messages.push_back(msg);
        }

        // Board Chair — focused on shareholder returns
        {
            CharacterMessage msg;
            msg.characterId = coreTeam_[4].id;
            msg.characterName = coreTeam_[4].name;
            msg.role = coreTeam_[4].role;
            double roe = (bank.capital > 0) ? bank.netIncome / bank.capital : 0;
            if (roe > 0.05) {
                msg.content = "The board is pleased with this quarter's performance. "
                    "Keep this trajectory and we'll discuss a dividend increase.";
                msg.tone = "confident";
                msg.sentiment = 0.7;
            } else {
                msg.content = "Shareholders are getting restless. ROE is "
                    + std::to_string((int)(roe * 100)) + "%. "
                    "The board expects at least 15%. What's your plan to improve profitability?";
                msg.tone = "worried";
                msg.sentiment = -0.5;
            }
            messages.push_back(msg);
        }

        return messages;
    }

    const std::vector<Character>& coreTeam() const { return coreTeam_; }
    const std::vector<std::string>& departureHeadlines() const { return departureHeadlines_; }

    // Quarterly evolution: loyalty drift, competence growth, departure checks.
    // ceoLeadership (0..1): high leaders buffer crisis morale damage and steadily lift loyalty.
    // ceoIntegrity (0..1):  honest CEOs raise team trustworthiness; corrupt CEOs erode it.
    std::vector<std::string> updateQuarterly(bool crisisOccurred, math::RandomEngine& rng,
                                              double ceoLeadership = 0.5,
                                              double ceoIntegrity = 0.5) {
        departureHeadlines_.clear();
        for (auto& c : coreTeam_) {
            // Crisis penalty to loyalty (leadership softens it: -3 base → -0.5 to -3)
            if (crisisOccurred) {
                double damage = std::max(0.5, 3.0 - ceoLeadership * 2.0);
                c.loyalty = std::clamp(c.loyalty - damage, 0.0, 100.0);
            }

            // Leadership lifts loyalty steadily (max +1.5/quarter at leadership=1)
            c.loyalty = std::clamp(c.loyalty + ceoLeadership * 1.5, 0.0, 100.0);

            // Integrity drift on team trustworthiness
            if (ceoIntegrity > 0.7) {
                c.trustworthiness = std::clamp(c.trustworthiness + 0.5, 0.0, 100.0);
            } else if (ceoIntegrity < 0.3) {
                c.trustworthiness = std::clamp(c.trustworthiness - 0.5, 0.0, 100.0);
            }

            // Gentle mean-reversion toward 65 (prevents runaway drift)
            c.loyalty += (65.0 - c.loyalty) * 0.02;

            // Small competence growth
            c.competence = std::clamp(c.competence + 0.3, 0.0, 100.0);

            // Departure check: loyalty < 20 -> 30% chance per quarter
            if (c.loyalty < 20.0 && rng.bernoulli(0.3)) {
                std::string headline = c.name + " (" + c.role + ") has resigned!";
                departureHeadlines_.push_back(headline);
                spdlog::info("DEPARTURE: {}", headline);

                // Replace with same role/bias, fresh stats
                std::string newName = generateReplacementName(c.role, rng);
                c.name = newName;
                c.trustworthiness = 50.0 + rng.uniform() * 30.0; // 50-80
                c.competence = 50.0 + rng.uniform() * 20.0; // 50-70
                c.loyalty = 60.0 + rng.uniform() * 20.0; // 60-80
                c.catchphrase = "I'm the new " + c.role + ".";

                departureHeadlines_.push_back("New " + c.role + " " + newName + " joins the team.");
            }
        }
        return departureHeadlines_;
    }

private:
    std::vector<Character> coreTeam_;
    std::vector<std::string> departureHeadlines_;

    std::string generateReplacementName(const std::string& role, math::RandomEngine& rng) {
        static const std::vector<std::string> firstNames = {
            "Alex", "Jordan", "Morgan", "Casey", "Taylor", "Jamie", "Robin",
            "Drew", "Quinn", "Avery", "Riley", "Sage", "Reese", "Dakota"
        };
        static const std::vector<std::string> lastNames = {
            "Chen", "Patel", "Williams", "Garcia", "Kim", "Singh", "Mueller",
            "Okafor", "Tanaka", "Fernandez", "Brooks", "Novak", "Hassan", "Park"
        };
        int fi = static_cast<int>(rng.uniform() * firstNames.size()) % firstNames.size();
        int li = static_cast<int>(rng.uniform() * lastNames.size()) % lastNames.size();
        return firstNames[fi] + " " + lastNames[li];
    }

    void initCoreTeam() {
        coreTeam_ = {
            {"char_cro", "Dr. Pat Martinez", "Chief Risk Officer",
             CharacterBias::RiskAverse, 85, 80, 90,
             "We're going to blow up!"},

            {"char_trader", "Marcus Kane", "Chief Trader",
             CharacterBias::ProfitSeeking, 60, 75, 55,
             "We made $50M today!"},

            {"char_compliance", "Janet Winters", "Chief Compliance Officer",
             CharacterBias::RuleFocused, 90, 85, 95,
             "This violates regulations..."},

            {"char_lobbyist", "Ray Kensington", "Chief Lobbyist",
             CharacterBias::Political, 45, 65, 40,
             "I can make problems disappear"},

            {"char_board", "Margaret Ashford", "Board Chair",
             CharacterBias::ShareholderFocused, 70, 70, 75,
             "Shareholders demand 20% ROE!"}
        };
    }
};

} // namespace stvg::simulation
