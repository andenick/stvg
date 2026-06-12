#pragma once

#include <cstdio>
#include <cmath>
#include <string>

// ════════════════════════════════════════════════════════════════════
// QuarterlyPhases — out-of-line definitions for QuarterlyTurnManager's
// three large quarterly-loop phase methods. Extracted from
// QuarterlyTurnManager.h in Phase 3.1 to keep that header navigable.
// Included from the BOTTOM of QuarterlyTurnManager.h, after the class
// definition is complete.
//
// Contains:
//   - onQuarterStart() — events, decisions, briefings, narrative opening
//   - runSimulation() — daily ticks (econ, markets, traders, clock)
//   - onQuarterEnd()   — consequences, scoring, crises, doom meters,
//                        endgame metrics, annual rollup
//
// All three are non-const member functions of QuarterlyTurnManager.
// generateAnnualReport() remains inline in the main header (small).
// ════════════════════════════════════════════════════════════════════

namespace stvg {

// 0.2: adaptive money formatter for server-built headline strings. Mirrors the
// frontend's fmtMoney() (lib/util/money.ts) so headlines read honestly at the
// $1M-scale world — a $850K quarter is "$850K", not the old "$0M" (which came
// from (netIncome / 1e6) truncating sub-$1M profits to zero). Negative-aware.
inline std::string fmtMoneyHeadline(double v) {
    double abs = std::abs(v);
    const char* sign = v < 0 ? "-" : "";
    char buf[48];
    if (abs >= 1e12)      std::snprintf(buf, sizeof(buf), "%s$%.2fT", sign, abs / 1e12);
    else if (abs >= 1e9)  std::snprintf(buf, sizeof(buf), "%s$%.2fB", sign, abs / 1e9);
    else if (abs >= 1e6)  std::snprintf(buf, sizeof(buf), "%s$%.2fM", sign, abs / 1e6);
    else if (abs >= 1e3)  std::snprintf(buf, sizeof(buf), "%s$%.1fK", sign, abs / 1e3);
    else                  std::snprintf(buf, sizeof(buf), "%s$%.0f", sign, abs);
    // Trim a trailing ".00"/".0" so "$12.00M" reads "$12M".
    std::string s(buf);
    auto dot = s.find('.');
    if (dot != std::string::npos) {
        std::size_t lastUnit = s.find_first_of("TBMK", dot);
        std::size_t end = (lastUnit == std::string::npos) ? s.size() : lastUnit;
        std::string frac = s.substr(dot, end - dot);   // e.g. ".00" or ".50"
        // strip trailing zeros then a trailing dot
        std::size_t cut = frac.find_last_not_of('0');
        if (cut != std::string::npos && frac[cut] == '.') cut--;   // ".0" -> ""
        std::string trimmed = frac.substr(0, cut + 1);
        if (trimmed == ".") trimmed.clear();
        s = s.substr(0, dot) + trimmed + s.substr(end);
    }
    return s;
}

inline void QuarterlyTurnManager::onQuarterStart() {
    spdlog::info("=== Quarter Start: {} Q{} [{}] ===",
        state_.currentYear, state_.currentQuarter, eraEngine_.currentEra().name);

    // Check for era transition
    if (eraEngine_.shouldTransition(state_.currentYear)) {
        eraEngine_.transition(state_.currentYear);
        econ_.setParams(eraEngine_.currentEra().economyParams);
        marketSim_.setGARCHConfig(eraEngine_.currentEra().garchParams);
        politicalEngine_.setEraModifier(eraEngine_.currentEra().id);
    }

    // Add era-gated markets as history unfolds
    if (state_.currentYear >= 1971 && !marketSim_.hasMarket("EURUSD")) {
        marketSim_.addMarketDynamic("EURUSD", "EUR/USD", MarketType::Currency,
            "EUR", "USD", 1.08, 0.00, 0.08, 3);
    }
    if (state_.currentYear >= 2009 && !marketSim_.hasMarket("BTC")) {
        marketSim_.addMarketDynamic("BTC", "Bitcoin", MarketType::Crypto,
            "BTC", "USD", 500.0, 0.15, 0.65, 4);
    }

    // Update regulations for current year (handles enact/repeal dates)
    regulatoryEngine_.updateForYear(state_.currentYear);

    // Reset action points for new quarter
    actionPoints_.reset(bank_.totalAssets);

    // P3.4 Stage A: derive a sign-bias from the realized macro state so the
    // drawn narrative matches the engine direction (crisis events when the
    // economy turns down, opportunity events when it rises). Weight-only —
    // does NOT change the rng_ draw count/order (lockstep-safe).
    simulation::EventBias eventBias =
        simulation::EventBias::fromEconomics(state_.economics);

    // Draw events from weighted pool
    quarterEvents_ = eventPool_.drawEvents(bank_, state_, rng_, 4, eventBias);

    // Blend in historical events (1-2 per quarter, era-filtered)
    if (historicalEventsLoaded_) {
        int histCount = 1 + (rng_.bernoulli(0.3) ? 1 : 0);
        auto histEvents = historicalLoader_.drawHistoricalEvents(
            state_.currentYear, histCount, rng_, eventBias);
        for (auto& he : histEvents) {
            he.isHistorical = true;
            // P5 §5.3 / P3.4 Stage B: register an event→market drift nudge for
            // the divisions this event benefits/suffers, over its duration.
            // Gated behind eventMarketCoupling (default OFF for deterministic
            // tests; live server sets it ON). Weight-only registration here —
            // no extra RNG draw, lockstep-safe.
            if (config_.eventMarketCoupling)
                registerEventDrift(he.id);
            quarterEvents_.push_back(std::move(he));
        }
    }

    // Generate decisions from events
    pendingDecisions_ = decisionEngine_.generateFromEvents(quarterEvents_, bank_, state_);

    // Apply CEO analyticalAbility as a post-process boost to all option success
    // probabilities. ±10% swing — quants reliably pick well, intuitives gamble.
    if (hasCeo_) {
        double boost = (ceoProfile_.personality.analyticalAbility - 0.5) * 0.20;
        for (auto& d : pendingDecisions_) {
            for (auto& opt : d.options) {
                opt.successProbability = std::clamp(opt.successProbability + boost, 0.05, 0.98);
            }
        }
    }

    // Add endgame decisions: climate (Q1 annually from 2020), AI (Q2 after emergence), acquisitions
    if (state_.currentYear >= 2020 && state_.currentQuarter == 1) {
        pendingDecisions_.push_back(
            decisionEngine_.generateClimateDecision(bank_, climateEngine_.state()));
    }
    if (aiCeoEngine_.hasEmerged() && state_.currentQuarter == 2) {
        pendingDecisions_.push_back(
            decisionEngine_.generateAICounterDecision(bank_, aiCeoEngine_.state()));
    }
    // Offer acquisitions when competitors fail
    for (const auto* failed : competitorEngine_.recentFailures(state_.currentYear)) {
        simulation::AcquisitionOffer offer;
        offer.targetId = failed->id;
        offer.targetName = failed->name;
        offer.price = failed->totalAssets * 0.15;
        offer.assetsGained = failed->totalAssets * 0.7;
        offer.hiddenRisk = failed->totalAssets * 0.1 * failed->personality.riskTolerance;
        offer.archetype = failed->archetype;
        pendingDecisions_.push_back(decisionEngine_.generateAcquisitionDecision(offer));
    }

    // Deleveraging decisions — offer ways to reduce leverage when the
    // bank is approaching the regulatory_seizure cliff or when conditions
    // make a buyback/divestiture sensible. Without these, revenue-maximizer
    // bots have no path off the leverage death spiral.
    if (!skipDeleveraging_) {
        int globalQ = state_.currentQuarter + (state_.currentYear - config_.startYear) * 4;
        if (simulation::DeleveragingDecisions::shouldOfferDividend(bank_, globalQ)) {
            pendingDecisions_.push_back(
                simulation::DeleveragingDecisions::generateDividendDecision(bank_, globalQ));
        }
        if (simulation::DeleveragingDecisions::shouldOfferBuyback(consecutiveProfitableQuarters_, bank_)) {
            pendingDecisions_.push_back(
                simulation::DeleveragingDecisions::generateBuybackDecision(bank_, globalQ));
        }
        if (simulation::DeleveragingDecisions::shouldOfferDivestiture(bank_)) {
            pendingDecisions_.push_back(
                simulation::DeleveragingDecisions::generateDivestitureDecision(bank_, globalQ));
        }
    }

    // Sprint 1: Annual banking policy decisions (Q1 only)
    if (state_.currentQuarter == 1) {
        // Loan spread decision
        simulation::Decision spreadDec;
        spreadDec.id = "dec_loanspread_y" + std::to_string(state_.currentYear);
        spreadDec.title = "Set Lending Rate Spread";
        spreadDec.description = "Your current loan spread is " +
            std::to_string((int)(bank_.loanSpread * 10000)) + "bps over market. "
            "Wider = more income per loan, slower growth. Narrower = volume play, thinner margin.";
        spreadDec.type = simulation::DecisionType::Strategic;
        spreadDec.actionPointCost = 0; // Policy decisions are free — strategic, not operational
        for (auto bps : {100, 150, 200, 250, 300}) {
            simulation::DecisionOption opt;
            opt.id = spreadDec.id + "_" + std::to_string(bps) + "bps";
            opt.title = std::to_string(bps) + " bps";
            opt.description = "Set loan spread to " + std::to_string(bps) + " basis points.";
            opt.financialImpact.expectedRevenue = bank_.loans * (bps / 10000.0 - bank_.loanSpread) / 4.0;
            opt.riskImpact = {(bps < 150) ? 0.05 : -0.02, 0,
                (bps < 150) ? "Tighter margins increase volume risk" : "Wider margins, safer"};
            opt.successProbability = 1.0;
            spreadDec.options.push_back(opt);
        }
        pendingDecisions_.push_back(spreadDec);

        // Credit standards decision
        simulation::Decision stdDec;
        stdDec.id = "dec_creditstd_y" + std::to_string(state_.currentYear);
        stdDec.title = "Set Credit Underwriting Standards";
        stdDec.description = "Current standards: " +
            std::string(bank_.creditStandards > 0.7 ? "TIGHT" :
                       bank_.creditStandards > 0.3 ? "MODERATE" : "LOOSE") +
            ". Loose = more loans now, higher defaults in 1-2 years. Tight = less volume, cleaner book.";
        stdDec.type = simulation::DecisionType::Strategic;
        stdDec.actionPointCost = 0;
        struct StdOpt { const char* label; double val; const char* desc; };
        for (auto& s : std::vector<StdOpt>{
            {"Loose (Aggressive Growth)", 0.2, "Approve most applications. Maximum loan volume."},
            {"Moderate", 0.5, "Standard underwriting. Balanced growth and quality."},
            {"Tight (Conservative)", 0.8, "Strict criteria. Fewer loans, lower defaults."}
        }) {
            simulation::DecisionOption opt;
            opt.id = stdDec.id + "_" + std::to_string((int)(s.val * 100));
            opt.title = s.label;
            opt.description = s.desc;
            opt.riskImpact = {(s.val < 0.4) ? 0.1 : -0.05, 0,
                (s.val < 0.4) ? "Higher future default risk" : "Lower future defaults"};
            opt.successProbability = 1.0;
            opt.longTermConsequences = {
                (s.val < 0.4) ? "Loan losses may spike in 4-8 quarters" :
                (s.val > 0.6) ? "Slower growth but cleaner balance sheet" :
                                "Balanced approach"
            };
            stdDec.options.push_back(opt);
        }
        pendingDecisions_.push_back(stdDec);

        // Duration target decision
        simulation::Decision durDec;
        durDec.id = "dec_duration_y" + std::to_string(state_.currentYear);
        durDec.title = "Set Duration Target";
        durDec.description = "Your securities portfolio duration affects yield and interest rate risk. "
            "Longer duration = higher yield but bigger mark-to-market swings when rates move.";
        durDec.type = simulation::DecisionType::Strategic;
        durDec.actionPointCost = 0;
        struct DurOpt { const char* label; const char* key; const char* desc; };
        for (auto& d : std::vector<DurOpt>{
            {"Short Duration (Safe)", "short", "60% short-term, 30% medium, 10% long. Low yield, minimal rate risk."},
            {"Balanced", "balanced", "30% short, 50% medium, 20% long. Standard yield-risk tradeoff."},
            {"Long Duration (Yield-Seeking)", "long", "10% short, 40% medium, 50% long. Maximum yield, SVB-style rate risk."}
        }) {
            simulation::DecisionOption opt;
            opt.id = durDec.id + "_" + d.key;
            opt.title = d.label;
            opt.description = d.desc;
            opt.riskImpact = {
                (std::string(d.key) == "long") ? 0.1 : (std::string(d.key) == "short") ? -0.05 : 0.0,
                0, (std::string(d.key) == "long") ? "Higher duration risk exposure" : "Conservative positioning"
            };
            opt.successProbability = 1.0;
            durDec.options.push_back(opt);
        }
        pendingDecisions_.push_back(durDec);
    }

    // Add character recommendations to decisions
    characterEngine_.addRecommendations(pendingDecisions_);

    // Generate character briefings
    quarterMessages_ = characterEngine_.generateBriefings(bank_, state_);

    // STAR_02 P6 ReputationLens: tilt the deal flow's risk by what you've become
    // (gunslinger shop → riskier flow; fortress bank → safer). Weight-only.
    dealPortfolio_.setDealRiskTilt(ReputationLens::compute(pathEngine_.state()).dealRiskTilt);
    // Generate deal opportunities for each active business line
    dealPortfolio_.generateOpportunities(bank_.divisions, state_.currentYear);

    // ── STAR_02 P6: candidate pool with era-flavored turnover + reputation tilt ──
    // Candidates persist across quarters and EXPIRE after 2-4 quarters (the
    // expiry was stamped when each arrived). At quarter start we (1) drop the
    // expired, then (2) top the pool back up toward its reputation-scaled target
    // with the ReputationLens family tilt. Intra-quarter arrivals (addTrickle-
    // Candidate) fill the gap between quarters. The first quarter seeds the pool.
    int globalQ0 = state_.currentQuarter + (state_.currentYear - config_.startYear) * 4;
    // Expire stale candidates.
    {
        auto newEnd = std::remove_if(hiringPool_.begin(), hiringPool_.end(),
            [&](const simulation::EmployeeCandidate& c) {
                auto it = candidateExpiry_.find(c.id);
                return it != candidateExpiry_.end() && globalQ0 >= it->second;
            });
        for (auto it = newEnd; it != hiringPool_.end(); ++it) candidateExpiry_.erase(it->id);
        hiringPool_.erase(newEnd, hiringPool_.end());
    }
    // Top up toward the reputation-scaled target with the reputation tilt.
    {
        int target = std::clamp(3 + (int)(bank_.reputation / 30.0), 3, 6);
        auto tilt = ReputationLens::compute(pathEngine_.state()).familyMult;
        while ((int)hiringPool_.size() < target) {
            auto fresh = charGen_.generateCandidates(1, state_.currentYear, bank_.reputation, &tilt);
            if (fresh.empty()) break;
            // Era-flavored turnover: each candidate stays 2-4 quarters.
            std::string id = fresh.front().id;
            candidateExpiry_[id] = globalQ0 + 2 + (rng_.bernoulli(0.5) ? 1 : 0) + (rng_.bernoulli(0.5) ? 1 : 0);
            hiringPool_.push_back(std::move(fresh.front()));
        }
    }

    // ── STAR_02 P6: poach-offer generation (every 2-6 quarters, era-scaled) ──
    // Era heat rises with deregulation/leverage decades so the ask multiple
    // climbs (1.5×→3×). A fresh offer streams in like a deal; it expires on its
    // own clock. RNG draws happen here at the quarter boundary (lockstep-safe).
    {
        // Drop expired offers.
        poachOffers_.erase(std::remove_if(poachOffers_.begin(), poachOffers_.end(),
            [&](const simulation::PoachOffer& o) { return globalQ0 >= o.expiresQuarter || !o.active; }),
            poachOffers_.end());
        if (quartersUntilNextPoach_ > 0) quartersUntilNextPoach_--;
        // Era heat: 0 in the calm 40s-60s, ~1 in the 80s/2000s leverage decades.
        double eraHeat = 0.2;
        int yr = state_.currentYear;
        if (yr >= 1980 && yr < 1991) eraHeat = 0.9;      // cowboy capitalism
        else if (yr >= 1999 && yr < 2008) eraHeat = 1.0; // structured-credit boom
        else if (yr >= 1991 && yr < 1999) eraHeat = 0.6;
        else if (yr >= 2008 && yr < 2015) eraHeat = 0.3; // post-GFC retrenchment
        else if (yr >= 2015) eraHeat = 0.7;
        if (quartersUntilNextPoach_ <= 0 && (int)poachOffers_.size() < 3) {
            auto offer = competitorEngine_.generatePoachOffer(
                state_.currentYear, globalQ0, eraHeat, poachCounter_);
            if (offer) {
                // Reputation lens tilts the ask: a gunslinger shop overpays for
                // wildcat teams; a fortress bank gets quoted gentler.
                double priceTilt = ReputationLens::compute(pathEngine_.state()).poachPriceTilt;
                offer->askPrice *= priceTilt;
                poachOffers_.push_back(std::move(*offer));
            }
            // Reset the countdown: 2-6 quarters, shorter when the era is hot.
            quartersUntilNextPoach_ = 2 + (int)(rng_.uniform() * (4.0 * (1.0 - 0.5 * eraHeat)));
        }
    }

    // Generate narrative opening
    simulation::NarrativeContext nctx{
        bank_, state_, lastReport_.score, progression_,
        crisisEngine_.activeCrises(), resolvedConsequences_,
        quarterEvents_, hasCeo_, hasCeo_ ? ceoProfile_.name : "",
        state_.currentQuarter
    };
    lastReport_.openingNarrative = simulation::NarrativeEngine::quarterOpening(nctx);

    // Add narrator message as first briefing
    simulation::CharacterMessage narratorMsg;
    narratorMsg.characterId = "narrator";
    narratorMsg.characterName = "Quarterly Brief";
    narratorMsg.role = "Narrative";
    narratorMsg.content = lastReport_.openingNarrative;
    narratorMsg.tone = (state_.regime == MarketRegime::Crisis || state_.regime == MarketRegime::Stressed)
        ? "urgent" : "neutral";
    narratorMsg.sentiment = 0;
    quarterMessages_.insert(quarterMessages_.begin(), narratorMsg);

    spdlog::info("{} events, {} decisions, {} messages, {} candidates generated",
        quarterEvents_.size(), pendingDecisions_.size(),
        quarterMessages_.size(), hiringPool_.size());
}

inline void QuarterlyTurnManager::runSimulation() {
    spdlog::info("Running {} day simulation...", config_.quarterDurationDays);

    simulationDigest_.clear();
    auto prevMarkets = marketSim_.snapshot();
    double prevStress = econ_.stressScore();

    for (int day = 0; day < config_.quarterDurationDays; ++day) {
        double dt = 1.0 / 252.0;

        // Economic + market simulation
        // v3: optional custom-economy injection
        if (customEcon_) customEcon_->dailyTick(dt, econ_);
        else             econ_.tick(dt);
        marketSim_.tick(dt);

        // Trader AI
        traderAI_.tickDay(traders_, marketSim_.snapshot(), econ_.stressScore());

        // Intra-quarter deal trickle (kept identical to tickSimulationDay so the
        // batch and day-by-day paths consume RNG in lockstep — see DayByDayMatchesBatch).
        if (rng_.bernoulli(0.035)) {
            dealPortfolio_.addTrickleDeal(bank_.divisions, state_.currentYear);
        }

        // STAR_02 P6: intra-quarter HIRING trickle (1-3 candidates arrive mid-
        // quarter). Drawn right after the deal trickle, at the SAME rate/site in
        // both the batch and day-by-day paths — lockstep-safe by construction.
        if (rng_.bernoulli(0.012)) {
            addTrickleCandidate();
        }

        // Advance clock
        clock_.advanceDay();

        // Collect notable market moves (>2% daily change)
        auto curMarkets = marketSim_.snapshot();
        for (size_t i = 0; i < curMarkets.size() && i < prevMarkets.size(); ++i) {
            if (prevMarkets[i].currentPrice > 0) {
                double pctChange = (curMarkets[i].currentPrice - prevMarkets[i].currentPrice)
                                 / prevMarkets[i].currentPrice;
                if (std::abs(pctChange) > 0.02) {
                    std::string dir = pctChange > 0 ? "surges" : "drops";
                    char buf[128];
                    std::snprintf(buf, sizeof(buf), "%s %s %.1f%%",
                        curMarkets[i].symbol.c_str(), dir.c_str(), std::abs(pctChange) * 100.0);
                    simulationDigest_.push_back({day + 1, std::string(buf), "market"});
                }
            }
        }

        // Detect stress regime changes
        double curStress = econ_.stressScore();
        if (curStress > 60 && prevStress <= 60) {
            simulationDigest_.push_back({day + 1, "Market stress enters danger zone", "economic"});
        } else if (curStress < 30 && prevStress >= 30) {
            simulationDigest_.push_back({day + 1, "Market stress subsides to calm levels", "economic"});
        }
        prevStress = curStress;
        prevMarkets = curMarkets;
    }

    // Limit to 5 most notable events
    if (simulationDigest_.size() > 5) {
        simulationDigest_.resize(5);
    }

    // Update state
    state_.currentYear = clock_.year();
    state_.currentQuarter = clock_.quarter();
    state_.currentDay = clock_.dayInQuarter();
    state_.totalDaysElapsed = clock_.totalDays();
    state_.economics = econ_.state();
    state_.marketState = econ_.toMarketState();
    state_.markets = marketSim_.snapshot();
    state_.regime = marketSim_.currentRegime(econ_.stressScore());
}

inline void QuarterlyTurnManager::onQuarterEnd() {
    spdlog::info("=== Quarter End: {} Q{} ===", state_.currentYear, state_.currentQuarter);

    // Resolve pending consequences from prior decisions
    int currentQ = state_.currentQuarter + (state_.currentYear - config_.startYear) * 4;
    resolvedConsequences_ = consequenceTracker_.resolveForQuarter(currentQ);
    for (const auto& c : resolvedConsequences_) {
        bank_.capital += c.financialImpact;
        bank_.reputation = std::clamp(bank_.reputation + c.reputationImpact, 0.0, 100.0);
        spdlog::info("Consequence resolved: {} (${:.0f}, rep {:.1f})",
            c.title, c.financialImpact, c.reputationImpact);
    }

    // End quarter for traders
    traderAI_.endQuarter(traders_);

    // Compute revenue and costs for all divisions dynamically
    // Infrastructure scales on capital (equity), not leveraged assets.
    // Real bank non-interest expense ~2-3% of assets = ~20-30% of equity.
    // 0.02/4 = 0.5% of capital per quarter = 2% annually.
    double infraCostTotal = bank_.capital * 0.02 / 4.0;
    // Regulatory compliance cost (era-driven, not hardcoded)
    double complianceCost = regulatoryEngine_.annualComplianceCost(bank_.totalRevenue) / 4.0
        + std::sqrt(bank_.totalAssets / 1e5) * 0.5e2; // Base operational overhead (rescaled /10,000)
    double infraPerDiv = (bank_.divisions.empty()) ? 0 : infraCostTotal / bank_.divisions.size();
    double compPerDiv = (bank_.divisions.empty()) ? 0 : complianceCost / bank_.divisions.size();

    // Record credit standards for the lagged PD model
    bank_.recordCreditStandards();

    // Sprint 4: Minsky procyclical lending — division budgets grow/shrink
    for (auto& div : bank_.divisions) {
        if (div.assetClass() == Division::AssetClass::Loans) {
            double growthRate = 0.005  // 0.5% base quarterly growth (~2% annual)
                * (1.3 - bank_.creditStandards * 0.6)   // loose = 30% faster
                * (1.0 + 0.3 * state_.economics.gdpGrowth) // mild procyclical
                * (1.0 - 0.2 * econ_.stressScore() / 100.0); // pull back in crisis
            growthRate = std::clamp(growthRate, -0.02, 0.03); // cap at ±3%/quarter
            div.budget *= (1.0 + growthRate);
            div.budget = std::max(div.budget, 1e2);   // floor rescaled /10,000 ($1M-scale)
        }
    }

    // Compute bank-wide lending metrics for NIM model
    double fedRate = state_.economics.fedFundsRate;
    double mktSpread = state_.economics.creditSpread;
    double bankLoanYield = bank_.loanYield(fedRate, mktSpread);
    double bankFundingRate = bank_.blendedFundingRate();
    double pd = bank_.probabilityOfDefault(state_.economics.gdpGrowth, mktSpread);
    int numLendingDivs = 0;
    for (const auto& d : bank_.divisions)
        if (d.assetClass() == Division::AssetClass::Loans) numLendingDivs++;

    bank_.quarterlyProvisions = 0;

    // ── STAR_02 P5 §5.1: archetype × macro → P&L distribution ──────────
    // Global tuning constants (a single β/σ scale knob, per the plan's
    // re-balance protocol — adjust these, not the JSON, to retune difficulty).
    constexpr double ARCHETYPE_BETA_SCALE  = 1.0;
    // P5 rebalance: σ scale kept at the calibrated 1.0. Raising it (tested 1.5)
    // did not move the autoplay aggressive-vs-conservative final-capital ratio
    // because bot outcomes are dominated by economic-seed shocks + leverage-
    // driven death (which truncates the aggressive distribution), not by the
    // archetype σ term. The σ RATIO between families (gunslinger 0.35 / lifer
    // 0.05 = 7×) is the real lever and is preserved at scale 1.0; the
    // aggressive-vs-conservative dispersion is demonstrated directly at the
    // division-revenue level (see test_archetype_rebalance) where it is ~7-49×,
    // untruncated by death. Median survival is preserved at 1.0.
    constexpr double ARCHETYPE_SIGMA_SCALE = 1.0;
    constexpr double kBaselineSigma = 0.04;

    // Macro state vector x_t = (gdpGrowth-ḡ, Δfed, -Δspread, sp500 qtr return,
    // -Δvix/20). The Δ terms compare to the previous quarter's snapshot; the
    // first quarter (no prior) uses zero changes. ḡ = 0.025 trend growth.
    const auto& ec = state_.economics;
    EconomicIndicators prevEc = prevQuarterEconValid_ ? prevQuarterEcon_ : ec;
    std::array<double, 5> xt{
        ec.gdpGrowth - 0.025,                       // gdp deviation
        ec.fedFundsRate - prevEc.fedFundsRate,      // Δfed (q/q)
        -(ec.creditSpread - prevEc.creditSpread),   // -Δspread (tightening = +)
        ec.sp500Return,                             // equity quarterly return proxy
        -(ec.vix - prevEc.vix) / 20.0               // -Δvix/20
    };

    // ── STAR_02 P6: crowding / saturation (full version, plan §P6) ──────────
    // Three named, data-driven effects replace P5's step first-cut:
    //
    //   (A) σ crowding (bank-wide, CONTINUOUS):
    //         crowdingMult = 1 + CROWD_SIGMA_SLOPE · max(0, maxClassShare − CROWD_SIGMA_KNEE) / (1 − CROWD_SIGMA_KNEE)
    //       — correlated risk rises smoothly as one crowdingClass dominates the
    //         whole bank, instead of a discontinuous ×1.5 step at 50%.
    //   (B) expected-return saturation (PER-DIVISION): a division whose staff is
    //         dominated by one crowdingClass earns less per head —
    //         expected × (1 − SATUR_RETURN_SLOPE · max(0, classShareInDiv − SATUR_RETURN_KNEE)).
    //   (C) industry saturation: if the player AND rivals pile into the same
    //         division type, that type's baseRevenueRate erodes up to INDUSTRY_MAX_EROSION.
    //
    // All constants named here (the rebalance protocol tunes these, not the JSON).
    constexpr double CROWD_SIGMA_KNEE   = 0.50;  // σ crowding starts above 50% share
    constexpr double CROWD_SIGMA_SLOPE  = 0.50;  // up to +50% σ at 100% share
    constexpr double SATUR_RETURN_KNEE  = 0.60;  // return saturation above 60% in-div share
    constexpr double SATUR_RETURN_SLOPE = 0.30;  // up to −30% expected at 100% in-div share
    constexpr double INDUSTRY_MAX_EROSION = 0.20; // up to −20% baseRevenueRate when crowded

    // (A) Bank-wide max crowdingClass share → continuous σ multiplier.
    double crowdingMult = 1.0;
    {
        std::map<std::string, int> classCount;
        int totalStaff = 0;
        const auto& reg = ArchetypeRegistry::instance();
        if (reg.loaded()) {
            for (const auto& d : bank_.divisions)
                for (const auto& emp : d.staff) {
                    if (const auto* fam = reg.family(emp.archetype)) {
                        if (!fam->crowdingClass.empty()) classCount[fam->crowdingClass]++;
                        totalStaff++;
                    }
                }
            double maxShare = 0.0;
            for (const auto& [cls, n] : classCount)
                if (totalStaff > 0) maxShare = std::max(maxShare, (double)n / totalStaff);
            crowdingMult = 1.0 + CROWD_SIGMA_SLOPE
                * std::max(0.0, maxShare - CROWD_SIGMA_KNEE) / (1.0 - CROWD_SIGMA_KNEE);
        }
    }

    // (C) Industry saturation per division TYPE: compare the player's budget
    // share in a type against the rivals' aggregate tilt toward that line. The
    // engine has no per-type rival book, so we proxy "rivals piling in" by the
    // fraction of living rivals whose archetype maps to this division type
    // (gunslinger→trading, innovator→fintech/securitization, …) — when both you
    // and the rivals concentrate on the same line, its baseRevenueRate erodes.
    std::map<DivisionType, double> industrySaturation;
    {
        // Player's budget share per type.
        double totalBudget = 0.0;
        std::map<DivisionType, double> playerBudget;
        for (const auto& d : bank_.divisions) { playerBudget[d.type] += d.budget; totalBudget += d.budget; }
        // Rival concentration per type (fraction of living rivals on this line).
        std::map<DivisionType, int> rivalOnType;
        int liveRivals = 0;
        for (const auto& c : competitorEngine_.competitors()) {
            if (!c.alive || c.foundedYear > state_.currentYear) continue;
            liveRivals++;
            rivalOnType[competitorEngine_.divisionForArchetype(c.archetype, state_.currentYear)]++;
        }
        for (const auto& [type, budget] : playerBudget) {
            double playerShare = totalBudget > 0 ? budget / totalBudget : 0.0;
            double rivalShare = liveRivals > 0 ? (double)rivalOnType[type] / liveRivals : 0.0;
            // Both crowded into the same line → erosion. Geometric overlap so it
            // only bites when BOTH are concentrated; capped at INDUSTRY_MAX_EROSION.
            double overlap = std::min(playerShare, rivalShare);
            industrySaturation[type] = std::min(INDUSTRY_MAX_EROSION, INDUSTRY_MAX_EROSION * 2.0 * overlap);
        }
    }

    for (auto& div : bank_.divisions) {
        double staffMult = div.staffQualityMultiplier();
        double ceoRevMult = hasCeo_ ? ceoProfile_.revenueMultiplier : 1.0;
        if (revenueBoostActive_) ceoRevMult *= 2.0;

        // Aggregate this division's staff archetype mix once (cheap, no RNG).
        ArchetypeProfile prof = div.archetypeProfile();

        if (div.assetClass() == Division::AssetClass::Loans && numLendingDivs > 0) {
            // Sprint 1: NIM-based revenue for lending divisions.
            double divLoanShare = div.budget * Bank::kDefaultLeverage;
            double nim = (bankLoanYield - bankFundingRate) / 4.0; // quarterly NIM
            div.revenue = divLoanShare * std::max(nim, 0.0) * staffMult * ceoRevMult;

            // P5: archetype enters lending as a PD multiplier (conservative
            // families lower PD ~0.8×, aggressive raise it ~1.3×, weighted).
            // Relationship sideBenefits.depositStickiness lowers the effective
            // funding-cost sensitivity — modeled here as a small PD relief
            // (sticky deposits = a calmer book through shocks).
            double divPd = pd * prof.pdMult;
            auto stickIt = prof.sideBenefits.find("depositStickiness");
            if (stickIt != prof.sideBenefits.end())
                divPd *= (1.0 - 0.10 * std::clamp(stickIt->second, 0.0, 1.0));
            divPd = std::clamp(divPd, 0.0, 1.0);

            // Loan loss provisions (lagged PD × LGD × loan book)
            double provisions = divLoanShare * divPd * div.lossGivenDefault() / 4.0;
            bank_.quarterlyProvisions += provisions;
            bank_.loanLossReserve += provisions;
            div.archetypePdMult_ = prof.pdMult; // expose for telemetry/headlines
        } else if (div.type == DivisionType::TradingDesk) {
            // Trading desk: trader P&L (TraderAI unchanged). Archetype effect
            // here scales the desk's hidden-risk accumulation by its aggressive
            // weight (handled below after the loop body).
            div.revenue = simulation::TraderAIEngine::divisionVisiblePnL(traders_) * staffMult;
            div.actualRisk = simulation::TraderAIEngine::divisionHiddenExposure(traders_)
                           / std::max(bank_.capital, 1.0);
        } else {
            // Fee-based and securities divisions: macro-coupled archetype
            // distribution (the owner's core equation).
            //   expected  = base·macroBoost + autonomyBonus  (legacy mean)
            //   macroMult = 1 + Σ_k Σ_a w_a·β_a,k·x_t,k     (archetype tilt)
            //   realized  = expected · macroMult · (1 + σ_div·Z)
            // STAR_02 P6 (C): industry saturation erodes baseRevenueRate up to
            // −20% when you AND the rivals pile into this division type.
            double industryMult = 1.0;
            {
                auto sit = industrySaturation.find(div.type);
                if (sit != industrySaturation.end()) industryMult = 1.0 - sit->second;
            }
            double base = div.budget * div.baseRevenueRate(state_.currentYear) * industryMult;
            double macroBoost = 1.0 + state_.economics.gdpGrowth * 3.0
                              + state_.economics.fedFundsRate * 2.0;
            double autonomyBonus = div.autonomyLevel * base * 1.5;
            double expected = (base * macroBoost + autonomyBonus) * staffMult * ceoRevMult;

            // STAR_02 P6 (B): per-division expected-return saturation. When one
            // crowdingClass dominates THIS division's staff, per-head return
            // saturates: expected × (1 − SATUR_RETURN_SLOPE · max(0, share − knee)).
            {
                const auto& reg = ArchetypeRegistry::instance();
                if (reg.loaded() && !div.staff.empty()) {
                    std::map<std::string, int> divClass;
                    for (const auto& emp : div.staff)
                        if (const auto* fam = reg.family(emp.archetype))
                            if (!fam->crowdingClass.empty()) divClass[fam->crowdingClass]++;
                    double maxDivShare = 0.0;
                    for (const auto& [cls, n] : divClass)
                        maxDivShare = std::max(maxDivShare, (double)n / div.staff.size());
                    expected *= (1.0 - SATUR_RETURN_SLOPE
                        * std::max(0.0, maxDivShare - SATUR_RETURN_KNEE));
                }
            }

            // macroMult from the staff archetype betas dotted with x_t.
            double tilt = 0.0;
            for (size_t k = 0; k < xt.size(); ++k)
                tilt += ARCHETYPE_BETA_SCALE * prof.beta[k] * xt[k];
            double macroMult = std::clamp(1.0 + tilt, 0.4, 1.8);

            // σ_div = sqrt(Σ w_a² σ_a²) · crowdingMult · ARCHETYPE_SIGMA_SCALE,
            // gated by config_.archetypePnlVariance (0 ⇒ deterministic legacy).
            double sigmaDiv = (prof.empty ? kBaselineSigma : prof.sigma)
                            * crowdingMult * ARCHETYPE_SIGMA_SCALE
                            * config_.archetypePnlVariance;

            // Draw Z ONCE per division, here at the quarter boundary, in the
            // fixed division-iteration order — lockstep-safe by construction.
            double z = (sigmaDiv > 0.0) ? rng_.normalSample() : 0.0;
            double realized = expected * macroMult * (1.0 + sigmaDiv * z);
            div.revenue = realized;

            // Relationship sideBenefits.dealflowQuality biases this division's
            // deal-generation quality (wired cheaply: a small positive revenue
            // tilt; the full deal-generator bias is deferred to P6).
            auto dfIt = prof.sideBenefits.find("dealflowQuality");
            if (dfIt != prof.sideBenefits.end())
                div.revenue *= (1.0 + 0.05 * std::clamp(dfIt->second, 0.0, 1.0));
        }

        // P5 §5.3: apply any active event-driven drift nudge for this division
        // (eventMarketCoupling). Empty map when the flag is off → no-op, so
        // deterministic tests are unaffected. SIGN-AWARE: a suffers event (drift
        // < 0) must ALWAYS reduce P&L — making a gain smaller OR a loss bigger —
        // and a benefits event must always improve it. Applying the drift to the
        // signed magnitude (not as a plain `*(1+drift)`) keeps this correct even
        // when a trading desk's revenue is negative for the quarter.
        if (!eventDivisionDrift_.empty()) {
            auto dit = eventDivisionDrift_.find(divisionTypePascalName(div.type));
            if (dit != eventDivisionDrift_.end() && dit->second.quartersLeft > 0)
                div.revenue += std::abs(div.revenue) * dit->second.perQuarter;
        }

        // Costs: actual staff salaries + infrastructure + compliance + bonuses
        double salary = div.staff.empty()
            ? div.employees * 7.5 / 4.0   // rescaled /10,000
            : div.totalSalaryCost();
        double bonus = std::max(div.revenue * 0.20, 0.0);
        div.costs = salary + infraPerDiv + compPerDiv + bonus;

        // Add loan loss provisions to lending division costs (archetype-adjusted
        // PD so conservative books cost less, aggressive books cost more).
        if (div.assetClass() == Division::AssetClass::Loans && numLendingDivs > 0) {
            double divLoanShare = div.budget * Bank::kDefaultLeverage;
            double divPd = std::clamp(pd * div.archetypePdMult_, 0.0, 1.0);
            div.costs += divLoanShare * divPd * div.lossGivenDefault() / 4.0 / numLendingDivs;
        }

        // P5: trading-desk hidden-risk accumulation scales with the desk's
        // aggressive-archetype weight (gunslinger-heavy desks build risk faster).
        if (div.type == DivisionType::TradingDesk && prof.aggressiveWeight > 0.0)
            div.actualRisk *= (1.0 + 0.5 * prof.aggressiveWeight);

        div.employees = div.staffCount();
        div.actualRisk += div.baseRiskRate() * div.autonomyLevel * 0.1;
    }

    // Capital recovery: auto-reduce costs when capital drops below 50% of peak
    if (bank_.capital < peakCapital_ * 0.5) {
        for (auto& div : bank_.divisions) {
            div.costs *= 0.80; // Emergency 20% cost reduction
        }
        lastReport_.headlines.push_back("AUSTERITY: Emergency cost cuts across all divisions");
        spdlog::info("Capital recovery triggered: capital ${:.0f} < 50% of peak ${:.0f}",
            bank_.capital, peakCapital_);
    }
    peakCapital_ = std::max(peakCapital_, bank_.capital);

    // Hidden risk causes undetected losses (fraud, unreported trading losses)
    for (const auto& div : bank_.divisions) {
        if (div.hiddenRisk > 0.5) {
            // Severe neglect: undetected fraud and unreported losses compound
            double undetectedLoss = div.budget * div.hiddenRisk * 0.03;
            bank_.capital -= undetectedLoss;
        } else if (div.hiddenRisk > 0.25) {
            // Moderate neglect: small leakage from unmonitored positions
            double undetectedLoss = div.budget * div.hiddenRisk * 0.01;
            bank_.capital -= undetectedLoss;
        }
    }

    // Resolve mature deals (reuse currentQ from consequence resolution above)
    double ceoPersuasion = hasCeo_ ? ceoProfile_.personality.persuasion : 0.5;
    auto resolvedDeals = dealPortfolio_.resolveDeals(currentQ, ceoPersuasion);
    for (const auto& deal : resolvedDeals) {
        bank_.capital += deal.investedCapital + deal.realizedReturn;
        if (deal.status == simulation::DealStatus::Windfall) {
            lastReport_.headlines.push_back("WINDFALL: " + deal.opportunity.title
                + " returns $" + std::to_string((long long)((deal.investedCapital + deal.realizedReturn) / 1e6)) + "M!");
        } else if (deal.status == simulation::DealStatus::Failed) {
            lastReport_.headlines.push_back("DEAL LOSS: " + deal.opportunity.title
                + " lost $" + std::to_string((long long)(std::abs(deal.realizedReturn) / 1e6)) + "M");
        } else {
            lastReport_.headlines.push_back("Deal closed: " + deal.opportunity.title
                + " returned $" + std::to_string((long long)(deal.realizedReturn / 1e6)) + "M");
        }
    }

    // Employee aging + departures
    for (auto& div : bank_.divisions) {
        for (auto& emp : div.staff) {
            emp.quartersEmployed++;
            if (emp.quartersEmployed % 4 == 0) emp.yearsExperience++;
            if (emp.quartersEmployed >= 4) emp.potentialRevealed = true;
        }
        // Check departures
        auto it = div.staff.begin();
        while (it != div.staff.end()) {
            if (rng_.bernoulli(it->departureProbability())) {
                lastReport_.headlines.push_back(it->name + " has left " + div.name);
                div.morale = std::max(div.morale - 3.0, 10.0);
                it = div.staff.erase(it);
            } else {
                ++it;
            }
        }
        div.employees = div.staffCount();
    }

    // Bank end-of-quarter processing (hidden risk, reported risk, financials)
    bank_.endQuarter();

    // ── Sprint 2: Duration Risk — mark-to-market unrealized gains/losses ──
    {
        double currentYield = state_.economics.treasuryYield10Y;
        durationRisk_.updateQuarterly(currentYield, bank_.securities);
        bank_.unrealizedLosses = durationRisk_.unrealizedGainsLosses;

        // SVB trigger: unrealized losses > 30% of capital AND deposits fleeing
        if (durationRisk_.forcedSaleTriggered(bank_.capital) &&
            bank_.depositToAssetRatio() < 0.5) {
            double loss = durationRisk_.forcedSaleLoss(bank_.capital);
            bank_.capital -= loss;
            durationRisk_.unrealizedGainsLosses += loss; // realize the losses
            bank_.unrealizedLosses = durationRisk_.unrealizedGainsLosses;
            lastReport_.headlines.push_back("FORCED ASSET SALE: Unrealized losses crystallized at $" +
                std::to_string((long long)(loss / 1e6)) + "M");
            spdlog::warn("Forced sale: ${:.0f}M loss realized, capital now ${:.0f}M",
                loss / 1e6, bank_.capital / 1e6);
        }
    }

    // ── SFC Phase C: Credit impulse → macro feedback ───────────
    {
        double currentSystemLoans = bank_.loans + competitorEngine_.sumCompetitorLoans();
        if (prevSystemLoans_ > 0 && currentSystemLoans > 0) {
            double nominalGDP = bank_.totalAssets * 10.0; // rough proxy
            double creditImpulse = (currentSystemLoans - prevSystemLoans_)
                                 / std::max(nominalGDP, 1e9);
            creditImpulse = std::clamp(creditImpulse, -0.05, 0.05);
            econ_.setCreditImpulse(creditImpulse);
        }
        prevSystemLoans_ = currentSystemLoans;
    }

    // Crisis check (with CEO modifiers)
    crisisEngine_.escalateActive();
    std::optional<simulation::CrisisArc> newCrisis;
    if (!crisisShieldActive_ && !skipCrises_) { // Bancroft's Fortress or --restrict no-crises
        double ceoRisk = hasCeo_ ? ceoProfile_.riskMultiplier : 1.0;
        newCrisis = crisisEngine_.checkTrigger(bank_, state_, regulatoryShieldActive_, ceoRisk);
    }
    bool hadCrisis = crisisEngine_.hasActiveCrisis();

    // Apply crisis financial impact (scaled by hidden risk)
    // Cap crisis loss at 2% of capital per quarter to prevent death spirals
    if (hadCrisis) {
        double hiddenRiskRatio = bank_.totalHiddenRisk() / std::max(bank_.capital, 1.0);
        double crisisLoss = crisisEngine_.totalFinancialImpact(bank_.capital, hiddenRiskRatio);
        // Phase 4 A1: severe crises (severity > 8) can be lethal; others capped at 5%
        double lossCap = crisisEngine_.maxSeverity() > 8 ? 0.15 : 0.05;
        crisisLoss = std::min(crisisLoss, bank_.capital * lossCap);
        bank_.capital -= crisisLoss;
        // Reputation damage from crises (scaled by difficulty)
        double repDamage = crisisEngine_.totalReputationDamage() * config_.reputationCrisisDamageMultiplier;
        repDamage = std::min(repDamage, 6.0); // Cap per-quarter reputation damage
        bank_.reputation -= repDamage;
    }

    // Reputation recovery: profitable quarters rebuild trust
    if (bank_.netIncome > 0) {
        double recovery = std::min(config_.reputationRecoveryRate,
            bank_.netIncome / (bank_.capital * 0.005));
        bank_.reputation += recovery;
    }

    // Stability bonus: crisis-free quarters earn gradual trust
    if (!hadCrisis) {
        bank_.reputation += 1.0; // Crisis-free quarters build trust
    }

    // Reputation floor: well-capitalized banks retain minimum trust
    double capRatio = bank_.capitalRatio();
    double repFloor = 0.0;
    if (capRatio > 0.12) repFloor = 40.0;
    else if (capRatio > 0.08) repFloor = 30.0;
    else if (capRatio > 0.04) repFloor = 15.0;
    bank_.reputation = std::clamp(bank_.reputation, repFloor, 100.0);

    // Phase 4 A1: reputation decay from sustained hidden risk (scandal leaking)
    if (bank_.totalHiddenRisk() > bank_.capital * 0.5) {
        consecutiveHighHiddenRiskQuarters_++;
        if (consecutiveHighHiddenRiskQuarters_ >= 4) {
            bank_.reputation -= 2.0;  // Scandal leaking: -2 rep/quarter
        }
    } else {
        consecutiveHighHiddenRiskQuarters_ = 0;
    }

    // ── SFC Phase B+Sprint 3: Deposit Dynamics (Retail + Wholesale) ──
    {
        double marketRate = state_.economics.fedFundsRate;
        bank_.depositRate = std::max(0.001, marketRate - 0.005);

        // Initialize retail/wholesale split if not yet done
        if (bank_.retailDeposits + bank_.wholesaleDeposits < bank_.totalDeposits * 0.5) {
            double retailShare = std::clamp(1.0 - bank_.totalAssets / 5e7, 0.40, 0.80);
            bank_.retailDeposits = bank_.totalDeposits * retailShare;
            bank_.wholesaleDeposits = bank_.totalDeposits * (1.0 - retailShare);
        }

        double stressScore = econ_.stressScore();
        double reputationFactor = (bank_.reputation - 50.0) / 50.0;

        // Retail deposits: sticky, FDIC insured (post-1933), slow-moving
        double retailFlow = bank_.retailDeposits * (
            0.003 * reputationFactor                    // reputation-driven
            + 0.002 * state_.economics.gdpGrowth        // macro
        );
        // Retail only flees in extreme panic (reputation < 15)
        if (bank_.reputation < 15.0) {
            retailFlow -= bank_.retailDeposits * 0.03;  // max 3%/quarter even worst case
            lastReport_.headlines.push_back("RETAIL PANIC: Even insured depositors withdrawing");
        }
        bank_.retailDeposits = std::max(bank_.capital * 0.3, bank_.retailDeposits + retailFlow);

        // Wholesale deposits: volatile, confidence-sensitive, can vanish fast
        double wholesaleFlow = bank_.wholesaleDeposits * (
            0.01 * (bank_.depositRate - marketRate)     // rate-sensitive
            + 0.005 * reputationFactor                   // reputation
            - 0.015 * std::max(0.0, stressScore - 40.0) / 60.0  // stress-sensitive
        );
        // Wholesale flight: when stress high OR capital ratio weak
        if (stressScore > 50 || bank_.capitalRatio() < 0.06) {
            double flightRate = 0.10 * std::max(0.0, 1.0 - bank_.capitalRatio() / 0.10);
            flightRate += 0.05 * std::max(0.0, (stressScore - 50.0) / 50.0);
            wholesaleFlow -= bank_.wholesaleDeposits * std::clamp(flightRate, 0.0, 0.30);
        }
        // Bank run: reputation + VIX dual trigger → wholesale devastation
        if (bank_.reputation < 25.0 && state_.economics.vix > 25.0) {
            double runSeverity = (25.0 - bank_.reputation) / 25.0 * (state_.economics.vix - 25.0) / 15.0;
            wholesaleFlow -= bank_.wholesaleDeposits * 0.15 * std::min(runSeverity, 1.0);
            if (runSeverity > 0.3) {
                lastReport_.headlines.push_back("BANK RUN: Wholesale funding fleeing amid crisis");
                spdlog::warn("Bank run: wholesale flight {:.1f}%, reputation {:.0f}, VIX {:.0f}",
                    runSeverity * 15, bank_.reputation, state_.economics.vix);
            }
        }
        bank_.wholesaleDeposits = std::max(0.0, bank_.wholesaleDeposits + wholesaleFlow);

        // Reconstruct total deposits
        bank_.totalDeposits = bank_.retailDeposits + bank_.wholesaleDeposits;
        bank_.totalDeposits = std::max(bank_.capital * 0.5, bank_.totalDeposits);

        // Interbank cost rises when deposit-to-asset ratio drops
        double dtar = bank_.depositToAssetRatio();
        if (dtar < 0.6) {
            double stressSpread = 0.02 * (0.6 - dtar) / 0.6;
            bank_.fundingCost += bank_.interbankBorrowing * stressSpread / 4.0;
        }
        // If capital ratio < 4%: wholesale market CLOSED, only Fed window at penalty rate
        if (bank_.capitalRatio() < 0.04) {
            bank_.fundingCost += bank_.interbankBorrowing * 0.05 / 4.0; // 500bps penalty
        }
    }

    // Phase 4 A1: fraud scandal trigger (probabilistic when conditions met)
    if (!fraudTriggered_ && bank_.totalHiddenRisk() > bank_.capital * 3.0) {
        // Count self-serving division heads (low integrity proxy)
        int selfServingCount = 0;
        int divCount = 0;
        for (const auto& div : bank_.divisions) {
            if (div.headHonesty == HeadHonesty::SelfServing) selfServingCount++;
            divCount++;
        }
        double selfServingRatio = divCount > 0 ? (double)selfServingCount / divCount : 0.0;
        if (selfServingRatio > 0.4 && rng_.bernoulli(0.15)) {
            fraudTriggered_ = true;
            spdlog::warn("FRAUD SCANDAL triggered: hidden risk {:.0f}B vs capital {:.0f}B, self-serving ratio {:.2f}",
                bank_.totalHiddenRisk() / 1e9, bank_.capital / 1e9, selfServingRatio);
        }
    }

    bank_.reputation = std::clamp(bank_.reputation, 0.0, 100.0);

    // Morale-reputation feedback loop
    for (auto& div : bank_.divisions) {
        if (bank_.reputation > 70.0) div.morale = std::min(div.morale + 0.5, 100.0);
        else if (bank_.reputation < 30.0) div.morale = std::max(div.morale - 0.5, 0.0);
    }

    // Scoring
    QuarterScore score;
    int correctDecisions = 0;
    for (const auto& d : pendingDecisions_) {
        if (d.resolved) correctDecisions++;
    }
    score.calculate(bank_, hadCrisis, correctDecisions, (int)pendingDecisions_.size(),
                    startingCapital_, eraEngine_.currentEraIndex());
    int levelBeforeXp = progression_.level;   // 0.3: detect a real promotion this quarter
    progression_.addExperience(score.xpEarned);
    bool promotedThisQuarter = progression_.level > levelBeforeXp;

    // Regulatory compliance check (CEO regulatoryBonus relaxes requirements)
    double ceoRegBonus = hasCeo_ ? ceoProfile_.regulatoryBonus : 0.0;
    if (!regulatoryEngine_.checkCompliance(bank_, ceoRegBonus)) {
        regulatoryEngine_.recordViolation("Capital ratio below regulatory minimum");
    }

    // Track endgame metrics
    quartersCompleted_++;
    totalScore_ += score.total;
    if (score.total < 20.0) consecutivePoorQuarters_++;
    else consecutivePoorQuarters_ = 0;

    // Phase 4 A1: track mediocre performance (score < 35 for board patience)
    if (score.total < 35.0) consecutiveMediocreQuarters_++;
    else consecutiveMediocreQuarters_ = 0;

    // Track profitable quarters for buyback eligibility
    if (bank_.netIncome > 0) consecutiveProfitableQuarters_++;
    else consecutiveProfitableQuarters_ = 0;

    // Update streaks
    streaks_.brokenMessage.clear();
    auto updateStreak = [&](int& streak, bool condition, const std::string& name) {
        if (condition) { streak++; }
        else if (streak >= 5) {
            streaks_.brokenMessage = name + " streak of " + std::to_string(streak) + " quarters broken!";
            streak = 0;
        } else { streak = 0; }
    };
    updateStreak(streaks_.profit, bank_.netIncome > 0, "Profit");
    updateStreak(streaks_.highScore, score.total > 60.0, "High score");
    updateStreak(streaks_.crisisFree, !hadCrisis, "Crisis-free");
    updateStreak(streaks_.growth, bank_.capital > peakCapital_ * 0.99, "Growth");
    if (bank_.capital > peakCapital_) peakCapital_ = bank_.capital;

    // Generate quarterly report
    lastReport_.year = state_.currentYear;
    lastReport_.quarter = state_.currentQuarter;
    lastReport_.totalRevenue = bank_.totalRevenue;
    lastReport_.totalCosts = bank_.totalCosts;
    lastReport_.netIncome = bank_.netIncome;
    lastReport_.capitalRatio = bank_.capitalRatio();
    lastReport_.leverageRatio = bank_.leverageRatio();
    lastReport_.score = score;
    lastReport_.progression = progression_;
    lastReport_.activeCrises = crisisEngine_.activeCrises();

    // Generate headlines
    lastReport_.headlines.clear();
    // 0.2: adaptive units ($850K / $12.5M / ...), no more "$0M". 0.3: stamp the
    // quarter so when the annual report accumulates four quarters of headlines
    // they're distinguishable ("Q1 profit" vs "Q2 profit") rather than reading
    // as duplicates, and the annual dedup keeps only genuinely-identical lines.
    {
        std::string q = "Q" + std::to_string(state_.currentQuarter);
        if (bank_.netIncome > 0) {
            lastReport_.headlines.push_back(bank_.name + " posts "
                + fmtMoneyHeadline(bank_.netIncome) + " " + q + " profit");
        } else {
            lastReport_.headlines.push_back(bank_.name + " reports "
                + fmtMoneyHeadline(std::abs(bank_.netIncome)) + " " + q + " loss");
        }
    }
    if (newCrisis) {
        lastReport_.headlines.push_back("BREAKING: " + newCrisis->title);
    }
    // 0.3: only headline a promotion on the quarter it actually happens — not
    // every quarter the level stays > 1 (which produced the "CEO promoted to
    // Bank Manager" x2/x3 duplicates in the annual report).
    if (promotedThisQuarter) {
        lastReport_.headlines.push_back("CEO promoted to " + progression_.title);
    }

    // Add consequence headlines
    for (const auto& c : resolvedConsequences_) {
        std::string prefix = c.positive ? "GOOD NEWS: " : "WARNING: ";
        lastReport_.headlines.push_back(prefix + c.title);
    }

    // Character evolution: loyalty drift, departures
    double ceoLeadership = hasCeo_ ? ceoProfile_.personality.leadership : 0.5;
    double ceoIntegrity = hasCeo_ ? ceoProfile_.personality.integrity : 0.5;
    auto charHeadlines = characterEngine_.updateQuarterly(hadCrisis, rng_, ceoLeadership, ceoIntegrity);
    for (const auto& h : charHeadlines) {
        lastReport_.headlines.push_back(h);
    }

    // Generate narrative summary and crisis narratives
    simulation::NarrativeContext nctx{
        bank_, state_, score, progression_,
        crisisEngine_.activeCrises(), resolvedConsequences_,
        quarterEvents_, hasCeo_, hasCeo_ ? ceoProfile_.name : "",
        state_.currentQuarter
    };
    lastReport_.narrative = simulation::NarrativeEngine::quarterSummary(nctx);
    lastReport_.crisisNarratives.clear();
    for (const auto& crisis : crisisEngine_.activeCrises()) {
        if (!crisis.resolved) {
            lastReport_.crisisNarratives.push_back(
                simulation::NarrativeEngine::crisisNarrative(crisis, bank_.totalAssets));
        }
    }

    // Reset CEO ability flags and tick cooldown
    revenueBoostActive_ = false;
    regulatoryShieldActive_ = false;
    crisisShieldActive_ = false;
    specialAbilityActive_ = false;
    if (specialAbilityCooldown_ > 0) specialAbilityCooldown_--;

    // Cleanup old resolved crises and consequences
    crisisEngine_.cleanup(state_.currentQuarter);
    consequenceTracker_.cleanup(currentQ);

    spdlog::info("Quarter Score: {:.1f} | XP: {} | Level: {} ({}) | Consequences: {}",
        score.total, score.xpEarned, progression_.level, progression_.title,
        resolvedConsequences_.size());

    // Generate "next quarter preview" (Zeigarnik Effect — incomplete tasks pull players forward)
    nextQuarterPreview_.clear();
    // Pending deal resolutions
    for (const auto& deal : dealPortfolio_.activeDeals()) {
        int resolveQ = state_.currentQuarter + (state_.currentYear - config_.startYear) * 4;
        if (deal.resolveQuarter <= resolveQ + 1) {
            nextQuarterPreview_.push_back("Your investment in \"" + deal.opportunity.title + "\" resolves soon");
        }
    }
    // Unresolved crises will escalate
    for (const auto& crisis : crisisEngine_.activeCrises()) {
        if (!crisis.resolved) {
            nextQuarterPreview_.push_back("Crisis \"" + crisis.title + "\" will escalate (severity " +
                std::to_string(crisis.severity) + " -> " + std::to_string(std::min(crisis.severity + 2, 10)) + ")");
        }
    }
    // XP to next level
    int xpNeeded = progression_.xpToNextLevel() - progression_.experience;
    if (xpNeeded > 0 && xpNeeded < 150) {
        nextQuarterPreview_.push_back("You're " + std::to_string(xpNeeded) + " XP from Level " +
            std::to_string(progression_.level + 1));
    }
    // Pending consequences
    if (consequenceTracker_.pendingCount() > 0) {
        nextQuarterPreview_.push_back(std::to_string(consequenceTracker_.pendingCount()) +
            " past decision(s) will bear fruit soon");
    }

    // Accumulate annual report data
    annualAccum_.accumulate(bank_.totalRevenue, bank_.totalCosts,
                            score.total, lastReport_.headlines);

    // At Q4: simulate yearly systems and generate annual report
    if (state_.currentQuarter == 4 || (state_.currentQuarter == 1 && annualAccum_.yearQuarterCount >= 4)) {
        double playerSavvy = hasCeo_ ? ceoProfile_.personality.politicalSavvy : 0.5;
        competitorEngine_.simulateYear(state_.currentYear, state_.regime,
                                       econ_.stressScore(), bank_, playerSavvy);

        // AI CEO: track investment and simulate
        double aiSpend = pathEngine_.state().techInvestment * 5.0; // $B based on tech investment
        aiCeoEngine_.recordAIInvestment(aiSpend);
        aiCeoEngine_.simulateYear(state_.currentYear);

        // Political: annual simulation (capital decay, opinion drift)
        politicalEngine_.simulateYear(state_.currentYear, bank_.reputation,
            crisisEngine_.hasActiveCrisis());

        // Climate: simulate and apply damage
        climateEngine_.simulateYear(state_.currentYear);
        if (state_.currentYear >= 2020) {
            double climateDamage = climateEngine_.physicalDamage(bank_);
            double strandedLoss = climateEngine_.strandedAssetLoss(bank_);
            bank_.capital -= (climateDamage + strandedLoss);
        }

        // Update doom meters from engine states
        auto& dm = state_.doomMeters;
        // AI: map singularity progress (0-100) directly
        dm.aiDisplacement = std::clamp(aiCeoEngine_.state().singularityProgress * 1.0, 0.0, 100.0);
        // Climate: map tipping points (0-5) to 0-100, plus temperature contribution
        dm.climateCatastrophe = std::clamp(
            climateEngine_.state().tippingPointsReached * 20.0
            + std::max(0.0, climateEngine_.state().globalTemp - 1.5) * 20.0,
            0.0, 100.0);

        // Global tensions: cold war drift + AI/climate cascade - cooperation buffer
        {
            double delta = 0.5; // baseline annual rise
            if (aiCeoEngine_.state().singularityProgress > 50) delta += 1.0; // AI race
            if (climateEngine_.state().globalTemp > 2.0) delta += 1.5;       // resource conflict
            if (politicalEngine_.state().publicOpinion > 0.5) delta -= 0.3;  // cooperation
            dm.globalTensions = std::clamp(dm.globalTensions + delta, 0.0, 100.0);
        }

        // Inequality: capital concentration + competitor failures - redistribution
        {
            double delta = 0.2; // baseline rise
            if (bank_.capital > startingCapital_ * 5) delta += 1.0; // bank dominance
            // Count failed competitors as a proxy for consolidation
            int failedCount = 0;
            for (const auto& c : competitorEngine_.competitors()) {
                if (!c.alive) failedCount++;
            }
            delta += failedCount * 0.5;
            if (recentDividendQuarters_ > 0) delta -= 1.0; // player redistributing
            dm.inequality = std::clamp(dm.inequality + delta, 0.0, 100.0);
        }
        if (recentDividendQuarters_ > 0) recentDividendQuarters_--;

        generateAnnualReport();
    }

    // P3.2: record this quarter's macro + market-close snapshot for chart
    // hydration. state_.economics is current (synced in runSimulation /
    // completeSimulationPhase) and marketSim_.snapshot() holds the quarter's
    // closes. No RNG draw — lockstep-safe.
    macroHistory_.record(state_.currentYear, state_.currentQuarter,
                         state_.economics, marketSim_.snapshot());

    // STAR_02 P6 ReputationLens: detect a tag transition (the world re-reads
    // what you've become) and log it server-side. The client mirrors this with a
    // reputation_tag_changed {from,to} telemetry event off the player-view tag.
    {
        std::string tagNow = ReputationLens::tagFor(pathEngine_.state());
        if (tagNow != lastReputationTag_) {
            if (!lastReputationTag_.empty())
                spdlog::info("REPUTATION: '{}' -> '{}'", lastReputationTag_, tagNow);
            lastReputationTag_ = tagNow;
        }
    }

    // P5 §5.1: snapshot this quarter's macro as "previous" for next quarter's
    // change terms (Δfed, Δspread, Δvix). No RNG — lockstep-safe.
    prevQuarterEcon_ = state_.economics;
    prevQuarterEconValid_ = true;

    // P5 §5.3: age out event-driven drift nudges (one quarter consumed); drop
    // expired entries. No-op when eventMarketCoupling is off.
    for (auto it = eventDivisionDrift_.begin(); it != eventDivisionDrift_.end(); ) {
        if (--it->second.quartersLeft <= 0) it = eventDivisionDrift_.erase(it);
        else ++it;
    }
}

// ════════════════════════════════════════════════════════════════════
// P5 §5.3 / P3.4 Stage B — event → market coupling
//
// When a historical event with affects.benefits/suffers fires, register a
// small per-quarter drift contribution to each affected division over the
// event's duration. Magnitude scales with the event's riskLevel (1-10);
// sign is + for beneficiaries, - for sufferers. Capped at ±2%/quarter so a
// single event never dominates the macro distribution. No RNG — lockstep-safe.
// ════════════════════════════════════════════════════════════════════
inline void QuarterlyTurnManager::registerEventDrift(const std::string& eventId) {
    const simulation::HistoricalEvent* src = nullptr;
    for (const auto& e : historicalLoader_.allEvents())
        if (e.id == eventId) { src = &e; break; }
    if (!src) return;

    int duration = std::max(1, src->risk.durationQuarters);
    // riskLevel 1-10 → magnitude 0.2%..2.0% per quarter (capped at ±2%).
    double mag = std::clamp(src->risk.riskLevel / 10.0 * 0.02, 0.0, 0.02);

    auto add = [&](const std::string& divName, double sign) {
        auto& d = eventDivisionDrift_[divName];
        d.perQuarter = std::clamp(d.perQuarter + sign * mag, -0.02, 0.02);
        d.quartersLeft = std::max(d.quartersLeft, duration);
    };
    for (const auto& b : src->benefits) add(b, +1.0);
    for (const auto& s : src->suffers)  add(s, -1.0);
}

// ════════════════════════════════════════════════════════════════════
// Real-time simulation support — day-by-day ticking
// ════════════════════════════════════════════════════════════════════

inline void QuarterlyTurnManager::prepareSimulation() {
    spdlog::info("Preparing day-by-day simulation ({} days)...", config_.quarterDurationDays);
    simDayCounter_ = 0;
    simulationDigest_.clear();
    simPrevMarkets_ = marketSim_.snapshot();
    simPrevStress_ = econ_.stressScore();
}

inline bool QuarterlyTurnManager::tickSimulationDay() {
    if (simDayCounter_ >= config_.quarterDurationDays) return true;

    double dt = 1.0 / 252.0;

    // v3: optional custom-economy injection
    if (customEcon_) customEcon_->dailyTick(dt, econ_);
    else             econ_.tick(dt);
    marketSim_.tick(dt);
    traderAI_.tickDay(traders_, marketSim_.snapshot(), econ_.stressScore());

    // Intra-quarter deal trickle: opportunities flow into the loan book through
    // the quarter (~every 15-30s of real time at default speed) instead of only
    // appearing at quarter boundaries.
    if (rng_.bernoulli(0.035)) {
        dealPortfolio_.addTrickleDeal(bank_.divisions, state_.currentYear);
    }

    // STAR_02 P6: intra-quarter HIRING trickle — identical draw to runSimulation
    // so the day-by-day and batch RNG sequences stay locked (DayByDayMatchesBatch).
    if (rng_.bernoulli(0.012)) {
        addTrickleCandidate();
    }

    clock_.advanceDay();

    auto curMarkets = marketSim_.snapshot();
    for (size_t i = 0; i < curMarkets.size() && i < simPrevMarkets_.size(); ++i) {
        if (simPrevMarkets_[i].currentPrice > 0) {
            double pctChange = (curMarkets[i].currentPrice - simPrevMarkets_[i].currentPrice)
                             / simPrevMarkets_[i].currentPrice;
            if (std::abs(pctChange) > 0.02) {
                std::string dir = pctChange > 0 ? "surges" : "drops";
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s %s %.1f%%",
                    curMarkets[i].symbol.c_str(), dir.c_str(), std::abs(pctChange) * 100.0);
                simulationDigest_.push_back({simDayCounter_ + 1, std::string(buf), "market"});
            }
        }
    }

