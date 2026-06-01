#include <gtest/gtest.h>
#include "stvg/game/CEOAbilityHandler.h"
#include "stvg/core/BankState.h"
#include "stvg/core/CEOProfile.h"
#include "stvg/simulation/PoliticalEngine.h"
#include "stvg/math/RandomEngine.h"

using namespace stvg;
using namespace stvg::game;

class CEOAbilityTest : public ::testing::Test {
protected:
    math::RandomEngine rng_{42};
    simulation::PoliticalEngine political_{rng_};
    Bank bank_ = Bank::create({});
    ActionPointBudget ap_;

    int cooldown_ = 0;
    bool abilityActive_ = false;
    bool revenueBoost_ = false;
    bool regShield_ = false;
    bool crisisShield_ = false;

    CEOAbilityHandler::AbilityState makeState() {
        return {cooldown_, abilityActive_, revenueBoost_, regShield_, crisisShield_};
    }

    void SetUp() override {
        ap_.reset(bank_.totalAssets);
    }
};

TEST_F(CEOAbilityTest, SterlingActivatesRevenueBoost) {
    auto* ceo = CEOProfile::findById("sterling");
    ASSERT_NE(ceo, nullptr);

    bool ok = CEOAbilityHandler::useAbility(*ceo, bank_, ap_, political_, makeState());
    EXPECT_TRUE(ok);
    EXPECT_TRUE(revenueBoost_);
    EXPECT_TRUE(abilityActive_);
    EXPECT_GT(cooldown_, 0);
}

TEST_F(CEOAbilityTest, BancroftActivatesCrisisShield) {
    auto* ceo = CEOProfile::findById("bancroft");
    ASSERT_NE(ceo, nullptr);

    bool ok = CEOAbilityHandler::useAbility(*ceo, bank_, ap_, political_, makeState());
    EXPECT_TRUE(ok);
    EXPECT_TRUE(crisisShield_);
}

TEST_F(CEOAbilityTest, OkonkwoInspectsAllDivisions) {
    auto* ceo = CEOProfile::findById("okonkwo");
    ASSERT_NE(ceo, nullptr);

    for (auto& div : bank_.divisions) div.inspectedThisQuarter = false;

    bool ok = CEOAbilityHandler::useAbility(*ceo, bank_, ap_, political_, makeState());
    EXPECT_TRUE(ok);
    for (const auto& div : bank_.divisions) {
        EXPECT_TRUE(div.inspectedThisQuarter) << "Division " << div.name << " not inspected";
    }
}

TEST_F(CEOAbilityTest, SullivanResetsHeadHonesty) {
    auto* ceo = CEOProfile::findById("sullivan");
    ASSERT_NE(ceo, nullptr);

    bank_.divisions[0].headHonesty = HeadHonesty::SelfServing;

    bool ok = CEOAbilityHandler::useAbility(*ceo, bank_, ap_, political_, makeState());
    EXPECT_TRUE(ok);
    for (const auto& div : bank_.divisions) {
        EXPECT_EQ(div.headHonesty, HeadHonesty::Honest);
    }
}

TEST_F(CEOAbilityTest, FailsOnCooldown) {
    auto* ceo = CEOProfile::findById("sterling");
    ASSERT_NE(ceo, nullptr);

    cooldown_ = 3;
    bool ok = CEOAbilityHandler::useAbility(*ceo, bank_, ap_, political_, makeState());
    EXPECT_FALSE(ok);
}

TEST_F(CEOAbilityTest, FailsWithInsufficientAP) {
    auto* ceo = CEOProfile::findById("sterling");
    ASSERT_NE(ceo, nullptr);

    ap_.spent = ap_.total; // exhaust AP
    bool ok = CEOAbilityHandler::useAbility(*ceo, bank_, ap_, political_, makeState());
    EXPECT_FALSE(ok);
}

TEST_F(CEOAbilityTest, WebbRequiresPoliticalCapital) {
    auto* ceo = CEOProfile::findById("webb");
    ASSERT_NE(ceo, nullptr);

    // Without political capital, should fail
    bool ok = CEOAbilityHandler::useAbility(*ceo, bank_, ap_, political_, makeState());
    EXPECT_FALSE(ok);
    // AP should be refunded
    EXPECT_EQ(ap_.remaining(), ap_.total);
}

TEST_F(CEOAbilityTest, WebbSucceedsWithPoliticalCapital) {
    auto* ceo = CEOProfile::findById("webb");
    ASSERT_NE(ceo, nullptr);

    political_.earnCapital(15.0, "test");

    bool ok = CEOAbilityHandler::useAbility(*ceo, bank_, ap_, political_, makeState());
    EXPECT_TRUE(ok);
    EXPECT_TRUE(regShield_);
}

TEST_F(CEOAbilityTest, Costs3AP) {
    auto* ceo = CEOProfile::findById("sterling");
    ASSERT_NE(ceo, nullptr);

    int before = ap_.remaining();
    CEOAbilityHandler::useAbility(*ceo, bank_, ap_, political_, makeState());
    EXPECT_EQ(ap_.remaining(), before - 3);
}
