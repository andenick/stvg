#pragma once

#include "GameTypes.h"
#include "BankState.h"
#include "CEOProfile.h"
#include "ScoringEngine.h"
#include "SimulationClock.h"
#include "ConsequenceTracker.h"
#include "MacroHistory.h"
#include "../math/RandomEngine.h"
#include "../simulation/MarketSimulator.h"
#include "../simulation/EconomicEngine.h"
#include "../simulation/TraderAI.h"
#include "../simulation/EventEngine.h"
#include "../simulation/DecisionEngine.h"
#include "../simulation/DeleveragingDecisions.h"
#include "../simulation/CharacterEngine.h"
#include "../simulation/CrisisEngine.h"
#include "../simulation/CharacterGenerator.h"
#include "../simulation/DealPortfolio.h"
#include "../simulation/PersonalBook.h"
#include "../simulation/NarrativeEngine.h"
#include "../simulation/HistoricalEventLoader.h"
#include "../simulation/CompetitorEngine.h"
#include "../simulation/AICEOEngine.h"
#include "../simulation/ClimateEngine.h"
#include "../simulation/PoliticalEngine.h"
#include "../simulation/DurationRisk.h"
#include "../game/DecisionHandler.h"
#include "../game/CEOAbilityHandler.h"
#include "../game/CrisisHandler.h"
#include "../game/AnnualReportBuilder.h"
#include "EraEngine.h"
#include "RiskWeightedAssets.h"
#include "RegulatoryEngine.h"
#include "PathEngine.h"
#include "ReputationLens.h"
#include "ArchetypeRegistry.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include <vector>
#include <optional>

namespace stvg {

// ════════════════════════════════════════════════════════════════════
// QuarterlyTurnManager — orchestrates the full gameplay loop
// ════════════════════════════════════════════════════════════════════

class QuarterlyTurnManager {
public:
    QuarterlyTurnManager(const SimulationConfig& config, const BankConfig& bankConfig = {},
                         const std::string& ceoId = "",
                         StartingPosition startPos = StartingPosition::CommercialBank)
        : rng_(config.seed)
        , clock_(config.startYear, config.startQuarter, config.quarterDurationDays)
        , econ_(rng_)
        , marketSim_(rng_, econ_)
        , traderAI_(rng_)
        , decisionEngine_(rng_)
        , crisisEngine_(rng_)
        , consequenceTracker_(rng_)
        , charGen_(rng_)
        , dealPortfolio_(rng_)
        , config_(config)
        , competitorEngine_(rng_)
        , climateEngine_(rng_)
        , politicalEngine_(rng_)
    {
        // Initialize era engine first so we can use era-specific economy values
        eraEngine_.init();
        regulatoryEngine_.init();
        regulatoryEngine_.updateForYear(config.startYear);

        // Initialize economy from era-specific starting values (not hardcoded)
        const auto& eraEcon = eraEngine_.currentEra().initialEconomy;
        EconomicIndicators initial;
        initial.fedFundsRate = eraEcon.fedFundsRate;
        initial.cpiInflation = eraEcon.cpiInflation;
        initial.gdpGrowth = eraEcon.gdpGrowth;
        initial.unemployment = eraEcon.unemployment;
        initial.vix = eraEcon.vix;
        initial.treasuryYield10Y = eraEcon.treasuryYield10Y;
        initial.creditSpread = eraEcon.creditSpread;
        econ_.init(initial);
        marketSim_.init(config.startYear);
        marketSim_.setVolatilityScale(config.marketVolScale);

        // Apply CEO profile if specified
        BankConfig effectiveBankConfig = bankConfig;
        if (!ceoId.empty()) {
            if (auto* profile = CEOProfile::findById(ceoId)) {
                ceoProfile_ = *profile;
                hasCeo_ = true;
                effectiveBankConfig.startingCapital = profile->startingCapital;
                effectiveBankConfig.startingReputation = profile->startingReputation;
                spdlog::info("CEO selected: {} ({})", profile->name, profile->nickname);
            }
        }

        // Initialize bank
        bank_ = Bank::create(effectiveBankConfig);
        if (hasCeo_) {
            bank_.visibilityBonus = ceoProfile_.visibilityBonus;
        }

        // Override divisions for alternative starting positions
        if (startPos != StartingPosition::CommercialBank) {
            bank_.divisions.clear();
            auto divTypes = divisionsForPosition(startPos);
            double budgetPerDiv = effectiveBankConfig.startingCapital / divTypes.size();
            int empPerDiv = effectiveBankConfig.startingEmployees / (int)divTypes.size();
            for (auto type : divTypes) {
                Division div;
                div.id = "div_" + std::to_string((int)type);
                div.type = type;
                div.budget = budgetPerDiv;
                div.employees = empPerDiv;
                div.autonomyLevel = 0.3;
                div.morale = 60.0;
                // Name from type
                nlohmann::json j = type;
                div.name = j.get<std::string>();
                bank_.divisions.push_back(div);
            }
            spdlog::info("Starting position: {} ({} divisions)",
                nlohmann::json(startPos).get<std::string>(), divTypes.size());
            bank_.recalcBalanceSheet();
        }

        actionPoints_.reset(bank_.totalAssets);

        // Generate initial traders
        traders_ = traderAI_.generateCandidates(5);

        // Generate initial staff for each division
        for (auto& div : bank_.divisions) {
            int staffCount = std::max(div.employees / 5, 2); // 2-10 initial staff
            div.staff = charGen_.generateCandidates(staffCount, config.startYear, bank_.reputation);
            div.employees = div.staffCount();
            // Assign division head personality (higher reputation attracts more honest heads)
            assignDivisionHeadHonesty(div);
        }

        // Initialize state
        state_.sessionId = "game_" + std::to_string(config.seed);
        state_.currentYear = config.startYear;
        state_.currentQuarter = config.startQuarter;
        state_.markets = marketSim_.snapshot();
        state_.economics = econ_.state();
        state_.marketState = econ_.toMarketState();
        state_.regime = marketSim_.currentRegime(econ_.stressScore());

        phase_ = TurnPhase::QuarterStart;

        // Load historical calibration data if available (overrides hand-tuned defaults)
        eraEngine_.loadCalibrationFromFile("data/calibration/historical_era_params.json");

        // Apply era-specific OU economy params (targets for mean-reversion)
        econ_.setParams(eraEngine_.currentEra().economyParams);
        politicalEngine_.setEraModifier(eraEngine_.currentEra().id);

        // Initialize competitor banks
        competitorEngine_.init(config.startYear);

        // Wire PoliticalEngine into DecisionEngine for live lobby probabilities
        decisionEngine_.setPoliticalEngine(&politicalEngine_);

        // Track starting and peak capital for balance mechanics
        startingCapital_ = bank_.capital;
        peakCapital_ = bank_.capital;
        annualAccum_.yearStartCapital = bank_.capital;

        // STAR_02 P7: seed the CEO's off-balance-sheet personal trading account
        // with 5% of starting capital (the light "tradey" hedge). Kept separate
        // from bank_.capital so the SFC identity in Bank::endQuarter() is
        // untouched — see PersonalBook.h for the accounting rationale.
        personalBook_.seed(startingCapital_);

        // Try loading historical events (non-fatal if file not found)
        for (const auto& path : {"data/events/historical_events.json",
                                  "../data/events/historical_events.json",
                                  "static/../data/events/historical_events.json"}) {
            if (historicalLoader_.loadFromFile(path)) {
                historicalEventsLoaded_ = true;
                break;
            }
        }

        spdlog::info("QuarterlyTurnManager initialized: {} Q{}{}{}", config.startYear, config.startQuarter,
            hasCeo_ ? " (CEO: " + ceoProfile_.name + ")" : "",
            historicalEventsLoaded_ ? " [historical events loaded]" : "");
    }

