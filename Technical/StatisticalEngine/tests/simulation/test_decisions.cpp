#include <gtest/gtest.h>
#include "stvg/simulation/DecisionEngine.h"
#include "stvg/simulation/PoliticalEngine.h"
#include "stvg/simulation/ClimateEngine.h"
#include "stvg/simulation/AICEOEngine.h"
#include "stvg/simulation/CompetitorEngine.h"
#include "stvg/core/BankState.h"
#include "stvg/core/QuarterlyTurnManager.h"
#include "stvg/simulation/HistoricalEventLoader.h"
#include "stvg/math/RandomEngine.h"

using namespace stvg;
using namespace stvg::simulation;

// ════════════════════════════════════════════════════════════════════
// Decision System Tests — endgame player agency
// ════════════════════════════════════════════════════════════════════

class DecisionTest : public ::testing::Test {
protected:
    math::RandomEngine rng_{42};
    DecisionEngine engine_{rng_};
    PoliticalEngine political_{rng_};
};

// ── 6A: PoliticalEngine → DecisionEngine Wiring ─────────────────

TEST_F(DecisionTest, LobbyProbUsesLiveEffectiveness) {
    // Without PoliticalEngine, lobby prob should be default 0.45
    GameEvent evt;
    evt.id = "test_reg";
    evt.title = "Test Regulation";
    evt.description = "Test";
    evt.category = EventCategory::Regulatory;
    Bank bank;
    bank.capital = 10e9;

    auto d1 = engine_.generateFromEvents({evt}, bank, SimulationState{});
    // Find lobby option
    double defaultProb = 0;
    for (const auto& dec : d1) {
        for (const auto& opt : dec.options) {
            if (opt.id.find("lobby") != std::string::npos) {
                defaultProb = opt.successProbability;
            }
        }
    }
    EXPECT_NEAR(defaultProb, 0.45, 0.01) << "Without PoliticalEngine, lobby should use default 0.45";

    // With PoliticalEngine in big_bang era (1.5x effectiveness) and high capital
    political_.earnCapital(80.0);
    political_.setEraModifier("big_bang");
    engine_.setPoliticalEngine(&political_);

    auto d2 = engine_.generateFromEvents({evt}, bank, SimulationState{});
    double boostedProb = 0;
    for (const auto& dec : d2) {
        for (const auto& opt : dec.options) {
            if (opt.id.find("lobby") != std::string::npos) {
                boostedProb = opt.successProbability;
            }
        }
    }
    EXPECT_GT(boostedProb, defaultProb)
        << "With high capital in big_bang era, lobby prob should be higher than default";
    EXPECT_LE(boostedProb, 0.90) << "Lobby prob capped at 0.90";
}

TEST_F(DecisionTest, LobbyProbLowInReformEra) {
    political_.earnCapital(20.0);
    political_.setEraModifier("reform"); // 0.5x effectiveness
    engine_.setPoliticalEngine(&political_);

    GameEvent evt;
    evt.category = EventCategory::Regulatory;
    evt.title = "Test"; evt.description = "Test"; evt.id = "test";
    Bank bank;
    bank.capital = 10e9;

    auto decisions = engine_.generateFromEvents({evt}, bank, SimulationState{});
    double prob = 0;
    for (const auto& dec : decisions) {
        for (const auto& opt : dec.options) {
            if (opt.id.find("lobby") != std::string::npos) prob = opt.successProbability;
        }
    }
    EXPECT_LT(prob, 0.45) << "Reform era should reduce lobby effectiveness below default";
}

// ── 6B: Webb CEO Ability ─────────────────────────────────────────

TEST_F(DecisionTest, WebbAbilityRequiresPoliticalCapital) {
    // Create a game with Webb as CEO
    SimulationConfig config;
    config.seed = 42;
    config.startYear = 1945;
    QuarterlyTurnManager mgr(config, {}, "webb");

    // Webb starts with 0 political capital — ability should fail
    bool result = mgr.useSpecialAbility();
    EXPECT_FALSE(result) << "Webb's ability should fail without political capital";
}

