#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <cstdint>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// Enums — string values match TypeScript frontend exactly
// ════════════════════════════════════════════════════════════════════

enum class MarketType {
    Stock, Bond, Commodity, Currency, Derivative, Crypto
};
NLOHMANN_JSON_SERIALIZE_ENUM(MarketType, {
    {MarketType::Stock, "stock"},
    {MarketType::Bond, "bond"},
    {MarketType::Commodity, "commodity"},
    {MarketType::Currency, "currency"},
    {MarketType::Derivative, "derivative"},
    {MarketType::Crypto, "crypto"},
})

enum class MarketStatus {
    Open, Closed, PreMarket, AfterHours, Halted, Weekend, Holiday
};
NLOHMANN_JSON_SERIALIZE_ENUM(MarketStatus, {
    {MarketStatus::Open, "open"},
    {MarketStatus::Closed, "closed"},
    {MarketStatus::PreMarket, "pre_market"},
    {MarketStatus::AfterHours, "after_hours"},
    {MarketStatus::Halted, "halted"},
    {MarketStatus::Weekend, "weekend"},
    {MarketStatus::Holiday, "holiday"},
})

enum class LiquidityLevel {
    VeryLow, Low, Medium, High, VeryHigh
};
NLOHMANN_JSON_SERIALIZE_ENUM(LiquidityLevel, {
    {LiquidityLevel::VeryLow, "very_low"},
    {LiquidityLevel::Low, "low"},
    {LiquidityLevel::Medium, "medium"},
    {LiquidityLevel::High, "high"},
    {LiquidityLevel::VeryHigh, "very_high"},
})

enum class MarketCycle {
    Expansion, Peak, Contraction, Trough, Recovery
};
NLOHMANN_JSON_SERIALIZE_ENUM(MarketCycle, {
    {MarketCycle::Expansion, "expansion"},
    {MarketCycle::Peak, "peak"},
    {MarketCycle::Contraction, "contraction"},
    {MarketCycle::Trough, "trough"},
    {MarketCycle::Recovery, "recovery"},
})

enum class MarketSentiment {
    Bullish, Neutral, Bearish, Optimistic, Pessimistic, Panicked, Euphoric
};
NLOHMANN_JSON_SERIALIZE_ENUM(MarketSentiment, {
    {MarketSentiment::Bullish, "bullish"},
    {MarketSentiment::Neutral, "neutral"},
    {MarketSentiment::Bearish, "bearish"},
    {MarketSentiment::Optimistic, "optimistic"},
    {MarketSentiment::Pessimistic, "pessimistic"},
    {MarketSentiment::Panicked, "panicked"},
    {MarketSentiment::Euphoric, "euphoric"},
})

enum class MarketRegime {
    Calm, Normal, Cautious, Stressed, Crisis
};
NLOHMANN_JSON_SERIALIZE_ENUM(MarketRegime, {
    {MarketRegime::Calm, "calm"},
    {MarketRegime::Normal, "normal"},
    {MarketRegime::Cautious, "cautious"},
    {MarketRegime::Stressed, "stressed"},
    {MarketRegime::Crisis, "crisis"},
})

enum class TrendDirection {
    Improving, Stable, Declining, Critical
};
NLOHMANN_JSON_SERIALIZE_ENUM(TrendDirection, {
    {TrendDirection::Improving, "improving"},
    {TrendDirection::Stable, "stable"},
    {TrendDirection::Declining, "declining"},
    {TrendDirection::Critical, "critical"},
})

enum class CrisisType {
    Liquidity, AssetQuality, Credit, Solvency, Contagion,
    Regulatory, Confidence, Technology, Concentration, ModelRisk
};
NLOHMANN_JSON_SERIALIZE_ENUM(CrisisType, {
    {CrisisType::Liquidity, "liquidity"},
    {CrisisType::AssetQuality, "asset_quality"},
    {CrisisType::Credit, "credit"},
    {CrisisType::Solvency, "solvency"},
    {CrisisType::Contagion, "contagion"},
    {CrisisType::Regulatory, "regulatory"},
    {CrisisType::Confidence, "confidence"},
    {CrisisType::Technology, "technology"},
    {CrisisType::Concentration, "concentration"},
    {CrisisType::ModelRisk, "model_risk"},
})

