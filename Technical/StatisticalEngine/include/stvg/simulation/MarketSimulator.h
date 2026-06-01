#pragma once

#include "../core/Types.h"
#include "../math/RandomEngine.h"
#include "../math/GARCH.h"
#include "../math/JumpDiffusion.h"
#include "../math/CorrelationEngine.h"
#include "EconomicEngine.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace stvg::simulation {

// Per-market simulation state
struct MarketSim {
    Market market;              // Public market data (sent to frontend)
    math::GARCHProcess garch;   // GARCH volatility process for this market
    math::JumpDiffusionProcess jump; // Jump-diffusion for crash events
    double drift = 0.0;         // Annual drift (expected return)
    double logPrice = 0.0;      // Log price for geometric Brownian motion
    int correlationIndex = -1;  // Index into CorrelationEngine (-1 = uncorrelated / derived)
    bool isDerived = false;     // True for VIX (derived from S&P vol, not independent)
};

// Central market simulation engine.
// 6 asset class markets with:
// - GARCH(1,1) volatility clustering
// - Correlated returns via Cholesky decomposition
// - Jump-diffusion for sudden crashes
// - VIX derived from S&P 500 implied volatility
class MarketSimulator {
public:
    MarketSimulator(math::RandomEngine& rng, EconomicEngine& econ)
        : rng_(rng), econ_(econ) {}

    void init(int startYear = 2024) {
        markets_.clear();
        startYear_ = startYear;

        // Era-appropriate starting prices for S&P 500
        double sp500Price = 4500.0;
        if      (startYear <= 1959) sp500Price = 15.0;
        else if (startYear <= 1972) sp500Price = 70.0;
        else if (startYear <= 1986) sp500Price = 120.0;
        else if (startYear <= 1999) sp500Price = 400.0;
        else if (startYear <= 2007) sp500Price = 1400.0;
        else if (startYear <= 2019) sp500Price = 1800.0;
        else                        sp500Price = 3500.0;

        // Gold: $35 fixed under Bretton Woods, free-floating after 1971
        double goldPrice = 2000.0;
        if      (startYear <= 1971) goldPrice = 35.0;   // Bretton Woods peg
        else if (startYear <= 1986) goldPrice = 350.0;
        else if (startYear <= 1999) goldPrice = 280.0;
        else if (startYear <= 2007) goldPrice = 650.0;
        else if (startYear <= 2019) goldPrice = 1300.0;
        else                        goldPrice = 1800.0;

        // Always-present markets
        addMarket("SP500",  "S&P 500 Index",  MarketType::Stock,     "SPX", "USD", sp500Price, 0.08, 0.18, 0);
        addMarket("UST10Y", "10Y Treasury",    MarketType::Bond,      "UST", "USD", 98.0,      0.03, 0.08, 1);
        addMarket("GOLD",   "Gold Futures",    MarketType::Commodity, "GLD", "USD", goldPrice,  0.04, 0.16, 2);

        // EUR/USD: only after Bretton Woods collapse (1971)
        if (startYear >= 1971) {
            addMarket("EURUSD", "EUR/USD",     MarketType::Currency,  "EUR", "USD", 1.08, 0.00, 0.08, 3);
        }

        // VIX (implied vol derived from S&P — always present as a concept)
        addMarket("SPXOPT", "SPX Options Vol", MarketType::Derivative,"VIX", "USD", 18.0, 0.00, 0.60, -1, true);

        // Bitcoin: only after 2009
        if (startYear >= 2009) {
            double btcPrice = (startYear <= 2015) ? 500.0 : (startYear <= 2019) ? 10000.0 : 45000.0;
            addMarket("BTC", "Bitcoin",        MarketType::Crypto,    "BTC", "USD", btcPrice, 0.15, 0.65, 4);
        }

        spdlog::info("MarketSimulator initialized with {} markets (startYear={})", markets_.size(), startYear);
    }