// ── 6C: Climate Decisions ────────────────────────────────────────

TEST_F(DecisionTest, ClimateDecisionHasFourOptions) {
    Bank bank;
    bank.capital = 10e9;
    ClimateState climate;
    climate.globalTemp = 1.5;
    climate.carbonPrice = 100.0;

    auto decision = engine_.generateClimateDecision(bank, climate);
    EXPECT_EQ(decision.options.size(), 4u);
    EXPECT_EQ(decision.type, DecisionType::Strategic);

    // Check option IDs
    bool hasGreen = false, hasESG = false, hasDivest = false, hasIgnore = false;
    for (const auto& opt : decision.options) {
        if (opt.id.find("green") != std::string::npos) hasGreen = true;
        if (opt.id.find("esg") != std::string::npos) hasESG = true;
        if (opt.id.find("divest") != std::string::npos) hasDivest = true;
        if (opt.id.find("ignore") != std::string::npos) hasIgnore = true;
    }
    EXPECT_TRUE(hasGreen && hasESG && hasDivest && hasIgnore)
        << "Climate decision should have green, esg, divest, and ignore options";
}

TEST_F(DecisionTest, GreenLendingReducesFossilExposure) {
    ClimateEngine climate(rng_);
    climate.simulateYear(2025); // Initialize climate state
    EXPECT_NEAR(climate.effectiveFossilExposure(), 0.05, 0.001)
        << "Default fossil exposure should be 5%";

    climate.applyGreenLendingShift(0.2); // 20% reduction
    EXPECT_NEAR(climate.effectiveFossilExposure(), 0.04, 0.001)
        << "After green lending, exposure should drop to 4%";
}

TEST_F(DecisionTest, DivestmentEliminatesFossilExposure) {
    ClimateEngine climate(rng_);
    climate.applyDivestment(1.0); // Full divestment
    EXPECT_NEAR(climate.effectiveFossilExposure(), 0.0, 0.001)
        << "Full divestment should eliminate fossil exposure";
}

// ── 6D: AI Counter-Decisions ─────────────────────────────────────

TEST_F(DecisionTest, AIDecisionHasFiveOptions) {
    Bank bank;
    bank.capital = 50e9;
    AICEOState aiState;
    aiState.emerged = true;
    aiState.efficiency = 2.0;
    aiState.marketCapture = 0.15;

    auto decision = engine_.generateAICounterDecision(bank, aiState);
    EXPECT_EQ(decision.options.size(), 5u);
    EXPECT_EQ(decision.urgency, DecisionUrgency::Urgent);
}

TEST_F(DecisionTest, AIRegulationSlowsEfficiencyGrowth) {
    AICEOEngine aiEngine;
    aiEngine.recordAIInvestment(600); // Trigger emergence
    aiEngine.simulateYear(2030);
    EXPECT_TRUE(aiEngine.hasEmerged());

    double effBefore = aiEngine.efficiency();
    aiEngine.applyRegulation(0.5); // 50% drag
    aiEngine.simulateYear(2031);
    double effAfter = aiEngine.efficiency();

    double growthWithDrag = effAfter - effBefore;
    EXPECT_LT(growthWithDrag, 0.2)
        << "With 50% regulatory drag, efficiency growth should be < 0.2/year";
    EXPECT_GT(growthWithDrag, 0.05)
        << "Even with drag, some growth should occur";
}

TEST_F(DecisionTest, AIPartnershipGeneratesRevenue) {
    AICEOEngine aiEngine;
    aiEngine.recordAIInvestment(600);
    aiEngine.simulateYear(2030);
    aiEngine.applyPartnership();
    aiEngine.simulateYear(2031);

    EXPECT_TRUE(aiEngine.state().partnershipActive);
    EXPECT_GT(aiEngine.state().partnershipRevenue, 0)
        << "Partnership should generate revenue proportional to AI efficiency";
}

