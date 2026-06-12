#pragma once

#include "../core/Types.h"
#include "../math/RandomEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>

namespace stvg::simulation {

// ════════════════════════════════════════════════════════════════════
// Trader personality and attributes
// ════════════════════════════════════════════════════════════════════

enum class TraderPersonality { Conservative, Aggressive, Rogue, Analytical };
NLOHMANN_JSON_SERIALIZE_ENUM(TraderPersonality, {
    {TraderPersonality::Conservative, "conservative"},
    {TraderPersonality::Aggressive, "aggressive"},
    {TraderPersonality::Rogue, "rogue"},
    {TraderPersonality::Analytical, "analytical"},
})

struct TraderAttributes {
    double intelligence = 100;    // 85-145 (IQ scale)
    double riskTolerance = 50;    // 0-100
    double integrity = 70;        // 0-100 (affects hidden position probability)
    double discipline = 60;       // 0-100 (affects limit compliance)
    double creativity = 50;       // 0-100
    double speed = 60;            // 0-100
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TraderAttributes,
    intelligence, riskTolerance, integrity, discipline, creativity, speed)

// A single trading position
struct Position {
    std::string marketId;
    double size = 0.0;           // Positive = long, negative = short
    double entryPrice = 0.0;
    double currentPrice = 0.0;
    double unrealizedPnL = 0.0;
    bool hidden = false;         // CEO can't see this unless investigated
};

inline void to_json(nlohmann::json& j, const Position& p) {
    j = nlohmann::json{
        {"marketId", p.marketId}, {"size", p.size},
        {"entryPrice", p.entryPrice}, {"currentPrice", p.currentPrice},
        {"unrealizedPnL", p.unrealizedPnL}, {"hidden", p.hidden}
    };
}

// ════════════════════════════════════════════════════════════════════
// Trader
// ════════════════════════════════════════════════════════════════════

struct Trader {
    std::string id;
    std::string name;
    TraderPersonality personality;
    TraderAttributes attributes;
    double salary = 200000;      // Annual
    double ytdPnL = 0.0;
    double totalPnL = 0.0;
    int quartersEmployed = 0;

    std::vector<Position> positions;
    std::vector<Position> hiddenPositions; // Not visible to CEO

    // Personality-based multipliers (from legacy RiskManager.cpp)
    double riskMultiplier() const {
        switch (personality) {
            case TraderPersonality::Conservative: return 0.7;
            case TraderPersonality::Aggressive:   return 1.5;
            case TraderPersonality::Rogue:        return 2.0;
            case TraderPersonality::Analytical:   return 1.1;
        }
        return 1.0;
    }

    double returnMultiplier() const {
        switch (personality) {
            case TraderPersonality::Conservative: return 0.9;
            case TraderPersonality::Aggressive:   return 1.3;
            case TraderPersonality::Rogue:        return 1.5;
            case TraderPersonality::Analytical:   return 1.2;
        }
        return 1.0;
    }

    // Skill factor based on intelligence and experience
    double skillFactor() const {
        double iqFactor = (attributes.intelligence - 85.0) / 60.0; // 0-1 range
        double expFactor = std::min(quartersEmployed / 20.0, 1.0); // Caps at 5 years
        return 0.5 + 0.3 * iqFactor + 0.2 * expFactor;
    }

    // Total visible P&L (what CEO sees)
    double visiblePnL() const {
        double pnl = ytdPnL;
        for (const auto& p : positions) pnl += p.unrealizedPnL;
        return pnl;
    }

    // True P&L including hidden positions
    double truePnL() const {
        double pnl = visiblePnL();
        for (const auto& p : hiddenPositions) pnl += p.unrealizedPnL;
        return pnl;
    }

    // Total hidden risk exposure
    double hiddenExposure() const {
        double exp = 0;
        for (const auto& p : hiddenPositions) exp += std::abs(p.size * p.currentPrice);
        return exp;
    }
};

inline void to_json(nlohmann::json& j, const Trader& t) {
    j = nlohmann::json{
        {"id", t.id}, {"name", t.name},
        {"personality", t.personality}, {"attributes", t.attributes},
        {"salary", t.salary}, {"ytdPnL", t.ytdPnL},
        {"totalPnL", t.totalPnL}, {"quartersEmployed", t.quartersEmployed},
        {"visiblePnL", t.visiblePnL()},
        {"positions", t.positions}
        // hiddenPositions NOT serialized — CEO can't see them
    };
}

// ════════════════════════════════════════════════════════════════════
// Trader AI Engine — manages all traders in the trading division
// ════════════════════════════════════════════════════════════════════

class TraderAIEngine {
public:
    explicit TraderAIEngine(math::RandomEngine& rng) : rng_(rng) {}

