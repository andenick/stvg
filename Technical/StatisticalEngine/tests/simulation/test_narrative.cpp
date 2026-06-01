#include <gtest/gtest.h>
#include <stvg/simulation/NarrativeEngine.h>
#include <stvg/core/BankState.h>
#include <stvg/core/ScoringEngine.h>

using namespace stvg;
using namespace stvg::simulation;

// Helper to create a minimal NarrativeContext
static NarrativeContext makeContext(MarketRegime regime = MarketRegime::Normal,
                                    double score = 50.0,
                                    int numCrises = 0) {
    static Bank bank = Bank::create({});
    static SimulationState state;
    static QuarterScore qscore;
    static PlayerProgression prog;
    static std::vector<CrisisArc> crises;
    static std::vector<Consequence> consequences;
    static std::vector<GameEvent> events;

    state.regime = regime;
    state.economics.vix = (regime == MarketRegime::Crisis) ? 45.0 : 18.0;
    state.economics.gdpGrowth = 2.5;
    state.economics.fedFundsRate = 4.5;
    state.economics.unemployment = 4.0;
    state.currentYear = 2025;
    state.currentQuarter = 2;
    qscore.total = score;
    prog.level = 5;
    prog.title = "Vice President";

    crises.clear();
    for (int i = 0; i < numCrises; ++i) {
        CrisisArc c;
        c.id = "crisis_" + std::to_string(i);
        c.type = CrisisType::Liquidity;
        c.title = "Liquidity Squeeze";
        c.description = "Test crisis";
        c.severity = 5 + i * 2;
        c.quartersActive = 1 + i;
        crises.push_back(c);
    }

    return NarrativeContext{
        bank, state, qscore, prog, crises, consequences, events,
        false, "", 2
    };
}

// ── Quarter Opening Tests ─────────────────────────────────────────

TEST(NarrativeEngine, QuarterOpeningNonEmpty) {
    auto ctx = makeContext(MarketRegime::Normal);
    std::string text = NarrativeEngine::quarterOpening(ctx);
    EXPECT_FALSE(text.empty());
    EXPECT_GT(text.size(), 20);
}

TEST(NarrativeEngine, QuarterOpeningCrisisRegime) {
    auto ctx = makeContext(MarketRegime::Crisis);
    std::string text = NarrativeEngine::quarterOpening(ctx);
    EXPECT_NE(text.find("freefall"), std::string::npos);
}

TEST(NarrativeEngine, QuarterOpeningWithActiveCrisis) {
    auto ctx = makeContext(MarketRegime::Normal, 50.0, 1);
    std::string text = NarrativeEngine::quarterOpening(ctx);
    EXPECT_NE(text.find("Liquidity"), std::string::npos);
}

TEST(NarrativeEngine, QuarterOpeningCalmHighScore) {
    auto ctx = makeContext(MarketRegime::Calm, 85.0);
    std::string text = NarrativeEngine::quarterOpening(ctx);
    EXPECT_NE(text.find("momentum"), std::string::npos);
}

// ── Quarter Summary Tests ─────────────────────────────────────────

TEST(NarrativeEngine, QuarterSummaryMentionsProfit) {
    auto ctx = makeContext();
    // Bank has positive net income by default
    std::string text = NarrativeEngine::quarterSummary(ctx);
    EXPECT_FALSE(text.empty());
    // Should mention either "income" or "loss"
    bool mentionsFinancials = text.find("income") != std::string::npos
                           || text.find("loss") != std::string::npos
                           || text.find("posted") != std::string::npos
                           || text.find("recorded") != std::string::npos;
    EXPECT_TRUE(mentionsFinancials);
}

TEST(NarrativeEngine, QuarterSummaryScoreOutlook) {
    auto ctx = makeContext(MarketRegime::Normal, 85.0);
    std::string text = NarrativeEngine::quarterSummary(ctx);
    EXPECT_NE(text.find("excellent"), std::string::npos);
}

TEST(NarrativeEngine, QuarterSummaryPoorScore) {
    auto ctx = makeContext(MarketRegime::Normal, 15.0);
    std::string text = NarrativeEngine::quarterSummary(ctx);
    EXPECT_NE(text.find("troubling"), std::string::npos);
}

// ── Crisis Narrative Tests ────────────────────────────────────────

TEST(NarrativeEngine, CrisisNarrativeScalesWithSeverity) {
    CrisisArc mild;
    mild.title = "Test Crisis";
    mild.severity = 3;
    mild.quartersActive = 1;

    CrisisArc severe;
    severe.title = "Test Crisis";
    severe.severity = 9;
    severe.quartersActive = 3;

    std::string mildText = NarrativeEngine::crisisNarrative(mild, 50e9);
    std::string severeText = NarrativeEngine::crisisNarrative(severe, 50e9);

    // Severe should mention "CRITICAL"
    EXPECT_NE(severeText.find("CRITICAL"), std::string::npos);
    EXPECT_EQ(mildText.find("CRITICAL"), std::string::npos);
}

// ── Division Report Tests ─────────────────────────────────────────

TEST(NarrativeEngine, DivisionReportDiffersByHonesty) {
    Division honest;
    honest.headHonesty = HeadHonesty::Honest;
    honest.morale = 30; // low morale
    honest.revenue = 100e6;
    honest.costs = 80e6;
    honest.hiddenRisk = 0;
    honest.actualRisk = 0.05;

    Division selfServing;
    selfServing.headHonesty = HeadHonesty::SelfServing;
    selfServing.morale = 30;
    selfServing.revenue = 100e6;
    selfServing.costs = 80e6;
    selfServing.hiddenRisk = 50e6;
    selfServing.actualRisk = 0.05;

    std::string honestText = NarrativeEngine::divisionReport(honest, 30.0);
    std::string selfText = NarrativeEngine::divisionReport(selfServing, 30.0);

    // Different honesty should produce different text
    EXPECT_NE(honestText, selfText);
    // Honest should flag morale
    EXPECT_NE(honestText.find("morale"), std::string::npos);
}

// ── Consequence Narrative Tests ───────────────────────────────────

TEST(NarrativeEngine, ConsequenceNarrativeMentionsSource) {
    Consequence c;
    c.sourceDecisionTitle = "Aggressive Lending Strategy";
    c.positive = true;
    c.financialImpact = 200e6;
    c.reputationImpact = 8;

    std::string text = NarrativeEngine::consequenceNarrative(c);
    EXPECT_NE(text.find("Aggressive Lending"), std::string::npos);
    EXPECT_NE(text.find("bearing fruit"), std::string::npos);
}

TEST(NarrativeEngine, NegativeConsequenceNarrative) {
    Consequence c;
    c.sourceDecisionTitle = "Risky Bet";
    c.positive = false;
    c.financialImpact = -300e6;
    c.reputationImpact = -10;

    std::string text = NarrativeEngine::consequenceNarrative(c);
    EXPECT_NE(text.find("fallout"), std::string::npos);
}

// ── Milestone Tests ───────────────────────────────────────────────

TEST(NarrativeEngine, MilestoneTextForLegend) {
    PlayerProgression prog;
    prog.level = 42;
    prog.title = "Wall Street Legend";
    std::string text = NarrativeEngine::milestoneText(prog, 500e9);
    EXPECT_NE(text.find("Legend"), std::string::npos);
}
