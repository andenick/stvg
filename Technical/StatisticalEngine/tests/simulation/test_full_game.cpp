#include <gtest/gtest.h>
#include <stvg/core/QuarterlyTurnManager.h>
#include <iostream>
#include <iomanip>

using namespace stvg;
using namespace stvg::simulation;

// ── Full Turn Loop Test ─────────────────────────────────────────────

TEST(TurnManager, InitializesCorrectly) {
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager mgr(cfg);

    EXPECT_EQ(mgr.currentPhase(), TurnPhase::QuarterStart);
    EXPECT_GT(mgr.bank().capital, 0);
    EXPECT_EQ(mgr.bank().branches, 1);
    EXPECT_EQ(mgr.traders().size(), 5);
    EXPECT_GT(mgr.actionPoints().total, 0);
}

TEST(TurnManager, QuarterStartGeneratesEvents) {
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager mgr(cfg);

    auto phase = mgr.advancePhase(); // QuarterStart -> DecisionPhase
    EXPECT_EQ(phase, TurnPhase::DecisionPhase);
    EXPECT_GT(mgr.quarterEvents().size(), 0);
    EXPECT_GT(mgr.pendingDecisions().size(), 0);
    EXPECT_GT(mgr.messages().size(), 0);
}

TEST(TurnManager, DecisionsHaveOptions) {
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager mgr(cfg);
    mgr.advancePhase(); // -> DecisionPhase

    for (const auto& dec : mgr.pendingDecisions()) {
        EXPECT_FALSE(dec.id.empty());
        EXPECT_FALSE(dec.title.empty());
        EXPECT_GE(dec.options.size(), 2);

        for (const auto& opt : dec.options) {
            EXPECT_FALSE(opt.id.empty());
            EXPECT_FALSE(opt.title.empty());
            // Each option should have character recommendations
            EXPECT_GT(opt.recommendations.size(), 0);
        }
    }
}

TEST(TurnManager, CharactersDisagree) {
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager mgr(cfg);
    mgr.advancePhase(); // -> DecisionPhase

    // For at least one decision, characters should disagree
    bool foundDisagreement = false;
    for (const auto& dec : mgr.pendingDecisions()) {
        for (const auto& opt : dec.options) {
            bool hasSupport = false, hasOppose = false;
            for (const auto& rec : opt.recommendations) {
                if (rec.recommendation == "Support") hasSupport = true;
                if (rec.recommendation == "Oppose") hasOppose = true;
            }
            if (hasSupport && hasOppose) foundDisagreement = true;
        }
    }
    EXPECT_TRUE(foundDisagreement);
}

TEST(TurnManager, CanSubmitDecisions) {
    SimulationConfig cfg;
    cfg.seed = 42;
    QuarterlyTurnManager mgr(cfg);
    mgr.advancePhase(); // -> DecisionPhase

    // Submit first decision
    const auto& decisions = mgr.pendingDecisions();
    ASSERT_GT(decisions.size(), 0);
    bool success = mgr.submitDecision(decisions[0].id, decisions[0].options[0].id);
    EXPECT_TRUE(success);
}

TEST(TurnManager, SimulationRunsQuarter) {
    SimulationConfig cfg;
    cfg.seed = 42;
    cfg.quarterDurationDays = 63;
    QuarterlyTurnManager mgr(cfg);

    mgr.advancePhase(); // -> DecisionPhase
    mgr.advancePhase(); // -> Simulation
    EXPECT_EQ(mgr.currentPhase(), TurnPhase::Simulation);
    mgr.advancePhase(); // -> QuarterEnd

    // State should have advanced after simulation ran
    EXPECT_EQ(mgr.state().totalDaysElapsed, 63);
    EXPECT_GT(mgr.state().markets.size(), 0);
}

TEST(TurnManager, QuarterEndProducesReport) {
    SimulationConfig cfg;
    cfg.seed = 42;
    cfg.quarterDurationDays = 63;
    QuarterlyTurnManager mgr(cfg);

    mgr.advancePhase(); // -> DecisionPhase
    mgr.advancePhase(); // -> Simulation
    mgr.advancePhase(); // -> QuarterEnd (triggers report calculation)

    // Advance through QuarterEnd and any crisis phase to QuarterComplete
    while (mgr.currentPhase() != TurnPhase::QuarterComplete) {
        mgr.advancePhase();
    }

    const auto& report = mgr.lastReport();
    EXPECT_GT(report.year, 0);
    EXPECT_GT(report.quarter, 0);
    EXPECT_GE(report.score.total, 0);
    EXPECT_LE(report.score.total, 100);
    EXPECT_GT(report.headlines.size(), 0);
}

// ── Full 4-Quarter Game Test ────────────────────────────────────────