    // ── Phase management ────────────────────────────────────────

    TurnPhase currentPhase() const { return phase_; }

    // Advance to next phase. Returns the new phase.
    TurnPhase advancePhase() {
        switch (phase_) {
            case TurnPhase::QuarterStart:
                onQuarterStart();
                phase_ = TurnPhase::DecisionPhase;
                break;

            case TurnPhase::DecisionPhase:
                // Player must submit decisions before advancing
                phase_ = TurnPhase::Simulation;
                break;

            case TurnPhase::Simulation:
                if (!realTimeSimulation_) {
                    runSimulation();
                    phase_ = TurnPhase::QuarterEnd;
                }
                break;

            case TurnPhase::QuarterEnd:
                onQuarterEnd();
                if (crisisEngine_.hasActiveCrisis()) {
                    phase_ = TurnPhase::CrisisResponse;
                } else {
                    phase_ = TurnPhase::QuarterComplete;
                }
                break;

            case TurnPhase::CrisisResponse: {
                // If player calls advancePhase() without responding, auto-resolve
                // with "measured" response (backwards-compatible fallback).
                // Preferred path: player calls respondToCrisis() per crisis via API.
                std::vector<std::string> unresolvedIds;
                for (const auto& crisis : crisisEngine_.activeCrises()) {
                    if (!crisis.resolved) unresolvedIds.push_back(crisis.id);
                }
                for (const auto& id : unresolvedIds) {
                    crisisEngine_.resolveWithResponse(id, "measured");
                }
                phase_ = TurnPhase::QuarterComplete;
                break;
            }

            case TurnPhase::QuarterComplete:
                // Reset for next quarter
                phase_ = TurnPhase::QuarterStart;
                break;
        }

        spdlog::info("Phase -> {}", nlohmann::json(phase_).get<std::string>());
        return phase_;
    }

    // ── Decision handling ───────────────────────────────────────

    bool submitDecision(const std::string& decisionId, const std::string& optionId) {
        bool ok = game::DecisionHandler::submit(
            decisionId, optionId, pendingDecisions_, actionPoints_,
            bank_, consequenceTracker_, traderAI_, traders_,
            recentDividendQuarters_, config_, state_);
        if (ok && decisionId.find("duration") != std::string::npos) {
            if (optionId.find("_short") != std::string::npos)
                durationRisk_.target = simulation::DurationTarget::Short;
            else if (optionId.find("_long") != std::string::npos)
                durationRisk_.target = simulation::DurationTarget::Long;
            else
                durationRisk_.target = simulation::DurationTarget::Balanced;
            spdlog::info("Duration target set to {}", optionId.find("_short") != std::string::npos ? "Short" :
                optionId.find("_long") != std::string::npos ? "Long" : "Balanced");
        }
        return ok;
    }

    // ── Crisis response ─────────────────────────────────────────

    bool respondToCrisis(const std::string& crisisId, const std::string& responseType) {
        if (phase_ != TurnPhase::CrisisResponse) return false;

        bool ok = game::CrisisHandler::respond(
            crisisId, responseType, crisisEngine_, actionPoints_,
            hasCeo_, ceoProfile_);
        if (!ok) return false;

        if (!crisisEngine_.hasActiveCrisis()) {
            phase_ = TurnPhase::QuarterComplete;
            spdlog::info("Phase -> quarter_complete (all crises resolved)");
        }
        return true;
    }

    // ── Save/Load ───────────────────────────────────────────────
    //
    // Save format version history:
    //   v1 — initial (Phase 0)
    //   v2 — Phase 3.2: added consecutiveProfitableQuarters, recentDividendQuarters,
    //        doomMeters, state.currentDay/totalDaysElapsed
    //   v3 — STAR_02 P3.2: added macroHistory (per-quarter macro+close ring).
    //        Backward-compatible: a v1/v2 save loads with empty macroHistory.
    //   v4 — STAR_02 P7: added personalBook (CEO off-balance-sheet trading
    //        account). Backward-compatible: an older save keeps the seeded book.
    static constexpr int kSaveVersion = 4;

    nlohmann::json saveGame() const {
        return {
            {"version", kSaveVersion},
            {"rng", rng_.toSaveJson()},
            {"clock", {{"year", clock_.year()}, {"quarter", clock_.quarter()},
                       {"day", clock_.dayInQuarter()}, {"totalDays", clock_.totalDays()}}},
            {"phase", phase_},
            {"bank", bank_},
            {"economic", econ_.toSaveJson()},
            {"market", marketSim_.toSaveJson()},
            {"macroHistory", macroHistory_.toSaveJson()},
            {"personalBook", personalBook_.toSaveJson()},   // STAR_02 P7
            {"crisis", crisisEngine_.toSaveJson()},
            {"progression", progression_},
            {"ceo", {{"id", hasCeo_ ? ceoProfile_.id : ""},
                     {"cooldown", specialAbilityCooldown_},
                     {"abilityActive", specialAbilityActive_},
                     {"revenueBoost", revenueBoostActive_},
                     {"regShield", regulatoryShieldActive_},
                     {"crisisShield", crisisShieldActive_}}},
            {"era", {{"currentIndex", eraEngine_.currentEraIndex()}}},
            {"tracking", {
                {"startingCapital", startingCapital_},
                {"peakCapital", peakCapital_},
                {"consecutivePoorQuarters", consecutivePoorQuarters_},
                {"consecutiveMediocreQuarters", consecutiveMediocreQuarters_},
                {"consecutiveHighHiddenRiskQuarters", consecutiveHighHiddenRiskQuarters_},
                {"fraudTriggered", fraudTriggered_},
                {"consecutiveProfitableQuarters", consecutiveProfitableQuarters_},
                {"recentDividendQuarters", recentDividendQuarters_},
                {"totalScore", totalScore_},
                {"quartersCompleted", quartersCompleted_}
            }},
            {"annual", {
                {"yearStartCapital", annualAccum_.yearStartCapital},
                {"yearTotalRevenue", annualAccum_.yearTotalRevenue},
                {"yearTotalCosts", annualAccum_.yearTotalCosts},
                {"yearTotalScore", annualAccum_.yearTotalScore},
                {"yearQuarterCount", annualAccum_.yearQuarterCount}
            }},
            {"state", {
                {"currentDay", state_.currentDay},
                {"totalDaysElapsed", state_.totalDaysElapsed},
                {"doomMeters", {
                    {"aiDisplacement", state_.doomMeters.aiDisplacement},
                    {"climateCatastrophe", state_.doomMeters.climateCatastrophe},
                    {"globalTensions", state_.doomMeters.globalTensions},
                    {"inequality", state_.doomMeters.inequality}
                }}
            }}
        };
    }

