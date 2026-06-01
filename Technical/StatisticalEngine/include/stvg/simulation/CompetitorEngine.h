#pragma once

#include "../core/Types.h"
#include "../core/BankState.h"
#include "../core/PathEngine.h"
#include "../core/PersonalityProfile.h"
#include "../math/RandomEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Persistent AI Competitor Banks — Civilization-style rivals that
// evolve alongside the player across the entire 1945-2040 game.
// All AI behavior is driven by the unified PersonalityProfile.
// ════════════════════════════════════════════════════════════════════

struct CompetitorBank {
    std::string id;
    std::string name;
    std::string ceoName;
    std::string archetype;     // "traditionalist", "gunslinger", "innovator", "politician", "survivor"
    PersonalityProfile personality;

    double totalAssets;
    double capital;
    double revenue = 0;
    double netIncome = 0;
    double reputation = 50.0;
    double marketShare = 0;

    int foundedYear;
    bool alive = true;
    std::string failureReason;
    int failureYear = -1;

    std::vector<std::string> recentDecisions;
    double totalLoans = 0;  // SFC Phase C: lending book for credit impulse
};

struct CompetitorStanding {
    std::string id;
    std::string name;
    std::string archetype;
    double totalAssets;
    double marketShare;
    double reputation;
    bool alive;
    int rank = 0;
};

inline void to_json(nlohmann::json& j, const CompetitorStanding& s) {
    j = nlohmann::json{
        {"id", s.id}, {"name", s.name}, {"archetype", s.archetype},
        {"totalAssets", s.totalAssets}, {"marketShare", s.marketShare},
        {"reputation", s.reputation}, {"alive", s.alive}, {"rank", s.rank}
    };
}

inline void to_json(nlohmann::json& j, const CompetitorBank& b) {
    j = nlohmann::json{
        {"id", b.id}, {"name", b.name}, {"ceoName", b.ceoName},
        {"archetype", b.archetype}, {"totalAssets", b.totalAssets},
        {"capital", b.capital}, {"revenue", b.revenue}, {"netIncome", b.netIncome},
        {"reputation", b.reputation}, {"marketShare", b.marketShare},
        {"foundedYear", b.foundedYear}, {"alive", b.alive},
        {"failureReason", b.failureReason}, {"failureYear", b.failureYear},
        {"recentDecisions", b.recentDecisions}
    };
}

class CompetitorEngine {
public:
    explicit CompetitorEngine(math::RandomEngine& rng) : rng_(rng) {}

    void init(int startYear) {
        competitors_.clear();

        // 4 founding competitors (1945) — personality drawn from canonical presets
        competitors_.push_back({
            "continental", "Continental Trust", "Harold Whitfield",
            "traditionalist", PersonalityProfile::conservative(),
            8e9, 1.2e9, 0, 0, 55, 0.08, 1945, true, "", -1, {}
        });
        competitors_.push_back({
            "apex", "Apex Securities", "Frank DiMaggio",
            "gunslinger", PersonalityProfile::aggressive(),
            5e9, 0.8e9, 0, 0, 45, 0.05, 1945, true, "", -1, {}
        });
        competitors_.push_back({
            "pacific", "Pacific National", "Eleanor Sato",
            "innovator", PersonalityProfile::wriston(),
            6e9, 1.0e9, 0, 0, 50, 0.06, 1945, true, "", -1, {}
        });
        competitors_.push_back({
            "federal_commerce", "Federal Commerce Bank", "Robert Kessler",
            "politician", PersonalityProfile::political(),
            7e9, 1.1e9, 0, 0, 60, 0.07, 1945, true, "", -1, {}
        });

        // Late entrants (stored but not active until their year)
        competitors_.push_back({
            "sterling", "Sterling Partners", "James Blackwell",
            "gunslinger", PersonalityProfile::gambler(),
            3e9, 0.5e9, 0, 0, 40, 0.03, 1982, true, "", -1, {}
        });
        competitors_.push_back({
            "novapay", "NovaPay", "Sarah Chen",
            "innovator", PersonalityProfile::griffin(),
            1e9, 0.2e9, 0, 0, 35, 0.01, 2012, true, "", -1, {}
        });

        for (auto& c : competitors_) c.totalLoans = c.totalAssets * 0.70;
        spdlog::info("CompetitorEngine initialized with {} banks", competitors_.size());
    }