    double curStress = econ_.stressScore();
    if (curStress > 60 && simPrevStress_ <= 60) {
        simulationDigest_.push_back({simDayCounter_ + 1, "Market stress enters danger zone", "economic"});
    } else if (curStress < 30 && simPrevStress_ >= 30) {
        simulationDigest_.push_back({simDayCounter_ + 1, "Market stress subsides to calm levels", "economic"});
    }
    simPrevStress_ = curStress;
    simPrevMarkets_ = curMarkets;
    simDayCounter_++;

    state_.currentDay = clock_.dayInQuarter();
    state_.totalDaysElapsed = clock_.totalDays();
    state_.currentYear = clock_.year();
    state_.currentQuarter = clock_.quarter();

    return simDayCounter_ >= config_.quarterDurationDays;
}

inline void QuarterlyTurnManager::completeSimulationPhase() {
    if (simulationDigest_.size() > 5) simulationDigest_.resize(5);

    state_.currentYear = clock_.year();
    state_.currentQuarter = clock_.quarter();
    state_.currentDay = clock_.dayInQuarter();
    state_.totalDaysElapsed = clock_.totalDays();
    state_.economics = econ_.state();
    state_.marketState = econ_.toMarketState();
    state_.markets = marketSim_.snapshot();
    state_.regime = marketSim_.currentRegime(econ_.stressScore());

    phase_ = TurnPhase::QuarterEnd;
    spdlog::info("Real-time simulation complete, transitioning to QuarterEnd");
}

} // namespace stvg