    // Restore game state from a saveGame() blob. Returns false on version
    // mismatch or missing required fields. Designed for crash recovery and
    // tuning experiments — not designed to be tamper-proof.
    bool loadGame(const nlohmann::json& j) {
        if (!j.contains("version")) return false;
        int ver = j["version"].get<int>();
        if (ver < 1 || ver > kSaveVersion) {
            spdlog::warn("loadGame: unsupported save version {} (expected 1..{})",
                ver, kSaveVersion);
            return false;
        }

        try {
            // Engines that already have loadSaveJson()
            if (j.contains("rng"))      rng_.loadSaveJson(j["rng"]);
            if (j.contains("economic")) econ_.loadSaveJson(j["economic"]);
            if (j.contains("market"))   marketSim_.loadSaveJson(j["market"]);
            // P3.2 (v3): macroHistory is backward-compatible — missing → empty.
            if (j.contains("macroHistory")) macroHistory_.loadSaveJson(j["macroHistory"]);
            else macroHistory_.clear();
            // STAR_02 P7: personal trading account (backward-compatible — a save
            // without it simply keeps the freshly-seeded book).
            if (j.contains("personalBook")) personalBook_.loadSaveJson(j["personalBook"]);
            if (j.contains("crisis"))   crisisEngine_.loadSaveJson(j["crisis"]);

            // Clock
            if (j.contains("clock")) {
                const auto& c = j["clock"];
                clock_.set(c.value("year", 1945), c.value("quarter", 1),
                           c.value("day", 0), c.value("totalDays", 0));
                state_.currentYear = clock_.year();
                state_.currentQuarter = clock_.quarter();
            }

            // Phase
            if (j.contains("phase")) phase_ = j["phase"].get<TurnPhase>();

            // Bank (custom helper — Bank has no from_json)
            if (j.contains("bank")) loadBankFromJson(bank_, j["bank"]);

            // Progression
            if (j.contains("progression")) progression_ = j["progression"].get<PlayerProgression>();

            // CEO (re-resolve profile by id; ability flags)
            if (j.contains("ceo")) {
                const auto& ce = j["ceo"];
                std::string id = ce.value("id", std::string{});
                if (!id.empty()) {
                    if (auto* p = CEOProfile::findById(id)) {
                        ceoProfile_ = *p;
                        hasCeo_ = true;
                    }
                } else {
                    hasCeo_ = false;
                }
                specialAbilityCooldown_ = ce.value("cooldown", 0);
                specialAbilityActive_   = ce.value("abilityActive", false);
                revenueBoostActive_     = ce.value("revenueBoost", false);
                regulatoryShieldActive_ = ce.value("regShield", false);
                crisisShieldActive_     = ce.value("crisisShield", false);
            }

            // Era — replay transitions up to the saved index
            if (j.contains("era")) {
                int targetIdx = j["era"].value("currentIndex", 0);
                while (eraEngine_.currentEraIndex() < targetIdx
                       && eraEngine_.shouldTransition(state_.currentYear)) {
                    eraEngine_.transition(state_.currentYear);
                }
            }

            // Bookkeeping
            if (j.contains("tracking")) {
                const auto& t = j["tracking"];
                startingCapital_              = t.value("startingCapital", startingCapital_);
                peakCapital_                  = t.value("peakCapital", peakCapital_);
                consecutivePoorQuarters_      = t.value("consecutivePoorQuarters", 0);
                consecutiveMediocreQuarters_  = t.value("consecutiveMediocreQuarters", 0);
                consecutiveHighHiddenRiskQuarters_ = t.value("consecutiveHighHiddenRiskQuarters", 0);
                fraudTriggered_               = t.value("fraudTriggered", false);
                consecutiveProfitableQuarters_= t.value("consecutiveProfitableQuarters", 0);
                recentDividendQuarters_       = t.value("recentDividendQuarters", 0);
                totalScore_                   = t.value("totalScore", 0.0);
                quartersCompleted_            = t.value("quartersCompleted", 0);
            }
            if (j.contains("annual")) {
                const auto& a = j["annual"];
                annualAccum_.yearStartCapital  = a.value("yearStartCapital", annualAccum_.yearStartCapital);
                annualAccum_.yearTotalRevenue  = a.value("yearTotalRevenue", 0.0);
                annualAccum_.yearTotalCosts    = a.value("yearTotalCosts", 0.0);
                annualAccum_.yearTotalScore    = a.value("yearTotalScore", 0.0);
                annualAccum_.yearQuarterCount  = a.value("yearQuarterCount", 0);
            }

            // SimulationState fragments (v2+)
            if (j.contains("state")) {
                const auto& s = j["state"];
                state_.currentDay       = s.value("currentDay", 0);
                state_.totalDaysElapsed = s.value("totalDaysElapsed", 0);
                if (s.contains("doomMeters")) {
                    const auto& dm = s["doomMeters"];
                    state_.doomMeters.aiDisplacement     = dm.value("aiDisplacement", 0.0);
                    state_.doomMeters.climateCatastrophe = dm.value("climateCatastrophe", 0.0);
                    state_.doomMeters.globalTensions     = dm.value("globalTensions", 0.0);
                    state_.doomMeters.inequality         = dm.value("inequality", 0.0);
                }
            }

            spdlog::info("loadGame: restored save v{} at {} Q{}",
                ver, state_.currentYear, state_.currentQuarter);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("loadGame failed: {}", e.what());
            return false;
        }
    }

    // ── Getters ─────────────────────────────────────────────────

