#pragma once

#include "../core/Types.h"
#include "../core/BankState.h"
#include "../core/PathEngine.h"
#include "../core/PersonalityProfile.h"
#include "../math/RandomEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
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

    // STAR_02 P6 (poaching): cumulative revenue this rival has booked since the
    // game began — the "this team made $X over Y years" track record a poach
    // offer is priced off. Accumulated in simulateCompetitor each year.
    double cumulativeRevenue = 0.0;
    int yearsOperating = 0;  // years this rival has been simulated (for "over Y years")
    double poachStrengthPenalty = 0.0; // 0..1 — accumulated drop from poached teams
    double aggressionVsPlayer = 0.0;   // 0..1 — rivalry: rises when you poach them
};

// ════════════════════════════════════════════════════════════════════
// PoachOffer (STAR_02 P6) — a rival's whole division put up for poaching.
// "This team made $X over Y years at RivalBank." Accepting deducts askPrice
// and imports the team's archetype mix + a chunk of their σ/β profile into
// the player's bank, drops the rival's strength, and raises their rivalry.
// ════════════════════════════════════════════════════════════════════
struct PoachOffer {
    std::string id;
    std::string rivalId;
    std::string rivalName;
    DivisionType divisionType;
    std::string divisionName;       // display name of the line
    int teamSize = 0;               // bankers in the team
    double trackRecord = 0.0;       // their cumulative revenue ("made $X")
    int yearsTrackRecord = 0;       // "over Y years"
    double askPrice = 0.0;          // cost to poach (multiple of trackRecord)
    std::vector<std::string> archetypeMix; // family ids the imported team carries
    bool active = true;
    int offeredQuarter = 0;
    int expiresQuarter = 0;
};