// ── 6E: Competitor Acquisitions ──────────────────────────────────

TEST_F(DecisionTest, AcquisitionDecisionHasTwoOptions) {
    AcquisitionOffer offer;
    offer.targetId = "test_bank";
    offer.targetName = "Test National Bank";
    offer.price = 5e9;
    offer.assetsGained = 20e9;
    offer.hiddenRisk = 2e9;
    offer.archetype = "Gunslinger";

    auto decision = engine_.generateAcquisitionDecision(offer);
    EXPECT_EQ(decision.options.size(), 2u);
    EXPECT_EQ(decision.type, DecisionType::DealApproval);
    EXPECT_EQ(decision.urgency, DecisionUrgency::Immediate);
    EXPECT_TRUE(decision.title.find("Test National Bank") != std::string::npos);
}

TEST_F(DecisionTest, AcquisitionBuyOptionHasCostAndGain) {
    AcquisitionOffer offer;
    offer.targetId = "apex";
    offer.targetName = "Apex Securities";
    offer.price = 3e9;
    offer.assetsGained = 15e9;
    offer.hiddenRisk = 1e9;
    offer.archetype = "Gunslinger";

    auto decision = engine_.generateAcquisitionDecision(offer);
    const auto& buyOpt = decision.options[0]; // First option is "buy"
    EXPECT_GT(buyOpt.financialImpact.immediateCost, 0)
        << "Acquisition should have a cost";
    EXPECT_GT(buyOpt.financialImpact.expectedRevenue, 0)
        << "Acquisition should have expected revenue from gained assets";
    EXPECT_GT(buyOpt.riskImpact.riskChange, 0)
        << "Acquiring a failed bank should carry risk";
}

// ── Era-Gated Filtering ─────────────────────────────────────

TEST(EraFilter, HistoricalEventsEraFilteredByLoader) {
    // Historical events with era ranges [2000,2007] for securitization
    // should NOT appear when loading events for year 1960
    HistoricalEventLoader loader;
    bool loaded = false;
    for (const auto& path : {"data/events/historical_events.json",
                              "../data/events/historical_events.json",
                              "../../data/events/historical_events.json"}) {
        if (loader.loadFromFile(path)) { loaded = true; break; }
    }
    if (!loaded) GTEST_SKIP() << "Historical events not found";

    // Get events for 1960 — should not include securitization/CDO/crypto events
    auto events1960 = loader.getEventsForYear(1960);
    for (const auto* evt : events1960) {
        // These events should not appear in 1960
        EXPECT_TRUE(evt->title.find("CDO") == std::string::npos)
            << "CDO event should not appear in 1960: " << evt->title;
        EXPECT_TRUE(evt->title.find("Subprime") == std::string::npos)
            << "Subprime event should not appear in 1960: " << evt->title;
        EXPECT_TRUE(evt->title.find("Bitcoin") == std::string::npos)
            << "Bitcoin event should not appear in 1960: " << evt->title;
        EXPECT_TRUE(evt->title.find("Crypto") == std::string::npos)
            << "Crypto event should not appear in 1960: " << evt->title;
    }
}

TEST(EraFilter, NoCryptoEventsIn2000) {
    HistoricalEventLoader loader;
    bool loaded = false;
    for (const auto& path : {"data/events/historical_events.json",
                              "../data/events/historical_events.json",
                              "../../data/events/historical_events.json"}) {
        if (loader.loadFromFile(path)) { loaded = true; break; }
    }
    if (!loaded) GTEST_SKIP();

    auto events2000 = loader.getEventsForYear(2000);
    for (const auto* evt : events2000) {
        EXPECT_TRUE(evt->title.find("Bitcoin") == std::string::npos)
            << "Bitcoin should not appear in 2000: " << evt->title;
        EXPECT_TRUE(evt->title.find("Crypto Custody") == std::string::npos)
            << "Crypto Custody should not appear in 2000: " << evt->title;
    }
}