    // Game-over: capital depleted or regulatory seizure
    const EraEngine& eraEngine() const { return eraEngine_; }
    const EraDefinition& currentEra() const { return eraEngine_.currentEra(); }
    const RegulatoryEngine& regulatoryEngine() const { return regulatoryEngine_; }
    const RegulatoryState& regulatoryState() const { return regulatoryEngine_.state(); }
    const AnnualReport& lastAnnualReport() const { return lastAnnualReport_; }
    const PathEngine& pathEngine() const { return pathEngine_; }
    PathEngine& pathEngine() { return pathEngine_; }
    const simulation::CompetitorEngine& competitorEngine() const { return competitorEngine_; }
    const simulation::CrisisEngine& crisisEngine() const { return crisisEngine_; }
    const std::vector<SimulationDigestEvent>& simulationDigest() const { return simulationDigest_; }

    bool isDivisionAvailable(DivisionType type) const {
        return eraEngine_.isDivisionAvailable(type, state_.currentYear)
            && !regulatoryEngine_.isDivisionProhibited(type)
            && pathEngine_.canOpenDivision(type);
    }

    bool isGameOver() const {
        return checkGameEnd().reason != GameEndReason::None;
    }

    int quartersCompleted() const { return quartersCompleted_; }
    double averageScore() const {
        return quartersCompleted_ > 0 ? totalScore_ / quartersCompleted_ : 0;
    }

    // Game-end checks: defined out-of-line in EndgameResolver.h (Phase 3.1 split).
    GameEndState checkGameEnd() const;
    GameEndState resolveEndgame() const;

    const std::vector<simulation::Decision>& pendingDecisions() const { return pendingDecisions_; }
    const std::vector<simulation::CharacterMessage>& messages() const { return quarterMessages_; }
    const std::vector<simulation::GameEvent>& quarterEvents() const { return quarterEvents_; }
    const Bank& bank() const { return bank_; }
    const SimulationState& state() const { return state_; }
    // P3.2: per-quarter macro history (for chart hydration + REST endpoint).
    const MacroHistory& macroHistory() const { return macroHistory_; }
    nlohmann::json macroHistoryJson() const { return macroHistory_.toApiJson(); }
    const ActionPointBudget& actionPoints() const { return actionPoints_; }
    const PlayerProgression& progression() const { return progression_; }
    const std::vector<simulation::Trader>& traders() const { return traders_; }
    const QuarterlyReport& lastReport() const { return lastReport_; }
    const std::vector<simulation::EmployeeCandidate>& hiringPool() const { return hiringPool_; }
    // STAR_02 P6: outstanding poach offers (rival divisions you can buy whole).
    const std::vector<simulation::PoachOffer>& poachOffers() const { return poachOffers_; }
    // STAR_02 P6: the ReputationLens tag the world recognizes you by.
    std::string reputationTag() const { return ReputationLens::tagFor(pathEngine_.state()); }
    const std::vector<simulation::DealOpportunity>& availableDeals() const { return dealPortfolio_.availableDeals(); }
    const std::vector<simulation::ActiveDeal>& activeDeals() const { return dealPortfolio_.activeDeals(); }

    // P5 §5.3 test/telemetry seam: active event-driven per-quarter drift for a
    // division (0 if none). Lets a test register a known event and assert the
    // resulting trading/division nudge without depending on RNG event draws.
    double eventDriftFor(const std::string& divisionPascalName) const {
        auto it = eventDivisionDrift_.find(divisionPascalName);
        return (it != eventDivisionDrift_.end() && it->second.quartersLeft > 0)
            ? it->second.perQuarter : 0.0;
    }
    bool historicalEventsLoaded() const { return historicalEventsLoaded_; }
    // Force-register an event's drift (bypasses the RNG draw) — used by tests
    // and only meaningful when eventMarketCoupling is enabled in config.
    void registerEventDriftForTest(const std::string& eventId) { registerEventDrift(eventId); }

    bool acceptDeal(const std::string& dealId, double investmentAmount) {
        // Guard against NaN/inf and non-positive amounts before any state change.
        if (!(investmentAmount > 0.0) || !std::isfinite(investmentAmount)) return false;
        int currentQ = state_.currentQuarter + (state_.currentYear - config_.startYear) * 4;
        if (investmentAmount > bank_.capital * 0.3) return false; // Max 30% of capital per deal
        // Only deduct capital AFTER the deal is confirmed to exist — otherwise an
        // unknown/expired dealId would silently leak capital (it returned false
        // to the route but had already debited the bank). Order matters.
        if (!dealPortfolio_.acceptDeal(dealId, investmentAmount, currentQ)) return false;
        bank_.capital -= investmentAmount;
        return true;
    }

    // ── STAR_02 P7: personal trading book ──────────────────────────────────────
    // Current mid price for a market id (0 if unknown). Reads the live snapshot.
    double marketPriceFor(const std::string& marketId) const {
        for (const auto& m : marketSim_.snapshot())
            if (m.id == marketId) return m.currentPrice;
        return 0.0;
    }

    // Live prices keyed by id — for mark-to-market.
    std::unordered_map<std::string, double> marketPriceMap() const {
        std::unordered_map<std::string, double> px;
        for (const auto& m : marketSim_.snapshot()) px[m.id] = m.currentPrice;
        return px;
    }

    // Execute a market order on the CEO's personal account. side = "buy"|"sell",
    // amount = dollar notional. Exogenous player input (no RNG → lockstep-safe).
    // Returns the PersonalBook::TradeResult (ok + reason on failure). Capital on
    // the bank's books is NEVER touched (off-balance-sheet — see PersonalBook.h).
    simulation::PersonalBook::TradeResult trade(const std::string& marketId,
                                                const std::string& side, double amount) {
        simulation::PersonalBook::TradeResult r;
        if (!marketSim_.hasMarket(marketId)) { r.err = "unknown market"; return r; }
        double price = marketPriceFor(marketId);
        if (side == "buy") {
            return personalBook_.buy(marketId, amount, price, bank_.capital, state_.currentYear);
        } else if (side == "sell") {
            return personalBook_.sell(marketId, amount, price);
        }
        r.err = "side must be buy or sell";
        return r;
    }

    // Player-view JSON for the personal account (positions + marked value/P&L).
    nlohmann::json tradeBookJson() const {
        return personalBook_.toPlayerJson(marketPriceMap());
    }

    const simulation::PersonalBook& personalBook() const { return personalBook_; }