    // Simulate one year for all active competitors.
    // playerPoliticalSavvy (0..1): when > 0.6, high-risk rivals temper their
    // aggression — they fear regulatory retaliation from a connected player.
    void simulateYear(int year, MarketRegime regime, double stressScore,
                      const Bank& playerBank, double playerPoliticalSavvy = 0.5) {
        double totalMarketAssets = playerBank.totalAssets;
        for (auto& comp : competitors_) {
            if (!comp.alive || year < comp.foundedYear) continue;
            totalMarketAssets += comp.totalAssets;
        }

        for (auto& comp : competitors_) {
            if (!comp.alive || year < comp.foundedYear) continue;
            simulateCompetitor(comp, year, regime, stressScore, totalMarketAssets, playerPoliticalSavvy);
        }
    }

    std::vector<CompetitorStanding> leaderboard(const Bank& playerBank) const {
        std::vector<CompetitorStanding> standings;

        // Add player
        standings.push_back({
            "player", playerBank.name, "player",
            playerBank.totalAssets, playerBank.marketShare,
            playerBank.reputation, true, 0
        });

        // Add competitors
        for (const auto& c : competitors_) {
            if (c.foundedYear > 2050) continue; // Skip future entrants
            standings.push_back({
                c.id, c.name, c.archetype,
                c.alive ? c.totalAssets : 0,
                c.alive ? c.marketShare : 0,
                c.reputation, c.alive, 0
            });
        }

        // Sort by total assets descending
        std::sort(standings.begin(), standings.end(),
            [](const auto& a, const auto& b) { return a.totalAssets > b.totalAssets; });

        for (int i = 0; i < (int)standings.size(); ++i) {
            standings[i].rank = i + 1;
        }

        return standings;
    }

    const std::vector<CompetitorBank>& competitors() const { return competitors_; }

    double sumCompetitorLoans() const {
        double sum = 0;
        for (const auto& c : competitors_) if (c.alive) sum += c.totalLoans;
        return sum;
    }

    const CompetitorBank* getCompetitor(const std::string& id) const {
        for (const auto& c : competitors_) {
            if (c.id == id) return &c;
        }
        return nullptr;
    }

    int activeCount(int year) const {
        int count = 0;
        for (const auto& c : competitors_) {
            if (c.alive && c.foundedYear <= year) count++;
        }
        return count;
    }

    // Get recently failed competitors (for acquisition opportunities)
    std::vector<const CompetitorBank*> recentFailures(int year) const {
        std::vector<const CompetitorBank*> failed;
        for (const auto& c : competitors_) {
            if (!c.alive && c.failureYear == year) {
                failed.push_back(&c);
            }
        }
        return failed;
    }

    nlohmann::json toJson() const {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& c : competitors_) j.push_back(c);
        return j;
    }

    nlohmann::json toPlayerJson(int currentYear) const {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& c : competitors_) {
            if (c.foundedYear > currentYear) continue;
            nlohmann::json cj;
            cj["id"] = c.id;
            cj["name"] = c.name;
            cj["ceoName"] = c.ceoName;
            cj["archetype"] = c.archetype;
            cj["totalAssets"] = c.alive ? c.totalAssets : 0;
            cj["reputation"] = c.reputation;
            cj["marketShare"] = c.marketShare;
            cj["alive"] = c.alive;
            cj["failureReason"] = c.failureReason;
            cj["failureYear"] = c.failureYear;
            cj["recentDecisions"] = c.recentDecisions;
            j.push_back(cj);
        }
        return j;
    }