    // Generate a pool of trader candidates for hiring
    std::vector<Trader> generateCandidates(int count = 5) {
        std::vector<Trader> candidates;
        for (int i = 0; i < count; ++i) {
            candidates.push_back(generateTrader());
        }
        return candidates;
    }

    // Simulate one trading day for all traders
    void tickDay(std::vector<Trader>& traders,
                 const std::vector<Market>& markets,
                 double stressLevel) {

        for (auto& trader : traders) {
            // Each trader manages their positions
            for (auto& pos : trader.positions) {
                updatePosition(pos, markets);
            }
            for (auto& pos : trader.hiddenPositions) {
                updatePosition(pos, markets);
            }

            // Trader may open new positions or adjust existing ones
            if (rng_.bernoulli(0.1)) { // 10% chance of new trade per day
                generateTrade(trader, markets, stressLevel);
            }

            // Check for position hiding behavior (low integrity + losing)
            checkHidingBehavior(trader, stressLevel);
        }
    }

    // End of quarter processing
    void endQuarter(std::vector<Trader>& traders) {
        for (auto& t : traders) {
            // Realize P&L from closed positions
            double quarterPnL = 0;
            for (const auto& p : t.positions) quarterPnL += p.unrealizedPnL;
            t.ytdPnL += quarterPnL;
            t.totalPnL += quarterPnL;
            t.quartersEmployed++;

            // Reset positions for next quarter (simplified)
            for (auto& p : t.positions) {
                p.entryPrice = p.currentPrice;
                p.unrealizedPnL = 0;
            }
            for (auto& p : t.hiddenPositions) {
                p.entryPrice = p.currentPrice;
                p.unrealizedPnL = 0;
            }
        }
    }

    // Investigate a trader — reveals hidden positions
    double investigateTrader(Trader& trader) {
        double revealedLoss = 0;
        for (auto& hp : trader.hiddenPositions) {
            revealedLoss += hp.unrealizedPnL;
            hp.hidden = false;
            trader.positions.push_back(hp);
        }
        trader.hiddenPositions.clear();
        return revealedLoss;
    }

    // Division-level P&L (visible to CEO)
    static double divisionVisiblePnL(const std::vector<Trader>& traders) {
        double total = 0;
        for (const auto& t : traders) total += t.visiblePnL();
        return total;
    }

    // Division-level true P&L (including hidden)
    static double divisionTruePnL(const std::vector<Trader>& traders) {
        double total = 0;
        for (const auto& t : traders) total += t.truePnL();
        return total;
    }

    // Division total hidden exposure
    static double divisionHiddenExposure(const std::vector<Trader>& traders) {
        double total = 0;
        for (const auto& t : traders) total += t.hiddenExposure();
        return total;
    }

private:
    math::RandomEngine& rng_;
    int traderCounter_ = 0;

    // Name pools for procedural generation
    static constexpr std::array firstNames = {
        "James", "Sarah", "Michael", "Emily", "David", "Rachel",
        "Robert", "Jessica", "William", "Amanda", "Daniel", "Lauren",
        "Andrew", "Nicole", "Christopher", "Megan", "Matthew", "Ashley",
        "Kevin", "Stephanie", "Brian", "Jennifer", "Steven", "Rebecca",
        "Thomas", "Maria", "Richard", "Catherine", "Mark", "Elizabeth"
    };

    static constexpr std::array lastNames = {
        "Chen", "Park", "Williams", "Thompson", "Rodriguez", "Kim",
        "Martinez", "Johnson", "Brown", "Lee", "Garcia", "Wilson",
        "Taylor", "Anderson", "Harris", "Clark", "Lewis", "Robinson",
        "Walker", "Young", "Hall", "Allen", "King", "Wright",
        "Scott", "Green", "Baker", "Adams", "Nelson", "Hill"
    };