    // Check if a market exists by ID
    bool hasMarket(const std::string& id) const {
        for (const auto& ms : markets_) {
            if (ms.market.id == id) return true;
        }
        return false;
    }

    // Add a market dynamically (for era transitions — e.g., forex after 1971)
    void addMarketDynamic(const std::string& id, const std::string& name,
                          MarketType type, const std::string& symbol,
                          const std::string& currency, double startPrice,
                          double annualDrift, double annualVol, int corrIndex) {
        if (hasMarket(id)) return; // Don't add duplicates
        addMarket(id, name, type, symbol, currency, startPrice, annualDrift, annualVol, corrIndex);
        spdlog::info("Market added dynamically: {} at ${:.2f}", name, startPrice);
    }

    // Advance all markets by one trading day
    void tick(double dt = 1.0 / 252.0) {
        const auto& eco = econ_.state();
        double stress = econ_.stressScore();

        // Generate correlated shocks for all asset classes
        auto correlatedShocks = corrEngine_.generateCorrelatedShocks(rng_, stress);

        // Update GARCH params based on regime
        auto regimeParams = selectGARCHParams(stress);

        for (auto& ms : markets_) {
            // Skip VIX — it's derived below
            if (ms.isDerived) continue;

            // Fix: preserve per-market calibrated omega, only update alpha/beta for regime
            {
                auto adjusted = ms.garch.params();
                adjusted.alpha = regimeParams.alpha;
                adjusted.beta = regimeParams.beta;
                ms.garch.setParams(adjusted);
            }

            // Market-specific drift adjustment from macro conditions
            double adjustedDrift = ms.drift;
            adjustedDrift += 0.5 * (eco.gdpGrowth - 0.025);
            adjustedDrift -= 0.3 * (eco.fedFundsRate - 0.04);

            // Bond prices: duration + convexity adjustment
            if (ms.market.type == MarketType::Bond) {
                double duration = 7.0; // Modified duration ~7 years for 10Y bond
                double convexity = duration * duration / (1.0 + eco.treasuryYield10Y); // ~80
                double rateDelta = eco.fedFundsRate - 0.04;
                // Price change = -D * Δy + 0.5 * C * Δy² (convexity benefit)
                adjustedDrift = -duration * rateDelta + 0.5 * convexity * rateDelta * rateDelta;
            }

            // GARCH volatility update (use correlated shock instead of independent)
            double vol = ms.garch.currentVolatility();
            double correlatedZ = (ms.correlationIndex >= 0 && ms.correlationIndex < math::NUM_ASSETS)
                ? correlatedShocks[ms.correlationIndex]
                : rng_.normalSample();

            // Base return: drift + vol * correlated shock
            double ret = adjustedDrift * dt + vol * std::sqrt(dt) * correlatedZ;

            // Update GARCH internal state with this return
            // (manually feed the return back for next period's variance)
            // Jump-diffusion component
            double jumpRet = ms.jump.step(rng_, stress);
            if (ms.jump.jumped()) {
                ret += jumpRet;
                // Post-jump volatility spike
                // (next tick's GARCH will naturally react via alpha * eps^2)
            }

            // Update log price
            ms.logPrice += ret;
            double newPrice = std::exp(ms.logPrice);

            // Feed realized return into GARCH for next-period variance
            ms.garch.feedReturn(ret);

            updateMarketData(ms, newPrice, stress, dt);
        }

        // Derive VIX from S&P 500 implied volatility
        deriveVIX(stress);
    }

    // Get current market data for all markets
    std::vector<Market> snapshot() const {
        std::vector<Market> result;
        result.reserve(markets_.size());
        for (const auto& ms : markets_) {
            result.push_back(ms.market);
        }
        return result;
    }