// ════════════════════════════════════════════════════════════════════
// Core data structures — match TS interfaces from frontend
// ════════════════════════════════════════════════════════════════════

// Matches TS Market from trading-engine.ts
struct Market {
    std::string id;
    std::string name;
    MarketType type;
    std::string symbol;
    std::string currency;
    MarketStatus status = MarketStatus::Closed;
    double currentPrice = 100.0;
    double previousClose = 100.0;
    double change = 0.0;
    double changePercent = 0.0;
    double volume = 0.0;
    double averageVolume = 0.0;
    double marketCap = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    double spread = 0.0;
    double volatility = 0.0;
    LiquidityLevel liquidity = LiquidityLevel::Medium;
    std::string lastUpdate;
};

inline void to_json(nlohmann::json& j, const Market& m) {
    j = nlohmann::json{
        {"id", m.id}, {"name", m.name}, {"type", m.type},
        {"symbol", m.symbol}, {"currency", m.currency},
        {"status", m.status}, {"currentPrice", m.currentPrice},
        {"previousClose", m.previousClose}, {"change", m.change},
        {"changePercent", m.changePercent}, {"volume", m.volume},
        {"averageVolume", m.averageVolume}, {"marketCap", m.marketCap},
        {"bid", m.bid}, {"ask", m.ask}, {"spread", m.spread},
        {"volatility", m.volatility}, {"liquidity", m.liquidity},
        {"lastUpdate", m.lastUpdate}
    };
}

inline void from_json(const nlohmann::json& j, Market& m) {
    m.id = j.value("id", m.id);
    m.name = j.value("name", m.name);
    if (j.contains("type")) m.type = j["type"].get<MarketType>();
    m.symbol = j.value("symbol", m.symbol);
    m.currency = j.value("currency", m.currency);
    if (j.contains("status")) m.status = j["status"].get<MarketStatus>();
    m.currentPrice = j.value("currentPrice", m.currentPrice);
    m.previousClose = j.value("previousClose", m.previousClose);
    m.change = j.value("change", m.change);
    m.changePercent = j.value("changePercent", m.changePercent);
    m.volume = j.value("volume", m.volume);
    m.averageVolume = j.value("averageVolume", m.averageVolume);
    m.marketCap = j.value("marketCap", m.marketCap);
    m.bid = j.value("bid", m.bid);
    m.ask = j.value("ask", m.ask);
    m.spread = j.value("spread", m.spread);
    m.volatility = j.value("volatility", m.volatility);
    if (j.contains("liquidity")) m.liquidity = j["liquidity"].get<LiquidityLevel>();
    m.lastUpdate = j.value("lastUpdate", m.lastUpdate);
}

// Matches TS MarketState from banking-game.ts
struct MarketState {
    MarketCycle cycle = MarketCycle::Expansion;
    double volatility = 0.15;
    double interestRate = 0.05;
    double inflation = 0.02;
    double gdpGrowth = 0.025;
    double unemployment = 0.04;
    double consumerConfidence = 70.0;
    MarketSentiment marketSentiment = MarketSentiment::Neutral;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MarketState,
    cycle, volatility, interestRate, inflation,
    gdpGrowth, unemployment, consumerConfidence, marketSentiment)

// Matches TS RiskMetrics from index.ts (bank-level)
struct BankRiskMetrics {
    double creditRisk = 0.0;
    double marketRisk = 0.0;
    double operationalRisk = 0.0;
    double liquidityRisk = 0.0;
    double concentrationRisk = 0.0;
    double interestRateRisk = 0.0;
    double systemicRisk = 0.0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BankRiskMetrics,
    creditRisk, marketRisk, operationalRisk, liquidityRisk,
    concentrationRisk, interestRateRisk, systemicRisk)

