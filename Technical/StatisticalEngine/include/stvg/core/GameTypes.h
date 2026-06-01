#pragma once

// GameTypes.h — Structs and enums for the game loop, extracted from
// QuarterlyTurnManager.h to break circular dependencies and enable
// handlers/serializers to reference these types independently.

#include "Types.h"
#include "ScoringEngine.h"
#include "RegulatoryEngine.h"
#include "../simulation/CrisisEngine.h"
#include "../simulation/EconomicEngine.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// Turn phases — the quarterly gameplay loop
// ════════════════════════════════════════════════════════════════════

enum class TurnPhase {
    QuarterStart,    // Generate events, messages, present dashboard
    DecisionPhase,   // Player makes choices via API
    Simulation,      // 63 daily ticks run
    QuarterEnd,      // Calculate results, check crises
    CrisisResponse,  // If crisis triggered, force immediate decisions
    QuarterComplete  // Ready for next quarter
};
NLOHMANN_JSON_SERIALIZE_ENUM(TurnPhase, {
    {TurnPhase::QuarterStart, "quarter_start"},
    {TurnPhase::DecisionPhase, "decision_phase"},
    {TurnPhase::Simulation, "simulation"},
    {TurnPhase::QuarterEnd, "quarter_end"},
    {TurnPhase::CrisisResponse, "crisis_response"},
    {TurnPhase::QuarterComplete, "quarter_complete"},
})

// Quarterly report — narrative presentation of results
struct QuarterlyReport {
    int year;
    int quarter;

    // Bank financials
    double totalRevenue = 0;
    double totalCosts = 0;
    double netIncome = 0;
    double capitalRatio = 0;
    double leverageRatio = 0;

    // Division results
    nlohmann::json divisionResults;

    // Risk
    BankRiskMetrics riskMetrics;
    std::vector<EarlyWarningIndicator> warnings;

    // Narrative
    std::vector<std::string> headlines;
    std::string narrative;                          // Quarter summary paragraph
    std::string openingNarrative;                   // Quarter opening paragraph
    std::vector<std::string> crisisNarratives;      // Per-crisis escalation text

    // Scoring
    QuarterScore score;
    PlayerProgression progression;

    // Crisis
    std::vector<simulation::CrisisArc> activeCrises;
};

inline void to_json(nlohmann::json& j, const QuarterlyReport& r) {
    j = nlohmann::json{
        {"year", r.year}, {"quarter", r.quarter},
        {"totalRevenue", r.totalRevenue}, {"totalCosts", r.totalCosts},
        {"netIncome", r.netIncome}, {"capitalRatio", r.capitalRatio},
        {"leverageRatio", r.leverageRatio},
        {"divisionResults", r.divisionResults},
        {"riskMetrics", r.riskMetrics}, {"warnings", r.warnings},
        {"headlines", r.headlines},
        {"narrative", r.narrative}, {"openingNarrative", r.openingNarrative},
        {"crisisNarratives", r.crisisNarratives},
        {"score", r.score}, {"progression", r.progression},
        {"activeCrises", r.activeCrises}
    };
}

// Annual report — aggregated yearly summary with full financial statements
struct AnnualReport {
    int year = 0;
    double totalRevenue = 0;
    double totalCosts = 0;
    double netIncome = 0;
    double capitalStart = 0;
    double capitalEnd = 0;
    double capitalGrowthPct = 0;
    double capitalRatio = 0;
    double leverageRatio = 0;
    double avgQuarterScore = 0;
    PlayerProgression progression;
    RegulatoryState regulatoryStatus;
    std::string eraName;
    std::vector<std::string> headlines;
    std::string narrative;
    std::vector<std::string> keyEvents;
    int quartersPlayed = 0;

    // Income statement decomposition
    double interestIncome = 0;
    double interestExpense = 0;
    double netInterestIncome = 0;
    double feeIncome = 0;
    double tradingPnL = 0;
    double loanLossProvisions = 0;
    double fundingCost = 0;

    // Balance sheet snapshot
    double loans = 0;
    double securities = 0;
    double deposits = 0;
    double retailDeposits = 0;
    double wholesaleDeposits = 0;
    double interbankBorrowing = 0;
    double unrealizedLosses = 0;

    // Key ratios
    double nim = 0;              // net interest margin
    double roe = 0;              // return on equity
    double roa = 0;              // return on assets
    double efficiencyRatio = 0;  // expenses / revenue
    double loanToDepositRatio = 0;
    double nplRatio = 0;         // proxy: provisions / loans
};

