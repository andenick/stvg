#include <gtest/gtest.h>
#include <stvg/core/PathEngine.h>

using namespace stvg;

TEST(PathEngine, DefaultArchetypeIsConservativeBank) {
    PathEngine engine;
    // Default: riskCulture=0.2, internationalFocus=0.0 → conservative
    EXPECT_EQ(engine.state().archetype(), "conservative_bank");
}

TEST(PathEngine, DecisionShiftsAxes) {
    PathEngine engine;
    double before = engine.state().riskCulture;

    PathDecision d;
    d.year = 1955;
    d.id = "hire_aggressive_trader";
    d.description = "Hired an aggressive trader for the lending desk";
    d.axisShifts = {{"riskCulture", 0.15}};
    engine.recordDecision(d);

    EXPECT_GT(engine.state().riskCulture, before);
}

TEST(PathEngine, HireEffectShiftsRiskCulture) {
    PathEngine engine;
    double before = engine.state().riskCulture;

    engine.applyHireEffect("aggressive");
    EXPECT_GT(engine.state().riskCulture, before);

    engine.applyHireEffect("conservative");
    // Should have moved back down slightly
    double after = engine.state().riskCulture;
    EXPECT_LT(after, before + 0.03); // Net effect is small
}

TEST(PathEngine, TradingDeskRequiresRiskCulture) {
    PathEngine engine;
    // Default riskCulture = 0.2, need >= 0.3
    EXPECT_FALSE(engine.canOpenDivision(DivisionType::TradingDesk));

    // Push risk culture up
    PathDecision d{1975, "risk_push", "Aggressive expansion", {{"riskCulture", 0.15}}};
    engine.recordDecision(d);
    EXPECT_TRUE(engine.canOpenDivision(DivisionType::TradingDesk));
}

TEST(PathEngine, CommercialLendingAlwaysAvailable) {
    PathEngine engine;
    EXPECT_TRUE(engine.canOpenDivision(DivisionType::CommercialLending));
    EXPECT_TRUE(engine.canOpenDivision(DivisionType::MortgageLending));
}

TEST(PathEngine, DerivativesDeskRequiresHighRiskAndInnovation) {
    PathEngine engine;
    EXPECT_FALSE(engine.canOpenDivision(DivisionType::DerivativesDesk));

    PathDecision d1{1995, "risk_up", "", {{"riskCulture", 0.35}}};
    PathDecision d2{1996, "innov_up", "", {{"innovationBias", 0.15}}};
    engine.recordDecision(d1);
    engine.recordDecision(d2);
    EXPECT_TRUE(engine.canOpenDivision(DivisionType::DerivativesDesk));
}

TEST(PathEngine, TradingFirmConversionRequiresHighRisk) {
    PathEngine engine;
    EXPECT_FALSE(engine.canConvertToTradingFirm());

    // Push risk culture to 0.7+
    for (int i = 0; i < 5; ++i) {
        PathDecision d{1980 + i, "risk_" + std::to_string(i), "", {{"riskCulture", 0.12}}};
        engine.recordDecision(d);
    }
    // Also need "trading_capability" unlocked (at 0.5 risk)
    EXPECT_TRUE(engine.state().unlockedPaths.count("trading_capability") > 0);
    EXPECT_TRUE(engine.canConvertToTradingFirm());
}

TEST(PathEngine, ArchetypeChangesWithAxes) {
    PathEngine engine;
    EXPECT_EQ(engine.state().archetype(), "conservative_bank");

    // Push to trading firm: need riskCulture > 0.7 and innovationBias > 0.5
    PathDecision d1{1985, "risk1", "", {{"riskCulture", 0.3}}};
    PathDecision d2{1986, "risk2", "", {{"riskCulture", 0.25}, {"innovationBias", 0.25}}};
    engine.recordDecision(d1);
    engine.recordDecision(d2);
    // riskCulture = 0.2 + 0.3 + 0.25 = 0.75 > 0.7 ✓
    // innovationBias = 0.3 + 0.25 = 0.55 > 0.5 ✓
    EXPECT_EQ(engine.state().archetype(), "trading_firm");
}

TEST(PathEngine, RevenueMultiplierForAlignedDivisions) {
    PathEngine engine;
    // Default: low risk culture → commercial lending gets bonus
    double commMult = engine.revenueMultiplier(DivisionType::CommercialLending);
    EXPECT_GT(commMult, 1.0); // Conservative bank lends better

    // Push risk culture up → trading gets bonus instead
    PathDecision d{1985, "risk", "", {{"riskCulture", 0.4}}};
    engine.recordDecision(d);
    double tradeMult = engine.revenueMultiplier(DivisionType::TradingDesk);
    EXPECT_GT(tradeMult, 1.0);
}

TEST(PathEngine, PathClosesOnHighRisk) {
    PathEngine engine;
    for (int i = 0; i < 6; ++i) {
        PathDecision d{1980 + i, "r" + std::to_string(i), "", {{"riskCulture", 0.1}}};
        engine.recordDecision(d);
    }
    EXPECT_TRUE(engine.state().closedPaths.count("conservative_reputation") > 0);
}

TEST(PathEngine, SerializesToJson) {
    PathEngine engine;
    PathDecision d{1950, "test", "Test decision", {{"riskCulture", 0.05}}};
    engine.recordDecision(d);

    nlohmann::json j = engine.state();
    EXPECT_TRUE(j.contains("riskCulture"));
    EXPECT_TRUE(j.contains("archetype"));
    EXPECT_TRUE(j.contains("majorDecisions"));
    EXPECT_EQ(j["majorDecisions"].size(), 1);
}