// Matches TS portfolioVar from trading-engine.ts RiskMetrics
struct PortfolioVaR {
    double oneDay = 0.0;
    double fiveDay = 0.0;
    double thirtyDay = 0.0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PortfolioVaR, oneDay, fiveDay, thirtyDay)

// Matches TS RiskMetrics from trading-engine.ts (trading-level)
struct TradingRiskMetrics {
    double portfolioValue = 0.0;
    double totalExposure = 0.0;
    double netExposure = 0.0;
    double grossExposure = 0.0;
    double leverage = 0.0;
    double marginUsed = 0.0;
    double marginAvailable = 0.0;
    bool marginCall = false;
    PortfolioVaR portfolioVar;
    double beta = 0.0;
    double sharpeRatio = 0.0;
    double maxDrawdown = 0.0;
    double currentDrawdown = 0.0;
    double volatility = 0.0;
};

inline void to_json(nlohmann::json& j, const TradingRiskMetrics& r) {
    j = nlohmann::json{
        {"portfolioValue", r.portfolioValue}, {"totalExposure", r.totalExposure},
        {"netExposure", r.netExposure}, {"grossExposure", r.grossExposure},
        {"leverage", r.leverage}, {"marginUsed", r.marginUsed},
        {"marginAvailable", r.marginAvailable}, {"marginCall", r.marginCall},
        {"portfolioVar", r.portfolioVar}, {"beta", r.beta},
        {"sharpeRatio", r.sharpeRatio}, {"maxDrawdown", r.maxDrawdown},
        {"currentDrawdown", r.currentDrawdown}, {"volatility", r.volatility}
    };
}

// Matches TS EarlyWarningIndicator from index.ts
struct EarlyWarningIndicator {
    std::string id;
    std::string name;
    std::string description;
    double currentValue = 0.0;
    double normalLevel = 0.0;
    double warningLevel = 0.0;
    double criticalLevel = 0.0;
    TrendDirection trend = TrendDirection::Stable;
    bool isTriggering = false;
    double predictiveAccuracy = 0.0;
    double leadTimeMonths = 0.0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EarlyWarningIndicator,
    id, name, description, currentValue, normalLevel, warningLevel,
    criticalLevel, trend, isTriggering, predictiveAccuracy, leadTimeMonths)

// ════════════════════════════════════════════════════════════════════
// Economic indicators
// ════════════════════════════════════════════════════════════════════

struct EconomicIndicators {
    double fedFundsRate = 0.05;
    double cpiInflation = 0.02;
    double gdpGrowth = 0.025;
    double unemployment = 0.04;
    double vix = 18.0;
    double sp500Return = 0.0;
    double treasuryYield10Y = 0.04;
    double creditSpread = 0.015;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EconomicIndicators,
    fedFundsRate, cpiInflation, gdpGrowth, unemployment,
    vix, sp500Return, treasuryYield10Y, creditSpread)

// ════════════════════════════════════════════════════════════════════
// Simulation session state
// ════════════════════════════════════════════════════════════════════

// ── Starting Position ─────────────────────────────────────────────
enum class StartingPosition {
    CommercialBank,    // Default: 1945 commercial bank (Commercial + Mortgage)
    TradingFirm,       // Unlockable: Lehman-style (Trading + Asset Management)
    InvestmentBank,    // Unlockable: Morgan Stanley-style (IB + Restructuring + Securitization)
    CommunityBank,     // Small local bank ($500M capital, high reputation)
    UniversalBank      // Post-Glass-Steagall giant ($200B, all era-available divisions)
};
NLOHMANN_JSON_SERIALIZE_ENUM(StartingPosition, {
    {StartingPosition::CommercialBank, "commercial_bank"},
    {StartingPosition::TradingFirm, "trading_firm"},
    {StartingPosition::InvestmentBank, "investment_bank"},
    {StartingPosition::CommunityBank, "community_bank"},
    {StartingPosition::UniversalBank, "universal_bank"},
})