    // Hire a candidate from the pool into a division
    bool hireEmployee(const std::string& candidateId, const std::string& divisionId) {
        auto* div = bank_.getDivision(divisionId);
        if (!div) return false;

        auto it = std::find_if(hiringPool_.begin(), hiringPool_.end(),
            [&](const simulation::EmployeeCandidate& c) { return c.id == candidateId; });
        if (it == hiringPool_.end()) return false;

        // Signing bonus = 1 quarter salary (honest wage -> in-economy cost, 0.9)
        double signingCost = it->annualSalary * simulation::SALARY_COST_SCALE / 4.0;
        bank_.capital -= signingCost;

        simulation::Archetype hiredArchetype = it->archetype;
        div->staff.push_back(std::move(*it));
        hiringPool_.erase(it);
        div->employees = div->staffCount();
        div->morale = std::min(div->morale + 2.0, 100.0); // New hire boosts morale

        // STAR_02 P5 §3: hiring shifts the bank's institutional culture along
        // the path axes via the family cultureShift vector (un-breaks the
        // NORTH_STAR "hiring gunslingers shifts culture" mechanic).
        pathEngine_.applyHireEffect(hiredArchetype);

        spdlog::info("Hired {} to {} (signing cost: ${:.0f})",
            div->staff.back().name, div->name, signingCost);
        return true;
    }

    // STAR_02 P6: accept a poach offer. Deducts askPrice, creates/boosts the
    // division with imported staff of the offer's archetype mix, imports a chunk
    // of the rival's σ/β by stamping the team's families, fires a reputation
    // event, and weakens + angers the poached rival. Returns false if unknown,
    // expired, or unaffordable.
    bool acceptPoach(const std::string& offerId) {
        auto it = std::find_if(poachOffers_.begin(), poachOffers_.end(),
            [&](const simulation::PoachOffer& o) { return o.id == offerId && o.active; });
        if (it == poachOffers_.end()) return false;
        if (it->askPrice > bank_.capital * 0.6) return false;  // can't bet >60% of capital
        if (!std::isfinite(it->askPrice) || it->askPrice <= 0) return false;

        // 1. Deduct the price.
        bank_.capital -= it->askPrice;

        // 2. Create or boost the division of that type.
        Division* div = bank_.getDivisionByType(it->divisionType);
        if (!div) {
            Division nd;
            nd.id = "div_poach_" + std::to_string(++poachCounter_);
            nd.type = it->divisionType;
            nd.name = it->divisionName;
            nd.autonomyLevel = 0.4;
            nd.morale = 70.0;
            // Seed the new division's budget from the price paid (capability bought).
            nd.budget = std::max(it->askPrice * 0.5, 100.0);
            bank_.divisions.push_back(std::move(nd));
            div = &bank_.divisions.back();
        } else {
            div->budget += it->askPrice * 0.3;   // boost existing line
            div->morale = std::min(div->morale + 5.0, 100.0);
        }

        // 3. Import the team — staff of the offer's archetype mix (imports their
        // σ/β by family, consumed by the P5 distribution + crowding).
        int idx = 0;
        for (const auto& famId : it->archetypeMix) {
            simulation::EmployeeCandidate emp;
            emp.id = "poach_" + it->id + "_" + std::to_string(++idx);
            // Map the family id string back to the Archetype enum.
            try { nlohmann::json j = famId; emp.archetype = j.get<simulation::Archetype>(); }
            catch (...) { emp.archetype = simulation::Archetype::Operator; }
            emp.name = it->rivalName + " alum " + std::to_string(idx);
            for (int s = 0; s < 10; ++s) emp.stats.stat(s) = 62; // seasoned team
            emp.yearsExperience = 12;
            emp.level = simulation::CareerLevel::Director;
            emp.annualSalary = 95000.0;
            emp.potentialRevealed = true; // a known team, not a blind hire
            div->staff.push_back(std::move(emp));
            // Each imported banker nudges culture (gunslinger team → riskier DNA).
            pathEngine_.applyHireEffect(div->staff.back().archetype);
        }
        div->employees = div->staffCount();

        // 4. Reputation event: a splashy raid lifts visibility/reputation.
        bank_.reputation = std::clamp(bank_.reputation + 3.0, 0.0, 100.0);
        lastReport_.headlines.push_back(
            "TEAM RAID: poached " + it->divisionName + " team from " + it->rivalName);

        // 5. Weaken + anger the poached rival.
        competitorEngine_.applyPoachToRival(it->rivalId, it->teamSize);
        bank_.recalcBalanceSheet();

        spdlog::info("POACH ACCEPTED: {} team from {} for ${:.0f} ({} bankers)",
            it->divisionName, it->rivalName, it->askPrice, it->teamSize);

        it->active = false;
        poachOffers_.erase(it);
        return true;
    }

    // STAR_02 P5 rebalance seam: populate each division with a small starting
    // staff whose archetype mix reflects a risk appetite (0..1). High risk →
    // gunslinger/dealmaker-heavy (high β, high σ); low risk → lifer/operator/
    // patrician (low β, low σ). This makes the archetype P&L distribution
    // actually engage for autoplay bots (which otherwise never hire), so the
    // aggressive-vs-conservative variance spread the rebalance checks is real.
    // Uses charGen_ (shared rng_) — call ONCE before the sim loop starts.
    void seedArchetypeStaff(double riskTolerance, int perDivision = 4) {
        using simulation::Archetype;
        // Aggressive pool vs conservative pool; blend by riskTolerance.
        const std::array<Archetype, 4> aggressive{
            Archetype::Gunslinger, Archetype::Dealmaker, Archetype::Prodigy, Archetype::Quant};
        const std::array<Archetype, 4> conservative{
            Archetype::Lifer, Archetype::Operator, Archetype::Patrician, Archetype::Rainmaker};
        int counter = 0;
        for (auto& div : bank_.divisions) {
            for (int i = 0; i < perDivision; ++i) {
                bool pickAggressive = rng_.uniform() < riskTolerance;
                const auto& pool = pickAggressive ? aggressive : conservative;
                Archetype a = pool[(size_t)(rng_.uniform() * pool.size()) % pool.size()];
                simulation::EmployeeCandidate emp;
                emp.id = "seed_" + std::to_string(++counter);
                emp.archetype = a;
                emp.name = "Staff " + std::to_string(counter);
                // Reasonable mid-career stats so staffQualityMultiplier is sane.
                for (int s = 0; s < 10; ++s) emp.stats.stat(s) = 55;
                emp.yearsExperience = 8;
                emp.annualSalary = 70000.0;
                emp.level = simulation::CareerLevel::VP;
                div.staff.push_back(std::move(emp));
            }
            div.employees = div.staffCount();
        }
    }