    // Determine regime from stress score
    MarketRegime currentRegime(double stress) const {
        if (stress >= 70.0) return MarketRegime::Crisis;
        if (stress >= 50.0) return MarketRegime::Stressed;
        if (stress >= 35.0) return MarketRegime::Cautious;
        if (stress >= 15.0) return MarketRegime::Normal;
        return MarketRegime::Calm;
    }

    size_t marketCount() const { return markets_.size(); }

    nlohmann::json toSaveJson() const {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& ms : markets_) {
            arr.push_back({
                {"market", ms.market},
                {"logPrice", ms.logPrice},
                {"drift", ms.drift},
                {"correlationIndex", ms.correlationIndex},
                {"isDerived", ms.isDerived},
                {"garch_variance", ms.garch.currentVariance()},
                {"garch_prevReturn", ms.garch.previousReturn()}
            });
        }
        return {{"markets", arr}, {"startYear", startYear_}};
    }
    void loadSaveJson(const nlohmann::json& j) {
        startYear_ = j.value("startYear", startYear_);
        if (j.contains("markets")) {
            const auto& arr = j["markets"];
            for (size_t i = 0; i < arr.size() && i < markets_.size(); ++i) {
                markets_[i].market = arr[i]["market"].get<Market>();
                markets_[i].logPrice = arr[i].value("logPrice", markets_[i].logPrice);
                markets_[i].drift = arr[i].value("drift", markets_[i].drift);
                if (arr[i].contains("garch_variance")) {
                    markets_[i].garch.loadState(
                        arr[i]["garch_variance"].get<double>(),
                        arr[i].value("garch_prevReturn", 0.0));
                }
            }
        }
    }

    // Access S&P 500 GARCH volatility (for VIX derivation and risk metrics)
    double sp500AnnualizedVol() const {
        if (!markets_.empty()) {
            return markets_[0].garch.currentVolatility() * std::sqrt(252.0);
        }
        return 0.18;
    }