struct SimulationConfig {
    uint64_t seed = 42;
    double ticksPerSecond = 1.0;       // Simulation ticks per real second
    double daysPerTick = 1.0;          // Simulated days per tick
    int quarterDurationDays = 63;      // ~63 trading days per quarter
    int startYear = 1945;
    int startQuarter = 1;
    int quartersPerGame = 0;           // 0 = unlimited (game ends via doom meters/events)

    // Difficulty-tunable reputation balance
    double reputationRecoveryRate = 5.0;          // Rep gained per profitable quarter
    double reputationCrisisDamageMultiplier = 1.0; // Multiplier on crisis rep damage (1.0 = severity * 1.0)
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SimulationConfig,
    seed, ticksPerSecond, daysPerTick, quarterDurationDays, startYear, startQuarter, quartersPerGame,
    reputationRecoveryRate, reputationCrisisDamageMultiplier)

// Four converging crises that force endgame resolution
struct DoomMeters {
    double aiDisplacement = 0.0;      // 0-100: AI singularity progress
    double climateCatastrophe = 0.0;  // 0-100: climate collapse progress
    double globalTensions = 0.0;      // 0-100: nuclear/war risk
    double inequality = 0.0;          // 0-100: political instability
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DoomMeters,
    aiDisplacement, climateCatastrophe, globalTensions, inequality)

struct SimulationState {
    std::string sessionId;
    int currentYear = 2024;
    int currentQuarter = 1;
    int currentDay = 0;            // Day within quarter (0..quarterDurationDays-1)
    int totalDaysElapsed = 0;
    bool paused = false;
    double speedMultiplier = 1.0;  // 0.5x, 1x, 2x, 4x
    MarketRegime regime = MarketRegime::Normal;

    MarketState marketState;
    EconomicIndicators economics;
    std::vector<Market> markets;
    TradingRiskMetrics tradingRisk;
    BankRiskMetrics bankRisk;
    std::vector<EarlyWarningIndicator> warnings;
    DoomMeters doomMeters;
};

inline void to_json(nlohmann::json& j, const SimulationState& s) {
    j = nlohmann::json{
        {"sessionId", s.sessionId}, {"currentYear", s.currentYear},
        {"currentQuarter", s.currentQuarter}, {"currentDay", s.currentDay},
        {"totalDaysElapsed", s.totalDaysElapsed}, {"paused", s.paused},
        {"speedMultiplier", s.speedMultiplier}, {"regime", s.regime},
        {"marketState", s.marketState}, {"economics", s.economics},
        {"markets", s.markets}, {"tradingRisk", s.tradingRisk},
        {"bankRisk", s.bankRisk}, {"warnings", s.warnings},
        {"doomMeters", s.doomMeters}
    };
}

// ════════════════════════════════════════════════════════════════════
// WebSocket message types — match TS WebSocketEvent from index.ts
// ════════════════════════════════════════════════════════════════════

struct WebSocketMessage {
    std::string type;
    nlohmann::json payload;
    std::string timestamp;

    nlohmann::json to_json() const {
        return nlohmann::json{
            {"type", type}, {"payload", payload}, {"timestamp", timestamp}
        };
    }
};

// ════════════════════════════════════════════════════════════════════
// API response — matches TS APIResponse from index.ts
// ════════════════════════════════════════════════════════════════════

template<typename T>
struct APIResponse {
    bool success;
    std::optional<T> data;
    std::optional<std::string> error;
    std::string timestamp;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["success"] = success;
        j["timestamp"] = timestamp;
        if (data) j["data"] = *data;
        if (error) j["error"] = nlohmann::json{{"code", "ERROR"}, {"message", *error}};
        return j;
    }
};

// ════════════════════════════════════════════════════════════════════
// Utility: ISO 8601 timestamp
// ════════════════════════════════════════════════════════════════════

inline std::string now_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time_t);
#else
    gmtime_r(&time_t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return std::string(buf) + "." + std::to_string(ms.count()) + "Z";
}

} // namespace stvg