    // STAR_02 P6: intra-quarter hiring trickle. Mirrors DealPortfolio's deal
    // trickle (called from the SAME daily-tick draw site in BOTH the batch and
    // day-by-day paths, so it is RNG-lockstep-safe). Appends ONE fresh candidate
    // to hiringPool_, capped, tilted by the current ReputationLens. The arriving
    // candidate's id is recorded so the frontend can animate it in.
    void addTrickleCandidate() {
        constexpr int kMaxPool = 8;
        if ((int)hiringPool_.size() >= kMaxPool) return;
        auto tilt = ReputationLens::compute(pathEngine_.state()).familyMult;
        auto fresh = charGen_.generateCandidates(1, state_.currentYear, bank_.reputation, &tilt);
        if (!fresh.empty()) hiringPool_.push_back(std::move(fresh.front()));
    }

    // Adjust division autonomy (costs 1 AP)
    bool setDivisionAutonomy(const std::string& divisionId, double level) {
        auto* div = bank_.getDivision(divisionId);
        if (!div) return false;
        if (!actionPoints_.spend(1)) return false;
        div->autonomyLevel = std::clamp(level, 0.0, 1.0);
        spdlog::info("Division {} autonomy set to {:.0f}%", div->name, div->autonomyLevel * 100);
        return true;
    }

    // Fire an employee from any division
    bool fireEmployee(const std::string& employeeId) {
        for (auto& div : bank_.divisions) {
            auto it = std::find_if(div.staff.begin(), div.staff.end(),
                [&](const simulation::EmployeeCandidate& e) { return e.id == employeeId; });
            if (it != div.staff.end()) {
                double severance = it->annualSalary * simulation::SALARY_COST_SCALE / 4.0;
                bank_.capital -= severance;
                div.morale = std::max(div.morale - 5.0, 10.0);

                spdlog::info("Fired {} from {} (severance: ${:.0f})", it->name, div.name, severance);
                div.staff.erase(it);
                div.employees = div.staffCount();
                return true;
            }
        }
        return false;
    }

    const SimulationConfig& config() const { return config_; }

    // ── Real-time simulation support ─────────────────────────────
    void setRealTimeSimulation(bool v) { realTimeSimulation_ = v; }
    bool isRealTimeSimulation() const { return realTimeSimulation_; }

    void prepareSimulation();
    bool tickSimulationDay();       // returns true when all days are done
    void completeSimulationPhase(); // finalize state after day-by-day sim

    nlohmann::json marketTickPayload() const {
        nlohmann::json prices = nlohmann::json::array();
        std::unordered_map<std::string, double> pxMap;
        for (const auto& m : marketSim_.snapshot()) {
            double dr = m.previousClose > 0
                ? (m.currentPrice - m.previousClose) / m.previousClose : 0.0;
            prices.push_back({{"id", m.id}, {"price", m.currentPrice}, {"dailyReturn", dr}});
            pxMap[m.id] = m.currentPrice;
        }
        return {
            {"day", clock_.dayInQuarter()},
            {"year", clock_.year()},
            {"quarter", clock_.quarter()},
            {"totalDay", clock_.totalDays()},
            {"prices", prices},
            {"bankCapital", bank_.capital},
            {"bankAssets", bank_.totalAssets},
            // Balance-sheet stream so the frontend can plot a live moving
            // balance sheet (equity / loans / assets) and show cash->investments.
            {"bankLoans", bank_.loans},
            {"bankSecurities", bank_.securities},
            {"bankReserves", bank_.reserves},
            {"bankDeposits", bank_.totalDeposits},
            {"bankInterbank", bank_.interbankBorrowing},
            // Stream live deal flow so trickle opportunities pop in mid-quarter.
            {"availableDeals", dealPortfolio_.availableDeals()},
            // STAR_02 P7: stream the loan-outcome feed + scoreboard so the Book
            // strip / outcome barks fire the moment a deal resolves, and the
            // personal trading book marks-to-market live against the ticking tape.
            {"dealOutcomes", dealPortfolio_.dealOutcomes()},
            {"loanBookStats", dealPortfolio_.loanBookStats()},
            {"personalBookValue", personalBook_.totalValue(pxMap)},
            {"personalBookPnl", personalBook_.unrealizedPnl(pxMap)},
            // STAR_02 P6: stream the hiring pool + poach offers so candidates
            // arrive intra-quarter (the frontend animates new ids) and poach
            // cards appear without waiting for the next quarter-boundary view.
            {"hiringPool", hiringPoolPlayerJson()},
            {"poachOffers", poachOffers_},
            // P3.2: per-day macro stream so macro lines move intraday like
            // markets do (EconomicEngine ticks daily). Read live from econ_.
            {"econ", {
                {"gdpLevel", econ_.state().gdpLevel},
                {"gdpGrowth", econ_.state().gdpGrowth},
                {"unemployment", econ_.state().unemployment},
                {"fedFundsRate", econ_.state().fedFundsRate},
                {"cpiInflation", econ_.state().cpiInflation},
                {"creditSpread", econ_.state().creditSpread},
                // P5 §5: economy-wide P&L series (v1 proxy).
                {"economyRevenue", econ_.state().economyRevenue},
                {"economyProfit", econ_.state().economyProfit}
            }},
            {"regime", marketSim_.currentRegime(econ_.stressScore())}
        };
    }

    // ── Restriction Flags (for --restrict diagnostic mode) ────────
    void setSkipDeleveraging(bool v) { skipDeleveraging_ = v; }
    void setSkipCrises(bool v) { skipCrises_ = v; }

    // ── CEO System ───────────────────────────────────────────────
    bool hasCeo() const { return hasCeo_; }
    const CEOProfile& ceoProfile() const { return ceoProfile_; }
    int specialAbilityCooldownRemaining() const { return specialAbilityCooldown_; }

    // Use the CEO's special ability (costs 3 AP, has cooldown)
    bool useSpecialAbility() {
        if (!hasCeo_) return false;

        game::CEOAbilityHandler::AbilityState abilityState{
            specialAbilityCooldown_, specialAbilityActive_,
            revenueBoostActive_, regulatoryShieldActive_, crisisShieldActive_
        };
        return game::CEOAbilityHandler::useAbility(
            ceoProfile_, bank_, actionPoints_, politicalEngine_, abilityState);
    }

    // Full state as JSON
    const std::vector<Consequence>& resolvedConsequences() const { return resolvedConsequences_; }
    int pendingConsequenceCount() const { return consequenceTracker_.pendingCount(); }

