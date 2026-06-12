#pragma once

// PersonalBook.h — STAR_02 P7 "CEO personal account" (light trading affordance).
//
// The early game leans loany (the Loan Book is the hero loop); this is the
// owner's "could also be tradey" hedge — the CEO's OWN small trading book, not a
// full trading desk. Market orders only, long-only (no shorts in v1), on the 6
// streamed markets.
//
// ── Accounting choice: OFF balance sheet ──────────────────────────────────────
// The book is held as a SEPARATE "CEO personal account" seeded with 5% of the
// bank's STARTING capital. It is NOT mixed into bank_.capital and therefore does
// NOT touch the SFC balance-sheet identity in Bank::endQuarter(). This is the
// SAFER of the two options the plan offered: full SFC integration would have to
// thread cost-basis + realized P&L through the A = L + E derivation and the
// money-multiplier/reserve steps, risking a silent identity break. Holding the
// book off-balance-sheet keeps endQuarter() byte-for-byte unchanged (so the
// lending P&L, leverage, and funding math are untouched) while still giving the
// player a real, mark-to-market trading surface. Trades move only the personal
// account's cash and positions; the bank's books never see them.
//
// ── Lockstep ──────────────────────────────────────────────────────────────────
// Trades are exogenous player input (like acceptDeal/hire) and draw NO RNG, so
// they cannot perturb the day-by-day-vs-batch RNG sequence. Mark-to-market reads
// live MarketSimulator prices and is pure arithmetic.

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>

namespace stvg::simulation {

// One long position in a single market.
struct PersonalPosition {
    std::string marketId;
    double qty = 0.0;       // units held (long-only, ≥ 0)
    double avgCost = 0.0;   // average cost per unit (cost basis)
};

inline void to_json(nlohmann::json& j, const PersonalPosition& p) {
    j = nlohmann::json{{"marketId", p.marketId}, {"qty", p.qty}, {"avgCost", p.avgCost}};
}

// Position-limit constants (named per the plan). The total book (cash committed
// to positions at cost) is capped as a fraction of the bank's capital so the CEO
// can't bet the whole institution on a personal punt.
inline constexpr double kPersonalBookSeedFraction   = 0.05;  // 5% of starting capital
inline constexpr double kPersonalBookLimitPhase1    = 0.20;  // ≤ 20% of capital early
inline constexpr double kPersonalBookLimitLater     = 0.40;  // ≤ 40% later

class PersonalBook {
public:
    // Seed the personal account with a fraction of starting capital. Idempotent
    // per game (call once at construction).
    void seed(double startingCapital) {
        cash_ = std::max(0.0, startingCapital * kPersonalBookSeedFraction);
        seedCapital_ = cash_;
    }

    double cash() const { return cash_; }

    // Position limit as a fraction of bank capital (phase 1 vs later). The plan
    // uses the trading year to pick: tighter early, looser once a desk exists. We
    // key off the game year (≤ 1960 = phase 1) which is a stable, save-safe proxy.
    static double limitFractionForYear(int year) {
        return (year <= 1960) ? kPersonalBookLimitPhase1 : kPersonalBookLimitLater;
    }

    // Total cost basis currently deployed in open positions.
    double costBasis() const {
        double sum = 0.0;
        for (const auto& [id, p] : positions_) sum += p.qty * p.avgCost;
        return sum;
    }

    // Mark-to-market value of all open positions at the supplied prices.
    double marketValue(const std::unordered_map<std::string, double>& prices) const {
        double sum = 0.0;
        for (const auto& [id, p] : positions_) {
            auto it = prices.find(id);
            double px = (it != prices.end()) ? it->second : p.avgCost;
            sum += p.qty * px;
        }
        return sum;
    }

    // Total account value = idle cash + marked positions.
    double totalValue(const std::unordered_map<std::string, double>& prices) const {
        return cash_ + marketValue(prices);
    }

    // Unrealized P&L vs the seed (how the personal punt is doing overall).
    double unrealizedPnl(const std::unordered_map<std::string, double>& prices) const {
        return totalValue(prices) - seedCapital_;
    }

    // ── Trade execution ────────────────────────────────────────────────────────
    // Returns true on success. On failure, `err` is set to a clean reason string
    // and no state changes. `bankCapital`+`year` gate the book-size limit; `price`
    // is the current market price (mid) for `marketId`; `amount` is the DOLLAR
    // notional to buy/sell at market.
    struct TradeResult {
        bool ok = false;
        std::string err;
        double qtyDelta = 0.0;     // signed units transacted (+buy, −sell)
        double cashAfter = 0.0;
        double positionQtyAfter = 0.0;
        double realizedPnl = 0.0;  // realized on a sell (0 on a buy)
    };

