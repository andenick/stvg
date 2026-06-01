#include <gtest/gtest.h>
#include <stvg/core/CEOProfile.h>
#include <stvg/core/QuarterlyTurnManager.h>

using namespace stvg;

// ── CEO Profile Tests ───────────────────────────────────────────────

TEST(CEOProfile, AllProfilesReturns25) {
    auto profiles = CEOProfile::allProfiles();
    EXPECT_EQ(profiles.size(), 24); // 5 fictional + 19 historical
}

TEST(CEOProfile, FindByIdReturnsCorrectProfiles) {
    EXPECT_NE(CEOProfile::findById("sterling"), nullptr);
    EXPECT_NE(CEOProfile::findById("webb"), nullptr);
    EXPECT_NE(CEOProfile::findById("okonkwo"), nullptr);
    EXPECT_NE(CEOProfile::findById("bancroft"), nullptr);
    EXPECT_NE(CEOProfile::findById("sullivan"), nullptr);
    EXPECT_EQ(CEOProfile::findById("nonexistent"), nullptr);
}

TEST(CEOProfile, SterlingHasCorrectModifiers) {
    auto* p = CEOProfile::findById("sterling");
    ASSERT_NE(p, nullptr);
    EXPECT_DOUBLE_EQ(p->riskMultiplier, 1.3);
    EXPECT_DOUBLE_EQ(p->revenueMultiplier, 1.2);
    EXPECT_EQ(p->nickname, "The Gambler");
}

TEST(CEOProfile, SerializesToJson) {
    auto* p = CEOProfile::findById("webb");
    ASSERT_NE(p, nullptr);
    nlohmann::json j = *p;
    EXPECT_EQ(j["id"], "webb");
    EXPECT_EQ(j["nickname"], "The Politician");
    EXPECT_TRUE(j.contains("specialAbilityName"));
}

// ── CEO Integration with Turn Manager ───────────────────────────────

TEST(CEOGame, ConstructorAppliesCEOCapital) {
    SimulationConfig cfg;
    cfg.seed = 100;
    auto* sterling = CEOProfile::findById("sterling");
    ASSERT_NE(sterling, nullptr);

    QuarterlyTurnManager mgr(cfg, {}, "sterling");
    EXPECT_TRUE(mgr.hasCeo());
    EXPECT_EQ(mgr.ceoProfile().id, "sterling");
    EXPECT_DOUBLE_EQ(mgr.bank().capital, sterling->startingCapital);
}

TEST(CEOGame, NoCeoByDefault) {
    SimulationConfig cfg;
    cfg.seed = 101;
    QuarterlyTurnManager mgr(cfg);
    EXPECT_FALSE(mgr.hasCeo());
}

TEST(CEOGame, SpecialAbilitySpends3AP) {
    SimulationConfig cfg;
    cfg.seed = 102;
    QuarterlyTurnManager mgr(cfg, {}, "sterling");

    int apBefore = mgr.actionPoints().total - mgr.actionPoints().spent;
    ASSERT_GE(apBefore, 3); // Must have enough AP

    bool ok = mgr.useSpecialAbility();
    EXPECT_TRUE(ok);

    int apAfter = mgr.actionPoints().total - mgr.actionPoints().spent;
    EXPECT_EQ(apAfter, apBefore - 3);
}

TEST(CEOGame, SpecialAbilityHasCooldown) {
    SimulationConfig cfg;
    cfg.seed = 103;
    QuarterlyTurnManager mgr(cfg, {}, "sterling");

    EXPECT_TRUE(mgr.useSpecialAbility());
    EXPECT_GT(mgr.specialAbilityCooldownRemaining(), 0);

    // Second use should fail (on cooldown)
    EXPECT_FALSE(mgr.useSpecialAbility());
}

TEST(CEOGame, SpecialAbilityFailsWithoutCeo) {
    SimulationConfig cfg;
    cfg.seed = 104;
    QuarterlyTurnManager mgr(cfg);
    EXPECT_FALSE(mgr.useSpecialAbility());
}

TEST(CEOGame, OkonkwoInspectsAllDivisions) {
    SimulationConfig cfg;
    cfg.seed = 105;
    QuarterlyTurnManager mgr(cfg, {}, "okonkwo");

    mgr.useSpecialAbility();

    // All divisions should be inspected
    for (const auto& div : mgr.bank().divisions) {
        EXPECT_TRUE(div.inspectedThisQuarter);
    }
}

TEST(CEOGame, SullivanResetsHeadHonesty) {
    SimulationConfig cfg;
    cfg.seed = 106;
    QuarterlyTurnManager mgr(cfg, {}, "sullivan");

    mgr.useSpecialAbility();

    for (const auto& div : mgr.bank().divisions) {
        EXPECT_EQ(div.headHonesty, HeadHonesty::Honest);
    }
}

TEST(CEOGame, BancroftBlocksCrises) {
    // Bancroft's Fortress Balance Sheet sets crisisShieldActive_
    // which should skip crisis generation in onQuarterEnd
    SimulationConfig cfg;
    cfg.seed = 107;
    QuarterlyTurnManager mgr(cfg, {}, "bancroft");

    EXPECT_TRUE(mgr.hasCeo());
    EXPECT_EQ(mgr.ceoProfile().id, "bancroft");
    // Low risk multiplier
    EXPECT_LT(mgr.ceoProfile().riskMultiplier, 1.0);
}