TEST(FullGame, FourQuartersComplete) {
    SimulationConfig cfg;
    cfg.seed = 42;
    cfg.quarterDurationDays = 63;
    QuarterlyTurnManager mgr(cfg);

    for (int q = 0; q < 4; ++q) {
        // Quarter Start
        auto phase = mgr.advancePhase();
        EXPECT_EQ(phase, TurnPhase::DecisionPhase);

        // Submit first available decision
        if (!mgr.pendingDecisions().empty()) {
            const auto& dec = mgr.pendingDecisions()[0];
            mgr.submitDecision(dec.id, dec.options[0].id);
        }

        // Simulation
        mgr.advancePhase(); // -> Simulation
        mgr.advancePhase(); // -> QuarterEnd

        // Handle crisis if needed, then advance to QuarterComplete
        while (mgr.currentPhase() != TurnPhase::QuarterComplete) {
            mgr.advancePhase();
        }

        mgr.advancePhase(); // -> QuarterStart (loops back)
    }

    // After 4 quarters, should have some XP
    EXPECT_GT(mgr.progression().totalExperience, 0);
    // Bank should still exist (not bankrupt from 4 quarters)
    EXPECT_GT(mgr.bank().capital, 0);
}

// ── Trader AI Tests ─────────────────────────────────────────────────

TEST(TraderAI, GeneratesCandidatesWithValidAttributes) {
    math::RandomEngine rng(42);
    TraderAIEngine engine(rng);
    auto candidates = engine.generateCandidates(10);

    EXPECT_EQ(candidates.size(), 10);
    for (const auto& t : candidates) {
        EXPECT_FALSE(t.name.empty());
        EXPECT_GE(t.attributes.intelligence, 85);
        EXPECT_LE(t.attributes.intelligence, 145);
        EXPECT_GE(t.attributes.integrity, 5);
        EXPECT_LE(t.attributes.integrity, 100);
        EXPECT_GT(t.salary, 0);
    }
}

TEST(TraderAI, PersonalityDistribution) {
    math::RandomEngine rng(42);
    TraderAIEngine engine(rng);
    auto candidates = engine.generateCandidates(100);

    int cons = 0, aggr = 0, rogue = 0, anal = 0;
    for (const auto& t : candidates) {
        switch (t.personality) {
            case TraderPersonality::Conservative: cons++; break;
            case TraderPersonality::Aggressive: aggr++; break;
            case TraderPersonality::Rogue: rogue++; break;
            case TraderPersonality::Analytical: anal++; break;
        }
    }

    // Expect roughly 40% conservative, 30% analytical, 20% aggressive, 10% rogue
    EXPECT_GT(cons, 25);  // At least 25% conservative
    EXPECT_GT(anal, 15);  // At least 15% analytical
    EXPECT_GT(aggr, 5);   // At least 5% aggressive
    EXPECT_GT(rogue, 2);  // At least 2% rogue
}

// ── Event Pool Tests ────────────────────────────────────────────────

TEST(EventPool, DrawsEventsFromPool) {
    math::RandomEngine rng(42);
    EventPool pool;
    Bank bank = Bank::create({});
    SimulationState state;
    state.economics.vix = 20;

    auto events = pool.drawEvents(bank, state, rng, 5);
    EXPECT_EQ(events.size(), 5);

    for (const auto& e : events) {
        EXPECT_FALSE(e.id.empty());
        EXPECT_FALSE(e.title.empty());
    }
}

TEST(EventPool, PoolHasEnoughEvents) {
    EventPool pool;
    EXPECT_GE(pool.poolSize(), 30); // At least 30 starter events
}

// ── Crisis Engine Tests ─────────────────────────────────────────────

TEST(CrisisEngine, TriggersMoreOftenUnderStress) {
    int normalCrises = 0, stressedCrises = 0;

    for (int trial = 0; trial < 100; ++trial) {
        math::RandomEngine rng(trial);
        CrisisEngine engine(rng);
        Bank bank = Bank::create({});
        SimulationState state;

        // Normal conditions
        state.regime = MarketRegime::Normal;
        if (!bank.divisions.empty()) bank.divisions[0].hiddenRisk = 0;
        if (engine.checkTrigger(bank, state)) normalCrises++;
    }

    for (int trial = 0; trial < 100; ++trial) {
        math::RandomEngine rng(trial + 1000);
        CrisisEngine engine(rng);
        Bank bank = Bank::create({});
        if (!bank.divisions.empty()) {
            bank.divisions[0].hiddenRisk = 2e9; // $2B hidden risk
            bank.divisions[0].autonomyLevel = 0.9;
        }
        SimulationState state;
        state.regime = MarketRegime::Stressed;
        if (engine.checkTrigger(bank, state)) stressedCrises++;
    }

    // Stressed + high hidden risk should trigger more crises
    EXPECT_GT(stressedCrises, normalCrises);
}

// ── Scoring Tests ───────────────────────────────────────────────────

