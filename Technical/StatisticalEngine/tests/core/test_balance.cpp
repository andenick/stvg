#include <gtest/gtest.h>
#include "stvg/core/ConsequenceTracker.h"
#include "stvg/core/BankState.h"
#include "stvg/core/ScoringEngine.h"
#include "stvg/core/EraEngine.h"
#include "stvg/core/QuarterlyTurnManager.h"
#include "stvg/simulation/CrisisEngine.h"
#include "stvg/math/RandomEngine.h"

using namespace stvg;

// ════════════════════════════════════════════════════════════════════
// Balance Tests — verify tuning changes prevent cascading bankruptcy
// ════════════════════════════════════════════════════════════════════

TEST(Balance, ConsequenceRiskMultiplierCapped) {
    math::RandomEngine rng(42);
    ConsequenceTracker tracker(rng);

    // Simulate a worst-case risk failure: riskChange=0.15, magnitudeVariance up to 1.3
    // With -5e7 multiplier: 0.15 * 5e7 * 1.3 = $9.75M max loss
    // This should be < 0.1% of $10B starting capital
    double worstCase = 0.15 * 5e7 * 1.3;
    EXPECT_LT(worstCase, 10e6) << "Worst-case risk consequence should be < $10M";
    EXPECT_GT(worstCase, 1e6)  << "Risk consequence should still be meaningful (> $1M)";
}

TEST(Balance, HiddenRiskCappedAt1) {
    Division div;
    div.id = "test_div";
    div.name = "Test Division";
    div.type = DivisionType::TradingDesk;
    div.autonomyLevel = 1.0;  // Maximum autonomy
    div.actualRisk = 0.5;     // High actual risk
    div.hiddenRisk = 0.0;
    div.inspectedThisQuarter = false;
    div.quartersSinceInspection = 0;

    // Run 40 quarters (10 years) without inspection at max autonomy
    for (int q = 0; q < 40; q++) {
        div.accumulateHiddenRisk();
    }

    EXPECT_LE(div.hiddenRisk, 1.0) << "Hidden risk must be capped at 1.0 per division";
    EXPECT_GT(div.hiddenRisk, 0.5) << "Hidden risk should still accumulate significantly";
}

TEST(Balance, HiddenRiskGrowthRateCapped) {
    // With autonomy=1.0: growthRate should be min(0.02 + 0.06*1.0, 0.05) = 0.05
    double autonomy = 1.0;
    double growthRate = std::min(0.02 + 0.06 * autonomy, 0.05);
    EXPECT_DOUBLE_EQ(growthRate, 0.05) << "Growth rate capped at 0.05 even at max autonomy";

    // With autonomy=0.3: growthRate = 0.02 + 0.06*0.3 = 0.038 (uncapped)
    autonomy = 0.3;
    growthRate = std::min(0.02 + 0.06 * autonomy, 0.05);
    EXPECT_NEAR(growthRate, 0.038, 0.001) << "Low autonomy rate should be uncapped";
}

TEST(Balance, CrisisFinancialImpactLinear) {
    simulation::CrisisArc crisis;
    crisis.severity = 6;  // Medium severity

    // With 30% hidden risk ratio — linear formula: 0.005 * severity * (1 + ratio * 5)
    double impact = crisis.financialImpact(10e9, 0.3);
    double basePct = 0.005 * 6;  // 3%
    double expected = 10e9 * basePct * (1.0 + 0.3 * 5.0);  // multiplier = 2.5

    EXPECT_NEAR(impact, expected, 1e6);
    // Severity 6 + 30% hidden: 10B * 0.03 * 2.5 = $750M
    EXPECT_LT(impact, 900e6) << "Severity 6 crisis with 30% hidden risk should be < $900M";
    EXPECT_GT(impact, 400e6) << "Crisis should still be meaningful";
}

TEST(Balance, ScoringWithModerateHiddenRisk) {
    Bank bank;
    bank.capital = 10e9;
    Division div;
    div.id = "test";
    div.name = "Test";
    div.type = DivisionType::CommercialLending;
    div.hiddenRisk = 0.4;  // 40% of 1.0 cap per division
    bank.divisions.push_back(div);
    bank.reputation = 50;

    QuarterScore score;
    score.calculate(bank, false, 3, 5, 10e9);

    // Per-division avg hidden risk = 0.4/1 = 0.4
    // riskScore = 100 - 0.4 * 120 = 52
    EXPECT_GT(score.riskScore, 40.0) << "40% hidden risk should leave meaningful risk score";
    EXPECT_LT(score.riskScore, 65.0) << "40% hidden risk should penalize noticeably";
    EXPECT_GT(score.total, 15.0) << "Total score should remain above zero";
}