private:
    math::RandomEngine& rng_;
    std::vector<CompetitorBank> competitors_;

    void simulateCompetitor(CompetitorBank& comp, int year, MarketRegime regime,
                            double stressScore, double totalMarketAssets,
                            double playerPoliticalSavvy = 0.5) {
        comp.recentDecisions.clear();

        // Base growth rate depends on personality and era conditions
        double growthRate = 0.03; // 3% base annual growth
        const auto& p = comp.personality;

        // Personality-driven adjustments
        if (regime == MarketRegime::Crisis || regime == MarketRegime::Stressed) {
            // Crisis: aggressive banks suffer, conservative banks hold
            growthRate -= p.riskTolerance * 0.08;
            growthRate += (1.0 - p.riskTolerance) * 0.02;
            // High compliance focus reduces crisis losses (defensive posture pays off)
            growthRate += p.complianceFocus * 0.015;

            if (p.riskTolerance > 0.7 && stressScore > 70) {
                // High-risk bank in severe crisis: chance of failure
                // Compliance focus reduces failure probability significantly
                double failProb = (stressScore - 70) / 100.0 * p.riskTolerance;
                failProb *= (1.0 - p.complianceFocus * 0.6);
                if (rng_.bernoulli(failProb * 0.1)) {
                    comp.alive = false;
                    comp.failureYear = year;
                    comp.failureReason = "Collapsed during " + std::to_string(year) + " crisis";
                    spdlog::warn("COMPETITOR FAILURE: {} ({})", comp.name, comp.failureReason);
                    return;
                }
                comp.recentDecisions.push_back("Scrambling to survive the crisis");
            }
        } else {
            // Normal times: aggressive banks grow faster
            growthRate += p.riskTolerance * 0.04;
            growthRate += p.growthAmbition * 0.02;
            // Long-term focus trades immediate growth for sustainability (slight drag)
            growthRate -= (p.longTermFocus - 0.5) * 0.01;
        }

        // Adaptability helps in all conditions
        growthRate += p.adaptability * 0.01;

        // Player politicalSavvy effect: high-risk rivals temper aggression
        // when the player has DC connections (fear of regulatory retaliation)
        if (playerPoliticalSavvy > 0.6 && p.riskTolerance > 0.6) {
            growthRate -= 0.02;
            comp.recentDecisions.push_back("Tempering aggression — player has DC connections");
        }

        // Apply growth with noise
        double noise = (rng_.uniform() - 0.5) * 0.04;
        comp.totalAssets *= (1.0 + growthRate + noise);
        comp.capital = comp.totalAssets * 0.10;
        comp.totalLoans = comp.totalAssets * 0.70; // SFC Phase C: loan book tracks assets
        comp.revenue = comp.totalAssets * (0.02 + rng_.uniform() * 0.02);
        comp.netIncome = comp.revenue * (0.15 + (rng_.uniform() - 0.5) * 0.1);

        // Update market share
        if (totalMarketAssets > 0) {
            comp.marketShare = comp.totalAssets / totalMarketAssets;
        }

        // Reputation evolves slowly — integrity influences direction
        double repDelta = (comp.netIncome > 0 ? 0.5 : -1.0);
        repDelta += (p.integrity - 0.5) * 0.4; // honest banks gain rep faster
        comp.reputation = std::clamp(comp.reputation + repDelta, 0.0, 100.0);

        // Generate decision narrative based on dominant personality trait
        if (p.riskTolerance > 0.6 && regime != MarketRegime::Crisis) {
            comp.recentDecisions.push_back("Expanding aggressively into new markets");
        } else if (p.growthAmbition > 0.6) {
            comp.recentDecisions.push_back("Investing in new technology and instruments");
        } else if (p.politicalSavvy > 0.6) {
            comp.recentDecisions.push_back("Lobbying for favorable regulatory changes");
        } else if (p.complianceFocus > 0.7) {
            comp.recentDecisions.push_back("Strengthening risk controls and audits");
        } else {
            comp.recentDecisions.push_back("Focusing on core operations");
        }
    }
};

} // namespace stvg::simulation