TEST(Scoring, CalculatesReasonableScore) {
    Bank bank = Bank::create({});
    bank.netIncome = 50e6; // $50M profit
    bank.capital = 10e9;

    QuarterScore score;
    score.calculate(bank, false, 3, 5);

    EXPECT_GT(score.financialScore, 5);  // Positive (ROE-based, no free baseline)
    EXPECT_GT(score.riskScore, 50);      // No hidden risk
    EXPECT_GT(score.total, 15);
    EXPECT_GT(score.xpEarned, 0);
}

TEST(Scoring, ProgressionLevelsUp) {
    PlayerProgression prog;
    EXPECT_EQ(prog.level, 1);
    EXPECT_EQ(prog.title, "Teller");

    prog.addExperience(100); // First level: 100 XP needed
    EXPECT_EQ(prog.level, 2);
    EXPECT_EQ(prog.skillPoints, 2);

    prog.addExperience(1000);
    EXPECT_GT(prog.level, 2);
    EXPECT_NE(prog.title, "Teller");
}

// ── Diagnostic: Print Full Game Report ──────────────────────────────

TEST(Diagnostic, PrintFullGameReport) {
    SimulationConfig cfg;
    cfg.seed = 42;
    cfg.quarterDurationDays = 63;
    QuarterlyTurnManager mgr(cfg);

    std::cout << "\n"
        << "══════════════════════════════════════════════════════════\n"
        << "  STVG FULL GAME SIMULATION (4 Quarters)\n"
        << "  Seed: 42 | Bank: " << mgr.bank().name << "\n"
        << "══════════════════════════════════════════════════════════\n";

    for (int q = 0; q < 4; ++q) {
        mgr.advancePhase(); // -> DecisionPhase

        std::cout << "\n──── " << mgr.state().currentYear
                  << " Q" << mgr.state().currentQuarter << " ────\n";
        std::cout << "  Events: " << mgr.quarterEvents().size()
                  << " | Decisions: " << mgr.pendingDecisions().size()
                  << " | Action Points: " << mgr.actionPoints().remaining() << "\n";

        // Print character messages
        std::cout << "\n  ADVISOR BRIEFINGS:\n";
        for (const auto& msg : mgr.messages()) {
            std::cout << "  [" << msg.role << "] " << msg.characterName
                      << " (" << msg.tone << "): "
                      << msg.content.substr(0, 80) << "...\n";
        }

        // Print decisions
        std::cout << "\n  DECISIONS:\n";
        for (const auto& dec : mgr.pendingDecisions()) {
            std::cout << "  * " << dec.title << " [" << dec.actionPointCost << " AP]\n";
            for (const auto& opt : dec.options) {
                int support = 0, oppose = 0;
                for (const auto& r : opt.recommendations) {
                    if (r.recommendation == "Support") support++;
                    if (r.recommendation == "Oppose") oppose++;
                }
                std::cout << "    -> " << opt.title
                          << " (success: " << (int)(opt.successProbability * 100) << "%"
                          << ", advisors: +" << support << "/-" << oppose << ")\n";
            }
        }

        // Submit first decision
        if (!mgr.pendingDecisions().empty()) {
            const auto& dec = mgr.pendingDecisions()[0];
            mgr.submitDecision(dec.id, dec.options[0].id);
        }

        mgr.advancePhase(); // -> Simulation
        mgr.advancePhase(); // -> QuarterEnd

        // Print report
        const auto& report = mgr.lastReport();
        std::cout << "\n  QUARTERLY REPORT:\n";
        std::cout << "  Revenue: $" << std::fixed << std::setprecision(1)
                  << (report.totalRevenue / 1e6) << "M\n";
        std::cout << "  Costs:   $" << (report.totalCosts / 1e6) << "M\n";
        std::cout << "  Net:     $" << (report.netIncome / 1e6) << "M\n";
        std::cout << "  Capital: $" << (mgr.bank().capital / 1e9) << "B\n";
        std::cout << "  Visibility: " << std::setprecision(1)
                  << mgr.bank().visibilityPct() << "%\n";
        std::cout << "  Score: " << std::setprecision(1) << report.score.total
                  << " | XP: " << report.score.xpEarned
                  << " | Level: " << report.progression.level
                  << " (" << report.progression.title << ")\n";

        for (const auto& headline : report.headlines) {
            std::cout << "  HEADLINE: " << headline << "\n";
        }

        if (mgr.currentPhase() == TurnPhase::CrisisResponse) {
            std::cout << "  *** CRISIS ACTIVE ***\n";
            mgr.advancePhase();
        }

        mgr.advancePhase(); // -> back to QuarterStart
    }

    std::cout << "\n══════════════════════════════════════════════════════════\n";
    std::cout << "  FINAL: Level " << mgr.progression().level
              << " | XP: " << mgr.progression().totalExperience
              << " | Capital: $" << std::setprecision(1) << (mgr.bank().capital / 1e9) << "B\n";
    std::cout << "══════════════════════════════════════════════════════════\n";
}