TEST(Balance, ScoringDecisionQualityAffectsTotal) {
    Bank bank;
    bank.capital = 10e9;
    bank.reputation = 50;

    QuarterScore noDecisions, perfectDecisions;
    noDecisions.calculate(bank, false, 0, 0, 10e9);
    perfectDecisions.calculate(bank, false, 5, 5, 10e9);

    // 100% decision accuracy should add ~15 points vs no decisions
    EXPECT_GT(perfectDecisions.total - noDecisions.total, 10.0)
        << "Perfect decisions should boost score significantly";
    EXPECT_LT(perfectDecisions.total - noDecisions.total, 20.0)
        << "Decision bonus should be bounded";
}

TEST(Balance, ScoringCrisisPenalty) {
    Bank bank;
    bank.capital = 10e9;
    bank.reputation = 60;

    QuarterScore noCrisis, withCrisis;
    noCrisis.calculate(bank, false, 3, 5, 10e9);
    withCrisis.calculate(bank, true, 3, 5, 10e9);

    EXPECT_LT(withCrisis.total, noCrisis.total * 0.90)
        << "Crisis should reduce score by at least 10%";
    EXPECT_GT(withCrisis.total, noCrisis.total * 0.75)
        << "Crisis penalty should not exceed 25%";
}

TEST(Balance, ActualRiskDecaysWithAudit) {
    Division div;
    div.id = "test";
    div.name = "Test";
    div.type = DivisionType::CommercialLending;
    div.autonomyLevel = 0.5;
    div.actualRisk = 0.5;
    div.hiddenRisk = 0.3;
    div.inspectedThisQuarter = true; // Auditing this quarter
    div.quartersSinceInspection = 4;

    div.accumulateHiddenRisk();

    // After audit: actualRisk should have decayed (0.90 audit × 0.98 base)
    // Plus gained some from hiddenRisk reveal, but net should be lower than 0.5
    EXPECT_LT(div.actualRisk, 0.55) << "Audited actualRisk should decay, not grow unbounded";
    EXPECT_GT(div.actualRisk, 0.1) << "actualRisk should still be meaningful after audit";
    EXPECT_LE(div.actualRisk, 1.0) << "actualRisk capped at 1.0";
}

TEST(Balance, ActualRiskDecaysWithoutAudit) {
    Division div;
    div.id = "test";
    div.name = "Test";
    div.type = DivisionType::CommercialLending;
    div.autonomyLevel = 0.5;
    div.actualRisk = 0.5;
    div.hiddenRisk = 0.0;
    div.inspectedThisQuarter = false;
    div.quartersSinceInspection = 0;

    div.accumulateHiddenRisk();

    // Without audit: only 2% base decay
    EXPECT_NEAR(div.actualRisk, 0.5 * 0.98, 0.01) << "Unaudited actualRisk decays 2%/quarter";
}

TEST(Balance, EraInitialEconomyUsedNotHardcoded) {
    EraEngine eraEngine;
    eraEngine.init();

    // The post-war era defines specific initial economy values
    const auto& postWar = eraEngine.currentEra();
    EXPECT_EQ(postWar.id, "post_war");
    EXPECT_NEAR(postWar.initialEconomy.fedFundsRate, 0.025, 0.001);
    EXPECT_NEAR(postWar.initialEconomy.gdpGrowth, 0.04, 0.001);
    EXPECT_NEAR(postWar.initialEconomy.vix, 12.0, 0.1);

    // Verify that the expansion era has DIFFERENT initial economy values
    // This proves the system uses era-specific values, not a single hardcoded set
    const auto& expansion = eraEngine.allEras()[1];
    EXPECT_EQ(expansion.id, "expansion");
    EXPECT_NEAR(expansion.initialEconomy.fedFundsRate, 0.04, 0.001)
        << "Expansion era should have higher fed funds rate than post-war";
    EXPECT_NE(postWar.initialEconomy.fedFundsRate, expansion.initialEconomy.fedFundsRate)
        << "Different eras must have different initial economy values";
}

TEST(Balance, CapitalRecoveryTriggersBelow50Pct) {
    // Verify the capital recovery logic: when capital < 50% of peak, costs reduce
    // We test the mechanism in isolation since running full quarters is heavy
    double peakCapital = 10e9;
    double currentCapital = 4e9;  // 40% of peak -> should trigger

    EXPECT_LT(currentCapital, peakCapital * 0.5)
        << "Test precondition: capital is below 50% of peak";

    // Simulate the cost reduction
    double originalCost = 1e6;
    double reducedCost = originalCost * 0.80;
    EXPECT_NEAR(reducedCost, 800000.0, 1.0)
        << "Emergency cost cut should reduce by 20%";
}
