#include <gtest/gtest.h>
#include <stvg/core/RegulatoryEngine.h>
#include <stvg/core/QuarterlyTurnManager.h>

using namespace stvg;

// ── Regulatory Engine Unit Tests ────────────────────────────────────

TEST(RegulatoryEngine, InitializesWithRegulations) {
    RegulatoryEngine engine;
    engine.init();
    EXPECT_GE(engine.allRegulations().size(), 7);
}

TEST(RegulatoryEngine, GlassSteagallActiveIn1945) {
    RegulatoryEngine engine;
    engine.init();
    engine.updateForYear(1945);

    EXPECT_TRUE(engine.isGlassWallActive());
    EXPECT_TRUE(engine.isDivisionProhibited(DivisionType::InvestmentBanking));
    EXPECT_TRUE(engine.isDivisionProhibited(DivisionType::Securitization));
    EXPECT_TRUE(engine.isDivisionProhibited(DivisionType::DerivativesDesk));
    EXPECT_TRUE(engine.isDivisionProhibited(DivisionType::PrivateEquity));
}

TEST(RegulatoryEngine, GlassSteagallRepealedIn1999) {
    RegulatoryEngine engine;
    engine.init();
    engine.updateForYear(1999);

    EXPECT_FALSE(engine.isGlassWallActive());
    EXPECT_FALSE(engine.isDivisionProhibited(DivisionType::InvestmentBanking));
    EXPECT_FALSE(engine.isDivisionProhibited(DivisionType::Securitization));
}

TEST(RegulatoryEngine, BrettonWoodsActivePreNixon) {
    RegulatoryEngine engine;
    engine.init();
    engine.updateForYear(1965);

    EXPECT_TRUE(engine.isBrettonWoodsActive());
}

TEST(RegulatoryEngine, BrettonWoodsEndedPostNixon) {
    RegulatoryEngine engine;
    engine.init();
    engine.updateForYear(1972);

    EXPECT_FALSE(engine.isBrettonWoodsActive());
}

TEST(RegulatoryEngine, NoCapitalRequirementBefore1988) {
    RegulatoryEngine engine;
    engine.init();
    engine.updateForYear(1985);

    EXPECT_EQ(engine.effectiveCapitalRequirement(), 0);
}

TEST(RegulatoryEngine, BaselICapitalRequirement) {
    RegulatoryEngine engine;
    engine.init();
    engine.updateForYear(1990);

    EXPECT_DOUBLE_EQ(engine.effectiveCapitalRequirement(), 0.08);
}

TEST(RegulatoryEngine, BaselIIIHigherRequirement) {
    RegulatoryEngine engine;
    engine.init();
    engine.updateForYear(2015);

    EXPECT_DOUBLE_EQ(engine.effectiveCapitalRequirement(), 0.105);
}

TEST(RegulatoryEngine, DoddFrankVolckerRule) {
    RegulatoryEngine engine;
    engine.init();
    engine.updateForYear(2015);

    EXPECT_TRUE(engine.isVolckerRuleActive());
}

TEST(RegulatoryEngine, ComplianceCostAccumulates) {
    RegulatoryEngine engine;
    engine.init();

    // 1950: Glass-Steagall + Reg Q + Bretton Woods
    engine.updateForYear(1950);
    double cost1950 = engine.state().totalComplianceCostPct;

    // 2015: Basel III + Dodd-Frank + more
    engine.updateForYear(2015);
    double cost2015 = engine.state().totalComplianceCostPct;

    EXPECT_GT(cost2015, cost1950); // Post-GFC compliance much higher
}

TEST(RegulatoryEngine, ViolationsAccumulate) {
    RegulatoryEngine engine;
    engine.init();
    engine.updateForYear(2015);

    EXPECT_FALSE(engine.state().underInvestigation);

    engine.recordViolation("Exceeded leverage limit");
    engine.recordViolation("Late stress test filing");
    EXPECT_EQ(engine.state().violations, 2);
    EXPECT_FALSE(engine.state().underInvestigation);

    engine.recordViolation("Capital adequacy breach");
    EXPECT_EQ(engine.state().violations, 3);
    EXPECT_TRUE(engine.state().underInvestigation);
}

TEST(RegulatoryEngine, ComplianceCheck) {
    RegulatoryEngine engine;
    engine.init();
    engine.updateForYear(2015); // Basel III active

    Bank goodBank = Bank::create({});
    goodBank.capital = 20e9;
    goodBank.totalAssets = 100e9; // 20% ratio > 10.5%
    EXPECT_TRUE(engine.checkCompliance(goodBank));

    Bank badBank = Bank::create({});
    badBank.capital = 5e9;
    badBank.totalAssets = 100e9; // 5% ratio < 10.5%
    EXPECT_FALSE(engine.checkCompliance(badBank));
}

TEST(RegulatoryEngine, SerializesToJson) {
    RegulatoryEngine engine;
    engine.init();
    engine.updateForYear(1950);

    nlohmann::json j = engine.state();
    EXPECT_TRUE(j.contains("glassWallActive"));
    EXPECT_TRUE(j["glassWallActive"].get<bool>());
    EXPECT_TRUE(j.contains("brettonWoodsActive"));
    EXPECT_TRUE(j.contains("activeRegulationIds"));
}

// ── Integration ─────────────────────────────────────────────────────

TEST(RegulatoryIntegration, TurnManagerHasRegulatory) {
    SimulationConfig cfg;
    cfg.seed = 400;
    QuarterlyTurnManager mgr(cfg);

    auto state = mgr.regulatoryState();
    EXPECT_TRUE(state.glassWallActive);    // 1945
    EXPECT_TRUE(state.brettonWoodsActive); // 1945

    auto j = mgr.toPlayerJson();
    EXPECT_TRUE(j.contains("regulatory"));
    EXPECT_TRUE(j["regulatory"]["glassWallActive"].get<bool>());
}