inline void to_json(nlohmann::json& j, const PoachOffer& o) {
    j = nlohmann::json{
        {"id", o.id}, {"rivalId", o.rivalId}, {"rivalName", o.rivalName},
        {"divisionType", o.divisionType}, {"divisionName", o.divisionName},
        {"teamSize", o.teamSize}, {"trackRecord", o.trackRecord},
        {"yearsTrackRecord", o.yearsTrackRecord}, {"askPrice", o.askPrice},
        {"archetypeMix", o.archetypeMix}, {"active", o.active},
        {"offeredQuarter", o.offeredQuarter}, {"expiresQuarter", o.expiresQuarter}
    };
}

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
            8e5, 1.2e5, 0, 0, 55, 0.08, 1945, true, "", -1, {}
        });
        competitors_.push_back({
            "apex", "Apex Securities", "Frank DiMaggio",
            "gunslinger", PersonalityProfile::aggressive(),
            5e5, 0.8e5, 0, 0, 45, 0.05, 1945, true, "", -1, {}
        });
        competitors_.push_back({
            "pacific", "Pacific National", "Eleanor Sato",
            "innovator", PersonalityProfile::wriston(),
            6e5, 1.0e5, 0, 0, 50, 0.06, 1945, true, "", -1, {}
        });
        competitors_.push_back({
            "federal_commerce", "Federal Commerce Bank", "Robert Kessler",
            "politician", PersonalityProfile::political(),
            7e5, 1.1e5, 0, 0, 60, 0.07, 1945, true, "", -1, {}
        });

        // Late entrants (stored but not active until their year)
        competitors_.push_back({
            "sterling", "Sterling Partners", "James Blackwell",
            "gunslinger", PersonalityProfile::gambler(),
            3e5, 0.5e5, 0, 0, 40, 0.03, 1982, true, "", -1, {}
        });
        competitors_.push_back({
            "novapay", "NovaPay", "Sarah Chen",
            "innovator", PersonalityProfile::griffin(),
            1e5, 0.2e5, 0, 0, 35, 0.01, 2012, true, "", -1, {}
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

    // ── STAR_02 P6: poaching ────────────────────────────────────────────
    // Generate a poach offer from a living rival, or return nullopt if none is
    // eligible. Era heat (1.5–3× track record) scales the ask price. divHeat
    // (a 0..1 "how hot is this division type this era" hint) lets the caller
    // bias the multiple. rng_ draws happen at a FIXED point (quarter boundary)
    // so they stay lockstep-safe (caller guards this).
    std::optional<PoachOffer> generatePoachOffer(int year, int globalQuarter,
                                                 double eraHeat, int& counter) {
        // Collect living rivals with a real track record (operated ≥ 2 years).
        std::vector<CompetitorBank*> pool;
        for (auto& c : competitors_) {
            if (c.alive && year >= c.foundedYear && c.yearsOperating >= 2
                && c.cumulativeRevenue > 0.0)
                pool.push_back(&c);
        }
        if (pool.empty()) return std::nullopt;

        CompetitorBank& rival = *pool[(size_t)(rng_.uniform() * pool.size()) % pool.size()];

        PoachOffer offer;
        offer.id = "poach_" + std::to_string(++counter);
        offer.rivalId = rival.id;
        offer.rivalName = rival.name;
        // Pick a division type appropriate to the rival's archetype + era.
        offer.divisionType = divisionForArchetype(rival.archetype, year);
        nlohmann::json dj = offer.divisionType;
        offer.divisionName = dj.get<std::string>();
        offer.teamSize = 3 + (int)(rng_.uniform() * 6);  // 3-8 bankers
        offer.trackRecord = rival.cumulativeRevenue;
        offer.yearsTrackRecord = rival.yearsOperating;
        // askPrice = (1.5 .. 3.0) × trackRecord, scaled by era heat (clamped).
        double mult = 1.5 + std::clamp(eraHeat, 0.0, 1.0) * 1.5;
        offer.askPrice = offer.trackRecord * mult;
        offer.archetypeMix = archetypeMixForRival(rival, offer.teamSize);
        offer.offeredQuarter = globalQuarter;
        offer.expiresQuarter = globalQuarter + 2 + (int)(rng_.uniform() * 3); // 2-4 quarters
        offer.active = true;
        return offer;
    }

    // Apply an accepted poach: drop the rival's strength, remember the grudge.
    // Returns false if the rival is gone. The bank-side import (staff, σ/β,
    // division boost, reputation) is handled by the caller (QuarterlyTurnManager)
    // which owns the player's Bank.
    bool applyPoachToRival(const std::string& rivalId, int teamSize) {
        for (auto& c : competitors_) {
            if (c.id != rivalId) continue;
            // Strength drop scales with the team size relative to the rival.
            double drop = std::clamp(0.05 + 0.02 * teamSize, 0.05, 0.30);
            c.totalAssets *= (1.0 - drop);
            c.capital = c.totalAssets * 0.10;
            c.totalLoans = c.totalAssets * 0.70;
            c.poachStrengthPenalty = std::clamp(c.poachStrengthPenalty + drop, 0.0, 0.9);
            // They remember: rivalry/aggression vs the player rises.
            c.aggressionVsPlayer = std::clamp(c.aggressionVsPlayer + 0.25, 0.0, 1.0);
            c.reputation = std::clamp(c.reputation - 4.0, 0.0, 100.0);
            c.recentDecisions.push_back("Vows revenge after a team raid");
            spdlog::info("POACH: {} lost a {}-banker team; strength -{:.0f}%, aggression {:.2f}",
                c.name, teamSize, drop * 100, c.aggressionVsPlayer);
            return true;
        }
        return false;
    }

    // Map a rival archetype + era to a plausible division type. Public so the
    // QuarterlyTurnManager's industry-saturation model can bucket rivals by line.
    DivisionType divisionForArchetype(const std::string& arch, int year) const {
        if (arch == "gunslinger") return DivisionType::TradingDesk;
        if (arch == "innovator") return year >= 2010 ? DivisionType::Fintech
                                       : (year >= 1985 ? DivisionType::Securitization : DivisionType::InternationalBanking);
        if (arch == "politician") return DivisionType::InvestmentBanking;
        if (arch == "survivor") return DivisionType::CommercialLending;
        return DivisionType::CommercialLending; // traditionalist + default
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

        // STAR_02 P6: accumulate the rival's track record for poach pricing.
        comp.cumulativeRevenue += comp.revenue;
        comp.yearsOperating++;

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

        // STAR_02 P6 rivalry: a rival you've poached comes after you harder.
        if (comp.aggressionVsPlayer > 0.3) {
            comp.recentDecisions.push_back("Targeting your clients in retaliation");
            // Decay the grudge slowly so it isn't permanent.
            comp.aggressionVsPlayer = std::clamp(comp.aggressionVsPlayer - 0.05, 0.0, 1.0);
        }
    }

    // The archetype-family mix the imported team carries (P6). A gunslinger
    // rival brings aggressive families; a traditionalist brings relationship/
    // operator families. Returns `teamSize` family-id strings.
    std::vector<std::string> archetypeMixForRival(const CompetitorBank& rival, int teamSize) {
        std::vector<std::string> aggressive{"gunslinger", "dealmaker", "prodigy", "quant"};
        std::vector<std::string> conservative{"lifer", "patrician", "operator", "rainmaker"};
        const auto& src = (rival.archetype == "gunslinger" || rival.archetype == "innovator")
            ? aggressive : conservative;
        std::vector<std::string> mix;
        for (int i = 0; i < std::max(1, teamSize); ++i)
            mix.push_back(src[(size_t)(rng_.uniform() * src.size()) % src.size()]);
        return mix;
    }
};

} // namespace stvg::simulation