    Trader generateTrader() {
        Trader t;
        t.id = "trader_" + std::to_string(++traderCounter_);

        // Random name
        int fi = (int)(rng_.uniform() * firstNames.size()) % firstNames.size();
        int li = (int)(rng_.uniform() * lastNames.size()) % lastNames.size();
        t.name = std::string(firstNames[fi]) + " " + std::string(lastNames[li]);

        // Personality distribution: 40% conservative, 30% analytical, 20% aggressive, 10% rogue
        double roll = rng_.uniform();
        if (roll < 0.4) t.personality = TraderPersonality::Conservative;
        else if (roll < 0.7) t.personality = TraderPersonality::Analytical;
        else if (roll < 0.9) t.personality = TraderPersonality::Aggressive;
        else t.personality = TraderPersonality::Rogue;

        // Attributes (from Beta-like distributions via normal + clamping)
        t.attributes.intelligence = std::clamp(rng_.normalSample(110, 15), 85.0, 145.0);
        t.attributes.riskTolerance = std::clamp(rng_.normalSample(50, 20), 10.0, 95.0);
        t.attributes.discipline = std::clamp(rng_.normalSample(60, 15), 20.0, 95.0);
        t.attributes.creativity = std::clamp(rng_.normalSample(50, 15), 15.0, 95.0);
        t.attributes.speed = std::clamp(rng_.normalSample(60, 15), 20.0, 95.0);

        // Integrity correlated with personality
        switch (t.personality) {
            case TraderPersonality::Conservative:
                t.attributes.integrity = std::clamp(rng_.normalSample(85, 8), 60.0, 100.0);
                break;
            case TraderPersonality::Analytical:
                t.attributes.integrity = std::clamp(rng_.normalSample(75, 10), 50.0, 95.0);
                break;
            case TraderPersonality::Aggressive:
                t.attributes.integrity = std::clamp(rng_.normalSample(50, 15), 20.0, 80.0);
                break;
            case TraderPersonality::Rogue:
                t.attributes.integrity = std::clamp(rng_.normalSample(25, 10), 5.0, 50.0);
                break;
        }

        // Risk tolerance correlated with personality
        if (t.personality == TraderPersonality::Conservative)
            t.attributes.riskTolerance = std::clamp(rng_.normalSample(25, 10), 10.0, 50.0);
        else if (t.personality == TraderPersonality::Aggressive || t.personality == TraderPersonality::Rogue)
            t.attributes.riskTolerance = std::clamp(rng_.normalSample(80, 10), 55.0, 100.0);

        // Salary based on intelligence + personality
        double baseSalary = 150000 + (t.attributes.intelligence - 85) * 5000;
        if (t.personality == TraderPersonality::Aggressive) baseSalary *= 1.3;
        if (t.personality == TraderPersonality::Rogue) baseSalary *= 1.5;
        t.salary = baseSalary;

        return t;
    }

    void updatePosition(Position& pos, const std::vector<Market>& markets) {
        for (const auto& m : markets) {
            if (m.id == pos.marketId) {
                pos.currentPrice = m.currentPrice;
                pos.unrealizedPnL = pos.size * (pos.currentPrice - pos.entryPrice);
                return;
            }
        }
    }

    void generateTrade(Trader& trader, const std::vector<Market>& markets,
                       double stressLevel) {
        if (markets.empty()) return;

        // Pick a random market (weighted toward equities)
        int idx = 0;
        double roll = rng_.uniform();
        if (roll < 0.4) idx = 0;       // S&P 500
        else if (roll < 0.6) idx = 1;  // Bonds
        else if (roll < 0.75) idx = 2; // Gold
        else if (roll < 0.85) idx = 3; // EUR/USD
        else idx = std::min((int)markets.size() - 1, 5); // BTC

        // Skip derived markets (VIX)
        if (idx == 4 && markets.size() > 5) idx = 5;

        const auto& market = markets[idx];

        // Position sizing: Kelly-inspired with personality factor
        double capitalAllocation = 1e2 * trader.riskMultiplier(); // Base per trader (rescaled /10,000 for $1M-scale bank)
        double kellyFraction = 0.1 * trader.skillFactor();        // Conservative Kelly
        double posSize = capitalAllocation * kellyFraction / market.currentPrice;

        // Direction: slight long bias (60/40) modified by stress
        bool goLong = rng_.uniform() < (0.6 - 0.2 * stressLevel / 100.0);
        if (!goLong) posSize = -posSize;

        Position pos;
        pos.marketId = market.id;
        pos.size = posSize;
        pos.entryPrice = market.currentPrice;
        pos.currentPrice = market.currentPrice;
        pos.unrealizedPnL = 0;
        pos.hidden = false;

        trader.positions.push_back(pos);
    }

    void checkHidingBehavior(Trader& trader, double stressLevel) {
        // Low integrity traders may hide losing positions
        double hideProbability = (1.0 - trader.attributes.integrity / 100.0) * 0.05;

        // More likely to hide when stressed
        hideProbability *= (1.0 + stressLevel / 100.0);

        // Check each visible position
        auto it = trader.positions.begin();
        while (it != trader.positions.end()) {
            if (it->unrealizedPnL < 0 && rng_.bernoulli(hideProbability)) {
                // Hide this losing position
                Position hidden = *it;
                hidden.hidden = true;
                trader.hiddenPositions.push_back(hidden);
                it = trader.positions.erase(it);
            } else {
                ++it;
            }
        }
    }
};

} // namespace stvg::simulation