    // God view: full truth (for autoplay telemetry, end-of-game reveal)
    nlohmann::json toGodJson() const {
        nlohmann::json j = {
            {"phase", phase_},
            {"state", state_},
            {"bank", bank_},
            {"actionPoints", actionPoints_},
            {"progression", progression_},
            {"pendingDecisions", pendingDecisions_},
            {"messages", quarterMessages_},
            {"events", quarterEvents_},
            {"activeCrises", crisisEngine_.activeCrises()},
            {"simulationDigest", simulationDigest_},
            {"nextQuarterPreview", nextQuarterPreview_},
            {"streaks", {{"profit", streaks_.profit}, {"highScore", streaks_.highScore},
                         {"crisisFree", streaks_.crisisFree}, {"growth", streaks_.growth},
                         {"brokenMessage", streaks_.brokenMessage}}},
            {"traders", traders_},
            {"resolvedConsequences", resolvedConsequences_},
            {"pendingConsequenceCount", consequenceTracker_.pendingCount()},
            {"hiringPool", hiringPool_},
            {"poachOffers", poachOffers_},
            {"reputationTag", ReputationLens::tagFor(pathEngine_.state())},
            {"availableDeals", dealPortfolio_.availableDeals()},
            {"activeDeals", dealPortfolio_.activeDeals()},
            {"era", eraEngine_.currentEra()},
            {"regulatory", regulatoryEngine_.state()},
            {"competitors", competitorEngine_.toJson()},
            {"leaderboard", competitorEngine_.leaderboard(bank_)},
            {"aiCeo", aiCeoEngine_.state()},
            {"climate", climateEngine_.state()},
            {"political", politicalEngine_.state()},
            {"path", pathEngine_.state()}
        };
        if (hasCeo_) {
            j["ceo"] = ceoProfile_;
            j["specialAbilityCooldown"] = specialAbilityCooldown_;
        }
        auto endState = checkGameEnd();
        if (endState.reason != GameEndReason::None) {
            j["gameEnd"] = endState;
        }
        return j;
    }

    // Backward compat alias
    nlohmann::json toJson() const { return toGodJson(); }

    // STAR_02 P6: the filtered hiring pool as player-view JSON (hidden stats
    // hidden — you hire blind). Extracted so both toPlayerJson() and the live
    // market_tick stream share one shape. Includes specializationId for the
    // P6 candidate-card specialization display.
    nlohmann::json hiringPoolPlayerJson() const {
        nlohmann::json filteredPool = nlohmann::json::array();
        for (const auto& c : hiringPool_) {
            nlohmann::json ej;
            ej["id"] = c.id;
            ej["name"] = c.name;
            ej["archetype"] = c.archetype;
            ej["specializationId"] = c.specializationId; // P6 display
            ej["level"] = c.level;
            ej["annualSalary"] = c.annualSalary;
            ej["yearsExperience"] = c.yearsExperience;
            ej["stats"] = c.stats;
            nlohmann::json traits = nlohmann::json::array();
            for (const auto& t : c.traits) {
                traits.push_back({{"name", t.name}, {"description", t.description}, {"positive", t.positive}});
            }
            ej["traits"] = traits;
            // Never reveal hidden stats for candidates (you're hiring blind)
            ej["expectedSharpe"] = c.expectedSharpe();
            filteredPool.push_back(ej);
        }
        return filteredPool;
    }

    // Player view: filtered through organizational complexity
    nlohmann::json toPlayerJson() const {
        // Filter hiring pool: hide hidden stats for unrevealed candidates
        nlohmann::json filteredPool = hiringPoolPlayerJson();

        // Build bank JSON with division commentary
        auto bankJson = bankToPlayerJson(bank_);
        double vis = bank_.visibilityPct();
        for (size_t i = 0; i < bank_.divisions.size() && i < bankJson["divisions"].size(); ++i) {
            bankJson["divisions"][i]["commentary"] =
                simulation::NarrativeEngine::divisionReport(bank_.divisions[i], vis);
        }

        nlohmann::json result = {
            {"phase", phase_},
            {"state", state_},
            {"bank", bankJson},
            {"actionPoints", actionPoints_},
            {"progression", progression_},
            {"pendingDecisions", pendingDecisions_},
            {"messages", quarterMessages_},
            {"events", quarterEvents_},
            {"activeCrises", crisisEngine_.activeCrises()},
            {"simulationDigest", simulationDigest_},
            {"resolvedConsequences", resolvedConsequences_},
            {"pendingConsequenceCount", consequenceTracker_.pendingCount()},
            {"hiringPool", filteredPool},
            {"poachOffers", poachOffers_},                 // P6
            {"reputationTag", ReputationLens::tagFor(pathEngine_.state())}, // P6
            {"availableDeals", dealPortfolio_.availableDeals()},
            {"activeDeals", dealPortfolio_.activeDeals()},
            // STAR_02 P7: loan-outcome feedback loop — the player-facing feed of
            // resolved deals + the cumulative loan-book scoreboard ("loany" heart).
            {"dealOutcomes", dealPortfolio_.dealOutcomes()},
            {"loanBookStats", dealPortfolio_.loanBookStats()},
            // STAR_02 P7: the CEO's personal trading account (off-balance-sheet).
            {"tradeBook", personalBook_.toPlayerJson(marketPriceMap())},
            {"era", eraEngine_.currentEra()},
            {"regulatory", regulatoryEngine_.state()},
            {"competitors", competitorEngine_.toPlayerJson(state_.currentYear)},
            {"leaderboard", competitorEngine_.leaderboard(bank_)},
            {"aiCeo", aiCeoEngine_.state()},
            {"climate", climateEngine_.state()},
            {"political", politicalEngine_.state()},
            {"path", pathEngine_.state()},
            {"annualReport", lastAnnualReport_}
        };
        if (hasCeo_) {
            result["ceo"] = ceoProfile_;
            result["specialAbilityCooldown"] = specialAbilityCooldown_;
        }

        // Check for game end
        auto endState = checkGameEnd();
        if (endState.reason != GameEndReason::None) {
            result["gameEnd"] = endState;
        }

        return result;
    }

    // v3: optional non-owning custom-economy hook. Null = baseline. Setter
    // is public below so the runner can swap in alternative IEconomyModels.
    void setCustomEconomy(CustomEconomyHook* hook) { customEcon_ = hook; }
    bool hasCustomEconomy() const { return customEcon_ != nullptr; }

private:
    math::RandomEngine rng_;
    SimulationClock clock_;
    simulation::EconomicEngine econ_;
    CustomEconomyHook* customEcon_ = nullptr;  // v3 injection seam
    simulation::MarketSimulator marketSim_;
    simulation::TraderAIEngine traderAI_;
    simulation::EventPool eventPool_;
    simulation::DecisionEngine decisionEngine_;
    simulation::CharacterEngine characterEngine_;
    simulation::CrisisEngine crisisEngine_;
    ConsequenceTracker consequenceTracker_;
    simulation::CharacterGenerator charGen_;
    simulation::DealPortfolio dealPortfolio_;
    simulation::PersonalBook personalBook_;   // STAR_02 P7: CEO personal trading account
    std::vector<SimulationDigestEvent> simulationDigest_;
    std::vector<std::string> nextQuarterPreview_;