private:
    math::RandomEngine& rng_;
    EconomicEngine& econ_;
    std::vector<MarketSim> markets_;
    math::CorrelationEngine corrEngine_;
    int startYear_ = 2024;

    void addMarket(const std::string& id, const std::string& name,
                   MarketType type, const std::string& symbol,
                   const std::string& currency, double startPrice,
                   double annualDrift, double annualVol,
                   int corrIndex, bool derived = false) {
        MarketSim ms;
        ms.market.id = id;
        ms.market.name = name;
        ms.market.type = type;
        ms.market.symbol = symbol;
        ms.market.currency = currency;
        ms.market.currentPrice = startPrice;
        ms.market.previousClose = startPrice;
        ms.market.averageVolume = 1e6;
        ms.market.liquidity = LiquidityLevel::High;
        ms.drift = annualDrift;
        ms.logPrice = std::log(startPrice);
        ms.correlationIndex = corrIndex;
        ms.isDerived = derived;

        // Spread varies by asset class
        switch (type) {
            case MarketType::Stock:      ms.market.spread = 0.0002; break;
            case MarketType::Bond:       ms.market.spread = 0.0005; break;
            case MarketType::Commodity:  ms.market.spread = 0.0003; break;
            case MarketType::Currency:   ms.market.spread = 0.0001; break;
            case MarketType::Derivative: ms.market.spread = 0.0010; break;
            case MarketType::Crypto:     ms.market.spread = 0.0008; break;
        }

        // Set GARCH initial volatility from annual vol
        double dailyVol = annualVol / std::sqrt(252.0);
        math::GARCHParams gp = math::GARCHParams::Normal();
        gp.omega = dailyVol * dailyVol * (1.0 - gp.alpha - gp.beta);
        ms.garch = math::GARCHProcess(gp);

        ms.market.bid = startPrice * (1.0 - ms.market.spread / 2.0);
        ms.market.ask = startPrice * (1.0 + ms.market.spread / 2.0);

        markets_.push_back(std::move(ms));
    }

    void updateMarketData(MarketSim& ms, double newPrice, double stress, double dt) {
        ms.market.previousClose = ms.market.currentPrice;
        ms.market.currentPrice = newPrice;
        ms.market.change = newPrice - ms.market.previousClose;
        ms.market.changePercent = (ms.market.previousClose > 0)
            ? (ms.market.change / ms.market.previousClose) * 100.0 : 0.0;
        ms.market.volatility = ms.garch.currentVolatility() * std::sqrt(252.0);
        ms.market.status = MarketStatus::Open;
        ms.market.lastUpdate = now_iso8601();

        // Spread widens during stress (bug fix: was static before)
        double baseSpread = ms.market.spread;
        double stressSpreadMult = 1.0 + 0.02 * stress; // Up to 3x at stress=100
        ms.market.bid = newPrice * (1.0 - baseSpread * stressSpreadMult / 2.0);
        ms.market.ask = newPrice * (1.0 + baseSpread * stressSpreadMult / 2.0);

        // Volume: base + noise + stress effect
        double baseVol = ms.market.averageVolume;
        ms.market.volume = baseVol * (0.8 + 0.4 * rng_.uniform() + 0.01 * stress);
    }

    // VIX = 100 * annualized S&P 500 implied volatility + noise
    void deriveVIX(double stress) {
        // Find VIX market (index 4, the Derivative)
        for (auto& ms : markets_) {
            if (!ms.isDerived) continue;

            // VIX is derived from S&P 500 GARCH volatility
            double sp500Vol = markets_[0].garch.currentVolatility() * std::sqrt(252.0);
            double vixLevel = sp500Vol * 100.0;

            // Add small noise for realism
            vixLevel += rng_.normalSample(0.0, 0.3);
            vixLevel = std::clamp(vixLevel, 8.0, 80.0);

            ms.market.previousClose = ms.market.currentPrice;
            ms.market.currentPrice = vixLevel;
            ms.market.change = vixLevel - ms.market.previousClose;
            ms.market.changePercent = (ms.market.previousClose > 0)
                ? (ms.market.change / ms.market.previousClose) * 100.0 : 0.0;
            ms.market.volatility = sp500Vol; // VIX vol tracks S&P vol
            ms.market.status = MarketStatus::Open;
            ms.market.lastUpdate = now_iso8601();
            ms.market.bid = vixLevel * 0.995;
            ms.market.ask = vixLevel * 1.005;
            ms.market.volume = ms.market.averageVolume * (0.8 + 0.4 * rng_.uniform() + 0.01 * stress);
        }
    }

    // Era-specific GARCH config (updated on era transition)
    GameConfig::GARCHConfig garchConfig_;
    bool hasEraGarch_ = false;

public:
    void setGARCHConfig(const GameConfig::GARCHConfig& config) {
        garchConfig_ = config;
        hasEraGarch_ = true;
    }

private:
    math::GARCHParams selectGARCHParams(double stress) const {
        if (hasEraGarch_) {
            math::GARCHParams p;
            if (stress >= 70.0) { p.alpha = garchConfig_.crisis.alpha; p.beta = garchConfig_.crisis.beta; }
            else if (stress >= 50.0) { p.alpha = garchConfig_.stressed.alpha; p.beta = garchConfig_.stressed.beta; }
            else if (stress >= 35.0) { p.alpha = garchConfig_.cautious.alpha; p.beta = garchConfig_.cautious.beta; }
            else if (stress >= 15.0) { p.alpha = garchConfig_.normal.alpha; p.beta = garchConfig_.normal.beta; }
            else { p.alpha = garchConfig_.calm.alpha; p.beta = garchConfig_.calm.beta; }
            return p;
        }
        // Fallback to hardcoded defaults
        if (stress >= 70.0) return math::GARCHParams::Crisis();
        if (stress >= 50.0) return math::GARCHParams::Stressed();
        if (stress >= 35.0) return math::GARCHParams::Normal();
        return math::GARCHParams::Calm();
    }
};

} // namespace stvg::simulation