inline void to_json(nlohmann::json& j, const AnnualReport& r) {
    j = nlohmann::json{
        {"year", r.year},
        {"totalRevenue", r.totalRevenue}, {"totalCosts", r.totalCosts},
        {"netIncome", r.netIncome},
        {"capitalStart", r.capitalStart}, {"capitalEnd", r.capitalEnd},
        {"capitalGrowthPct", r.capitalGrowthPct},
        {"capitalRatio", r.capitalRatio}, {"leverageRatio", r.leverageRatio},
        {"avgQuarterScore", r.avgQuarterScore},
        {"progression", r.progression},
        {"regulatoryStatus", r.regulatoryStatus},
        {"eraName", r.eraName},
        {"headlines", r.headlines}, {"narrative", r.narrative},
        {"keyEvents", r.keyEvents}, {"quartersPlayed", r.quartersPlayed},
        {"interestIncome", r.interestIncome},
        {"interestExpense", r.interestExpense},
        {"netInterestIncome", r.netInterestIncome},
        {"feeIncome", r.feeIncome},
        {"tradingPnL", r.tradingPnL},
        {"loanLossProvisions", r.loanLossProvisions},
        {"fundingCost", r.fundingCost},
        {"loans", r.loans}, {"securities", r.securities},
        {"deposits", r.deposits},
        {"retailDeposits", r.retailDeposits},
        {"wholesaleDeposits", r.wholesaleDeposits},
        {"interbankBorrowing", r.interbankBorrowing},
        {"unrealizedLosses", r.unrealizedLosses},
        {"nim", r.nim}, {"roe", r.roe}, {"roa", r.roa},
        {"efficiencyRatio", r.efficiencyRatio},
        {"loanToDepositRatio", r.loanToDepositRatio},
        {"nplRatio", r.nplRatio}
    };
}

// ════════════════════════════════════════════════════════════════════
// Simulation Digest — notable events from the 63-day simulation
// ════════════════════════════════════════════════════════════════════

struct SimulationDigestEvent {
    int day;
    std::string description;
    std::string type; // "market", "trader", "economic"
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SimulationDigestEvent, day, description, type)

// ════════════════════════════════════════════════════════════════════
// Game End — win/lose conditions
// ════════════════════════════════════════════════════════════════════

enum class GameEndReason {
    None,
    // Losses
    Bankrupt,           // capital <= 0
    RegulatorySeizure,  // capitalRatio < 2%
    ReputationCollapse, // reputation <= 5
    BoardFired,         // score < 20 for 4 consecutive quarters (or < 35 for 8)
    LiquidityCrisis,    // deposit-to-asset ratio collapse (bank run)
    FraudScandal,       // hidden risk surfaced as fraud
    // Wins
    GrowthTarget,       // totalAssets >= $2T
    LegendStatus,       // level >= 40
    SurvivorWin,        // survived all quarters with avg score > 60
    // Endgame (legacy — kept for save-game compat, no longer triggered directly)
    AISingularity,
    ClimateCollapse,
    // Endgame resolution paths
    EndgameCollapse,        // Path 1 — multiple meters maxed, world in ruins
    EndgameFortress,        // Path 2 — bank survived but abandoned the world
    EndgameManagedDecline,  // Path 3 — meters in 60-90 range, status quo barely held
    EndgameReconstruction,  // Path 4 — capital burned fighting crises, world recovered
    EndgameTransformation,  // Path 5 — actively reshaped the system
    EndgameAbolition        // Path 6 — bank deliberately dismantled into public utility
};
NLOHMANN_JSON_SERIALIZE_ENUM(GameEndReason, {
    {GameEndReason::None, "none"},
    {GameEndReason::Bankrupt, "bankrupt"},
    {GameEndReason::RegulatorySeizure, "regulatory_seizure"},
    {GameEndReason::ReputationCollapse, "reputation_collapse"},
    {GameEndReason::BoardFired, "board_fired"},
    {GameEndReason::LiquidityCrisis, "liquidity_crisis"},
    {GameEndReason::FraudScandal, "fraud_scandal"},
    {GameEndReason::GrowthTarget, "growth_target"},
    {GameEndReason::LegendStatus, "legend_status"},
    {GameEndReason::SurvivorWin, "survivor_win"},
    {GameEndReason::AISingularity, "ai_singularity"},
    {GameEndReason::ClimateCollapse, "climate_collapse"},
    {GameEndReason::EndgameCollapse, "endgame_collapse"},
    {GameEndReason::EndgameFortress, "endgame_fortress"},
    {GameEndReason::EndgameManagedDecline, "endgame_managed_decline"},
    {GameEndReason::EndgameReconstruction, "endgame_reconstruction"},
    {GameEndReason::EndgameTransformation, "endgame_transformation"},
    {GameEndReason::EndgameAbolition, "endgame_abolition"},
})

struct GameEndState {
    GameEndReason reason = GameEndReason::None;
    bool isVictory = false;
    std::string title;
    std::string narrative;
    int quartersPlayed = 0;
    double avgScore = 0;
};

inline void to_json(nlohmann::json& j, const GameEndState& e) {
    j = nlohmann::json{
        {"reason", e.reason}, {"isVictory", e.isVictory},
        {"title", e.title}, {"narrative", e.narrative},
        {"quartersPlayed", e.quartersPlayed}, {"avgScore", e.avgScore}
    };
}

// ════════════════════════════════════════════════════════════════════
// Custom-economy injection seam (v3)
// ════════════════════════════════════════════════════════════════════
class CustomEconomyHook {
public:
    virtual ~CustomEconomyHook() = default;
    virtual void dailyTick(double dt, simulation::EconomicEngine& engine) = 0;
    virtual void quarterTick(simulation::EconomicEngine& engine) = 0;
};

} // namespace stvg