    // Streak tracking
    struct Streaks {
        int profit = 0;      // Consecutive profitable quarters
        int highScore = 0;   // Consecutive quarters with score > 60
        int crisisFree = 0;  // Consecutive quarters without crisis
        int growth = 0;      // Consecutive quarters capital grew
        std::string brokenMessage; // Set when a streak breaks
    } streaks_;

    SimulationConfig config_;
    SimulationState state_;
    MacroHistory macroHistory_;  // P3.2: per-quarter macro+market-close ring
    Bank bank_;
    std::vector<simulation::EmployeeCandidate> hiringPool_;
    // STAR_02 P6: per-candidate expiry quarter (era-flavored turnover); when the
    // global quarter passes this, the candidate leaves the pool. Keyed by id.
    std::map<std::string, int> candidateExpiry_;
    // STAR_02 P6: outstanding poach offers + a monotonic counter for ids.
    std::vector<simulation::PoachOffer> poachOffers_;
    int poachCounter_ = 0;
    int quartersUntilNextPoach_ = 0;  // countdown to the next offer (era-scaled)
    // STAR_02 P6 ReputationLens: last tag, to fire reputation_tag_changed only
    // on a real transition (telemetry-only — the tag itself is in the player view).
    std::string lastReputationTag_;
    ActionPointBudget actionPoints_;
    PlayerProgression progression_;
    TurnPhase phase_ = TurnPhase::QuarterStart;

    // CEO system
    CEOProfile ceoProfile_;
    bool hasCeo_ = false;
    int specialAbilityCooldown_ = 0;
    bool specialAbilityActive_ = false;
    bool revenueBoostActive_ = false;
    bool regulatoryShieldActive_ = false;
    bool crisisShieldActive_ = false;

    // Real-time simulation state
    bool realTimeSimulation_ = false;
    int simDayCounter_ = 0;
    std::vector<Market> simPrevMarkets_;
    double simPrevStress_ = 0;

    // Restriction flags (for --restrict diagnostic mode)
    bool skipDeleveraging_ = false;
    bool skipCrises_ = false;

    // Era, regulatory, path, competitor & endgame systems
    EraEngine eraEngine_;
    RegulatoryEngine regulatoryEngine_;
    PathEngine pathEngine_;
    simulation::CompetitorEngine competitorEngine_;
    simulation::AICEOEngine aiCeoEngine_;
    simulation::ClimateEngine climateEngine_;
    simulation::PoliticalEngine politicalEngine_;

    // Annual report tracking
    AnnualReport lastAnnualReport_;
    game::AnnualReportBuilder::AnnualAccumulator annualAccum_;

    // Historical events
    simulation::HistoricalEventLoader historicalLoader_;
    bool historicalEventsLoaded_ = false;

    // STAR_02 P5 §5.1: previous-quarter macro snapshot, used to form the
    // change terms (Δfed, Δspread, Δvix) in the archetype×macro P&L vector.
    // Captured at the end of onQuarterEnd; seeded from the initial econ.
    EconomicIndicators prevQuarterEcon_;
    bool prevQuarterEconValid_ = false;

    // STAR_02 P5 §5.3 (P3.4 Stage B): active event-driven drift nudges, keyed
    // by DivisionType json name, decremented each quarter they apply. Only
    // populated when config_.eventMarketCoupling is true.
    struct EventDrift { double perQuarter = 0.0; int quartersLeft = 0; };
    std::map<std::string, EventDrift> eventDivisionDrift_;

    // Balance tracking
    double startingCapital_ = 1e6;   // rescaled from 10e9
    double peakCapital_ = 1e6;

    // Endgame tracking
    int consecutivePoorQuarters_ = 0;
    int consecutiveMediocreQuarters_ = 0;  // Phase 4 A1: score < 35
    int consecutiveHighHiddenRiskQuarters_ = 0;  // Phase 4 A1: hidden risk > 50% capital
    bool fraudTriggered_ = false;  // Phase 4 A1: one-shot fraud scandal flag
    int consecutiveProfitableQuarters_ = 0;  // for buyback eligibility
    int recentDividendQuarters_ = 0;          // counts down 4 quarters after a dividend
    double totalScore_ = 0;
    int quartersCompleted_ = 0;
    double prevSystemLoans_ = 0;
    simulation::DurationRiskState durationRisk_; // Sprint 2

    std::vector<simulation::Trader> traders_;
    std::vector<simulation::GameEvent> quarterEvents_;
    std::vector<simulation::Decision> pendingDecisions_;
    std::vector<simulation::CharacterMessage> quarterMessages_;
    std::vector<Consequence> resolvedConsequences_;
    QuarterlyReport lastReport_;

    void assignDivisionHeadHonesty(Division& div) {
        // Higher reputation → more honest heads
        double honestyBias = bank_.reputation / 100.0; // 0-1
        double roll = rng_.uniform();
        // Base: 40% honest, 40% mixed, 20% self-serving
        // With max rep: 55% honest, 30% mixed, 15% self-serving
        double honestThresh = 0.40 + 0.15 * honestyBias;
        double mixedThresh = honestThresh + 0.40 - 0.10 * honestyBias;
        if (roll < honestThresh) {
            div.headHonesty = HeadHonesty::Honest;
        } else if (roll < mixedThresh) {
            div.headHonesty = HeadHonesty::Mixed;
        } else {
            div.headHonesty = HeadHonesty::SelfServing;
        }
        div.headAmbition = 0.3 + rng_.uniform() * 0.6;    // 0.3-0.9
        div.headCompetence = 0.4 + rng_.uniform() * 0.5;  // 0.4-0.9
    }

    // Quarterly phase methods: defined out-of-line in QuarterlyPhases.h
    // (Phase 3.1 split — kept QuarterlyTurnManager.h navigable).
    void onQuarterStart();
    void runSimulation();
    void onQuarterEnd();

    // P5 §5.3: register a per-division drift nudge from a drawn historical
    // event's affects/risk (eventMarketCoupling). Defined in QuarterlyPhases.h.
    void registerEventDrift(const std::string& eventId);


    void generateAnnualReport() {
        game::AnnualReportBuilder::generate(
            lastAnnualReport_, state_.currentYear, bank_, progression_,
            regulatoryEngine_.state(), eraEngine_.currentEra().name, annualAccum_);
        annualAccum_.reset(bank_.capital);
    }
};

} // namespace stvg

// Phase 3.1 split: out-of-line member function definitions.
// Must be included AFTER the class is fully declared above.
#include "EndgameResolver.h"
#include "QuarterlyPhases.h"