    TradeResult buy(const std::string& marketId, double amount, double price,
                    double bankCapital, int year) {
        TradeResult r;
        if (!(amount > 0.0) || !std::isfinite(amount)) { r.err = "invalid amount"; return r; }
        if (!(price > 0.0) || !std::isfinite(price))   { r.err = "no market price"; return r; }
        if (amount > cash_ + 1e-6) { r.err = "insufficient personal cash"; return r; }
        // Book-size limit: cost basis after this buy must stay within the cap.
        double limit = bankCapital * limitFractionForYear(year);
        if (costBasis() + amount > limit + 1e-6) { r.err = "exceeds personal book limit"; return r; }

        double qty = amount / price;
        auto& p = positions_[marketId];
        p.marketId = marketId;
        double newQty = p.qty + qty;
        // New average cost = blended cost basis.
        p.avgCost = (p.qty * p.avgCost + qty * price) / std::max(newQty, 1e-12);
        p.qty = newQty;
        cash_ -= amount;

        r.ok = true;
        r.qtyDelta = qty;
        r.cashAfter = cash_;
        r.positionQtyAfter = p.qty;
        return r;
    }

    // Sell `amount` of DOLLAR notional (at market) from an existing long. No
    // shorts: selling more than held is rejected cleanly. Realizes P&L vs avgCost.
    TradeResult sell(const std::string& marketId, double amount, double price) {
        TradeResult r;
        if (!(amount > 0.0) || !std::isfinite(amount)) { r.err = "invalid amount"; return r; }
        if (!(price > 0.0) || !std::isfinite(price))   { r.err = "no market price"; return r; }
        auto it = positions_.find(marketId);
        if (it == positions_.end() || it->second.qty <= 0.0) {
            r.err = "no position to sell"; return r;
        }
        double qty = amount / price;
        if (qty > it->second.qty + 1e-9) { r.err = "cannot sell more than held (no shorts)"; return r; }

        double proceeds = qty * price;
        double cost = qty * it->second.avgCost;
        it->second.qty -= qty;
        cash_ += proceeds;
        r.realizedPnl = proceeds - cost;
        // Drop fully-closed positions so the book stays clean.
        if (it->second.qty <= 1e-9) positions_.erase(it);
        else r.positionQtyAfter = it->second.qty;

        r.ok = true;
        r.qtyDelta = -qty;
        r.cashAfter = cash_;
        return r;
    }

    // Quantity held in one market (0 if none).
    double qtyOf(const std::string& marketId) const {
        auto it = positions_.find(marketId);
        return it != positions_.end() ? it->second.qty : 0.0;
    }

    const std::unordered_map<std::string, PersonalPosition>& positions() const { return positions_; }

    // Player-view JSON: positions + marked value/P&L at the supplied prices.
    nlohmann::json toPlayerJson(const std::unordered_map<std::string, double>& prices) const {
        nlohmann::json pos = nlohmann::json::array();
        for (const auto& [id, p] : positions_) {
            auto it = prices.find(id);
            double px = (it != prices.end()) ? it->second : p.avgCost;
            pos.push_back({
                {"marketId", p.marketId}, {"qty", p.qty}, {"avgCost", p.avgCost},
                {"price", px}, {"value", p.qty * px},
                {"pnl", p.qty * (px - p.avgCost)},
            });
        }
        return {
            {"cash", cash_},
            {"seedCapital", seedCapital_},
            {"costBasis", costBasis()},
            {"value", totalValue(prices)},
            {"pnl", unrealizedPnl(prices)},
            {"positions", pos},
        };
    }

    // ── Save/load ──────────────────────────────────────────────────────────────
    nlohmann::json toSaveJson() const {
        nlohmann::json pos = nlohmann::json::array();
        for (const auto& [id, p] : positions_) pos.push_back(p);
        return {{"cash", cash_}, {"seedCapital", seedCapital_}, {"positions", pos}};
    }

    void loadSaveJson(const nlohmann::json& j) {
        cash_ = j.value("cash", cash_);
        seedCapital_ = j.value("seedCapital", seedCapital_);
        positions_.clear();
        if (j.contains("positions") && j["positions"].is_array()) {
            for (const auto& pj : j["positions"]) {
                PersonalPosition p;
                p.marketId = pj.value("marketId", "");
                p.qty = pj.value("qty", 0.0);
                p.avgCost = pj.value("avgCost", 0.0);
                if (!p.marketId.empty() && p.qty > 0.0) positions_[p.marketId] = p;
            }
        }
    }

private:
    double cash_ = 0.0;
    double seedCapital_ = 0.0;
    std::unordered_map<std::string, PersonalPosition> positions_;
};

} // namespace stvg::simulation
